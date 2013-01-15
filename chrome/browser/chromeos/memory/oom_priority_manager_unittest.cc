// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <vector>

#include "base/logging.h"
#include "base/string16.h"
#include "base/time.h"
#include "chrome/browser/chromeos/memory/oom_priority_manager.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

typedef testing::Test OomPriorityManagerTest;

namespace {
enum TestIndicies {
  kSelected,
  kPinned,
  kApp,
  kRecent,
  kOld,
  kReallyOld,
  kOldButPinned,
  kReloadableUI,
};
}  // namespace

// Tests the sorting comparator so that we know it's producing the
// desired order.
TEST_F(OomPriorityManagerTest, Comparator) {
  chromeos::OomPriorityManager::TabStatsList test_list;
  const base::TimeTicks now = base::TimeTicks::Now();

  // Add kSelected last to verify we are sorting the array.

  {
    OomPriorityManager::TabStats stats;
    stats.is_pinned = true;
    stats.renderer_handle = kPinned;
    test_list.push_back(stats);
  }

  {
    OomPriorityManager::TabStats stats;
    stats.is_app = true;
    stats.renderer_handle = kApp;
    test_list.push_back(stats);
  }

  {
    OomPriorityManager::TabStats stats;
    stats.last_selected = now - base::TimeDelta::FromSeconds(10);
    stats.renderer_handle = kRecent;
    test_list.push_back(stats);
  }

  {
    OomPriorityManager::TabStats stats;
    stats.last_selected = now - base::TimeDelta::FromMinutes(15);
    stats.renderer_handle = kOld;
    test_list.push_back(stats);
  }

  {
    OomPriorityManager::TabStats stats;
    stats.last_selected = now - base::TimeDelta::FromDays(365);
    stats.renderer_handle = kReallyOld;
    test_list.push_back(stats);
  }

  {
    OomPriorityManager::TabStats stats;
    stats.is_pinned = true;
    stats.last_selected = now - base::TimeDelta::FromDays(365);
    stats.renderer_handle = kOldButPinned;
    test_list.push_back(stats);
  }

  {
    OomPriorityManager::TabStats stats;
    stats.is_reloadable_ui = true;
    stats.renderer_handle = kReloadableUI;
    test_list.push_back(stats);
  }

  // This entry sorts to the front, so by adding it last we verify that
  // we are actually sorting the array.
  {
    OomPriorityManager::TabStats stats;
    stats.is_selected = true;
    stats.renderer_handle = kSelected;
    test_list.push_back(stats);
  }

  std::sort(test_list.begin(),
            test_list.end(),
            OomPriorityManager::CompareTabStats);

  EXPECT_EQ(kSelected, test_list[0].renderer_handle);
  EXPECT_EQ(kPinned, test_list[1].renderer_handle);
  EXPECT_EQ(kOldButPinned, test_list[2].renderer_handle);
  EXPECT_EQ(kApp, test_list[3].renderer_handle);
  EXPECT_EQ(kRecent, test_list[4].renderer_handle);
  EXPECT_EQ(kOld, test_list[5].renderer_handle);
  EXPECT_EQ(kReallyOld, test_list[6].renderer_handle);
  EXPECT_EQ(kReloadableUI, test_list[7].renderer_handle);
}

TEST_F(OomPriorityManagerTest, IsReloadableUI) {
  EXPECT_TRUE(OomPriorityManager::IsReloadableUI(
      GURL(chrome::kChromeUIDownloadsURL)));
  EXPECT_TRUE(OomPriorityManager::IsReloadableUI(
      GURL(chrome::kChromeUIHistoryURL)));
  EXPECT_TRUE(OomPriorityManager::IsReloadableUI(
      GURL(chrome::kChromeUINewTabURL)));
  EXPECT_TRUE(OomPriorityManager::IsReloadableUI(
      GURL(chrome::kChromeUISettingsURL)));

  // Debugging URLs are not included.
  EXPECT_FALSE(OomPriorityManager::IsReloadableUI(
      GURL(chrome::kChromeUIDiscardsURL)));
  EXPECT_FALSE(OomPriorityManager::IsReloadableUI(
      GURL(chrome::kChromeUINetInternalsURL)));

  // Prefix matches are included.
  EXPECT_TRUE(OomPriorityManager::IsReloadableUI(
      GURL("chrome://settings/fakeSetting")));
}

}  // namespace chromeos
