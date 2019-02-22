// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/metric_evaluator_helper_win.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace performance_monitor {

using MetricEvaluatorsHelperWinTest = testing::Test;

TEST_F(MetricEvaluatorsHelperWinTest, GetFreeMemory) {
  MetricEvaluatorsHelperWin metric_evaluator_helper;
  auto value = metric_evaluator_helper.GetFreePhysicalMemoryMb();
  EXPECT_TRUE(value);
  EXPECT_GT(value.value(), 0);
}

}  // namespace performance_monitor
