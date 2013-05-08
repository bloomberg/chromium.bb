// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/rate_limiter.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/time/tick_clock.h"

namespace policy {

RateLimiter::RateLimiter(size_t max_requests,
                         const base::TimeDelta& duration,
                         const base::Closure& callback,
                         scoped_refptr<base::SequencedTaskRunner> task_runner,
                         scoped_ptr<base::TickClock> clock)
    : max_requests_(max_requests),
      duration_(duration),
      callback_(callback),
      task_runner_(task_runner),
      clock_(clock.Pass()) {
  DCHECK_GT(max_requests_, 0u);
}

RateLimiter::~RateLimiter() {}

void RateLimiter::PostRequest() {
  DCHECK(CalledOnValidThread());

  const base::TimeTicks now = clock_->NowTicks();
  const base::TimeTicks period_start = now - duration_;
  while (!invocation_times_.empty() &&
         invocation_times_.front() <= period_start) {
    invocation_times_.pop();
  }

  delayed_callback_.Cancel();

  if (invocation_times_.size() < max_requests_) {
    invocation_times_.push(now);
    callback_.Run();
  } else {
    // From the while() loop above we have front() > period_start,
    // so time_until_next_callback > 0.
    const base::TimeDelta time_until_next_callback =
        invocation_times_.front() - period_start;
    delayed_callback_.Reset(
        base::Bind(&RateLimiter::PostRequest, base::Unretained(this)));
    task_runner_->PostDelayedTask(
        FROM_HERE, delayed_callback_.callback(), time_until_next_callback);
  }
}

}  // namespace policy
