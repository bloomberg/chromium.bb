// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <vector>

#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/memory/oom_priority_manager.h"
#include "chrome/common/url_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace chromeos {

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
    stats.child_process_host_id = kPinned;
    test_list.push_back(stats);
  }

  {
    OomPriorityManager::TabStats stats;
    stats.is_app = true;
    stats.renderer_handle = kApp;
    stats.child_process_host_id = kApp;
    test_list.push_back(stats);
  }

  {
    OomPriorityManager::TabStats stats;
    stats.is_playing_audio = true;
    stats.renderer_handle = kPlayingAudio;
    stats.child_process_host_id = kPlayingAudio;
    test_list.push_back(stats);
  }

  {
    OomPriorityManager::TabStats stats;
    stats.last_active = now - base::TimeDelta::FromSeconds(10);
    stats.renderer_handle = kRecent;
    stats.child_process_host_id = kRecent;
    test_list.push_back(stats);
  }

  {
    OomPriorityManager::TabStats stats;
    stats.last_active = now - base::TimeDelta::FromMinutes(15);
    stats.renderer_handle = kOld;
    stats.child_process_host_id = kOld;
    test_list.push_back(stats);
  }

  {
    OomPriorityManager::TabStats stats;
    stats.last_active = now - base::TimeDelta::FromDays(365);
    stats.renderer_handle = kReallyOld;
    stats.child_process_host_id = kReallyOld;
    test_list.push_back(stats);
  }

  {
    OomPriorityManager::TabStats stats;
    stats.is_pinned = true;
    stats.last_active = now - base::TimeDelta::FromDays(365);
    stats.renderer_handle = kOldButPinned;
    stats.child_process_host_id = kOldButPinned;
    test_list.push_back(stats);
  }

  {
    OomPriorityManager::TabStats stats;
    stats.is_reloadable_ui = true;
    stats.renderer_handle = kReloadableUI;
    stats.child_process_host_id = kReloadableUI;
    test_list.push_back(stats);
  }

  // This entry sorts to the front, so by adding it last we verify that
  // we are actually sorting the array.
  {
    OomPriorityManager::TabStats stats;
    stats.is_selected = true;
    stats.renderer_handle = kSelected;
    stats.child_process_host_id = kSelected;
    test_list.push_back(stats);
  }

  std::sort(test_list.begin(),
            test_list.end(),
            OomPriorityManager::CompareTabStats);

  int index = 0;
  EXPECT_EQ(kSelected, test_list[index++].renderer_handle);
  EXPECT_EQ(kPinned, test_list[index++].renderer_handle);
  EXPECT_EQ(kOldButPinned, test_list[index++].renderer_handle);
  EXPECT_EQ(kApp, test_list[index++].renderer_handle);
  EXPECT_EQ(kPlayingAudio, test_list[index++].renderer_handle);
  EXPECT_EQ(kRecent, test_list[index++].renderer_handle);
  EXPECT_EQ(kOld, test_list[index++].renderer_handle);
  EXPECT_EQ(kReallyOld, test_list[index++].renderer_handle);
  EXPECT_EQ(kReloadableUI, test_list[index++].renderer_handle);

  index = 0;
  EXPECT_EQ(kSelected, test_list[index++].child_process_host_id);
  EXPECT_EQ(kPinned, test_list[index++].child_process_host_id);
  EXPECT_EQ(kOldButPinned, test_list[index++].child_process_host_id);
  EXPECT_EQ(kApp, test_list[index++].child_process_host_id);
  EXPECT_EQ(kPlayingAudio, test_list[index++].child_process_host_id);
  EXPECT_EQ(kRecent, test_list[index++].child_process_host_id);
  EXPECT_EQ(kOld, test_list[index++].child_process_host_id);
  EXPECT_EQ(kReallyOld, test_list[index++].child_process_host_id);
  EXPECT_EQ(kReloadableUI, test_list[index++].child_process_host_id);
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

TEST_F(OomPriorityManagerTest, GetProcessHandles) {
  OomPriorityManager::TabStats stats;
  std::vector<OomPriorityManager::ProcessInfo> process_id_pairs;

  // Empty stats list gives empty |process_id_pairs| list.
  OomPriorityManager::TabStatsList empty_list;
  process_id_pairs =
      OomPriorityManager::GetChildProcessInfos(empty_list);
  EXPECT_EQ(0u, process_id_pairs.size());

  // Two tabs in two different processes generates two
  // |child_process_host_id| out.
  OomPriorityManager::TabStatsList two_list;
  stats.child_process_host_id = 100;
  stats.renderer_handle = 101;
  two_list.push_back(stats);
  stats.child_process_host_id = 200;
  stats.renderer_handle = 201;
  two_list.push_back(stats);
  process_id_pairs = OomPriorityManager::GetChildProcessInfos(two_list);
  EXPECT_EQ(2u, process_id_pairs.size());
  EXPECT_EQ(100, process_id_pairs[0].first);
  EXPECT_EQ(101, process_id_pairs[0].second);
  EXPECT_EQ(200, process_id_pairs[1].first);
  EXPECT_EQ(201, process_id_pairs[1].second);

  // Zero handles are removed.
  OomPriorityManager::TabStatsList zero_handle_list;
  stats.child_process_host_id = 100;
  stats.renderer_handle = 0;
  zero_handle_list.push_back(stats);
  process_id_pairs =
      OomPriorityManager::GetChildProcessInfos(zero_handle_list);
  EXPECT_EQ(0u, process_id_pairs.size());

  // Two tabs in the same process generates one handle out. When a duplicate
  // occurs the later instance is dropped.
  OomPriorityManager::TabStatsList same_process_list;
  stats.child_process_host_id = 100;
  stats.renderer_handle = 101;
  same_process_list.push_back(stats);
  stats.child_process_host_id = 200;
  stats.renderer_handle = 201;
  same_process_list.push_back(stats);
  stats.child_process_host_id = 300;
  stats.renderer_handle = 101;  // Duplicate.
  same_process_list.push_back(stats);
  process_id_pairs =
      OomPriorityManager::GetChildProcessInfos(same_process_list);
  EXPECT_EQ(2u, process_id_pairs.size());
  EXPECT_EQ(100, process_id_pairs[0].first);
  EXPECT_EQ(101, process_id_pairs[0].second);
  EXPECT_EQ(200, process_id_pairs[1].first);
  EXPECT_EQ(201, process_id_pairs[1].second);
}

}  // namespace chromeos
