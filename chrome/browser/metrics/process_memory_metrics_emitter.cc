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
#include "services/resource_coordinator/public/cpp/resource_coordinator_features.h"
#include "services/resource_coordinator/public/interfaces/service_constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "url/gurl.h"

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
                                pmd->chrome_dump->malloc_total_kb / 1024);
  TryAddMetric(builder, "Malloc", pmd->chrome_dump->malloc_total_kb / 1024);

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
                                pmd->chrome_dump->malloc_total_kb / 1024);
  TryAddMetric(builder, "Malloc", pmd->chrome_dump->malloc_total_kb / 1024);

  UMA_HISTOGRAM_MEMORY_LARGE_MB(
      "Memory.Experimental.Renderer2.PrivateMemoryFootprint",
      pmd->os_dump->private_footprint_kb / 1024);
  UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Renderer.PrivateMemoryFootprint",
                                pmd->os_dump->private_footprint_kb / 1024);
  TryAddMetric(builder, "PrivateMemoryFootprint",
               pmd->os_dump->private_footprint_kb / 1024);

  UMA_HISTOGRAM_MEMORY_LARGE_MB(
      "Memory.Experimental.Renderer2.PartitionAlloc",
      pmd->chrome_dump->partition_alloc_total_kb / 1024);
  TryAddMetric(builder, "PartitionAlloc",
               pmd->chrome_dump->partition_alloc_total_kb / 1024);

  UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Experimental.Renderer2.BlinkGC",
                                pmd->chrome_dump->blink_gc_total_kb / 1024);
  TryAddMetric(builder, "BlinkGC", pmd->chrome_dump->blink_gc_total_kb / 1024);

  UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Experimental.Renderer2.V8",
                                pmd->chrome_dump->v8_total_kb / 1024);
  TryAddMetric(builder, "V8", pmd->chrome_dump->v8_total_kb / 1024);
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
                                pmd->chrome_dump->malloc_total_kb / 1024);
  TryAddMetric(builder, "Malloc", pmd->chrome_dump->malloc_total_kb / 1024);

  UMA_HISTOGRAM_MEMORY_LARGE_MB(
      "Memory.Experimental.Gpu2.CommandBuffer",
      pmd->chrome_dump->command_buffer_total_kb / 1024);
  TryAddMetric(builder, "CommandBuffer",
               pmd->chrome_dump->command_buffer_total_kb / 1024);

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
  MarkServiceRequestsInProgress();

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

  // The callback keeps this object alive until the callback is invoked.
  if (IsResourceCoordinatorEnabled()) {
    connector->BindInterface(resource_coordinator::mojom::kServiceName,
                             mojo::MakeRequest(&introspector_));
    auto callback2 =
        base::Bind(&ProcessMemoryMetricsEmitter::ReceivedProcessInfos, this);
    introspector_->GetProcessToURLMap(callback2);
  }
}

void ProcessMemoryMetricsEmitter::MarkServiceRequestsInProgress() {
  memory_dump_in_progress_ = true;
  if (IsResourceCoordinatorEnabled())
    get_process_urls_in_progress_ = true;
}

ProcessMemoryMetricsEmitter::~ProcessMemoryMetricsEmitter() {}

std::unique_ptr<ukm::UkmEntryBuilder>
ProcessMemoryMetricsEmitter::CreateUkmBuilder(const GURL& url) {
  static const char event_name[] = "Memory.Experimental";
  ukm::UkmRecorder* ukm_recorder = GetUkmRecorder();
  if (!ukm_recorder)
    return nullptr;

  const int32_t source = ukm_recorder->GetNewSourceID();
  ukm_recorder->UpdateSourceURL(source, url);
  return ukm_recorder->GetEntryBuilder(source, event_name);
}

void ProcessMemoryMetricsEmitter::ReceivedMemoryDump(
    bool success,
    uint64_t dump_guid,
    memory_instrumentation::mojom::GlobalMemoryDumpPtr ptr) {
  memory_dump_in_progress_ = false;
  if (!success)
    return;
  global_dump_ = std::move(ptr);
  CollateResults();
}

void ProcessMemoryMetricsEmitter::ReceivedProcessInfos(
    std::vector<resource_coordinator::mojom::ProcessInfoPtr> process_infos) {
  get_process_urls_in_progress_ = false;
  process_infos_.clear();
  process_infos_.reserve(process_infos.size());

  // If there are duplicate pids, keep the latest ProcessInfoPtr.
  for (resource_coordinator::mojom::ProcessInfoPtr& process_info :
       process_infos) {
    base::ProcessId pid = process_info->pid;
    process_infos_[pid] = std::move(process_info);
  }
  CollateResults();
}

bool ProcessMemoryMetricsEmitter::IsResourceCoordinatorEnabled() {
  return resource_coordinator::IsResourceCoordinatorEnabled();
}

ukm::UkmRecorder* ProcessMemoryMetricsEmitter::GetUkmRecorder() {
  return ukm::UkmRecorder::Get();
}

void ProcessMemoryMetricsEmitter::CollateResults() {
  if (memory_dump_in_progress_ || get_process_urls_in_progress_)
    return;
  if (!global_dump_)
    return;

  uint32_t private_footprint_total_kb = 0;
  for (const ProcessMemoryDumpPtr& pmd : global_dump_->process_dumps) {
    private_footprint_total_kb += pmd->os_dump->private_footprint_kb;

    switch (pmd->process_type) {
      case memory_instrumentation::mojom::ProcessType::BROWSER: {
        std::unique_ptr<ukm::UkmEntryBuilder> builder =
            CreateUkmBuilder(GURL());
        EmitBrowserMemoryMetrics(pmd, builder.get());
        break;
      }
      case memory_instrumentation::mojom::ProcessType::RENDERER: {
        GURL gurl;
        // If there is more than one frame being hosted in a renderer, don't
        // emit any URLs. This is not ideal, but UKM does not support
        // multiple-URLs per entry, and we must have one entry per process.
        if (process_infos_.find(pmd->pid) != process_infos_.end()) {
          const resource_coordinator::mojom::ProcessInfoPtr& process_info =
              process_infos_[pmd->pid];
          if (process_info->urls.size() == 1) {
            gurl = GURL(process_info->urls[0]);
          }
        }
        std::unique_ptr<ukm::UkmEntryBuilder> builder = CreateUkmBuilder(gurl);
        EmitRendererMemoryMetrics(pmd, builder.get());
        break;
      }
      case memory_instrumentation::mojom::ProcessType::GPU: {
        std::unique_ptr<ukm::UkmEntryBuilder> builder =
            CreateUkmBuilder(GURL());
        EmitGpuMemoryMetrics(pmd, builder.get());
        break;
      }
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

  std::unique_ptr<ukm::UkmEntryBuilder> builder = CreateUkmBuilder(GURL());
  TryAddMetric(builder.get(), "Total2.PrivateMemoryFootprint",
               private_footprint_total_kb / 1024);
}
