// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory/tab_manager_delegate_chromeos.h"

#include <algorithm>
#include <vector>

#include "base/logging.h"
#include "chrome/browser/chromeos/arc/arc_process.h"
#include "chrome/browser/memory/tab_stats.h"
#include "chrome/common/url_constants.h"
#include "components/arc/common/process.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace memory {

typedef testing::Test TabManagerDelegateTest;

TEST_F(TabManagerDelegateTest, GetProcessHandles) {
  TabStats stats;
  std::vector<TabManagerDelegate::ProcessInfo> process_id_pairs;

  // Empty stats list gives empty |process_id_pairs| list.
  TabStatsList empty_list;
  process_id_pairs =
      TabManagerDelegate::GetChildProcessInfos(empty_list);
  EXPECT_EQ(0u, process_id_pairs.size());

  // Two tabs in two different processes generates two
  // |child_process_host_id| out.
  TabStatsList two_list;
  stats.child_process_host_id = 100;
  stats.renderer_handle = 101;
  two_list.push_back(stats);
  stats.child_process_host_id = 200;
  stats.renderer_handle = 201;
  two_list.push_back(stats);
  process_id_pairs = TabManagerDelegate::GetChildProcessInfos(two_list);
  EXPECT_EQ(2u, process_id_pairs.size());
  EXPECT_EQ(100, process_id_pairs[0].first);
  EXPECT_EQ(101, process_id_pairs[0].second);
  EXPECT_EQ(200, process_id_pairs[1].first);
  EXPECT_EQ(201, process_id_pairs[1].second);

  // Zero handles are removed.
  TabStatsList zero_handle_list;
  stats.child_process_host_id = 100;
  stats.renderer_handle = 0;
  zero_handle_list.push_back(stats);
  process_id_pairs =
      TabManagerDelegate::GetChildProcessInfos(zero_handle_list);
  EXPECT_EQ(0u, process_id_pairs.size());

  // Two tabs in the same process generates one handle out. When a duplicate
  // occurs the later instance is dropped.
  TabStatsList same_process_list;
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
      TabManagerDelegate::GetChildProcessInfos(same_process_list);
  EXPECT_EQ(2u, process_id_pairs.size());
  EXPECT_EQ(100, process_id_pairs[0].first);
  EXPECT_EQ(101, process_id_pairs[0].second);
  EXPECT_EQ(200, process_id_pairs[1].first);
  EXPECT_EQ(201, process_id_pairs[1].second);
}

TEST_F(TabManagerDelegateTest, LowMemoryKill) {
  std::vector<arc::ArcProcess> arc_processes = {
    {1, 10, "top", arc::ProcessState::TOP},
    {2, 20, "foreground", arc::ProcessState::FOREGROUND_SERVICE},
    {3, 30, "service", arc::ProcessState::SERVICE}
  };

  TabStats tab1, tab2, tab3, tab4, tab5;
  tab1.tab_contents_id = 100;
  tab1.is_pinned = true;

  tab2.tab_contents_id = 200;
  tab2.is_internal_page = true;

  tab3.tab_contents_id = 300;
  tab3.is_pinned = true;
  tab3.is_media = true;

  tab4.tab_contents_id = 400;
  tab4.is_media = true;

  tab5.tab_contents_id = 500;
  tab5.is_app = true;
  TabStatsList tab_list = {
    tab1, tab2, tab3, tab4, tab5
  };

  std::vector<TabManagerDelegate::KillCandidate> candidates =
      TabManagerDelegate::GetSortedKillCandidates(
          tab_list, arc_processes);
  EXPECT_EQ(8U, candidates.size());

  EXPECT_EQ("service", candidates[0].app->process_name);
  EXPECT_EQ("foreground", candidates[1].app->process_name);
  EXPECT_EQ(200, candidates[2].tab->tab_contents_id);
  EXPECT_EQ(500, candidates[3].tab->tab_contents_id);
  EXPECT_EQ(100, candidates[4].tab->tab_contents_id);
  EXPECT_EQ(400, candidates[5].tab->tab_contents_id);
  EXPECT_EQ(300, candidates[6].tab->tab_contents_id);
  EXPECT_EQ("top", candidates[7].app->process_name);
}

}  // namespace memory
