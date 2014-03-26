// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SYSTEM_WAITER_H_
#define MOJO_SYSTEM_WAITER_H_

#include "base/basictypes.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "mojo/public/c/system/core.h"
#include "mojo/system/system_impl_export.h"

namespace mojo {
namespace system {

// IMPORTANT (all-caps gets your attention, right?): |Waiter| methods are called
// under other locks, in particular, |Dispatcher::lock_|s, so |Waiter| methods
// must never call out to other objects (in particular, |Dispatcher|s). This
// class is thread-safe.
class MOJO_SYSTEM_IMPL_EXPORT Waiter {
 public:
  Waiter();
  ~Waiter();

  // A |Waiter| can be used multiple times; |Init()| should be called before
  // each time it's used.
  void Init();

  // Waits until a suitable |Awake()| is called.
  // Returns:
  //  - The |wake_result| passed to |Dispatcher::AddWaiter()| if it was woken up
  //    by that dispatcher for the reason specified by |flags| (in the call to
  //    |AddWaiter()|).
  //  - |MOJO_RESULT_CANCELLED| if a handle (on which |MojoWait()| was called)
  //    was closed; and
  //  - |MOJO_RESULT_FAILED_PRECONDITION| if the reasons for being awoken given
  //    by |flags| cannot (or can no longer) be satisfied (e.g., if the other
  //    end of a pipe is closed).
  MojoResult Wait(MojoDeadline deadline);

  // Wake the waiter up with the given result (or no-op if it's been woken up
  // already).
  void Awake(MojoResult wait_result);

 private:
  base::ConditionVariable cv_;  // Associated to |lock_|.
  base::Lock lock_;  // Protects the following members.
#ifndef NDEBUG
  bool initialized_;
#endif
  bool awoken_;
  MojoResult wait_result_;

  DISALLOW_COPY_AND_ASSIGN(Waiter);
};

}  // namespace system
}  // namespace mojo

#endif  // MOJO_SYSTEM_WAITER_H_
