// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <vector>

#include "base/logging.h"
#include "base/string16.h"
#include "base/time.h"
#include "chrome/browser/chromeos/oom_priority_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

typedef testing::Test OomPriorityManagerTest;

namespace {
enum TestIndicies {
  kMostImportant,
  kNotPinned,
  kNotSelected,
  kSimilarTime,
  kSimilarTimeOverThreshold,
  kReallyOld,
  kOldButPinned
};
}  // namespace

// Tests the sorting comparator so that we know it's producing the
// desired order.
TEST_F(OomPriorityManagerTest, Comparator) {
  chromeos::OomPriorityManager::TabStatsList test_list;
  const base::TimeTicks now = base::TimeTicks::Now();

  {
    OomPriorityManager::TabStats stats;
    stats.is_selected = true;
    stats.is_pinned = true;
    stats.last_selected = now;
    stats.renderer_handle = kMostImportant;
    test_list.push_back(stats);
  }

  {
    OomPriorityManager::TabStats stats;
    stats.is_selected = true;
    stats.is_pinned = false;
    stats.last_selected = now;
    stats.renderer_handle = kNotPinned;
    test_list.push_back(stats);
  }

  {
    OomPriorityManager::TabStats stats;
    stats.is_selected = false;
    stats.is_pinned = false;
    stats.last_selected = now;
    stats.renderer_handle = kNotSelected;
    test_list.push_back(stats);
  }

  {
    OomPriorityManager::TabStats stats;
    stats.is_selected = false;
    stats.is_pinned = false;
    stats.last_selected = now - base::TimeDelta::FromSeconds(10);
    stats.renderer_handle = kSimilarTime;
    test_list.push_back(stats);
  }

  {
    OomPriorityManager::TabStats stats;
    stats.is_selected = false;
    stats.is_pinned = false;
    stats.last_selected = now - base::TimeDelta::FromMinutes(15);
    stats.renderer_handle = kSimilarTimeOverThreshold;
    test_list.push_back(stats);
  }

  {
    OomPriorityManager::TabStats stats;
    stats.is_selected = false;
    stats.is_pinned = false;
    stats.last_selected = now - base::TimeDelta::FromDays(365);
    stats.renderer_handle = kReallyOld;
    test_list.push_back(stats);
  }

  // This also is out of order, so verifies that we are actually
  // sorting the array.
  {
    OomPriorityManager::TabStats stats;
    stats.is_selected = false;
    stats.is_pinned = true;
    stats.last_selected = now - base::TimeDelta::FromDays(365);
    stats.renderer_handle = kOldButPinned;
    test_list.push_back(stats);
  }

  std::sort(test_list.begin(),
            test_list.end(),
            OomPriorityManager::CompareTabStats);

  EXPECT_EQ(kMostImportant, test_list[0].renderer_handle);
  EXPECT_EQ(kNotPinned, test_list[1].renderer_handle);
  EXPECT_EQ(kOldButPinned, test_list[2].renderer_handle);
  // The order of kNotSelected and kSimilarTime is indeterminate:
  // they are equal in the eyes of the sort.
  EXPECT_TRUE((test_list[3].renderer_handle == kNotSelected &&
               test_list[4].renderer_handle == kSimilarTime) ||
              (test_list[3].renderer_handle == kSimilarTime &&
               test_list[4].renderer_handle == kNotSelected));
  EXPECT_EQ(kSimilarTimeOverThreshold, test_list[5].renderer_handle);
  EXPECT_EQ(kReallyOld, test_list[6].renderer_handle);
}

}  // namespace chromeos
