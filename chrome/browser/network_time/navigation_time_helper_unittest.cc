// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/network_time/navigation_time_helper.h"

#include "content/public/browser/navigation_entry.h"
#include "testing/gtest/include/gtest/gtest.h"

class TestNavigationTimeHelper : public NavigationTimeHelper {
 public:
  void SetDelta(base::TimeDelta delta) {
    delta_ = delta;
  }
  virtual base::Time GetNetworkTime(base::Time t) OVERRIDE {
    return t + delta_;
  }

 private:
  base::TimeDelta delta_;
};

TEST(NavigationTimeHelperTest, QueryNavigationTime) {
  TestNavigationTimeHelper time_helper;
  time_helper.SetDelta(base::TimeDelta::FromHours(1));

  scoped_ptr<content::NavigationEntry> entry1(
      content::NavigationEntry::Create());
  entry1->SetTimestamp(base::Time::Now());

  EXPECT_EQ(entry1->GetTimestamp() + base::TimeDelta::FromHours(1),
            time_helper.GetNavigationTime(entry1.get()));

  // Adjusting delta shouldn't affect navigation time of unchanged entry.
  time_helper.SetDelta(base::TimeDelta::FromHours(2));
  EXPECT_EQ(entry1->GetTimestamp() + base::TimeDelta::FromHours(1),
            time_helper.GetNavigationTime(entry1.get()));

  // New delta is applied to new entry even if it has same local time.
  scoped_ptr<content::NavigationEntry> entry2(
      content::NavigationEntry::Create(*entry1));
  EXPECT_EQ(entry2->GetTimestamp() + base::TimeDelta::FromHours(2),
            time_helper.GetNavigationTime(entry2.get()));

  // New delta is applied if existing entry has new navigation.
  entry1->SetTimestamp(
      entry1->GetTimestamp() + base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(entry1->GetTimestamp() + base::TimeDelta::FromHours(2),
            time_helper.GetNavigationTime(entry1.get()));
}
