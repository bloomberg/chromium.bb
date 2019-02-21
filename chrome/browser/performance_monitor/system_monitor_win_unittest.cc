// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/system_monitor.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace performance_monitor {

namespace {

class TestSystemMonitor : public SystemMonitor {
 public:
  using SystemMonitor::FreePhysMemoryMetricEvaluator;
};

}  // namespace

using SystemMonitorWinTest = testing::Test;

TEST_F(SystemMonitorWinTest, GetFreeMemory) {
  TestSystemMonitor::FreePhysMemoryMetricEvaluator metric_evaluator;
  EXPECT_FALSE(metric_evaluator.has_value());
  metric_evaluator.Evaluate();
  EXPECT_TRUE(metric_evaluator.has_value());
  EXPECT_GT(metric_evaluator.value(), 0);
}

}  // namespace performance_monitor
