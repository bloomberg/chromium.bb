// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/process_memory_metrics_emitter.h"

#include "base/metrics/histogram_macros.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/service_names.mojom.h"
#include "extensions/features/features.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/resource_coordinator/public/cpp/resource_coordinator_features.h"
#include "services/resource_coordinator/public/interfaces/service_constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_map.h"
#include "extensions/common/extension.h"
#endif

using ProcessMemoryDumpPtr =
    memory_instrumentation::mojom::ProcessMemoryDumpPtr;

namespace {

void EmitBrowserMemoryMetrics(const ProcessMemoryDumpPtr& pmd,
                              ukm::SourceId ukm_source_id,
                              ukm::UkmRecorder* ukm_recorder,
                              const base::Optional<base::TimeDelta>& uptime) {
  ukm::builders::Memory_Experimental builder(ukm_source_id);
  builder.SetProcessType(static_cast<int64_t>(
      memory_instrumentation::mojom::ProcessType::BROWSER));

  UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Experimental.Browser2.Resident",
                                pmd->os_dump->resident_set_kb / 1024);
  builder.SetResident(pmd->os_dump->resident_set_kb / 1024);

  UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Experimental.Browser2.Malloc",
                                pmd->chrome_dump->malloc_total_kb / 1024);
  builder.SetMalloc(pmd->chrome_dump->malloc_total_kb / 1024);

  UMA_HISTOGRAM_MEMORY_LARGE_MB(
      "Memory.Experimental.Browser2.PrivateMemoryFootprint",
      pmd->os_dump->private_footprint_kb / 1024);
  UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Browser.PrivateMemoryFootprint",
                                pmd->os_dump->private_footprint_kb / 1024);
  builder.SetPrivateMemoryFootprint(pmd->os_dump->private_footprint_kb / 1024);
  if (uptime)
    builder.SetUptime(uptime.value().InSeconds());
  builder.Record(ukm_recorder);
}

#define RENDERER_MEMORY_UMA_HISTOGRAMS(type)                                   \
  do {                                                                         \
    UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Experimental." type "2.Resident",    \
                                  pmd->os_dump->resident_set_kb / 1024);       \
    UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Experimental." type "2.Malloc",      \
                                  pmd->chrome_dump->malloc_total_kb / 1024);   \
    UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Experimental." type                  \
                                  "2.PrivateMemoryFootprint",                  \
                                  pmd->os_dump->private_footprint_kb / 1024);  \
    UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory." type ".PrivateMemoryFootprint",    \
                                  pmd->os_dump->private_footprint_kb / 1024);  \
    UMA_HISTOGRAM_MEMORY_LARGE_MB(                                             \
        "Memory.Experimental." type "2.PartitionAlloc",                        \
        pmd->chrome_dump->partition_alloc_total_kb / 1024);                    \
    UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Experimental." type "2.BlinkGC",     \
                                  pmd->chrome_dump->blink_gc_total_kb / 1024); \
    UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Experimental." type "2.V8",          \
                                  pmd->chrome_dump->v8_total_kb / 1024);       \
  } while (false)

void EmitRendererMemoryMetrics(
    const ProcessMemoryDumpPtr& pmd,
    const resource_coordinator::mojom::PageInfoPtr& page_info,
    ukm::UkmRecorder* ukm_recorder,
    int number_of_extensions,
    const base::Optional<base::TimeDelta>& uptime) {
  // UMA
  if (number_of_extensions == 0) {
    RENDERER_MEMORY_UMA_HISTOGRAMS("Renderer");
  } else {
    RENDERER_MEMORY_UMA_HISTOGRAMS("Extension");
  }
  // UKM
  ukm::SourceId ukm_source_id = page_info.is_null()
                                    ? ukm::UkmRecorder::GetNewSourceID()
                                    : page_info->ukm_source_id;
  ukm::builders::Memory_Experimental builder(ukm_source_id);
  builder.SetProcessType(static_cast<int64_t>(
      memory_instrumentation::mojom::ProcessType::RENDERER));
  builder.SetResident(pmd->os_dump->resident_set_kb / 1024);
  builder.SetMalloc(pmd->chrome_dump->malloc_total_kb / 1024);
  builder.SetPrivateMemoryFootprint(pmd->os_dump->private_footprint_kb / 1024);
  builder.SetPartitionAlloc(pmd->chrome_dump->partition_alloc_total_kb / 1024);
  builder.SetBlinkGC(pmd->chrome_dump->blink_gc_total_kb / 1024);
  builder.SetV8(pmd->chrome_dump->v8_total_kb / 1024);
  builder.SetNumberOfExtensions(number_of_extensions);

  if (!page_info.is_null()) {
    builder.SetIsVisible(page_info->is_visible);
    builder.SetTimeSinceLastVisibilityChange(
        page_info->time_since_last_visibility_change.InSeconds());
    builder.SetTimeSinceLastNavigation(
        page_info->time_since_last_navigation.InSeconds());
  }

  if (uptime)
    builder.SetUptime(uptime.value().InSeconds());

  builder.Record(ukm_recorder);
}

void EmitGpuMemoryMetrics(const ProcessMemoryDumpPtr& pmd,
                          ukm::SourceId ukm_source_id,
                          ukm::UkmRecorder* ukm_recorder,
                          const base::Optional<base::TimeDelta>& uptime) {
  ukm::builders::Memory_Experimental builder(ukm_source_id);
  builder.SetProcessType(
      static_cast<int64_t>(memory_instrumentation::mojom::ProcessType::GPU));

  UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Experimental.Gpu2.Resident",
                                pmd->os_dump->resident_set_kb / 1024);
  builder.SetResident(pmd->os_dump->resident_set_kb / 1024);

  UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Experimental.Gpu2.Malloc",
                                pmd->chrome_dump->malloc_total_kb / 1024);
  builder.SetMalloc(pmd->chrome_dump->malloc_total_kb / 1024);

  UMA_HISTOGRAM_MEMORY_LARGE_MB(
      "Memory.Experimental.Gpu2.CommandBuffer",
      pmd->chrome_dump->command_buffer_total_kb / 1024);
  builder.SetCommandBuffer(pmd->chrome_dump->command_buffer_total_kb / 1024);

  UMA_HISTOGRAM_MEMORY_LARGE_MB(
      "Memory.Experimental.Gpu2.PrivateMemoryFootprint",
      pmd->os_dump->private_footprint_kb / 1024);
  UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Gpu.PrivateMemoryFootprint",
                                pmd->os_dump->private_footprint_kb / 1024);
  builder.SetPrivateMemoryFootprint(pmd->os_dump->private_footprint_kb / 1024);
  if (uptime)
    builder.SetUptime(uptime.value().InSeconds());
  builder.Record(ukm_recorder);
}

}  // namespace

ProcessMemoryMetricsEmitter::ProcessMemoryMetricsEmitter() {}

void ProcessMemoryMetricsEmitter::FetchAndEmitProcessMemoryMetrics() {
  MarkServiceRequestsInProgress();

  service_manager::Connector* connector =
      content::ServiceManagerConnection::GetForProcess()->GetConnector();
  connector->BindInterface(resource_coordinator::mojom::kServiceName,
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

void ProcessMemoryMetricsEmitter::ReceivedMemoryDump(
    bool success,
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

int ProcessMemoryMetricsEmitter::GetNumberOfExtensions(base::ProcessId pid) {
  int number_of_extensions = 0;
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Retrieve the renderer process host for the given pid.
  int rph_id = -1;
  auto iter = content::RenderProcessHost::AllHostsIterator();
  while (!iter.IsAtEnd()) {
    if (base::GetProcId(iter.GetCurrentValue()->GetHandle()) == pid) {
      rph_id = iter.GetCurrentValue()->GetID();
      break;
    }
    iter.Advance();
  }
  if (iter.IsAtEnd())
    return 0;

  // Count the number of extensions associated with that renderer process host
  // in all profiles.
  for (Profile* profile :
       g_browser_process->profile_manager()->GetLoadedProfiles()) {
    extensions::ProcessMap* process_map = extensions::ProcessMap::Get(profile);
    extensions::ExtensionRegistry* registry =
        extensions::ExtensionRegistry::Get(profile);
    std::set<std::string> extension_ids =
        process_map->GetExtensionsInProcess(rph_id);
    for (const std::string& extension_id : extension_ids) {
      // Only count non hosted apps extensions.
      const extensions::Extension* extension =
          registry->enabled_extensions().GetByID(extension_id);
      if (extension && !extension->is_hosted_app())
        number_of_extensions++;
    }
  }
#endif
  return number_of_extensions;
}

base::Optional<base::TimeDelta> ProcessMemoryMetricsEmitter::GetProcessUptime(
    const base::Time& now,
    base::ProcessId pid) {
  auto process_info = process_infos_.find(pid);
  if (process_info != process_infos_.end()) {
    if (process_info->second->launch_time)
      return now - process_info->second->launch_time.value();
  }
  return base::Optional<base::TimeDelta>();
}

void ProcessMemoryMetricsEmitter::CollateResults() {
  if (memory_dump_in_progress_ || get_process_urls_in_progress_)
    return;
  if (!global_dump_)
    return;

  uint32_t private_footprint_total_kb = 0;
  base::Time now = base::Time::Now();
  for (const ProcessMemoryDumpPtr& pmd : global_dump_->process_dumps) {
    private_footprint_total_kb += pmd->os_dump->private_footprint_kb;
    switch (pmd->process_type) {
      case memory_instrumentation::mojom::ProcessType::BROWSER: {
        EmitBrowserMemoryMetrics(pmd, ukm::UkmRecorder::GetNewSourceID(),
                                 GetUkmRecorder(),
                                 GetProcessUptime(now, pmd->pid));
        break;
      }
      case memory_instrumentation::mojom::ProcessType::RENDERER: {
        resource_coordinator::mojom::PageInfoPtr page_info;
        // If there is more than one frame being hosted in a renderer, don't
        // emit any URLs. This is not ideal, but UKM does not support
        // multiple-URLs per entry, and we must have one entry per process.
        if (process_infos_.find(pmd->pid) != process_infos_.end()) {
          const resource_coordinator::mojom::ProcessInfoPtr& process_info =
              process_infos_[pmd->pid];
          if (process_info->page_infos.size() == 1) {
            page_info = std::move(process_info->page_infos[0]);
          }
        }

        int number_of_extensions = GetNumberOfExtensions(pmd->pid);
        EmitRendererMemoryMetrics(pmd, page_info, GetUkmRecorder(),
                                  number_of_extensions,
                                  GetProcessUptime(now, pmd->pid));
        break;
      }
      case memory_instrumentation::mojom::ProcessType::GPU: {
        EmitGpuMemoryMetrics(pmd, ukm::UkmRecorder::GetNewSourceID(),
                             GetUkmRecorder(), GetProcessUptime(now, pmd->pid));
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

  ukm::builders::Memory_Experimental(ukm::UkmRecorder::GetNewSourceID())
      .SetTotal2_PrivateMemoryFootprint(private_footprint_total_kb / 1024)
      .Record(GetUkmRecorder());
}
