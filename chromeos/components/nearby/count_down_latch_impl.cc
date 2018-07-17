// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/nearby/count_down_latch_impl.h"

#include "chromeos/components/nearby/library/exception.h"

namespace chromeos {

namespace nearby {

CountDownLatchImpl::CountDownLatchImpl(int32_t count)
    : count_(count),
      count_waitable_event_(
          base::WaitableEvent::ResetPolicy::MANUAL /* reset_policy */,
          count <= 0 ? base::WaitableEvent::InitialState::SIGNALED
                     : base::WaitableEvent::InitialState::
                           NOT_SIGNALED /* initial_state*/) {}

CountDownLatchImpl::~CountDownLatchImpl() = default;

location::nearby::Exception::Value CountDownLatchImpl::await() {
  if (count_waitable_event_.IsSignaled())
    return location::nearby::Exception::NONE;

  count_waitable_event_.Wait();
  return location::nearby::Exception::NONE;
}

location::nearby::ExceptionOr<bool> CountDownLatchImpl::await(
    int32_t timeout_millis) {
  if (count_waitable_event_.IsSignaled())
    return location::nearby::ExceptionOr<bool>(true);

  // Return true if |count_waitable_event_| is signaled before it times out.
  // Otherwise, this returns false due to timing out.
  return location::nearby::ExceptionOr<bool>(count_waitable_event_.TimedWait(
      base::TimeDelta::FromMilliseconds(static_cast<int64_t>(timeout_millis))));
}

void CountDownLatchImpl::countDown() {
  // Signal |count_waitable_event_| when (and only the one exact time when)
  // |count_| decrements to 0.
  if (!count_.Decrement())
    count_waitable_event_.Signal();
}

}  // namespace nearby

}  // namespace chromeos
