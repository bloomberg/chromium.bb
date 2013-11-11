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
}

bool SimpleBindingsSupport::AsyncWait(Handle handle,
                                      MojoWaitFlags flags,
                                      MojoDeadline deadline,
                                      AsyncWaitCallback* callback) {
  Waiter waiter;
  waiter.handle = handle;
  waiter.flags = flags;
  waiter.deadline = deadline;
  waiter.callback = callback;
  waiters_.push_back(waiter);
  return true;
}

void SimpleBindingsSupport::CancelWait(AsyncWaitCallback* callback) {
  std::list<Waiter>::iterator it = waiters_.begin();
  while (it != waiters_.end()) {
    if (it->callback == callback) {
      std::list<Waiter>::iterator doomed = it++;
      waiters_.erase(doomed);
    } else {
      ++it;
    }
  }
}

void SimpleBindingsSupport::Process() {
  typedef std::pair<AsyncWaitCallback*, MojoResult> Result;
  std::list<Result> results;

  // TODO(darin): Honor given deadline.

  std::list<Waiter>::iterator it = waiters_.begin();
  while (it != waiters_.end()) {
    const Waiter& waiter = *it;
    MojoResult result;
    if (IsReady(waiter.handle, waiter.flags, &result)) {
      results.push_back(std::make_pair(waiter.callback, result));
      std::list<Waiter>::iterator doomed = it++;
      waiters_.erase(doomed);
    } else {
      ++it;
    }
  }

  for (std::list<Result>::const_iterator it = results.begin();
       it != results.end();
       ++it) {
    it->first->OnHandleReady(it->second);
  }
}

bool SimpleBindingsSupport::IsReady(Handle handle, MojoWaitFlags flags,
                                    MojoResult* result) {
  *result = Wait(handle, flags, 0);
  return *result != MOJO_RESULT_DEADLINE_EXCEEDED;
}

}  // namespace test
}  // namespace mojo
