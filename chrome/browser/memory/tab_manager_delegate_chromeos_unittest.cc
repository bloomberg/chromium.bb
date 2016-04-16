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
#include "components/arc/test/fake_arc_bridge_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace memory {

typedef testing::Test TabManagerDelegateTest;

TEST_F(TabManagerDelegateTest, LowMemoryKill) {
  std::vector<arc::ArcProcess> arc_processes = {
      {1, 10, "top", arc::mojom::ProcessState::TOP},
      {2, 20, "foreground", arc::mojom::ProcessState::FOREGROUND_SERVICE},
      {3, 30, "service", arc::mojom::ProcessState::SERVICE}};

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

class MockTabManagerDelegate : public TabManagerDelegate {
 protected:
  // Nullify the operation for nuit test.
  void SetOomScoreAdjForTabs(
      const std::vector<std::pair<base::ProcessHandle, int>>& entries)
      override {}
};

TEST_F(TabManagerDelegateTest, SetOomScoreAdj) {
  arc::FakeArcBridgeService fake_arc_bridge_service;
  MockTabManagerDelegate tab_manager_delegate;

  std::vector<arc::ArcProcess> arc_processes = {
      {1, 10, "top", arc::mojom::ProcessState::TOP},
      {2, 20, "foreground", arc::mojom::ProcessState::FOREGROUND_SERVICE},
      {3, 30, "service", arc::mojom::ProcessState::SERVICE}};

  TabStats tab1, tab2, tab3, tab4, tab5;
  tab1.is_pinned = true;
  tab1.renderer_handle = 11;

  tab2.is_internal_page = true;
  tab2.renderer_handle = 11;

  tab3.is_pinned = true;
  tab3.is_media = true;
  tab3.renderer_handle = 12;

  tab4.is_media = true;
  tab4.renderer_handle = 12;

  tab5.is_app = true;
  tab5.renderer_handle = 12;
  TabStatsList tab_list = {tab1, tab2, tab3, tab4, tab5};

  // Sorted order:
  // app "service"     pid: 30  oom_socre_adj: 825
  // app "foreground"  pid: 20  oom_score_adj: 650
  // tab2              pid: 11  oom_socre_adj: 417
  // tab5              pid: 12  oom_score_adj: 358
  // tab1              pid: 11  oom_socre_adj: 417
  // tab4              pid: 12  oom_socre_adj: 358
  // tab3              pid: 12  oom_score_adj: 358
  // app "top"         pid: 10  oom_score_adj: 300
  tab_manager_delegate.AdjustOomPrioritiesImpl(tab_list, arc_processes);
  auto& oom_score_map = tab_manager_delegate.oom_score_map_;

  EXPECT_EQ(5U, oom_score_map.size());

  // Higher priority part.
  EXPECT_EQ(300, oom_score_map[10]);
  EXPECT_EQ(358, oom_score_map[12]);
  EXPECT_EQ(417, oom_score_map[11]);

  // Lower priority part.
  EXPECT_EQ(650, oom_score_map[20]);
  EXPECT_EQ(825, oom_score_map[30]);
}

}  // namespace memory
