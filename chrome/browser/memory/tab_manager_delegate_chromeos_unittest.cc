// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory/tab_manager_delegate_chromeos.h"

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process_handle.h"
#include "chrome/browser/chromeos/arc/arc_process.h"
#include "chrome/browser/memory/tab_manager.h"
#include "chrome/browser/memory/tab_stats.h"
#include "chrome/common/url_constants.h"
#include "components/arc/common/process.mojom.h"
#include "components/arc/test/fake_arc_bridge_service.h"
#include "components/exo/shell_surface.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/wm/public/activation_client.h"
#include "url/gurl.h"

namespace memory {

namespace {

const char kExoShellSurfaceWindowName[] = "ExoShellSurface";
const char kArcProcessNamePrefix[] = "org.chromium.arc.";

}  // namespace

class TabManagerDelegateTest : public ash::test::AshTestBase {
 public:
  TabManagerDelegateTest() : application_id_(kArcProcessNamePrefix) {}
  ~TabManagerDelegateTest() override {}

  void SetUp() override {
    AshTestBase::SetUp();

    arc_window_ = CreateTestWindowInShellWithId(0);
    arc_window_->SetName(kExoShellSurfaceWindowName);
    exo::ShellSurface::SetApplicationId(arc_window_,
                                        &application_id_);
  }

 protected:
  void ActivateArcWindow() {
    GetActivationClient()->ActivateWindow(arc_window_);
  }
  void DeactivateArcWindow() {
    GetActivationClient()->DeactivateWindow(arc_window_);
  }

 private:
  aura::client::ActivationClient* GetActivationClient() {
    return aura::client::GetActivationClient(
        ash::Shell::GetPrimaryRootWindow());
  }

  aura::Window* arc_window_;
  std::string application_id_;
};

TEST_F(TabManagerDelegateTest, CandidatesSorted) {
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

  std::vector<TabManagerDelegate::Candidate> candidates;

  // Case 1: ARC window in the foreground.
  ActivateArcWindow();
  candidates = TabManagerDelegate::GetSortedCandidates(
          tab_list, arc_processes);
  EXPECT_EQ(8U, candidates.size());

  EXPECT_EQ("service", candidates[0].app->process_name);
  EXPECT_EQ("foreground", candidates[1].app->process_name);
  // internal page.
  EXPECT_EQ(200, candidates[2].tab->tab_contents_id);
  // chrome app.
  EXPECT_EQ(500, candidates[3].tab->tab_contents_id);
  // pinned.
  EXPECT_EQ(100, candidates[4].tab->tab_contents_id);
  // media.
  EXPECT_EQ(400, candidates[5].tab->tab_contents_id);
  // pinned and media.
  EXPECT_EQ(300, candidates[6].tab->tab_contents_id);
  // ARC window is the active window, so top app has highest priority.
  EXPECT_EQ("top", candidates[7].app->process_name);

  // Case 2: ARC window in the background.
  DeactivateArcWindow();
  candidates = TabManagerDelegate::GetSortedCandidates(
          tab_list, arc_processes);
  EXPECT_EQ(8U, candidates.size());

  EXPECT_EQ("service", candidates[0].app->process_name);
  EXPECT_EQ("foreground", candidates[1].app->process_name);
  // internal page.
  EXPECT_EQ(200, candidates[2].tab->tab_contents_id);

  // Chrome app and android app are tied, so both orders are correct.
  if (candidates[3].is_arc_app) {
    EXPECT_EQ("top", candidates[3].app->process_name);
    // chrome app.
    EXPECT_EQ(500, candidates[4].tab->tab_contents_id);
  } else {
    // chrome app.
    EXPECT_EQ(500, candidates[3].tab->tab_contents_id);
    EXPECT_EQ("top", candidates[4].app->process_name);
  }

  // pinned.
  EXPECT_EQ(100, candidates[5].tab->tab_contents_id);
  // media.
  EXPECT_EQ(400, candidates[6].tab->tab_contents_id);
  // pinned and media.
  EXPECT_EQ(300, candidates[7].tab->tab_contents_id);
}

class MockTabManagerDelegate : public TabManagerDelegate {
 public:
  MockTabManagerDelegate(): TabManagerDelegate(nullptr) {
  }

  explicit MockTabManagerDelegate(TabManagerDelegate::MemoryStat* mem_stat)
      : TabManagerDelegate(nullptr, mem_stat) {
  }

  // unit test.
  std::vector<int> GetKilledArcProcesses() {
    return killed_arc_processes_;
  }

  // unit test.
  std::vector<int64_t> GetKilledTabs() {
    return killed_tabs_;
  }

  // unit test.
  void Clear() {
    killed_arc_processes_.clear();
    killed_tabs_.clear();
  }

 protected:
  // Nullify the operation for unit test.
  void SetOomScoreAdjForTabs(
      const std::vector<std::pair<base::ProcessHandle, int>>& entries)
      override {}

  bool KillArcProcess(const int nspid) override {
    killed_arc_processes_.push_back(nspid);
    return true;
  }

  bool KillTab(int64_t tab_id) override {
    killed_tabs_.push_back(tab_id);
    return true;
  }

 private:
  std::vector<int> killed_arc_processes_;
  std::vector<int64_t> killed_tabs_;
};

class MockMemoryStat : public TabManagerDelegate::MemoryStat {
 public:
  MockMemoryStat() {}
  virtual ~MockMemoryStat() {}

  int TargetMemoryToFreeKB() override {
    return target_memory_to_free_kb_;
  }

  int EstimatedMemoryFreedKB(base::ProcessHandle pid) override {
    return process_pss_[pid];
  }

  // unittest.
  void SetTargetMemoryToFreeKB(const int target) {
    target_memory_to_free_kb_ = target;
  }

  // unittest.
  void SetProcessPss(base::ProcessHandle pid, int pss) {
    process_pss_[pid] = pss;
  }

 private:
  int target_memory_to_free_kb_;
  std::map<base::ProcessHandle, int> process_pss_;
};

TEST_F(TabManagerDelegateTest, SetOomScoreAdj) {
  arc::FakeArcBridgeService fake_arc_bridge_service;
  MockTabManagerDelegate tab_manager_delegate;

  ActivateArcWindow();
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

TEST_F(TabManagerDelegateTest, KillMultipleProcesses) {
  arc::FakeArcBridgeService fake_arc_bridge_service;

  // Not owned.
  MockMemoryStat* memory_stat = new MockMemoryStat();

  // Instantiate the mock instance.
  MockTabManagerDelegate tab_manager_delegate(memory_stat);

  ActivateArcWindow();

  std::vector<arc::ArcProcess> arc_processes = {
      {10001, 100, "top", arc::mojom::ProcessState::TOP},
      {10002, 200, "foreground", arc::mojom::ProcessState::FOREGROUND_SERVICE},
      {10003, 300, "service", arc::mojom::ProcessState::SERVICE}};

  TabStats tab1, tab2, tab3, tab4, tab5;
  tab1.is_pinned = true;
  tab1.renderer_handle = 11;
  tab1.tab_contents_id = 1;

  tab2.is_internal_page = true;
  tab2.renderer_handle = 11;
  tab2.tab_contents_id = 2;

  tab3.is_pinned = true;
  tab3.is_media = true;
  tab3.renderer_handle = 12;
  tab3.tab_contents_id = 3;

  tab4.is_media = true;
  tab4.renderer_handle = 12;
  tab4.tab_contents_id = 4;

  tab5.is_app = true;
  tab5.renderer_handle = 12;
  tab5.tab_contents_id = 5;
  TabStatsList tab_list = {tab1, tab2, tab3, tab4, tab5};

  // Sorted order:
  // app "service"     pid: 30
  // app "foreground"  pid: 20
  // tab2              pid: 11
  // tab5              pid: 12
  // tab1              pid: 11
  // tab4              pid: 12
  // tab3              pid: 12
  // app "top"         pid: 10
  memory_stat->SetTargetMemoryToFreeKB(250000);
  // The 3 entities to be killed.
  memory_stat->SetProcessPss(300, 10000);
  memory_stat->SetProcessPss(200, 200000);
  memory_stat->SetProcessPss(11, 50000);
  // Should not be used.
  memory_stat->SetProcessPss(100, 30000);
  memory_stat->SetProcessPss(12, 100000);

  tab_manager_delegate.LowMemoryKillImpl(tab_list, arc_processes);

  auto killed_arc_processes = tab_manager_delegate.GetKilledArcProcesses();
  auto killed_tabs = tab_manager_delegate.GetKilledTabs();

  EXPECT_EQ(2U, killed_arc_processes.size());
  EXPECT_EQ(1U, killed_tabs.size());

  // nspid.
  EXPECT_EQ(10003, killed_arc_processes[0]);
  EXPECT_EQ(10002, killed_arc_processes[1]);
  // tab content id.
  EXPECT_EQ(2, killed_tabs[0]);
}

}  // namespace memory
