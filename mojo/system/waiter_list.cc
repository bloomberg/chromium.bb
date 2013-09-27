// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/waiter_list.h"

#include "base/logging.h"
#include "mojo/system/waiter.h"

namespace mojo {
namespace system {

WaiterList::WaiterList() {
}

WaiterList::~WaiterList() {
  DCHECK(waiters_.empty());
}

void WaiterList::AwakeWaitersForStateChange(MojoWaitFlags satisfied_flags,
                                            MojoWaitFlags satisfiable_flags) {
  for (WaiterInfoList::iterator it = waiters_.begin(); it != waiters_.end();
       ++it) {
    if (it->flags & satisfied_flags)
      it->waiter->Awake(it->wake_result);
    else if (!(it->flags & satisfiable_flags))
      it->waiter->Awake(MOJO_RESULT_FAILED_PRECONDITION);
  }
}

void WaiterList::CancelAllWaiters() {
  for (WaiterInfoList::iterator it = waiters_.begin(); it != waiters_.end();
       ++it) {
    it->waiter->Awake(MOJO_RESULT_CANCELLED);
  }
  waiters_.clear();
}

void WaiterList::AddWaiter(Waiter* waiter,
                           MojoWaitFlags flags,
                           MojoResult wake_result) {
  waiters_.push_back(WaiterInfo(waiter, flags, wake_result));
}

void WaiterList::RemoveWaiter(Waiter* waiter) {
  // We allow a thread to wait on the same handle multiple times simultaneously,
  // so we need to scan the entire list and remove all occurrences of |waiter|.
  for (WaiterInfoList::iterator it = waiters_.begin(); it != waiters_.end();) {
    WaiterInfoList::iterator maybe_delete = it;
    ++it;
    if (maybe_delete->waiter == waiter)
      waiters_.erase(maybe_delete);
  }
}

}  // namespace system
}  // namespace mojo
