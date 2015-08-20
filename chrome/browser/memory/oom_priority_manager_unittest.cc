// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory/oom_priority_manager.h"

#include <algorithm>
#include <vector>

#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "chrome/browser/memory/tab_stats.h"
#include "chrome/common/url_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace memory {

typedef testing::Test OomPriorityManagerTest;

namespace {
enum TestIndicies {
  kSelected,
  kPinned,
  kApp,
  kPlayingAudio,
  kRecent,
  kOld,
  kReallyOld,
  kOldButPinned,
  kInternalPage,
};
}  // namespace

// Tests the sorting comparator so that we know it's producing the
// desired order.
TEST_F(OomPriorityManagerTest, Comparator) {
  TabStatsList test_list;
  const base::TimeTicks now = base::TimeTicks::Now();

  // Add kSelected last to verify we are sorting the array.

  {
    TabStats stats;
    stats.is_pinned = true;
    stats.child_process_host_id = kPinned;
    test_list.push_back(stats);
  }

  {
    TabStats stats;
    stats.is_app = true;
    stats.child_process_host_id = kApp;
    test_list.push_back(stats);
  }

  {
    TabStats stats;
    stats.is_playing_audio = true;
    stats.child_process_host_id = kPlayingAudio;
    test_list.push_back(stats);
  }

  {
    TabStats stats;
    stats.last_active = now - base::TimeDelta::FromSeconds(10);
    stats.child_process_host_id = kRecent;
    test_list.push_back(stats);
  }

  {
    TabStats stats;
    stats.last_active = now - base::TimeDelta::FromMinutes(15);
    stats.child_process_host_id = kOld;
    test_list.push_back(stats);
  }

  {
    TabStats stats;
    stats.last_active = now - base::TimeDelta::FromDays(365);
    stats.child_process_host_id = kReallyOld;
    test_list.push_back(stats);
  }

  {
    TabStats stats;
    stats.is_pinned = true;
    stats.last_active = now - base::TimeDelta::FromDays(365);
    stats.child_process_host_id = kOldButPinned;
    test_list.push_back(stats);
  }

  {
    TabStats stats;
    stats.is_internal_page = true;
    stats.child_process_host_id = kInternalPage;
    test_list.push_back(stats);
  }

  // This entry sorts to the front, so by adding it last we verify that
  // we are actually sorting the array.
  {
    TabStats stats;
    stats.is_selected = true;
    stats.child_process_host_id = kSelected;
    test_list.push_back(stats);
  }

  std::sort(test_list.begin(), test_list.end(),
            OomPriorityManager::CompareTabStats);

  int index = 0;
  EXPECT_EQ(kSelected, test_list[index++].child_process_host_id);
  EXPECT_EQ(kPlayingAudio, test_list[index++].child_process_host_id);
  EXPECT_EQ(kPinned, test_list[index++].child_process_host_id);
  EXPECT_EQ(kOldButPinned, test_list[index++].child_process_host_id);
  EXPECT_EQ(kApp, test_list[index++].child_process_host_id);
  EXPECT_EQ(kRecent, test_list[index++].child_process_host_id);
  EXPECT_EQ(kOld, test_list[index++].child_process_host_id);
  EXPECT_EQ(kReallyOld, test_list[index++].child_process_host_id);
  EXPECT_EQ(kInternalPage, test_list[index++].child_process_host_id);
}

TEST_F(OomPriorityManagerTest, IsInternalPage) {
  EXPECT_TRUE(
      OomPriorityManager::IsInternalPage(GURL(chrome::kChromeUIDownloadsURL)));
  EXPECT_TRUE(
      OomPriorityManager::IsInternalPage(GURL(chrome::kChromeUIHistoryURL)));
  EXPECT_TRUE(
      OomPriorityManager::IsInternalPage(GURL(chrome::kChromeUINewTabURL)));
  EXPECT_TRUE(
      OomPriorityManager::IsInternalPage(GURL(chrome::kChromeUISettingsURL)));

// Debugging URLs are not included.
#if defined(OS_CHROMEOS)
  EXPECT_FALSE(
      OomPriorityManager::IsInternalPage(GURL(chrome::kChromeUIDiscardsURL)));
#endif
  EXPECT_FALSE(OomPriorityManager::IsInternalPage(
      GURL(chrome::kChromeUINetInternalsURL)));

  // Prefix matches are included.
  EXPECT_TRUE(OomPriorityManager::IsInternalPage(
      GURL("chrome://settings/fakeSetting")));
}

}  // namespace memory
