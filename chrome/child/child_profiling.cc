// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/child/child_profiling.h"

#include "base/debug/profiler.h"
#include "base/logging.h"
#include "gin/public/debug.h"
#include "v8/include/v8.h"

namespace {

base::debug::AddDynamicSymbol add_dynamic_symbol_func = NULL;
base::debug::MoveDynamicSymbol move_dynamic_symbol_func = NULL;

void JitCodeEventHandler(const v8::JitCodeEvent* event) {
  DCHECK_NE(static_cast<base::debug::AddDynamicSymbol>(NULL),
            add_dynamic_symbol_func);
  DCHECK_NE(static_cast<base::debug::MoveDynamicSymbol>(NULL),
            move_dynamic_symbol_func);

  switch (event->type) {
    case v8::JitCodeEvent::CODE_ADDED:
      add_dynamic_symbol_func(event->code_start, event->code_len,
                              event->name.str, event->name.len);
      break;

    case v8::JitCodeEvent::CODE_MOVED:
      move_dynamic_symbol_func(event->code_start, event->new_code_start);
      break;

    default:
      break;
  }
}

}  // namespace

// static
void ChildProfiling::ProcessStarted() {
  // Establish the V8 profiling hooks if we're an instrumented binary.
  if (base::debug::IsBinaryInstrumented()) {
    base::debug::ReturnAddressLocationResolver resolve_func =
        base::debug::GetProfilerReturnAddrResolutionFunc();

    if (resolve_func != NULL) {
      v8::V8::SetReturnAddressLocationResolver(resolve_func);
    }

    // Set up the JIT code entry handler and the symbol callbacks if the
    // profiler supplies them.
    // TODO(siggi): Maybe add a switch or an environment variable to turn off
    //     V8 profiling?
    base::debug::DynamicFunctionEntryHook entry_hook_func =
        base::debug::GetProfilerDynamicFunctionEntryHookFunc();
    add_dynamic_symbol_func = base::debug::GetProfilerAddDynamicSymbolFunc();
    move_dynamic_symbol_func = base::debug::GetProfilerMoveDynamicSymbolFunc();

    if (entry_hook_func != NULL &&
        add_dynamic_symbol_func != NULL &&
        move_dynamic_symbol_func != NULL) {
      gin::Debug::SetFunctionEntryHook(entry_hook_func);
      gin::Debug::SetJitCodeEventHandler(&JitCodeEventHandler);
    }
  }
}
