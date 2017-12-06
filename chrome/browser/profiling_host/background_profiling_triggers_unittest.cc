// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiling_host/background_profiling_triggers.h"

#include <set>
#include <utility>
#include <vector>

#include "base/test/scoped_task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/profiling_host/profiling_process_host.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/coordinator.h"
#include "services/resource_coordinator/public/interfaces/memory_instrumentation/memory_instrumentation.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using GlobalMemoryDumpPtr = memory_instrumentation::mojom::GlobalMemoryDumpPtr;
using ProcessMemoryDumpPtr =
    memory_instrumentation::mojom::ProcessMemoryDumpPtr;
using OSMemDumpPtr = memory_instrumentation::mojom::OSMemDumpPtr;
using ProcessType = memory_instrumentation::mojom::ProcessType;

namespace profiling {
namespace {

constexpr uint32_t kProcessMallocTriggerKb = 2 * 1024 * 1024;  // 2 Gig

OSMemDumpPtr GetFakeOSMemDump(uint32_t resident_set_kb,
                              uint32_t private_footprint_kb,
                              uint32_t shared_footprint_kb) {
  using memory_instrumentation::mojom::VmRegion;
  std::vector<memory_instrumentation::mojom::VmRegionPtr> vm_regions;
  return memory_instrumentation::mojom::OSMemDump::New(
      resident_set_kb, private_footprint_kb, shared_footprint_kb,
      std::move(vm_regions), 0);
}

void PopulateMetrics(GlobalMemoryDumpPtr* global_dump,
                     base::ProcessId pid,
                     ProcessType process_type,
                     uint32_t resident_set_kb,
                     uint32_t private_memory_kb,
                     uint32_t shared_footprint_kb) {
  ProcessMemoryDumpPtr pmd(
      memory_instrumentation::mojom::ProcessMemoryDump::New());
  pmd->pid = pid;
  pmd->process_type = process_type;
  pmd->chrome_dump = memory_instrumentation::mojom::ChromeMemDump::New();
  pmd->os_dump =
      GetFakeOSMemDump(resident_set_kb, private_memory_kb, shared_footprint_kb);
  (*global_dump)->process_dumps.push_back(std::move(pmd));
}

}  // namespace

class FakeBackgroundProfilingTriggers : public BackgroundProfilingTriggers {
 public:
  explicit FakeBackgroundProfilingTriggers(ProfilingProcessHost* host)
      : BackgroundProfilingTriggers(host), was_report_triggered_(false) {}

  using BackgroundProfilingTriggers::OnReceivedMemoryDump;

  void Reset() { was_report_triggered_ = false; }
  bool WasReportTriggered() const { return was_report_triggered_; }

 private:
  void TriggerMemoryReport() override { was_report_triggered_ = true; }

  bool was_report_triggered_;
};

class BackgroundProfilingTriggersTest : public testing::Test {
 public:
  BackgroundProfilingTriggersTest()
      : scoped_task_envrionment_(
            base::test::ScopedTaskEnvironment::MainThreadType::UI),
        testing_profile_manager_(TestingBrowserProcess::GetGlobal()),
        triggers_(&host_),
        is_metrics_enabled_(true) {}

  void SetUp() override {
    ASSERT_TRUE(testing_profile_manager_.SetUp());
    ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
        &is_metrics_enabled_);
    for (base::ProcessId i = 1; i <= 10; ++i)
      profiled_pids_.push_back(i);
  }

  void TearDown() override {
    ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
        nullptr);
  }

  void SetMode(ProfilingProcessHost::Mode mode) { host_.SetMode(mode); }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_envrionment_;
  content::TestBrowserThreadBundle thread_bundle;
  TestingProfileManager testing_profile_manager_;

  ProfilingProcessHost host_;
  FakeBackgroundProfilingTriggers triggers_;
  std::vector<base::ProcessId> profiled_pids_;

  bool is_metrics_enabled_;
};

// Ensures:
//  * robust to empty memory dumps.
//  * does not trigger if below a size threshold.
TEST_F(BackgroundProfilingTriggersTest, OnReceivedMemoryDump_EmptyCases) {
  SetMode(ProfilingProcessHost::Mode::kAll);

  GlobalMemoryDumpPtr dump_empty(
      memory_instrumentation::mojom::GlobalMemoryDump::New());
  triggers_.OnReceivedMemoryDump(profiled_pids_, true, std::move(dump_empty));
  EXPECT_FALSE(triggers_.WasReportTriggered());
  triggers_.Reset();

  GlobalMemoryDumpPtr dump_browser(
      memory_instrumentation::mojom::GlobalMemoryDump::New());
  PopulateMetrics(&dump_browser, 1, ProcessType::BROWSER, 1, 1, 1);
  triggers_.OnReceivedMemoryDump(profiled_pids_, true, std::move(dump_browser));
  EXPECT_FALSE(triggers_.WasReportTriggered());
  triggers_.Reset();

  GlobalMemoryDumpPtr dump_gpu(
      memory_instrumentation::mojom::GlobalMemoryDump::New());
  PopulateMetrics(&dump_gpu, 1, ProcessType::GPU, 1, 1, 1);
  triggers_.OnReceivedMemoryDump(profiled_pids_, true, std::move(dump_gpu));
  EXPECT_FALSE(triggers_.WasReportTriggered());
  triggers_.Reset();

  GlobalMemoryDumpPtr dump_renderer(
      memory_instrumentation::mojom::GlobalMemoryDump::New());
  PopulateMetrics(&dump_renderer, 1, ProcessType::RENDERER, 1, 1, 1);
  triggers_.OnReceivedMemoryDump(profiled_pids_, true,
                                 std::move(dump_renderer));
  EXPECT_FALSE(triggers_.WasReportTriggered());
  triggers_.Reset();

  GlobalMemoryDumpPtr dump_other(
      memory_instrumentation::mojom::GlobalMemoryDump::New());
  PopulateMetrics(&dump_other, 1, ProcessType::OTHER, 1, 1, 1);
  triggers_.OnReceivedMemoryDump(profiled_pids_, true, std::move(dump_other));
  EXPECT_FALSE(triggers_.WasReportTriggered());
  triggers_.Reset();
}

// kNone mode shold trigger nothing.
TEST_F(BackgroundProfilingTriggersTest, OnReceivedMemoryDump_ModeNone) {
  SetMode(ProfilingProcessHost::Mode::kNone);

  GlobalMemoryDumpPtr dump(
      memory_instrumentation::mojom::GlobalMemoryDump::New());
  PopulateMetrics(&dump, 1, ProcessType::BROWSER, kProcessMallocTriggerKb,
                  kProcessMallocTriggerKb, kProcessMallocTriggerKb);
  PopulateMetrics(&dump, 2, ProcessType::RENDERER, kProcessMallocTriggerKb,
                  kProcessMallocTriggerKb, kProcessMallocTriggerKb);
  PopulateMetrics(&dump, 3, ProcessType::RENDERER, 1, 1, 1);
  PopulateMetrics(&dump, 4, ProcessType::GPU, kProcessMallocTriggerKb,
                  kProcessMallocTriggerKb, kProcessMallocTriggerKb);
  PopulateMetrics(&dump, 5, ProcessType::OTHER, kProcessMallocTriggerKb,
                  kProcessMallocTriggerKb, kProcessMallocTriggerKb);
  PopulateMetrics(&dump, 6, ProcessType::RENDERER, kProcessMallocTriggerKb,
                  kProcessMallocTriggerKb, kProcessMallocTriggerKb);

  triggers_.OnReceivedMemoryDump(profiled_pids_, true, std::move(dump));
  EXPECT_FALSE(triggers_.WasReportTriggered());
}

// kAll mode only reports everything over the large threshold.
TEST_F(BackgroundProfilingTriggersTest, OnReceivedMemoryDump_ModeAll) {
  SetMode(ProfilingProcessHost::Mode::kAll);

  GlobalMemoryDumpPtr dump(
      memory_instrumentation::mojom::GlobalMemoryDump::New());
  PopulateMetrics(&dump, 1, ProcessType::RENDERER, 1, 1, 1);
  PopulateMetrics(&dump, 2, ProcessType::OTHER, kProcessMallocTriggerKb,
                  kProcessMallocTriggerKb, kProcessMallocTriggerKb);

  // Small processes and OTHER type processes don't trigger.
  triggers_.OnReceivedMemoryDump(profiled_pids_, true, std::move(dump));
  EXPECT_FALSE(triggers_.WasReportTriggered());

  // Ensure each process type triggers.
  triggers_.Reset();
  dump = memory_instrumentation::mojom::GlobalMemoryDump::New();
  PopulateMetrics(&dump, 1, ProcessType::BROWSER, kProcessMallocTriggerKb,
                  kProcessMallocTriggerKb, kProcessMallocTriggerKb);
  triggers_.OnReceivedMemoryDump(profiled_pids_, true, std::move(dump));
  EXPECT_TRUE(triggers_.WasReportTriggered());

  triggers_.Reset();
  dump = memory_instrumentation::mojom::GlobalMemoryDump::New();
  PopulateMetrics(&dump, 1, ProcessType::RENDERER, kProcessMallocTriggerKb,
                  kProcessMallocTriggerKb, kProcessMallocTriggerKb);
  triggers_.OnReceivedMemoryDump(profiled_pids_, true, std::move(dump));
  EXPECT_TRUE(triggers_.WasReportTriggered());

  triggers_.Reset();
  dump = memory_instrumentation::mojom::GlobalMemoryDump::New();
  PopulateMetrics(&dump, 1, ProcessType::GPU, kProcessMallocTriggerKb,
                  kProcessMallocTriggerKb, kProcessMallocTriggerKb);
  triggers_.OnReceivedMemoryDump(profiled_pids_, true, std::move(dump));
  EXPECT_TRUE(triggers_.WasReportTriggered());
}

// Non-profiled processes don't trigger.
TEST_F(BackgroundProfilingTriggersTest, OnlyProfiledProcessesTrigger) {
  SetMode(ProfilingProcessHost::Mode::kAll);

  GlobalMemoryDumpPtr dump(
      memory_instrumentation::mojom::GlobalMemoryDump::New());
  PopulateMetrics(&dump, 101, ProcessType::BROWSER, kProcessMallocTriggerKb,
                  kProcessMallocTriggerKb, kProcessMallocTriggerKb);
  triggers_.OnReceivedMemoryDump(profiled_pids_, true, std::move(dump));
  EXPECT_FALSE(triggers_.WasReportTriggered());

  dump = memory_instrumentation::mojom::GlobalMemoryDump::New();
  PopulateMetrics(&dump, 1, ProcessType::BROWSER, kProcessMallocTriggerKb,
                  kProcessMallocTriggerKb, kProcessMallocTriggerKb);
  triggers_.OnReceivedMemoryDump(profiled_pids_, true, std::move(dump));
  EXPECT_TRUE(triggers_.WasReportTriggered());
}

// kMinimal mode only reports browser and gpu processes.
TEST_F(BackgroundProfilingTriggersTest, OnReceivedMemoryDump_ModeMinimal) {
  SetMode(ProfilingProcessHost::Mode::kMinimal);

  GlobalMemoryDumpPtr dump(
      memory_instrumentation::mojom::GlobalMemoryDump::New());
  PopulateMetrics(&dump, 1, ProcessType::BROWSER, 1, 1, 1);
  PopulateMetrics(&dump, 2, ProcessType::GPU, 1, 1, 1);
  PopulateMetrics(&dump, 3, ProcessType::OTHER, kProcessMallocTriggerKb,
                  kProcessMallocTriggerKb, kProcessMallocTriggerKb);
  PopulateMetrics(&dump, 4, ProcessType::RENDERER, kProcessMallocTriggerKb,
                  kProcessMallocTriggerKb, kProcessMallocTriggerKb);

  // Ensure Browser and GPU processes under threshold do not trigger.
  // Ensure other process types ignored.
  triggers_.OnReceivedMemoryDump(profiled_pids_, true, std::move(dump));
  EXPECT_FALSE(triggers_.WasReportTriggered());

  // Ensure BROWSER and GPU types trigger.
  triggers_.Reset();
  dump = memory_instrumentation::mojom::GlobalMemoryDump::New();
  PopulateMetrics(&dump, 1, ProcessType::BROWSER, kProcessMallocTriggerKb,
                  kProcessMallocTriggerKb, kProcessMallocTriggerKb);
  PopulateMetrics(&dump, 2, ProcessType::GPU, 1, 1, 1);
  triggers_.OnReceivedMemoryDump(profiled_pids_, true, std::move(dump));
  EXPECT_TRUE(triggers_.WasReportTriggered());

  triggers_.Reset();
  dump = memory_instrumentation::mojom::GlobalMemoryDump::New();
  PopulateMetrics(&dump, 1, ProcessType::GPU, kProcessMallocTriggerKb,
                  kProcessMallocTriggerKb, kProcessMallocTriggerKb);
  PopulateMetrics(&dump, 2, ProcessType::BROWSER, 1, 1, 1);
  triggers_.OnReceivedMemoryDump(profiled_pids_, true, std::move(dump));
  EXPECT_TRUE(triggers_.WasReportTriggered());
}

// kBrowser mode only reports browser.
TEST_F(BackgroundProfilingTriggersTest, OnReceivedMemoryDump_ModeBrowser) {
  SetMode(ProfilingProcessHost::Mode::kBrowser);

  GlobalMemoryDumpPtr dump(
      memory_instrumentation::mojom::GlobalMemoryDump::New());
  PopulateMetrics(&dump, 1, ProcessType::BROWSER, 1, 1, 1);
  PopulateMetrics(&dump, 2, ProcessType::GPU, kProcessMallocTriggerKb,
                  kProcessMallocTriggerKb, kProcessMallocTriggerKb);
  PopulateMetrics(&dump, 3, ProcessType::OTHER, kProcessMallocTriggerKb,
                  kProcessMallocTriggerKb, kProcessMallocTriggerKb);
  PopulateMetrics(&dump, 4, ProcessType::RENDERER, kProcessMallocTriggerKb,
                  kProcessMallocTriggerKb, kProcessMallocTriggerKb);

  // Ensure Browser processes under threshold do not trigger.
  // Ensure other process types ignored.
  triggers_.OnReceivedMemoryDump(profiled_pids_, true, std::move(dump));
  EXPECT_FALSE(triggers_.WasReportTriggered());

  // Ensure BROWSER type triggers.
  triggers_.Reset();
  dump = memory_instrumentation::mojom::GlobalMemoryDump::New();
  PopulateMetrics(&dump, 1, ProcessType::BROWSER, kProcessMallocTriggerKb,
                  kProcessMallocTriggerKb, kProcessMallocTriggerKb);
  triggers_.OnReceivedMemoryDump(profiled_pids_, true, std::move(dump));
  EXPECT_TRUE(triggers_.WasReportTriggered());
}

// kGpu mode only reports gpu.
TEST_F(BackgroundProfilingTriggersTest, OnReceivedMemoryDump_ModeGpu) {
  SetMode(ProfilingProcessHost::Mode::kGpu);

  GlobalMemoryDumpPtr dump(
      memory_instrumentation::mojom::GlobalMemoryDump::New());
  PopulateMetrics(&dump, 1, ProcessType::GPU, 1, 1, 1);
  PopulateMetrics(&dump, 2, ProcessType::BROWSER, kProcessMallocTriggerKb,
                  kProcessMallocTriggerKb, kProcessMallocTriggerKb);
  PopulateMetrics(&dump, 3, ProcessType::OTHER, kProcessMallocTriggerKb,
                  kProcessMallocTriggerKb, kProcessMallocTriggerKb);
  PopulateMetrics(&dump, 4, ProcessType::RENDERER, kProcessMallocTriggerKb,
                  kProcessMallocTriggerKb, kProcessMallocTriggerKb);

  // Ensure GPU processes under threshold do not trigger.
  // Ensure other process types ignored.
  triggers_.OnReceivedMemoryDump(profiled_pids_, true, std::move(dump));
  EXPECT_FALSE(triggers_.WasReportTriggered());

  // Ensure GPU type triggers.
  triggers_.Reset();
  dump = memory_instrumentation::mojom::GlobalMemoryDump::New();
  PopulateMetrics(&dump, 1, ProcessType::GPU, kProcessMallocTriggerKb,
                  kProcessMallocTriggerKb, kProcessMallocTriggerKb);
  triggers_.OnReceivedMemoryDump(profiled_pids_, true, std::move(dump));
  EXPECT_TRUE(triggers_.WasReportTriggered());
}

// kRendererSampling mode only the single profiled renderer.
TEST_F(BackgroundProfilingTriggersTest,
       DISABLED_OnReceivedMemoryDump_ModeRendererSampling) {
  SetMode(ProfilingProcessHost::Mode::kRendererSampling);

  GlobalMemoryDumpPtr dump(
      memory_instrumentation::mojom::GlobalMemoryDump::New());
  PopulateMetrics(&dump, 1, ProcessType::RENDERER, 1, 1, 1);
  PopulateMetrics(&dump, 2, ProcessType::BROWSER, kProcessMallocTriggerKb,
                  kProcessMallocTriggerKb, kProcessMallocTriggerKb);
  PopulateMetrics(&dump, 3, ProcessType::GPU, kProcessMallocTriggerKb,
                  kProcessMallocTriggerKb, kProcessMallocTriggerKb);
  PopulateMetrics(&dump, 4, ProcessType::OTHER, kProcessMallocTriggerKb,
                  kProcessMallocTriggerKb, kProcessMallocTriggerKb);

  // Ensure RENDERER processes under threshold do not trigger.
  // Ensure other process types ignored.
  triggers_.OnReceivedMemoryDump(profiled_pids_, true, std::move(dump));
  EXPECT_FALSE(triggers_.WasReportTriggered());

  // Ensure RENDERER type triggers.
  triggers_.Reset();
  dump = memory_instrumentation::mojom::GlobalMemoryDump::New();
  PopulateMetrics(&dump, 1, ProcessType::RENDERER, kProcessMallocTriggerKb,
                  kProcessMallocTriggerKb, kProcessMallocTriggerKb);
  triggers_.OnReceivedMemoryDump(profiled_pids_, true, std::move(dump));
  EXPECT_TRUE(triggers_.WasReportTriggered());
}

// Ensure IsAllowedToUpload() respects metrics collection settings.
TEST_F(BackgroundProfilingTriggersTest, IsAllowedToUpload_Metrics) {
  EXPECT_TRUE(triggers_.IsAllowedToUpload());

  is_metrics_enabled_ = false;
  EXPECT_FALSE(triggers_.IsAllowedToUpload());
}

// Ensure IsAllowedToUpload() respects incognito sessions. Checke that behavior
//   * respsects incognito sessions in primary profile.
//   * respsects incognito sessions in non-primary profiles.
//   * handles overlapping incognito sessions.
//
// NOTE: As of this test writing, TestingProfile::DestroyOffTheRecordProfile()
// is mocked out to do nothing. Currently, using
// TestingProfile::SetOffTheRecordProfile(nullptr) to fake destruction.
TEST_F(BackgroundProfilingTriggersTest, IsAllowedToUpload_Incognito) {
  // Create 2 profiles. The first is considered the primary.
  TestingProfile* primary_profile =
      testing_profile_manager_.CreateTestingProfile("primary");
  TestingProfile* secondary_profile =
      testing_profile_manager_.CreateTestingProfile("secondary");
  ASSERT_FALSE(primary_profile->HasOffTheRecordProfile());

  // Test IsAllowedToUpload() maps to incognito session in primary profile.
  EXPECT_TRUE(triggers_.IsAllowedToUpload());

  primary_profile->GetOffTheRecordProfile();
  EXPECT_FALSE(triggers_.IsAllowedToUpload());

  primary_profile->SetOffTheRecordProfile(nullptr);
  EXPECT_TRUE(triggers_.IsAllowedToUpload());

  // Test IsAllowedToUpload() maps to incognito session in secondary profile.
  secondary_profile->GetOffTheRecordProfile();
  EXPECT_FALSE(triggers_.IsAllowedToUpload());
  secondary_profile->SetOffTheRecordProfile(nullptr);
  EXPECT_TRUE(triggers_.IsAllowedToUpload());

  // Test overlapping incognitos sessions.
  primary_profile->GetOffTheRecordProfile();
  secondary_profile->GetOffTheRecordProfile();
  EXPECT_FALSE(triggers_.IsAllowedToUpload());

  secondary_profile->SetOffTheRecordProfile(nullptr);
  EXPECT_FALSE(triggers_.IsAllowedToUpload());

  secondary_profile->GetOffTheRecordProfile();
  primary_profile->SetOffTheRecordProfile(nullptr);
  EXPECT_FALSE(triggers_.IsAllowedToUpload());

  secondary_profile->SetOffTheRecordProfile(nullptr);
  EXPECT_TRUE(triggers_.IsAllowedToUpload());
}

}  // namespace profiling
