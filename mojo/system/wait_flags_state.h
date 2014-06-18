// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SYSTEM_WAIT_FLAGS_STATE_H_
#define MOJO_SYSTEM_WAIT_FLAGS_STATE_H_

#include "base/macros.h"
#include "mojo/public/c/system/types.h"
#include "mojo/system/system_impl_export.h"

namespace mojo {
namespace system {

// Just "add" some constructors and methods to the C struct |MojoWaitFlagsState|
// (for convenience). This should add no overhead.
struct MOJO_SYSTEM_IMPL_EXPORT WaitFlagsState : public MojoWaitFlagsState {
  WaitFlagsState() {
    satisfied_signals = MOJO_HANDLE_SIGNAL_NONE;
    satisfiable_signals = MOJO_HANDLE_SIGNAL_NONE;
  }
  WaitFlagsState(MojoHandleSignals satisfied, MojoHandleSignals satisfiable) {
    satisfied_signals = satisfied;
    satisfiable_signals = satisfiable;
  }

  bool equals(const WaitFlagsState& other) const {
    return satisfied_signals == other.satisfied_signals &&
           satisfiable_signals == other.satisfiable_signals;
  }

  bool satisfies(MojoHandleSignals signals) const {
    return !!(satisfied_signals & signals);
  }

  bool can_satisfy(MojoHandleSignals signals) const {
    return !!(satisfiable_signals & signals);
  }

  // (Copy and assignment allowed.)
};
COMPILE_ASSERT(sizeof(WaitFlagsState) == sizeof(MojoWaitFlagsState),
               WaitFlagsState_should_add_no_overhead);

}  // namespace system
}  // namespace mojo

#endif  // MOJO_SYSTEM_WAIT_FLAGS_STATE_H_
