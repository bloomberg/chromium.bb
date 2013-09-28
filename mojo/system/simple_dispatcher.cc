// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/simple_dispatcher.h"

#include "base/logging.h"

namespace mojo {
namespace system {

SimpleDispatcher::SimpleDispatcher() {
}

SimpleDispatcher::~SimpleDispatcher() {
}

void SimpleDispatcher::StateChangedNoLock() {
  lock().AssertAcquired();
  waiter_list_.AwakeWaitersForStateChange(SatisfiedFlagsNoLock(),
                                          SatisfiableFlagsNoLock());
}

void SimpleDispatcher::CancelAllWaitersNoLock() {
  lock().AssertAcquired();
  waiter_list_.CancelAllWaiters();
}

MojoResult SimpleDispatcher::AddWaiterImplNoLock(Waiter* waiter,
                                                 MojoWaitFlags flags,
                                                 MojoResult wake_result) {
  lock().AssertAcquired();

  if ((flags & SatisfiedFlagsNoLock()))
    return MOJO_RESULT_ALREADY_EXISTS;
  if (!(flags & SatisfiableFlagsNoLock()))
    return MOJO_RESULT_FAILED_PRECONDITION;

  waiter_list_.AddWaiter(waiter, flags, wake_result);
  return MOJO_RESULT_OK;
}

void SimpleDispatcher::RemoveWaiterImplNoLock(Waiter* waiter) {
  lock().AssertAcquired();
  waiter_list_.RemoveWaiter(waiter);
}

}  // namespace system
}  // namespace mojo
