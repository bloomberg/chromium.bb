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

void SimpleDispatcher::HandleSignalsStateChangedNoLock() {
  lock().AssertAcquired();
  waiter_list_.AwakeWaitersForStateChange(GetHandleSignalsStateImplNoLock());
}

void SimpleDispatcher::CancelAllWaitersNoLock() {
  lock().AssertAcquired();
  waiter_list_.CancelAllWaiters();
}

MojoResult SimpleDispatcher::AddWaiterImplNoLock(
    Waiter* waiter,
    MojoHandleSignals signals,
    uint32_t context,
    HandleSignalsState* signals_state) {
  lock().AssertAcquired();

  HandleSignalsState state(GetHandleSignalsStateImplNoLock());
  if (state.satisfies(signals)) {
    if (signals_state)
      *signals_state = state;
    return MOJO_RESULT_ALREADY_EXISTS;
  }
  if (!state.can_satisfy(signals)) {
    if (signals_state)
      *signals_state = state;
    return MOJO_RESULT_FAILED_PRECONDITION;
  }

  waiter_list_.AddWaiter(waiter, signals, context);
  return MOJO_RESULT_OK;
}

void SimpleDispatcher::RemoveWaiterImplNoLock(
    Waiter* waiter,
    HandleSignalsState* signals_state) {
  lock().AssertAcquired();
  waiter_list_.RemoveWaiter(waiter);
  if (signals_state)
    *signals_state = GetHandleSignalsStateImplNoLock();
}

}  // namespace system
}  // namespace mojo
