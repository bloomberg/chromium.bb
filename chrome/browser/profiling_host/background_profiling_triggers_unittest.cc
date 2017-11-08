// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiling_host/background_profiling_triggers.h"

#include "base/time/time.h"
#include "chrome/browser/profiling_host/profiling_process_host.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/coordinator.h"
#include "services/resource_coordinator/public/interfaces/memory_instrumentation/memory_instrumentation.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace profiling {

namespace {

const uint32_t kProcessMallocTriggerKb = 2 * 1024 * 1024;  // 2 Gig

using GlobalMemoryDumpPtr = memory_instrumentation::mojom::GlobalMemoryDumpPtr;
using ProcessMemoryDumpPtr =
    memory_instrumentation::mojom::ProcessMemoryDumpPtr;
using OSMemDumpPtr = memory_instrumentation::mojom::OSMemDumpPtr;
using ProcessType = memory_instrumentation::mojom::ProcessType;

class MockProfilingProcessHost : public ProfilingProcessHost {
 public:
  MockProfilingProcessHost() {}
  ~MockProfilingProcessHost() override {}

  void SetMode(Mode mode) { ProfilingProcessHost::SetMode(mode); }
};

class TestBackgroundProfilingTriggers : public BackgroundProfilingTriggers {
 public:
  TestBackgroundProfilingTriggers() : BackgroundProfilingTriggers(&host_) {}
  void OnReceivedMemoryDump(bool success, GlobalMemoryDumpPtr ptr) override {
    BackgroundProfilingTriggers::OnReceivedMemoryDump(success, std::move(ptr));
  }

  void TriggerMemoryReportForProcess(base::ProcessId pid) override {
    pids_.insert(pid);
  }

  void Reset() { pids_.clear(); }

  std::set<base::ProcessId> pids_;
  MockProfilingProcessHost host_;
};

OSMemDumpPtr GetFakeOSMemDump(uint32_t resident_set_kb,
                              uint32_t private_footprint_kb) {
  using memory_instrumentation::mojom::VmRegion;
  std::vector<memory_instrumentation::mojom::VmRegionPtr> vm_regions;
  return memory_instrumentation::mojom::OSMemDump::New(
      resident_set_kb, private_footprint_kb, std::move(vm_regions));
}

void PopulateMetrics(GlobalMemoryDumpPtr& global_dump,
                     base::ProcessId pid,
                     ProcessType process_type,
                     uint32_t resident_set_kb,
                     uint32_t private_memory_kb) {
  ProcessMemoryDumpPtr pmd(
      memory_instrumentation::mojom::ProcessMemoryDump::New());
  pmd->pid = pid;
  pmd->process_type = process_type;
  pmd->chrome_dump = memory_instrumentation::mojom::ChromeMemDump::New();
  pmd->os_dump = GetFakeOSMemDump(resident_set_kb, private_memory_kb);
  global_dump->process_dumps.push_back(std::move(pmd));
}

GlobalMemoryDumpPtr GetLargeMemoryDump() {
  GlobalMemoryDumpPtr dump(
      memory_instrumentation::mojom::GlobalMemoryDump::New());
  PopulateMetrics(dump, 1, ProcessType::BROWSER, kProcessMallocTriggerKb,
                  kProcessMallocTriggerKb);
  PopulateMetrics(dump, 2, ProcessType::RENDERER, kProcessMallocTriggerKb,
                  kProcessMallocTriggerKb);
  PopulateMetrics(dump, 3, ProcessType::RENDERER, 1, 1);
  PopulateMetrics(dump, 4, ProcessType::GPU, kProcessMallocTriggerKb,
                  kProcessMallocTriggerKb);
  return dump;
}

}  // namespace.

TEST(BackgroungProfilingTriggersTest, OnReceivedMemoryDump) {
  TestBackgroundProfilingTriggers triggers;

  // Validate triggers for browser processes only.
  triggers.host_.SetMode(ProfilingProcessHost::Mode::kMinimal);

  GlobalMemoryDumpPtr dump_empty(
      memory_instrumentation::mojom::GlobalMemoryDump::New());
  triggers.OnReceivedMemoryDump(true, std::move(dump_empty));
  EXPECT_TRUE(triggers.pids_.empty());

  // A small browser process doesn't trigger a report.
  GlobalMemoryDumpPtr dump_browser(
      memory_instrumentation::mojom::GlobalMemoryDump::New());
  PopulateMetrics(dump_browser, 1, ProcessType::BROWSER, 1, 1);
  triggers.OnReceivedMemoryDump(true, std::move(dump_browser));
  EXPECT_TRUE(triggers.pids_.empty());

  // A larger browser and GPU process must trigger a report. Renderers are not
  // reported.
  triggers.OnReceivedMemoryDump(true, GetLargeMemoryDump());
  EXPECT_THAT(triggers.pids_, testing::ElementsAre(1, 4));
  triggers.Reset();

  // A small gpu process doesn't trigger a report.
  GlobalMemoryDumpPtr dump_gpu(
      memory_instrumentation::mojom::GlobalMemoryDump::New());
  PopulateMetrics(dump_gpu, 1, ProcessType::GPU, 1, 1);
  triggers.OnReceivedMemoryDump(true, std::move(dump_gpu));
  EXPECT_TRUE(triggers.pids_.empty());

  // Validate triggers for all processes.
  triggers.host_.SetMode(ProfilingProcessHost::Mode::kAll);

  // Both browser and renderer must be reported.
  triggers.OnReceivedMemoryDump(true, GetLargeMemoryDump());
  EXPECT_THAT(triggers.pids_, testing::ElementsAre(1, 2, 4));
  triggers.Reset();

  // Validate triggers when off.
  triggers.host_.SetMode(ProfilingProcessHost::Mode::kNone);

  triggers.OnReceivedMemoryDump(true, GetLargeMemoryDump());
  EXPECT_TRUE(triggers.pids_.empty());
  triggers.Reset();
}

}  // namespace profiling
