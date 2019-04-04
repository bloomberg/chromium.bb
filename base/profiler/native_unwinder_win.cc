// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/native_unwinder_win.h"

#include "base/profiler/native_unwinder.h"
#include "base/profiler/win32_stack_frame_unwinder.h"

namespace base {

bool NativeUnwinderWin::CanUnwindFrom(const Frame* current_frame) const {
  return current_frame->module;
}

// Attempts to unwind the frame represented by the context values. If
// successful appends frames onto the stack and returns true. Otherwise
// returns false.
UnwindResult NativeUnwinderWin::TryUnwind(RegisterContext* thread_context,
                                          uintptr_t stack_top,
                                          ModuleCache* module_cache,
                                          std::vector<Frame>* stack) const {
  // We expect the frame correponding to the |thread_context| register state to
  // exist within |stack|.
  DCHECK_GT(stack->size(), 0u);

  Win32StackFrameUnwinder frame_unwinder;
  for (;;) {
    if (!stack->back().module) {
      // There's no loaded module corresponding to the current frame. This can
      // be due to executing code that is not in a module (e.g. V8 generated
      // code or runtime-generated code associated with third-party injected
      // DLLs). It can also be due to the the module having been unloaded since
      // we recorded the stack.  In the latter case the function unwind
      // information was part of the unloaded module, so it's not possible to
      // unwind further.
      //
      // If a module was found, it's still theoretically possible for the
      // detected module module to be different than the one that was loaded
      // when the stack was copied (i.e. if the module was unloaded and a
      // different module loaded in overlapping memory). This likely would cause
      // a crash, but has not been observed in practice.
      //
      // We return UNRECOGNIZED_FRAME on the optimistic assumption that this may
      // be a frame the AuxUnwinder knows how to handle (e.g. a frame in V8
      // generated code).
      return UnwindResult::UNRECOGNIZED_FRAME;
    }

    if (!frame_unwinder.TryUnwind(stack->size() == 1u, thread_context,
                                  stack->back().module)) {
      return UnwindResult::ABORTED;
    }

    if (ContextPC(thread_context) == 0)
      return UnwindResult::COMPLETED;

    // Record the frame to which we just unwound.
    stack->emplace_back(
        ContextPC(thread_context),
        module_cache->GetModuleForAddress(ContextPC(thread_context)));
  }

  NOTREACHED();
  return UnwindResult::COMPLETED;
}

std::unique_ptr<Unwinder> CreateNativeUnwinder(ModuleCache* module_cache) {
  return std::make_unique<NativeUnwinderWin>();
}

}  // namespace base
