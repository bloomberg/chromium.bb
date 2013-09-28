// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/waiter_test_utils.h"

namespace mojo {
namespace system {
namespace test {

SimpleWaiterThread::SimpleWaiterThread(MojoResult* result)
    : base::SimpleThread("waiter_thread"),
      result_(result) {
  waiter_.Init();
  *result_ = -5420734;  // Totally invalid result.
}

SimpleWaiterThread::~SimpleWaiterThread() {
  Join();
}

void SimpleWaiterThread::Run() {
  *result_ = waiter_.Wait(MOJO_DEADLINE_INDEFINITE);
}

WaiterThread::WaiterThread(scoped_refptr<Dispatcher> dispatcher,
             MojoWaitFlags wait_flags,
             MojoDeadline deadline,
             MojoResult success_result,
             bool* did_wait_out,
             MojoResult* result_out)
    : base::SimpleThread("waiter_thread"),
      dispatcher_(dispatcher),
      wait_flags_(wait_flags),
      deadline_(deadline),
      success_result_(success_result),
      did_wait_out_(did_wait_out),
      result_out_(result_out) {
  *did_wait_out_ = false;
  *result_out_ = -8542346;  // Totally invalid result.
}

WaiterThread::~WaiterThread() {
  Join();
}

void WaiterThread::Run() {
  waiter_.Init();

  *result_out_ = dispatcher_->AddWaiter(&waiter_,
                                        wait_flags_,
                                        success_result_);
  if (*result_out_ != MOJO_RESULT_OK)
    return;

  *did_wait_out_ = true;
  *result_out_ = waiter_.Wait(deadline_);
  dispatcher_->RemoveWaiter(&waiter_);
}

}  // namespace test
}  // namespace system
}  // namespace mojo
