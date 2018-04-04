// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/test/perf_test_utils.h"
#include "testing/perf/perf_test.h"

namespace vr {

void PrintResults(const std::string& suite_name,
                  const std::string& test_name,
                  cc::LapTimer* timer) {
  perf_test::PrintResult(suite_name, ".render_time_avg", test_name,
                         timer->MsPerLap(), "ms", true);
  perf_test::PrintResult(suite_name, ".number_of_runs", test_name,
                         static_cast<size_t>(timer->NumLaps()), "runs", true);
}

}  // namespace vr
