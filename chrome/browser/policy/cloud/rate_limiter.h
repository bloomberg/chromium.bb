// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_RATE_LIMITER_H_
#define CHROME_BROWSER_POLICY_CLOUD_RATE_LIMITER_H_

#include <queue>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/cancelable_callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "base/time.h"

namespace base {
class SequencedTaskRunner;
class TickClock;
}

namespace policy {

// A simple class to limit the rate at which a callback is invoked.
class RateLimiter : public base::NonThreadSafe {
 public:
  // Will limit invocations of |callback| to |max_requests| per |duration|.
  // |task_runner| is used to post delayed tasks, and |clock| is used to
  // measure elapsed time.
  RateLimiter(size_t max_requests,
              const base::TimeDelta& duration,
              const base::Closure& callback,
              scoped_refptr<base::SequencedTaskRunner> task_runner,
              scoped_ptr<base::TickClock> clock);
  ~RateLimiter();

  // Posts a request to invoke |callback_|. It is invoked immediately if the
  // rate in the preceding |duration_| period is within the limit, otherwise
  // the callback will be invoked later, ensuring the allowed rate is not
  // exceeded.
  void PostRequest();

 private:
  const size_t max_requests_;
  const base::TimeDelta duration_;
  base::Closure callback_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  scoped_ptr<base::TickClock> clock_;

  std::queue<base::TimeTicks> invocation_times_;
  base::CancelableClosure delayed_callback_;

  DISALLOW_COPY_AND_ASSIGN(RateLimiter);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_RATE_LIMITER_H_
