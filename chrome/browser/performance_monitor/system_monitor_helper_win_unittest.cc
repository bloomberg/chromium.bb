// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/system_monitor_helper_win.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace performance_monitor {
namespace win {

namespace {

class TestSystemMonitorHelperWin : public SystemMonitorHelperWin {
 public:
  TestSystemMonitorHelperWin() = default;
  ~TestSystemMonitorHelperWin() override = default;

  using SystemMonitorHelperWin::RefreshMetrics;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestSystemMonitorHelperWin);
};

using SystemMonitorHelperWinTest = testing::Test;

TEST_F(SystemMonitorHelperWinTest, GetFreeMemory) {
  TestSystemMonitorHelperWin helper;
  auto refreshed_metrics = helper.RefreshMetrics(
      {.free_phys_memory_mb_frequency =
           SystemMonitor::SamplingFrequency::kDefaultFrequency},
      base::TimeTicks::Now());
  EXPECT_GT(refreshed_metrics.free_phys_memory_mb.metric_value, 0);
}

}  // namespace

}  // namespace win
}  // namespace performance_monitor
