// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/tests/test_support.h"

#include "base/test/perf_log.h"
#include "base/time/time.h"

namespace mojo {
namespace test {

void IterateAndReportPerf(const char* test_name,
                          base::Callback<void()> single_iteration) {
  // TODO(vtl): These should be specifiable using command-line flags.
  static const size_t kGranularity = 100;
  static const double kPerftestTimeSeconds = 3.0;

  const base::TimeTicks start_time = base::TimeTicks::HighResNow();
  base::TimeTicks end_time;
  size_t iterations = 0;
  do {
    for (size_t i = 0; i < kGranularity; i++)
      single_iteration.Run();
    iterations += kGranularity;

    end_time = base::TimeTicks::HighResNow();
  } while ((end_time - start_time).InSecondsF() < kPerftestTimeSeconds);

  base::LogPerfResult(test_name,
                      iterations / (end_time - start_time).InSecondsF(),
                      "iterations/second");
}

}  // namespace test
}  // namespace mojo
