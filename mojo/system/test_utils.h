// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SYSTEM_TEST_UTILS_H_
#define MOJO_SYSTEM_TEST_UTILS_H_

#include <stdint.h>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/task_runner.h"
#include "base/threading/thread.h"
#include "base/time/time.h"

namespace tracked_objects {
class Location;
}

namespace mojo {
namespace system {
namespace test {

// Posts the given task (to the given task runner) and waits for it to complete.
// (Note: Doesn't spin the current thread's message loop, so if you're careless
// this could easily deadlock.)
void PostTaskAndWait(scoped_refptr<base::TaskRunner> task_runner,
                     const tracked_objects::Location& from_here,
                     const base::Closure& task);

// A timeout smaller than |TestTimeouts::tiny_timeout()|. Warning: This may lead
// to flakiness, but this is unavoidable if, e.g., you're trying to ensure that
// functions with timeouts are reasonably accurate. We want this to be as small
// as possible without causing too much flakiness.
base::TimeDelta EpsilonTimeout();

// Stopwatch -------------------------------------------------------------------

// A simple "stopwatch" for measuring time elapsed from a given starting point.
class Stopwatch {
 public:
  Stopwatch() {}
  ~Stopwatch() {}

  void Start() { start_time_ = base::TimeTicks::Now(); }

  base::TimeDelta Elapsed() { return base::TimeTicks::Now() - start_time_; }

 private:
  base::TimeTicks start_time_;

  DISALLOW_COPY_AND_ASSIGN(Stopwatch);
};

}  // namespace test
}  // namespace system
}  // namespace mojo

#endif  // MOJO_SYSTEM_TEST_UTILS_H_
