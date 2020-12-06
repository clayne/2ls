/*******************************************************************\

Module: Instrument Goto Program with Inferred Information

Author: Peter Schrammel, Björn Wachter

\*******************************************************************/

/// \file
/// Instrument Goto Program with Inferred Information

#include <util/string2int.h>

// #define DEBUG

#ifdef DEBUG
#include <iostream>
#endif

#include  "instrument_goto.h"

local_SSAt::locationt find_loop_by_guard(
  const local_SSAt &SSA,
  const symbol_exprt &guard)
{
  std::string gstr=id2string(guard.get_identifier());
  unsigned pos1=gstr.find("#")+1;
  unsigned pos2=gstr.find("%", pos1);
  unsigned n=safe_string2unsigned(gstr.substr(pos1, pos2));

  local_SSAt::nodest::const_iterator n_it=SSA.nodes.begin();

  for(; n_it!=SSA.nodes.end(); n_it++)
  {
    if(n_it->location->location_number==n)
    {
      // find end of loop
      break;
    }
  }

  if(n_it->loophead==SSA.nodes.end())
    return n_it->location;
  else
    return n_it->loophead->location;
}

void instrument_gotot::instrument_instruction(
  const exprt &expr,
  goto_programt &dest,
  goto_programt::targett &target)
{
  goto_programt::targett where=target;

#ifdef DEBUG
  std::cout << "target " << target->type << " : "
            << target->source_location << std::endl;
#endif

  for(; ; ++where)
  {
    if(where->is_goto() && where->get_target()==target)
      break;
  }

  goto_programt tmp;

  goto_programt::targett assumption=tmp.add_instruction();
  assumption->make_assumption(expr);
  assumption->source_location=target->source_location;
  assumption->source_location.set_comment("invariant generated by 2LS");

  dest.insert_before_swap(where, tmp);

#ifdef DEBUG
  std::cout << "instrumenting instruction " << std::endl;
#endif
}

extern void purify_identifiers(exprt &expr);

void instrument_gotot::instrument_body(
  const local_SSAt &SSA,
  const exprt &expr,
  goto_functionst::goto_functiont &function)
{
  // expected format (/\_j g_j)=> inv
  const exprt &impl=expr.op0();
  exprt inv=expr.op1(); // copy

  std::cout << "Invariant " << from_expr(inv) << std::endl;

  purify_identifiers(inv);

  local_SSAt::locationt loc;
  if(impl.id()==ID_symbol)
  {
    loc=find_loop_by_guard(SSA, to_symbol_expr(impl));
  }
  else if(impl.id()==ID_and)
  {
    assert(impl.op0().id()==ID_symbol);
    loc=find_loop_by_guard(SSA, to_symbol_expr(impl.op0()));
  }
  else
    assert(false);

  Forall_goto_program_instructions(it, function.body)
  {
    if(it==loc)
    {
      instrument_instruction(inv, function.body, it);
      break;
    }
  }
}

void instrument_gotot::instrument_function(
  const irep_idt &function_name,
  goto_functionst::goto_functiont &function)
{
  #ifdef DEBUG
  std::cout << "instrumenting function " << function_name << std::endl;
  #endif

  if(!summary_db.exists(function_name))
    return;

  const summaryt &summary=summary_db.get(function_name);

  if(!ssa_db.exists(function_name))
    return;

  const local_SSAt &SSA=ssa_db.get(function_name);

  if(summary.fw_invariant.is_nil())
    return;
  if(summary.fw_invariant.is_true())
    return;

  // expected format /\_i g_i=> inv_i
  if(summary.fw_invariant.id()==ID_implies)
  {
    instrument_body(SSA, summary.fw_invariant, function);
  }
  else if(summary.fw_invariant.id()==ID_and)
  {
    for(unsigned i=0; i<summary.fw_invariant.operands().size(); i++)
    {
      assert(summary.fw_invariant.operands()[i].id()==ID_implies);
      instrument_body(SSA, summary.fw_invariant.operands()[i], function);
    }
  }
  else
    assert(false);
}

void instrument_gotot::operator()(goto_modelt &goto_model)
{
  goto_functionst &goto_functions=goto_model.goto_functions;

  typedef goto_functions_templatet<goto_programt>::function_mapt
    function_mapt;

  function_mapt &function_map=goto_functions.function_map;

  for(function_mapt::iterator
      fit=function_map.begin();
      fit!=function_map.end();
      ++fit)
  {
    instrument_function(fit->first, fit->second);
  }

  goto_model.goto_functions.update();
}
