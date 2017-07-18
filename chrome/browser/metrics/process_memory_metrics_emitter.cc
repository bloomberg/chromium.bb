// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/process_memory_metrics_emitter.h"

#include "base/metrics/histogram_macros.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/service_names.mojom.h"
#include "services/metrics/public/cpp/ukm_entry_builder.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/service_manager/public/cpp/connector.h"

using ProcessMemoryDumpPtr =
    memory_instrumentation::mojom::ProcessMemoryDumpPtr;

namespace {

void TryAddMetric(ukm::UkmEntryBuilder* builder,
                  const char* metric_name,
                  int64_t value) {
  // Builder might be null if it was created before the UKMService was started.
  // In that case, just no-op.
  if (builder)
    builder->AddMetric(metric_name, value);
}

void EmitBrowserMemoryMetrics(const ProcessMemoryDumpPtr& pmd,
                              ukm::UkmEntryBuilder* builder) {
  TryAddMetric(builder, "ProcessType",
               static_cast<int64_t>(
                   memory_instrumentation::mojom::ProcessType::BROWSER));

  UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Experimental.Browser2.Resident",
                                pmd->os_dump->resident_set_kb / 1024);
  TryAddMetric(builder, "Resident", pmd->os_dump->resident_set_kb / 1024);

  UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Experimental.Browser2.Malloc",
                                pmd->chrome_dump.malloc_total_kb / 1024);
  TryAddMetric(builder, "Malloc", pmd->chrome_dump.malloc_total_kb / 1024);

  UMA_HISTOGRAM_MEMORY_LARGE_MB(
      "Memory.Experimental.Browser2.PrivateMemoryFootprint",
      pmd->os_dump->private_footprint_kb / 1024);
  UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Browser.PrivateMemoryFootprint",
                                pmd->os_dump->private_footprint_kb / 1024);
  TryAddMetric(builder, "PrivateMemoryFootprint",
               pmd->os_dump->private_footprint_kb / 1024);
}

void EmitRendererMemoryMetrics(const ProcessMemoryDumpPtr& pmd,
                               ukm::UkmEntryBuilder* builder) {
  TryAddMetric(builder, "ProcessType",
               static_cast<int64_t>(
                   memory_instrumentation::mojom::ProcessType::RENDERER));

  UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Experimental.Renderer2.Resident",
                                pmd->os_dump->resident_set_kb / 1024);
  TryAddMetric(builder, "Resident", pmd->os_dump->resident_set_kb / 1024);

  UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Experimental.Renderer2.Malloc",
                                pmd->chrome_dump.malloc_total_kb / 1024);
  TryAddMetric(builder, "Malloc", pmd->chrome_dump.malloc_total_kb / 1024);

  UMA_HISTOGRAM_MEMORY_LARGE_MB(
      "Memory.Experimental.Renderer2.PrivateMemoryFootprint",
      pmd->os_dump->private_footprint_kb / 1024);
  UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Renderer.PrivateMemoryFootprint",
                                pmd->os_dump->private_footprint_kb / 1024);
  TryAddMetric(builder, "PrivateMemoryFootprint",
               pmd->os_dump->private_footprint_kb / 1024);

  UMA_HISTOGRAM_MEMORY_LARGE_MB(
      "Memory.Experimental.Renderer2.PartitionAlloc",
      pmd->chrome_dump.partition_alloc_total_kb / 1024);
  TryAddMetric(builder, "PartitionAlloc",
               pmd->chrome_dump.partition_alloc_total_kb / 1024);

  UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Experimental.Renderer2.BlinkGC",
                                pmd->chrome_dump.blink_gc_total_kb / 1024);
  TryAddMetric(builder, "BlinkGC", pmd->chrome_dump.blink_gc_total_kb / 1024);

  UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Experimental.Renderer2.V8",
                                pmd->chrome_dump.v8_total_kb / 1024);
  TryAddMetric(builder, "V8", pmd->chrome_dump.v8_total_kb / 1024);
}

void EmitGpuMemoryMetrics(const ProcessMemoryDumpPtr& pmd,
                          ukm::UkmEntryBuilder* builder) {
  TryAddMetric(
      builder, "ProcessType",
      static_cast<int64_t>(memory_instrumentation::mojom::ProcessType::GPU));

  UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Experimental.Gpu2.Resident",
                                pmd->os_dump->resident_set_kb / 1024);
  TryAddMetric(builder, "Resident", pmd->os_dump->resident_set_kb / 1024);

  UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Experimental.Gpu2.Malloc",
                                pmd->chrome_dump.malloc_total_kb / 1024);
  TryAddMetric(builder, "Malloc", pmd->chrome_dump.malloc_total_kb / 1024);

  UMA_HISTOGRAM_MEMORY_LARGE_MB(
      "Memory.Experimental.Gpu2.CommandBuffer",
      pmd->chrome_dump.command_buffer_total_kb / 1024);
  TryAddMetric(builder, "CommandBuffer",
               pmd->chrome_dump.command_buffer_total_kb / 1024);

  UMA_HISTOGRAM_MEMORY_LARGE_MB(
      "Memory.Experimental.Gpu2.PrivateMemoryFootprint",
      pmd->os_dump->private_footprint_kb / 1024);
  UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Gpu.PrivateMemoryFootprint",
                                pmd->os_dump->private_footprint_kb / 1024);
  TryAddMetric(builder, "PrivateMemoryFootprint",
               pmd->os_dump->private_footprint_kb / 1024);
}

}  // namespace

ProcessMemoryMetricsEmitter::ProcessMemoryMetricsEmitter() {}

void ProcessMemoryMetricsEmitter::FetchAndEmitProcessMemoryMetrics() {
  service_manager::Connector* connector =
      content::ServiceManagerConnection::GetForProcess()->GetConnector();
  connector->BindInterface(content::mojom::kBrowserServiceName,
                           mojo::MakeRequest(&coordinator_));

  // The callback keeps this object alive until the callback is invoked..
  auto callback =
      base::Bind(&ProcessMemoryMetricsEmitter::ReceivedMemoryDump, this);

  base::trace_event::MemoryDumpRequestArgs args = {
      0, base::trace_event::MemoryDumpType::SUMMARY_ONLY,
      base::trace_event::MemoryDumpLevelOfDetail::BACKGROUND};
  coordinator_->RequestGlobalMemoryDump(args, callback);
}

ProcessMemoryMetricsEmitter::~ProcessMemoryMetricsEmitter() {}

std::unique_ptr<ukm::UkmEntryBuilder>
ProcessMemoryMetricsEmitter::CreateUkmBuilder(const char* event_name) {
  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
  if (!ukm_recorder)
    return nullptr;

  const int32_t source_id = ukm_recorder->GetNewSourceID();
  return ukm_recorder->GetEntryBuilder(source_id, event_name);
}

void ProcessMemoryMetricsEmitter::ReceivedMemoryDump(
    bool success,
    uint64_t dump_guid,
    memory_instrumentation::mojom::GlobalMemoryDumpPtr ptr) {
  if (!success)
    return;
  if (!ptr)
    return;

  uint32_t private_footprint_total_kb = 0;
  for (const ProcessMemoryDumpPtr& pmd : ptr->process_dumps) {
    private_footprint_total_kb += pmd->os_dump->private_footprint_kb;

    // Populate a new entry for each ProcessMemoryDumpPtr in the global dump,
    // annotating each entry with the dump GUID so that entries in the same
    // global dump can be correlated with each other.
    // TODO(jchinlee): Add URLs.
    std::unique_ptr<ukm::UkmEntryBuilder> builder =
        CreateUkmBuilder("Memory.Experimental");

    switch (pmd->process_type) {
      case memory_instrumentation::mojom::ProcessType::BROWSER:
        EmitBrowserMemoryMetrics(pmd, builder.get());
        break;
      case memory_instrumentation::mojom::ProcessType::RENDERER:
        EmitRendererMemoryMetrics(pmd, builder.get());
        break;
      case memory_instrumentation::mojom::ProcessType::GPU:
        EmitGpuMemoryMetrics(pmd, builder.get());
        break;
      case memory_instrumentation::mojom::ProcessType::UTILITY:
      case memory_instrumentation::mojom::ProcessType::PLUGIN:
      case memory_instrumentation::mojom::ProcessType::OTHER:
        break;
    }
  }
  UMA_HISTOGRAM_MEMORY_LARGE_MB(
      "Memory.Experimental.Total2.PrivateMemoryFootprint",
      private_footprint_total_kb / 1024);
  UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Total.PrivateMemoryFootprint",
                                private_footprint_total_kb / 1024);

  std::unique_ptr<ukm::UkmEntryBuilder> builder =
      CreateUkmBuilder("Memory.Experimental");
  TryAddMetric(builder.get(), "Total2.PrivateMemoryFootprint",
               private_footprint_total_kb / 1024);
}
