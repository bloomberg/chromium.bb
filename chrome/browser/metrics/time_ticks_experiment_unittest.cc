// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/time_ticks_experiment_win.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)

namespace chrome {
namespace {

// Makes sure the test runs. Mostly so we don't break by using illegal functions
// on older versions of Windows. The results vary depending on the host
// machine's capabilities, so it'd be hard to write proper expectations.
TEST(TimeTicksExperimentTest, RunTimeTicksExperiment) {
  CollectTimeTicksStats();
}

}  // anonymous namespace
}  // namespace chrome_browser_metrics

#endif  // defined(OS_WIN)
