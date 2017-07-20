// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/process_memory_metrics_emitter.h"

#include "base/containers/flat_map.h"
#include "base/memory/ref_counted.h"
#include "components/ukm/test_ukm_recorder.h"
#include "testing/gtest/include/gtest/gtest.h"

using GlobalMemoryDumpPtr = memory_instrumentation::mojom::GlobalMemoryDumpPtr;
using ProcessMemoryDumpPtr =
    memory_instrumentation::mojom::ProcessMemoryDumpPtr;
using OSMemDumpPtr = memory_instrumentation::mojom::OSMemDumpPtr;
using ProcessType = memory_instrumentation::mojom::ProcessType;

namespace {

// Provide fake to surface ReceivedMemoryDump to public visibility.
class ProcessMemoryMetricsEmitterFake : public ProcessMemoryMetricsEmitter {
 public:
  ProcessMemoryMetricsEmitterFake() {}

  void ReceivedMemoryDump(
      bool success,
      uint64_t dump_guid,
      memory_instrumentation::mojom::GlobalMemoryDumpPtr ptr) override {
    ProcessMemoryMetricsEmitter::ReceivedMemoryDump(success, dump_guid,
                                                    std::move(ptr));
  }

 private:
  ~ProcessMemoryMetricsEmitterFake() override {}

  DISALLOW_COPY_AND_ASSIGN(ProcessMemoryMetricsEmitterFake);
};

void PopulateBrowserMetrics(GlobalMemoryDumpPtr& global_dump,
                            base::flat_map<const char*, int64_t>& metrics_mb) {
  ProcessMemoryDumpPtr pmd(
      memory_instrumentation::mojom::ProcessMemoryDump::New());
  pmd->process_type = ProcessType::BROWSER;
  pmd->chrome_dump.malloc_total_kb = metrics_mb["Malloc"] * 1024;
  OSMemDumpPtr os_dump(memory_instrumentation::mojom::OSMemDump::New(
      metrics_mb["Resident"] * 1024,
      metrics_mb["PrivateMemoryFootprint"] * 1024));
  pmd->os_dump = std::move(os_dump);
  global_dump->process_dumps.push_back(std::move(pmd));
}

base::flat_map<const char*, int64_t> GetExpectedBrowserMetrics() {
  return base::flat_map<const char*, int64_t>(
      {
          {"ProcessType", static_cast<int64_t>(ProcessType::BROWSER)},
          {"Resident", 10},
          {"Malloc", 20},
          {"PrivateMemoryFootprint", 30},
      },
      base::KEEP_FIRST_OF_DUPES);
}

void PopulateRendererMetrics(GlobalMemoryDumpPtr& global_dump,
                             base::flat_map<const char*, int64_t>& metrics_mb) {
  ProcessMemoryDumpPtr pmd(
      memory_instrumentation::mojom::ProcessMemoryDump::New());
  pmd->process_type = ProcessType::RENDERER;
  pmd->chrome_dump.malloc_total_kb = metrics_mb["Malloc"] * 1024;
  pmd->chrome_dump.partition_alloc_total_kb =
      metrics_mb["PartitionAlloc"] * 1024;
  pmd->chrome_dump.blink_gc_total_kb = metrics_mb["BlinkGC"] * 1024;
  pmd->chrome_dump.v8_total_kb = metrics_mb["V8"] * 1024;
  OSMemDumpPtr os_dump(memory_instrumentation::mojom::OSMemDump::New(
      metrics_mb["Resident"] * 1024,
      metrics_mb["PrivateMemoryFootprint"] * 1024));
  pmd->os_dump = std::move(os_dump);
  global_dump->process_dumps.push_back(std::move(pmd));
}

base::flat_map<const char*, int64_t> GetExpectedRendererMetrics() {
  return base::flat_map<const char*, int64_t>(
      {
          {"ProcessType", static_cast<int64_t>(ProcessType::RENDERER)},
          {"Resident", 110},
          {"Malloc", 120},
          {"PrivateMemoryFootprint", 130},
          {"PartitionAlloc", 140},
          {"BlinkGC", 150},
          {"V8", 160},
      },
      base::KEEP_FIRST_OF_DUPES);
}

void PopulateGpuMetrics(GlobalMemoryDumpPtr& global_dump,
                        base::flat_map<const char*, int64_t>& metrics_mb) {
  ProcessMemoryDumpPtr pmd(
      memory_instrumentation::mojom::ProcessMemoryDump::New());
  pmd->process_type = ProcessType::GPU;
  pmd->chrome_dump.malloc_total_kb = metrics_mb["Malloc"] * 1024;
  pmd->chrome_dump.command_buffer_total_kb = metrics_mb["CommandBuffer"] * 1024;
  OSMemDumpPtr os_dump(memory_instrumentation::mojom::OSMemDump::New(
      metrics_mb["Resident"] * 1024,
      metrics_mb["PrivateMemoryFootprint"] * 1024));
  pmd->os_dump = std::move(os_dump);
  global_dump->process_dumps.push_back(std::move(pmd));
}

base::flat_map<const char*, int64_t> GetExpectedGpuMetrics() {
  return base::flat_map<const char*, int64_t>(
      {
          {"ProcessType", static_cast<int64_t>(ProcessType::GPU)},
          {"Resident", 210},
          {"Malloc", 220},
          {"PrivateMemoryFootprint", 230},
          {"CommandBuffer", 240},
      },
      base::KEEP_FIRST_OF_DUPES);
}

void PopulateMetrics(GlobalMemoryDumpPtr& global_dump,
                     ProcessType ptype,
                     base::flat_map<const char*, int64_t>& metrics_mb) {
  switch (ptype) {
    case ProcessType::BROWSER:
      PopulateBrowserMetrics(global_dump, metrics_mb);
      return;
    case ProcessType::RENDERER:
      PopulateRendererMetrics(global_dump, metrics_mb);
      return;
    case ProcessType::GPU:
      PopulateGpuMetrics(global_dump, metrics_mb);
      return;
    case ProcessType::UTILITY:
    case ProcessType::PLUGIN:
    case ProcessType::OTHER:
      break;
  }

  // We shouldn't reach here.
  FAIL() << "Unknown process type case " << ptype << ".";
}

base::flat_map<const char*, int64_t> GetExpectedProcessMetrics(
    ProcessType ptype) {
  switch (ptype) {
    case ProcessType::BROWSER:
      return GetExpectedBrowserMetrics();
    case ProcessType::RENDERER:
      return GetExpectedRendererMetrics();
    case ProcessType::GPU:
      return GetExpectedGpuMetrics();
    case ProcessType::UTILITY:
    case ProcessType::PLUGIN:
    case ProcessType::OTHER:
      break;
  }

  // We shouldn't reach here.
  CHECK(false);
  return base::flat_map<const char*, int64_t>();
}

}  // namespace

class ProcessMemoryMetricsEmitterTest
    : public testing::TestWithParam<ProcessType> {
 public:
  ProcessMemoryMetricsEmitterTest() {}
  ~ProcessMemoryMetricsEmitterTest() override {}

 protected:
  void CheckMemoryUkmEntryMetrics(
      size_t entry_num,
      base::flat_map<const char*, int64_t> expected) {
    const ukm::mojom::UkmEntry* entry = test_ukm_recorder_.GetEntry(entry_num);
    CHECK(entry != nullptr);
    EXPECT_EQ(expected.size(), entry->metrics.size());
    for (auto it = expected.begin(); it != expected.end(); ++it) {
      const ukm::mojom::UkmMetric* actual =
          test_ukm_recorder_.FindMetric(entry, it->first);
      CHECK(actual != nullptr);
      EXPECT_EQ(it->second, actual->value);
    }
  }

  ukm::TestAutoSetUkmRecorder test_ukm_recorder_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProcessMemoryMetricsEmitterTest);
};

TEST_P(ProcessMemoryMetricsEmitterTest, CollectsSingleProcessUKMs) {
  base::flat_map<const char*, int64_t> expected_metrics =
      GetExpectedProcessMetrics(GetParam());
  uint64_t dump_guid = 333;

  GlobalMemoryDumpPtr global_dump(
      memory_instrumentation::mojom::GlobalMemoryDump::New());
  PopulateMetrics(global_dump, GetParam(), expected_metrics);

  scoped_refptr<ProcessMemoryMetricsEmitterFake> emitter(
      new ProcessMemoryMetricsEmitterFake());
  emitter->ReceivedMemoryDump(true, dump_guid, std::move(global_dump));

  EXPECT_EQ(2u, test_ukm_recorder_.entries_count());
  CheckMemoryUkmEntryMetrics(0, expected_metrics);
}

INSTANTIATE_TEST_CASE_P(SinglePtype,
                        ProcessMemoryMetricsEmitterTest,
                        testing::Values(ProcessType::BROWSER,
                                        ProcessType::RENDERER,
                                        ProcessType::GPU));

TEST_F(ProcessMemoryMetricsEmitterTest, CollectsManyProcessUKMsSingleDump) {
  std::vector<ProcessType> entries_ptypes = {
      ProcessType::BROWSER, ProcessType::RENDERER, ProcessType::GPU,
      ProcessType::GPU,     ProcessType::RENDERER, ProcessType::BROWSER,
  };
  uint64_t dump_guid = 333;

  GlobalMemoryDumpPtr global_dump(
      memory_instrumentation::mojom::GlobalMemoryDump::New());
  std::vector<base::flat_map<const char*, int64_t>> entries_metrics;
  for (const auto& ptype : entries_ptypes) {
    auto expected_metrics = GetExpectedProcessMetrics(ptype);
    PopulateMetrics(global_dump, ptype, expected_metrics);
    entries_metrics.push_back(expected_metrics);
  }

  scoped_refptr<ProcessMemoryMetricsEmitterFake> emitter(
      new ProcessMemoryMetricsEmitterFake());
  emitter->ReceivedMemoryDump(true, dump_guid, std::move(global_dump));

  EXPECT_EQ(7u, test_ukm_recorder_.entries_count());
  for (size_t i = 0; i < entries_ptypes.size(); ++i) {
    CheckMemoryUkmEntryMetrics(i, entries_metrics[i]);
  }
}

TEST_F(ProcessMemoryMetricsEmitterTest, CollectsManyProcessUKMsManyDumps) {
  std::vector<std::vector<ProcessType>> entries_ptypes = {
      {ProcessType::BROWSER, ProcessType::RENDERER, ProcessType::GPU},
      {ProcessType::GPU, ProcessType::RENDERER, ProcessType::BROWSER},
  };

  std::vector<base::flat_map<const char*, int64_t>> entries_metrics;
  scoped_refptr<ProcessMemoryMetricsEmitterFake> emitter(
      new ProcessMemoryMetricsEmitterFake());
  for (int i = 0; i < 2; ++i) {
    GlobalMemoryDumpPtr global_dump(
        memory_instrumentation::mojom::GlobalMemoryDump::New());
    for (const auto& ptype : entries_ptypes[i]) {
      auto expected_metrics = GetExpectedProcessMetrics(ptype);
      PopulateMetrics(global_dump, ptype, expected_metrics);
      entries_metrics.push_back(expected_metrics);
    }
    emitter->ReceivedMemoryDump(true, i, std::move(global_dump));
  }

  EXPECT_EQ(8u, test_ukm_recorder_.entries_count());
  for (size_t i = 0; i < entries_ptypes.size(); ++i) {
    CheckMemoryUkmEntryMetrics(i, entries_metrics[i]);
  }
}
