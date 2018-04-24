// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/resource_coordinator/resource_coordinator_render_process_probe.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/test/test_utils.h"
#include "services/resource_coordinator/public/cpp/resource_coordinator_features.h"
#include "url/gurl.h"

namespace resource_coordinator {

class TestingResourceCoordinatorRenderProcessProbe
    : public ResourceCoordinatorRenderProcessProbe {
 public:
  TestingResourceCoordinatorRenderProcessProbe() = default;
  ~TestingResourceCoordinatorRenderProcessProbe() override = default;

  bool DispatchMetrics() override {
    current_run_loop_->QuitWhenIdle();
    return false;
  }

  // Returns |true| if all of the elements in |*render_process_info_map_|
  // are up-to-date with |current_gather_cycle_|.
  bool AllMeasurementsAreAtCurrentCycle() const {
    for (auto& render_process_info_map_entry : render_process_info_map_) {
      auto& render_process_info = render_process_info_map_entry.second;
      if (render_process_info.last_gather_cycle_active !=
              current_gather_cycle_ ||
          render_process_info.cpu_usage < 0.0) {
        return false;
      }
    }
    return true;
  }

  const RenderProcessInfoMap& render_process_info_map() const {
    return render_process_info_map_;
  }

  size_t current_gather_cycle() const { return current_gather_cycle_; }
  bool is_gather_cycle_started() const { return is_gather_cycle_started_; }

  void WaitForGather() {
    base::RunLoop run_loop;
    current_run_loop_ = &run_loop;

    run_loop.Run();

    current_run_loop_ = nullptr;
  }

  void StartGatherCycleAndWait() {
    StartGatherCycle();
    WaitForGather();
  }

 private:
  base::RunLoop* current_run_loop_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(TestingResourceCoordinatorRenderProcessProbe);
};

class ResourceCoordinatorRenderProcessProbeBrowserTest
    : public InProcessBrowserTest {
 public:
  ResourceCoordinatorRenderProcessProbeBrowserTest() = default;
  ~ResourceCoordinatorRenderProcessProbeBrowserTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(ResourceCoordinatorRenderProcessProbeBrowserTest);
};

IN_PROC_BROWSER_TEST_F(ResourceCoordinatorRenderProcessProbeBrowserTest,
                       TrackAndMeasureActiveRenderProcesses) {
#if defined(OS_WIN)
  // TODO(https://crbug.com/833430): Spare-RPH-related failures when run with
  // --site-per-process on Windows.
  if (content::AreAllSitesIsolatedForTesting())
    return;
#endif
  // Ensure that the |resource_coordinator| service is enabled.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({features::kGlobalResourceCoordinator,
                                 features::kGRCRenderProcessCPUProfiling},
                                {});

  TestingResourceCoordinatorRenderProcessProbe probe;

  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0u, probe.current_gather_cycle());

  // A tab is already open when the test begins.
  probe.StartGatherCycleAndWait();
  EXPECT_EQ(1u, probe.current_gather_cycle());
  size_t initial_size = probe.render_process_info_map().size();
  EXPECT_LE(1u, initial_size);
  EXPECT_GE(2u, initial_size);  // If a spare RenderProcessHost is present.
  EXPECT_TRUE(probe.AllMeasurementsAreAtCurrentCycle());

  // Open a second tab and complete a navigation.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), embedded_test_server()->GetURL("/title1.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
          ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  probe.StartGatherCycleAndWait();
  EXPECT_EQ(2u, probe.current_gather_cycle());
  EXPECT_EQ(initial_size + 1u, probe.render_process_info_map().size());
  EXPECT_TRUE(probe.AllMeasurementsAreAtCurrentCycle());

  // Verify that the elements in the map are reused across multiple
  // measurement cycles.
  std::map<int, const RenderProcessInfo*> info_map;
  for (const auto& entry : probe.render_process_info_map()) {
    const int key = entry.first;
    const RenderProcessInfo& info = entry.second;
    EXPECT_EQ(key, info.render_process_host_id);
    EXPECT_TRUE(info_map.insert(std::make_pair(entry.first, &info)).second);
  }

  size_t info_map_size = info_map.size();
  probe.StartGatherCycleAndWait();

  EXPECT_EQ(info_map_size, info_map.size());
  for (const auto& entry : probe.render_process_info_map()) {
    const int key = entry.first;
    const RenderProcessInfo& info = entry.second;

    EXPECT_EQ(&info, info_map[key]);
  }

  // Kill one of the two open tabs.
  EXPECT_TRUE(browser()
                  ->tab_strip_model()
                  ->GetActiveWebContents()
                  ->GetMainFrame()
                  ->GetProcess()
                  ->FastShutdownIfPossible());
  probe.StartGatherCycleAndWait();
  EXPECT_EQ(4u, probe.current_gather_cycle());
  EXPECT_EQ(initial_size, probe.render_process_info_map().size());
  EXPECT_TRUE(probe.AllMeasurementsAreAtCurrentCycle());
}

IN_PROC_BROWSER_TEST_F(ResourceCoordinatorRenderProcessProbeBrowserTest,
                       StartSingleGather) {
  // Ensure that the |resource_coordinator| service is enabled.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({features::kGlobalResourceCoordinator,
                                 features::kGRCRenderProcessCPUProfiling},
                                {});

  TestingResourceCoordinatorRenderProcessProbe probe;

  // Test the gather cycle state.
  EXPECT_FALSE(probe.is_gather_cycle_started());
  probe.StartGatherCycle();
  EXPECT_TRUE(probe.is_gather_cycle_started());
  probe.WaitForGather();
  EXPECT_FALSE(probe.is_gather_cycle_started());
  EXPECT_EQ(1u, probe.current_gather_cycle());

  // Test a single gather while the gather cycle is disabled.
  probe.StartSingleGather();
  EXPECT_FALSE(probe.is_gather_cycle_started());
  probe.WaitForGather();
  EXPECT_FALSE(probe.is_gather_cycle_started());
  EXPECT_EQ(2u, probe.current_gather_cycle());

  // Test a single gather followed by starting the gather cycle.
  probe.StartSingleGather();
  EXPECT_FALSE(probe.is_gather_cycle_started());
  probe.StartGatherCycle();
  EXPECT_TRUE(probe.is_gather_cycle_started());
  probe.WaitForGather();
  EXPECT_FALSE(probe.is_gather_cycle_started());
  EXPECT_EQ(3u, probe.current_gather_cycle());

  // And now a single gather after the cycle is started.
  probe.StartGatherCycle();
  EXPECT_TRUE(probe.is_gather_cycle_started());
  probe.StartSingleGather();
  probe.WaitForGather();
  EXPECT_FALSE(probe.is_gather_cycle_started());
  EXPECT_EQ(4u, probe.current_gather_cycle());
}

}  // namespace resource_coordinator
