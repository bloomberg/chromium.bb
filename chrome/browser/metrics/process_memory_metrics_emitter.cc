// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/process_memory_metrics_emitter.h"

#include "base/metrics/histogram_macros.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/service_names.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

using ProcessMemoryDumpPtr =
    memory_instrumentation::mojom::ProcessMemoryDumpPtr;

namespace {

void EmitBrowserMemoryMetrics(const ProcessMemoryDumpPtr& pmd) {
  UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Experimental.Browser2.Resident",
                                pmd->os_dump.resident_set_kb / 1024);
  UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Experimental.Browser2.Malloc",
                                pmd->chrome_dump.malloc_total_kb / 1024);
  UMA_HISTOGRAM_MEMORY_LARGE_MB(
      "Memory.Experimental.Browser2.PrivateMemoryFootprint",
      pmd->private_footprint / 1024);
}

void EmitRendererMemoryMetrics(const ProcessMemoryDumpPtr& pmd) {
  UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Experimental.Renderer2.Resident",
                                pmd->os_dump.resident_set_kb / 1024);
  UMA_HISTOGRAM_MEMORY_LARGE_MB(
      "Memory.Experimental.Renderer2.PrivateMemoryFootprint",
      pmd->private_footprint / 1024);
  UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Experimental.Renderer2.Malloc",
                                pmd->chrome_dump.malloc_total_kb / 1024);
  UMA_HISTOGRAM_MEMORY_LARGE_MB(
      "Memory.Experimental.Renderer2.PartitionAlloc",
      pmd->chrome_dump.partition_alloc_total_kb / 1024);
  UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Experimental.Renderer2.BlinkGC",
                                pmd->chrome_dump.blink_gc_total_kb / 1024);
  UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Experimental.Renderer2.V8",
                                pmd->chrome_dump.v8_total_kb / 1024);
}

void EmitGpuMemoryMetrics(const ProcessMemoryDumpPtr& pmd) {
  UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Experimental.Gpu2.Resident",
                                pmd->os_dump.resident_set_kb / 1024);
  UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Experimental.Gpu2.Malloc",
                                pmd->chrome_dump.malloc_total_kb / 1024);
  UMA_HISTOGRAM_MEMORY_LARGE_MB(
      "Memory.Experimental.Gpu2.CommandBuffer",
      pmd->chrome_dump.command_buffer_total_kb / 1024);
  UMA_HISTOGRAM_MEMORY_LARGE_MB(
      "Memory.Experimental.Gpu2.PrivateMemoryFootprint",
      pmd->private_footprint / 1024);
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

void ProcessMemoryMetricsEmitter::ReceivedMemoryDump(
    uint64_t dump_guid,
    bool success,
    memory_instrumentation::mojom::GlobalMemoryDumpPtr ptr) {
  if (!success)
    return;
  if (!ptr)
    return;

  uint32_t private_footprint_total_kb = 0;
  for (const ProcessMemoryDumpPtr& pmd : ptr->process_dumps) {
    private_footprint_total_kb += pmd->private_footprint;
    switch (pmd->process_type) {
      case memory_instrumentation::mojom::ProcessType::BROWSER:
        EmitBrowserMemoryMetrics(pmd);
        break;
      case memory_instrumentation::mojom::ProcessType::RENDERER:
        EmitRendererMemoryMetrics(pmd);
        break;
      case memory_instrumentation::mojom::ProcessType::GPU:
        EmitGpuMemoryMetrics(pmd);
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
}
