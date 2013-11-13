// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/tests/simple_bindings_support.h"

#include <stdlib.h>

namespace mojo {
namespace test {

SimpleBindingsSupport::SimpleBindingsSupport() {
  BindingsSupport::Set(this);
}

SimpleBindingsSupport::~SimpleBindingsSupport() {
  BindingsSupport::Set(NULL);

  for (WaiterList::iterator it = waiters_.begin(); it != waiters_.end(); ++it)
    delete *it;
}

BindingsSupport::AsyncWaitID SimpleBindingsSupport::AsyncWait(
    Handle handle,
    MojoWaitFlags flags,
    AsyncWaitCallback* callback) {
  Waiter* waiter = new Waiter();
  waiter->handle = handle;
  waiter->flags = flags;
  waiter->callback = callback;
  waiters_.push_back(waiter);
  return waiter;
}

void SimpleBindingsSupport::CancelWait(AsyncWaitID async_wait_id) {
  Waiter* waiter = static_cast<Waiter*>(async_wait_id);

  WaiterList::iterator it = waiters_.begin();
  while (it != waiters_.end()) {
    if (*it == waiter) {
      WaiterList::iterator doomed = it++;
      waiters_.erase(doomed);
    } else {
      ++it;
    }
  }

  delete waiter;
}

void SimpleBindingsSupport::Process() {
  for (;;) {
    typedef std::pair<AsyncWaitCallback*, MojoResult> Result;
    std::list<Result> results;

    WaiterList::iterator it = waiters_.begin();
    while (it != waiters_.end()) {
      Waiter* waiter = *it;
      MojoResult result;
      if (IsReady(waiter->handle, waiter->flags, &result)) {
        results.push_back(std::make_pair(waiter->callback, result));
        WaiterList::iterator doomed = it++;
        waiters_.erase(doomed);
        delete waiter;
      } else {
        ++it;
      }
    }

    for (std::list<Result>::const_iterator it = results.begin();
         it != results.end();
         ++it) {
      it->first->OnHandleReady(it->second);
    }
    if (results.empty())
      break;
  }
}

bool SimpleBindingsSupport::IsReady(Handle handle, MojoWaitFlags flags,
                                    MojoResult* result) {
  *result = Wait(handle, flags, 0);
  return *result != MOJO_RESULT_DEADLINE_EXCEEDED;
}

}  // namespace test
}  // namespace mojo
