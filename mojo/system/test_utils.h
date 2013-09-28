// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SYSTEM_TEST_UTILS_H_
#define MOJO_SYSTEM_TEST_UTILS_H_

#include "base/basictypes.h"
#include "base/time/time.h"

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

}  // namespace test
}  // namespace system
}  // namespace mojo

#endif  // MOJO_SYSTEM_TEST_UTILS_H_
