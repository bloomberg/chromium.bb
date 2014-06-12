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
    satisfied_flags = MOJO_WAIT_FLAG_NONE;
    satisfiable_flags = MOJO_WAIT_FLAG_NONE;
  }
  WaitFlagsState(MojoWaitFlags satisfied, MojoWaitFlags satisfiable) {
    satisfied_flags = satisfied;
    satisfiable_flags = satisfiable;
  }

  bool equals(const WaitFlagsState& other) const {
    return satisfied_flags == other.satisfied_flags &&
           satisfiable_flags == other.satisfiable_flags;
  }

  bool satisfies(MojoWaitFlags flags) const {
    return !!(satisfied_flags & flags);
  }

  bool can_satisfy(MojoWaitFlags flags) const {
    return !!(satisfiable_flags & flags);
  }

  // (Copy and assignment allowed.)
};
COMPILE_ASSERT(sizeof(WaitFlagsState) == sizeof(MojoWaitFlagsState),
               WaitFlagsState_should_add_no_overhead);

}  // namespace system
}  // namespace mojo

#endif  // MOJO_SYSTEM_WAIT_FLAGS_STATE_H_
