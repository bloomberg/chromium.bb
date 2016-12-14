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
#include "base/message_loop/message_loop.h"
#include "base/process/process_handle.h"
#include "chrome/browser/chromeos/arc/process/arc_process.h"
#include "chrome/browser/memory/tab_manager.h"
#include "chrome/browser/memory/tab_stats.h"
#include "chrome/common/url_constants.h"
#include "chromeos/dbus/fake_debug_daemon_client.h"
#include "components/arc/common/process.mojom.h"
#include "components/exo/shell_surface.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/wm/public/activation_client.h"
#include "url/gurl.h"

namespace memory {

using TabManagerDelegateTest = testing::Test;

namespace {
constexpr bool kIsFocused = true;
constexpr bool kNotFocused = false;
}  // namespace

TEST_F(TabManagerDelegateTest, CandidatesSorted) {
  std::vector<arc::ArcProcess> arc_processes;
  arc_processes.emplace_back(1, 10, "focused", arc::mojom::ProcessState::TOP,
                             kIsFocused, 100);
  arc_processes.emplace_back(2, 20, "visible1", arc::mojom::ProcessState::TOP,
                             kNotFocused, 200);
  arc_processes.emplace_back(
      3, 30, "service", arc::mojom::ProcessState::SERVICE, kNotFocused, 500);
  arc_processes.emplace_back(4, 40, "visible2", arc::mojom::ProcessState::TOP,
                             kNotFocused, 150);

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

  candidates = TabManagerDelegate::GetSortedCandidates(
          tab_list, arc_processes);
  EXPECT_EQ(9U, candidates.size());

  // focused app.
  EXPECT_EQ("focused", candidates[0].app()->process_name());
  // visible app 1, last_activity_time larger than visible app 2.
  EXPECT_EQ("visible1", candidates[1].app()->process_name());
  // visible app 2, last_activity_time less than visible app 1.
  EXPECT_EQ("visible2", candidates[2].app()->process_name());
  // pinned and media.
  EXPECT_EQ(300, candidates[3].tab()->tab_contents_id);
  // media.
  EXPECT_EQ(400, candidates[4].tab()->tab_contents_id);
  // pinned.
  EXPECT_EQ(100, candidates[5].tab()->tab_contents_id);
  // chrome app.
  EXPECT_EQ(500, candidates[6].tab()->tab_contents_id);
  // internal page.
  EXPECT_EQ(200, candidates[7].tab()->tab_contents_id);
  // background service.
  EXPECT_EQ("service", candidates[8].app()->process_name());
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
  bool KillArcProcess(const int nspid) override {
    killed_arc_processes_.push_back(nspid);
    return true;
  }

  bool KillTab(int64_t tab_id) override {
    killed_tabs_.push_back(tab_id);
    return true;
  }

  chromeos::DebugDaemonClient* GetDebugDaemonClient() override {
    return &debugd_client_;
  }

 private:
  chromeos::FakeDebugDaemonClient debugd_client_;
  std::vector<int> killed_arc_processes_;
  std::vector<int64_t> killed_tabs_;
};

class MockMemoryStat : public TabManagerDelegate::MemoryStat {
 public:
  MockMemoryStat() {}
  ~MockMemoryStat() override {}

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
  base::MessageLoop message_loop;
  MockTabManagerDelegate tab_manager_delegate;

  std::vector<arc::ArcProcess> arc_processes;
  arc_processes.emplace_back(1, 10, "focused", arc::mojom::ProcessState::TOP,
                             kIsFocused, 100);
  arc_processes.emplace_back(2, 20, "visible1", arc::mojom::ProcessState::TOP,
                             kNotFocused, 200);
  arc_processes.emplace_back(
      3, 30, "service", arc::mojom::ProcessState::SERVICE, kNotFocused, 500);
  arc_processes.emplace_back(4, 40, "visible2", arc::mojom::ProcessState::TOP,
                             kNotFocused, 150);

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
  // app "focused"     pid: 10
  // app "visible1"    pid: 20
  // app "visible2"    pid: 40
  // tab3              pid: 12
  // tab4              pid: 12
  // tab1              pid: 11
  // tab5              pid: 12
  // tab2              pid: 11
  // app "service"     pid: 30
  tab_manager_delegate.AdjustOomPrioritiesImpl(tab_list, arc_processes);
  auto& oom_score_map = tab_manager_delegate.oom_score_map_;

  EXPECT_EQ(6U, oom_score_map.size());

  // Higher priority part.
  EXPECT_EQ(300, oom_score_map[10]);
  EXPECT_EQ(344, oom_score_map[20]);
  EXPECT_EQ(388, oom_score_map[40]);
  EXPECT_EQ(431, oom_score_map[12]);
  EXPECT_EQ(475, oom_score_map[11]);

  // Lower priority part.
  EXPECT_EQ(650, oom_score_map[30]);
}

TEST_F(TabManagerDelegateTest, KillMultipleProcesses) {
  // Not owned.
  MockMemoryStat* memory_stat = new MockMemoryStat();

  // Instantiate the mock instance.
  MockTabManagerDelegate tab_manager_delegate(memory_stat);

  std::vector<arc::ArcProcess> arc_processes;
  arc_processes.emplace_back(1, 10, "focused", arc::mojom::ProcessState::TOP,
                             kIsFocused, 100);
  arc_processes.emplace_back(2, 20, "visible1", arc::mojom::ProcessState::TOP,
                             kNotFocused, 200);
  arc_processes.emplace_back(
      3, 30, "service", arc::mojom::ProcessState::SERVICE, kNotFocused, 500);
  arc_processes.emplace_back(4, 40, "visible2", arc::mojom::ProcessState::TOP,
                             kNotFocused, 150);

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
  // app "focused"     pid: 10  nspid 1
  // app "visible1"    pid: 20  nspid 2
  // app "visible2"    pid: 40  nspid 4
  // tab3              pid: 12  tab_contents_id 3
  // tab4              pid: 12  tab_contents_id 4
  // tab1              pid: 11  tab_contents_id 1
  // tab5              pid: 12  tab_contents_id 5
  // tab2              pid: 11  tab_contents_id 2
  // app "service"     pid: 30  nspid 3
  memory_stat->SetTargetMemoryToFreeKB(250000);
  // Entities to be killed.
  memory_stat->SetProcessPss(30, 10000);
  memory_stat->SetProcessPss(11, 200000);
  memory_stat->SetProcessPss(12, 30000);
  // Should not be used.
  memory_stat->SetProcessPss(40, 50000);
  memory_stat->SetProcessPss(20, 30000);
  memory_stat->SetProcessPss(10, 100000);

  tab_manager_delegate.LowMemoryKillImpl(tab_list, arc_processes);

  auto killed_arc_processes = tab_manager_delegate.GetKilledArcProcesses();
  auto killed_tabs = tab_manager_delegate.GetKilledTabs();

  // Killed apps and their nspid.
  EXPECT_EQ(1U, killed_arc_processes.size());
  EXPECT_EQ(3, killed_arc_processes[0]);
  // Killed tabs and their content id.
  // Note that process with pid 11 is counted twice. But so far I don't have a
  // good way to estimate the memory freed if multiple tabs share one process.
  EXPECT_EQ(3U, killed_tabs.size());
  EXPECT_EQ(2, killed_tabs[0]);
  EXPECT_EQ(5, killed_tabs[1]);
  EXPECT_EQ(1, killed_tabs[2]);
}

}  // namespace memory
