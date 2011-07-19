// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/network_message_observer.h"

#include "base/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

CellularDataPlan CreateDataPlan(CellularDataPlanType type, int64 start_sec,
                                int64 end_sec, int64 bytes, int64 used) {
  CellularDataPlan plan;
  plan.plan_type = type;
  plan.plan_start_time = base::Time::FromDoubleT(start_sec);
  plan.plan_end_time = base::Time::FromDoubleT(end_sec);
  plan.plan_data_bytes = bytes;
  plan.data_bytes_used = used;
  return plan;
}

}  // namespace

// Test the code that checks if a data plan is an applicable backup plan.
TEST(NetworkMessageObserverTest, TestIsApplicableBackupPlan) {
  // IsApplicableBackupPlan returns true if:
  //   (unlimited OR used bytes < max bytes) AND
  //   ((start time - 1 sec) <= end time of currently active plan).

  // Current plan that ends at 100.
  CellularDataPlan plan =
      CreateDataPlan(CELLULAR_DATA_PLAN_UNLIMITED, 0, 100, 0, 0);

  // Test unlimited plans.
  CellularDataPlan time_50 =
      CreateDataPlan(CELLULAR_DATA_PLAN_UNLIMITED, 50, 500, 0, 0);
  CellularDataPlan time_100 =
      CreateDataPlan(CELLULAR_DATA_PLAN_UNLIMITED, 100, 500, 0, 0);
  CellularDataPlan time_101 =
      CreateDataPlan(CELLULAR_DATA_PLAN_UNLIMITED, 101, 500, 0, 0);
  CellularDataPlan time_102 =
      CreateDataPlan(CELLULAR_DATA_PLAN_UNLIMITED, 102, 500, 0, 0);
  EXPECT_TRUE(NetworkMessageObserver::IsApplicableBackupPlan(&plan, &time_50));
  EXPECT_TRUE(NetworkMessageObserver::IsApplicableBackupPlan(&plan, &time_100));
  EXPECT_TRUE(NetworkMessageObserver::IsApplicableBackupPlan(&plan, &time_101));
  EXPECT_FALSE(NetworkMessageObserver::IsApplicableBackupPlan(&plan,
                                                              &time_102));

  // Test data plans.
  CellularDataPlan data_0_0 =
      CreateDataPlan(CELLULAR_DATA_PLAN_METERED_PAID, 100, 500, 0, 0);
  CellularDataPlan data_100_0 =
      CreateDataPlan(CELLULAR_DATA_PLAN_METERED_PAID, 100, 500, 100, 0);
  CellularDataPlan data_100_50 =
      CreateDataPlan(CELLULAR_DATA_PLAN_METERED_PAID, 100, 500, 100, 50);
  CellularDataPlan data_100_100 =
      CreateDataPlan(CELLULAR_DATA_PLAN_METERED_PAID, 100, 500, 100, 100);
  CellularDataPlan data_100_200 =
      CreateDataPlan(CELLULAR_DATA_PLAN_METERED_PAID, 100, 500, 100, 200);
  CellularDataPlan data_100_0_gap =
      CreateDataPlan(CELLULAR_DATA_PLAN_METERED_PAID, 200, 500, 100, 0);
  EXPECT_FALSE(NetworkMessageObserver::IsApplicableBackupPlan(&plan,
                                                              &data_0_0));
  EXPECT_TRUE(NetworkMessageObserver::IsApplicableBackupPlan(&plan,
                                                             &data_100_0));
  EXPECT_TRUE(NetworkMessageObserver::IsApplicableBackupPlan(&plan,
                                                             &data_100_50));
  EXPECT_FALSE(NetworkMessageObserver::IsApplicableBackupPlan(&plan,
                                                              &data_100_100));
  EXPECT_FALSE(NetworkMessageObserver::IsApplicableBackupPlan(&plan,
                                                              &data_100_200));
  EXPECT_FALSE(NetworkMessageObserver::IsApplicableBackupPlan(&plan,
                                                              &data_100_0_gap));
}


}  // namespace chromeos
