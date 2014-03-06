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

class Stopwatch {
 public:
  Stopwatch() {}
  ~Stopwatch() {}

  void Start() {
    start_time_ = base::TimeTicks::HighResNow();
  }

  int64_t Elapsed() {
    return (base::TimeTicks::HighResNow() - start_time_).InMicroseconds();
  }

 private:
  base::TimeTicks start_time_;

  DISALLOW_COPY_AND_ASSIGN(Stopwatch);
};

// Posts the given task (to the given task runner) and waits for it to complete.
// (Note: Doesn't spin the current thread's message loop, so if you're careless
// this could easily deadlock.)
void PostTaskAndWait(scoped_refptr<base::TaskRunner> task_runner,
                     const tracked_objects::Location& from_here,
                     const base::Closure& task);

// TestIOThread ----------------------------------------------------------------

class TestIOThread {
 public:
  // Note: The I/O thread is started on construction and stopped on destruction.
  TestIOThread();
  ~TestIOThread();

  base::MessageLoopForIO* message_loop() {
    return static_cast<base::MessageLoopForIO*>(io_thread_.message_loop());
  }

  scoped_refptr<base::TaskRunner> task_runner() {
    return message_loop()->message_loop_proxy();
  }

 private:
  base::Thread io_thread_;

  DISALLOW_COPY_AND_ASSIGN(TestIOThread);
};

}  // namespace test
}  // namespace system
}  // namespace mojo

#endif  // MOJO_SYSTEM_TEST_UTILS_H_
