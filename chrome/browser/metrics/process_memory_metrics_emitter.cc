// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/process_memory_metrics_emitter.h"

#include "base/metrics/histogram_macros.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/service_names.mojom.h"
#include "extensions/buildflags/buildflags.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"
#include "services/resource_coordinator/public/cpp/resource_coordinator_features.h"
#include "services/resource_coordinator/public/mojom/service_constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_map.h"
#include "extensions/common/extension.h"
#endif

using memory_instrumentation::GlobalMemoryDump;
using ukm::builders::Memory_Experimental;

namespace {

const char kEffectiveSize[] = "effective_size";

struct Metric {
  const char* const dump_name;
  const char* const ukm_name;
  const char* const metric;
  Memory_Experimental& (Memory_Experimental::*setter)(int64_t);
};
const Metric kAllocatorDumpNamesForMetrics[] = {
    {"blink_gc", "BlinkGC", kEffectiveSize, &Memory_Experimental::SetBlinkGC},
    {"blink_gc/allocated_objects", "BlinkGC.AllocatedObjects", kEffectiveSize,
     &Memory_Experimental::SetBlinkGC_AllocatedObjects},
    {"blink_objects/Document", "NumberOfDocuments",
     base::trace_event::MemoryAllocatorDump::kNameObjectCount,
     &Memory_Experimental::SetNumberOfDocuments},
    {"blink_objects/Frame", "NumberOfFrames",
     base::trace_event::MemoryAllocatorDump::kNameObjectCount,
     &Memory_Experimental::SetNumberOfFrames},
    {"blink_objects/LayoutObject", "NumberOfLayoutObjects",
     base::trace_event::MemoryAllocatorDump::kNameObjectCount,
     &Memory_Experimental::SetNumberOfLayoutObjects},
    {"blink_objects/Node", "NumberOfNodes",
     base::trace_event::MemoryAllocatorDump::kNameObjectCount,
     &Memory_Experimental::SetNumberOfNodes},
    {"components/download", "DownloadService", kEffectiveSize,
     &Memory_Experimental::SetDownloadService},
    {"discardable", "Discardable", kEffectiveSize,
     &Memory_Experimental::SetDiscardable},
    {"extensions/value_store", "Extensions.ValueStore", kEffectiveSize,
     &Memory_Experimental::SetExtensions_ValueStore},
    {"font_caches", "FontCaches", kEffectiveSize,
     &Memory_Experimental::SetFontCaches},
    {"gpu/gl", "CommandBuffer", kEffectiveSize,
     &Memory_Experimental::SetCommandBuffer},
    {"history", "History", kEffectiveSize, &Memory_Experimental::SetHistory},
    {"java_heap", "JavaHeap", kEffectiveSize,
     &Memory_Experimental::SetJavaHeap},
    {"leveldatabase", "LevelDatabase", kEffectiveSize,
     &Memory_Experimental::SetLevelDatabase},
    {"malloc", "Malloc", kEffectiveSize, &Memory_Experimental::SetMalloc},
    {"mojo", "NumberOfMojoHandles",
     base::trace_event::MemoryAllocatorDump::kNameObjectCount,
     &Memory_Experimental::SetNumberOfMojoHandles},
    {"net", "Net", kEffectiveSize, &Memory_Experimental::SetNet},
    {"net/url_request_context", "Net.UrlRequestContext", kEffectiveSize,
     &Memory_Experimental::SetNet_UrlRequestContext},
    {"omnibox", "OmniboxSuggestions", kEffectiveSize,
     &Memory_Experimental::SetOmniboxSuggestions},
    {"partition_alloc", "PartitionAlloc", kEffectiveSize,
     &Memory_Experimental::SetPartitionAlloc},
    {"partition_alloc/allocated_objects", "PartitionAlloc.AllocatedObjects",
     kEffectiveSize, &Memory_Experimental::SetPartitionAlloc_AllocatedObjects},
    {"partition_alloc/partitions/array_buffer",
     "PartitionAlloc.Partitions.ArrayBuffer", kEffectiveSize,
     &Memory_Experimental::SetPartitionAlloc_Partitions_ArrayBuffer},
    {"partition_alloc/partitions/buffer", "PartitionAlloc.Partitions.Buffer",
     kEffectiveSize, &Memory_Experimental::SetPartitionAlloc_Partitions_Buffer},
    {"partition_alloc/partitions/fast_malloc",
     "PartitionAlloc.Partitions.FastMalloc", kEffectiveSize,
     &Memory_Experimental::SetPartitionAlloc_Partitions_FastMalloc},
    {"partition_alloc/partitions/layout", "PartitionAlloc.Partitions.Layout",
     kEffectiveSize, &Memory_Experimental::SetPartitionAlloc_Partitions_Layout},
    {"site_storage", "SiteStorage", kEffectiveSize,
     &Memory_Experimental::SetSiteStorage},
    {"site_storage/blob_storage", "SiteStorage.BlobStorage", kEffectiveSize,
     &Memory_Experimental::SetSiteStorage_BlobStorage},
    {"site_storage/index_db", "SiteStorage.IndexDB", kEffectiveSize,
     &Memory_Experimental::SetSiteStorage_IndexDB},
    {"site_storage/localstorage", "SiteStorage.LocalStorage", kEffectiveSize,
     &Memory_Experimental::SetSiteStorage_LocalStorage},
    {"site_storage/session_storage", "SiteStorage.SessionStorage",
     kEffectiveSize, &Memory_Experimental::SetSiteStorage_SessionStorage},
    {"skia", "Skia", kEffectiveSize, &Memory_Experimental::SetSkia},
    {"skia/sk_glyph_cache", "Skia.SkGlyphCache", kEffectiveSize,
     &Memory_Experimental::SetSkia_SkGlyphCache},
    {"skia/sk_resource_cache", "Skia.SkResourceCache", kEffectiveSize,
     &Memory_Experimental::SetSkia_SkResourceCache},
    {"sqlite", "Sqlite", kEffectiveSize, &Memory_Experimental::SetSqlite},
    {"sync", "Sync", kEffectiveSize, &Memory_Experimental::SetSync},
    {"tab_restore", "TabRestore", kEffectiveSize,
     &Memory_Experimental::SetTabRestore},
    {"ui", "UI", kEffectiveSize, &Memory_Experimental::SetUI},
    {"v8", "V8", kEffectiveSize, &Memory_Experimental::SetV8},
    {"web_cache", "WebCache", kEffectiveSize,
     &Memory_Experimental::SetWebCache},
    {"web_cache/Image_resources", "WebCache.ImageResources", kEffectiveSize,
     &Memory_Experimental::SetWebCache_ImageResources},
    {"web_cache/CSS stylesheet_resources", "WebCache.CSSStylesheetResources",
     kEffectiveSize, &Memory_Experimental::SetWebCache_CSSStylesheetResources},
    {"web_cache/Script_resources", "WebCache.ScriptResources", kEffectiveSize,
     &Memory_Experimental::SetWebCache_ScriptResources},
    {"web_cache/XSL stylesheet_resources", "WebCache.XSLStylesheetResources",
     kEffectiveSize, &Memory_Experimental::SetWebCache_XSLStylesheetResources},
    {"web_cache/Font_resources", "WebCache.FontResources", kEffectiveSize,
     &Memory_Experimental::SetWebCache_FontResources},
    {"web_cache/Other_resources", "WebCache.OtherResources", kEffectiveSize,
     &Memory_Experimental::SetWebCache_OtherResources},
};

void EmitProcessUkm(const GlobalMemoryDump::ProcessDump& pmd,
                    const base::Optional<base::TimeDelta>& uptime,
                    Memory_Experimental* builder) {
  for (const auto& item : kAllocatorDumpNamesForMetrics) {
    base::Optional<uint64_t> value = pmd.GetMetric(item.dump_name, item.metric);
    if (value) {
      if (base::StringPiece(item.metric) ==
          base::trace_event::MemoryAllocatorDump::kNameObjectCount) {
        ((*builder).*(item.setter))(value.value());
      } else {
        ((*builder).*(item.setter))(value.value() / 1024 / 1024);
      }
    }
  }
  builder->SetResident(pmd.os_dump().resident_set_kb / 1024);
  builder->SetPrivateMemoryFootprint(pmd.os_dump().private_footprint_kb / 1024);
  builder->SetSharedMemoryFootprint(pmd.os_dump().shared_footprint_kb / 1024);
#if defined(OS_LINUX) || defined(OS_ANDROID)
  builder->SetPrivateSwapFootprint(pmd.os_dump().private_footprint_swap_kb /
                                   1024);
#endif
  if (uptime)
    builder->SetUptime(uptime.value().InSeconds());
}

void EmitBrowserMemoryMetrics(const GlobalMemoryDump::ProcessDump& pmd,
                              ukm::SourceId ukm_source_id,
                              ukm::UkmRecorder* ukm_recorder,
                              const base::Optional<base::TimeDelta>& uptime) {
  Memory_Experimental builder(ukm_source_id);
  builder.SetProcessType(static_cast<int64_t>(
      memory_instrumentation::mojom::ProcessType::BROWSER));
  EmitProcessUkm(pmd, uptime, &builder);

  UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Experimental.Browser2.Resident",
                                pmd.os_dump().resident_set_kb / 1024);

#if !defined(OS_WIN)
  UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Experimental.Browser2.Malloc",
                                pmd.chrome_dump().malloc_total_kb / 1024);
#endif
  UMA_HISTOGRAM_MEMORY_LARGE_MB(
      "Memory.Experimental.Browser2.PrivateMemoryFootprint",
      pmd.os_dump().private_footprint_kb / 1024);
  UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Browser.PrivateMemoryFootprint",
                                pmd.os_dump().private_footprint_kb / 1024);
  UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Browser.SharedMemoryFootprint",
                                pmd.os_dump().shared_footprint_kb / 1024);
#if defined(OS_LINUX) || defined(OS_ANDROID)
  UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Browser.PrivateSwapFootprint",
                                pmd.os_dump().private_footprint_swap_kb / 1024);
#endif
  // It is possible to run without a separate GPU process.
  // When that happens, we should log common GPU metrics from the browser proc.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kInProcessGPU)) {
    UMA_HISTOGRAM_MEMORY_LARGE_MB(
        "Memory.Experimental.Gpu2.CommandBuffer",
        pmd.chrome_dump().command_buffer_total_kb / 1024);
  }

  builder.Record(ukm_recorder);
}

#define RENDERER_MEMORY_UMA_HISTOGRAMS(type)                                   \
  do {                                                                         \
    UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Experimental." type "2.Resident",    \
                                  pmd.os_dump().resident_set_kb / 1024);       \
    UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Experimental." type                  \
                                  "2.PrivateMemoryFootprint",                  \
                                  pmd.os_dump().private_footprint_kb / 1024);  \
    UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory." type ".PrivateMemoryFootprint",    \
                                  pmd.os_dump().private_footprint_kb / 1024);  \
    UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory." type ".SharedMemoryFootprint",     \
                                  pmd.os_dump().shared_footprint_kb / 1024);   \
    UMA_HISTOGRAM_MEMORY_LARGE_MB(                                             \
        "Memory.Experimental." type "2.PartitionAlloc",                        \
        pmd.chrome_dump().partition_alloc_total_kb / 1024);                    \
    UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Experimental." type "2.BlinkGC",     \
                                  pmd.chrome_dump().blink_gc_total_kb / 1024); \
    UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Experimental." type "2.V8",          \
                                  pmd.chrome_dump().v8_total_kb / 1024);       \
  } while (false)

void EmitRendererMemoryMetrics(
    const GlobalMemoryDump::ProcessDump& pmd,
    const resource_coordinator::mojom::PageInfoPtr& page_info,
    ukm::UkmRecorder* ukm_recorder,
    int number_of_extensions,
    const base::Optional<base::TimeDelta>& uptime) {
  // UMA
  if (number_of_extensions == 0) {
    RENDERER_MEMORY_UMA_HISTOGRAMS("Renderer");
#if !defined(OS_WIN)
    UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Experimental.Renderer2.Malloc",
                                  pmd.chrome_dump().malloc_total_kb / 1024);
#endif
#if defined(OS_LINUX) || defined(OS_ANDROID)
    UMA_HISTOGRAM_MEMORY_LARGE_MB(
        "Memory.Experimental.Renderer2.PrivateSwapFootprint",
        pmd.os_dump().private_footprint_swap_kb / 1024);
#endif
  } else {
    RENDERER_MEMORY_UMA_HISTOGRAMS("Extension");
#if !defined(OS_WIN)
    UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Experimental.Extension2.Malloc",
                                  pmd.chrome_dump().malloc_total_kb / 1024);
#endif
#if defined(OS_LINUX) || defined(OS_ANDROID)
    UMA_HISTOGRAM_MEMORY_LARGE_MB(
        "Memory.Experimental.Extension2.PrivateSwapFootprint",
        pmd.os_dump().private_footprint_swap_kb / 1024);
#endif
  }
  // UKM
  ukm::SourceId ukm_source_id = page_info.is_null()
                                    ? ukm::UkmRecorder::GetNewSourceID()
                                    : page_info->ukm_source_id;
  Memory_Experimental builder(ukm_source_id);
  builder.SetProcessType(static_cast<int64_t>(
      memory_instrumentation::mojom::ProcessType::RENDERER));
  builder.SetNumberOfExtensions(number_of_extensions);
  EmitProcessUkm(pmd, uptime, &builder);

  if (!page_info.is_null()) {
    builder.SetIsVisible(page_info->is_visible);
    builder.SetTimeSinceLastVisibilityChange(
        page_info->time_since_last_visibility_change.InSeconds());
    builder.SetTimeSinceLastNavigation(
        page_info->time_since_last_navigation.InSeconds());
  }

  builder.Record(ukm_recorder);
}

void EmitGpuMemoryMetrics(const GlobalMemoryDump::ProcessDump& pmd,
                          ukm::SourceId ukm_source_id,
                          ukm::UkmRecorder* ukm_recorder,
                          const base::Optional<base::TimeDelta>& uptime) {
  Memory_Experimental builder(ukm_source_id);
  builder.SetProcessType(
      static_cast<int64_t>(memory_instrumentation::mojom::ProcessType::GPU));
  EmitProcessUkm(pmd, uptime, &builder);

  UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Experimental.Gpu2.Resident",
                                pmd.os_dump().resident_set_kb / 1024);

#if !defined(OS_WIN)
  UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Experimental.Gpu2.Malloc",
                                pmd.chrome_dump().malloc_total_kb / 1024);
#endif

  UMA_HISTOGRAM_MEMORY_LARGE_MB(
      "Memory.Experimental.Gpu2.PrivateMemoryFootprint",
      pmd.os_dump().private_footprint_kb / 1024);
  UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Gpu.PrivateMemoryFootprint",
                                pmd.os_dump().private_footprint_kb / 1024);
  UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Gpu.SharedMemoryFootprint",
                                pmd.os_dump().shared_footprint_kb / 1024);
#if defined(OS_LINUX) || defined(OS_ANDROID)
  UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Gpu.PrivateSwapFootprint",
                                pmd.os_dump().private_footprint_swap_kb / 1024);
#endif
  UMA_HISTOGRAM_MEMORY_LARGE_MB(
      "Memory.Experimental.Gpu2.CommandBuffer",
      pmd.chrome_dump().command_buffer_total_kb / 1024);

  builder.Record(ukm_recorder);
}

}  // namespace

ProcessMemoryMetricsEmitter::ProcessMemoryMetricsEmitter() {}

void ProcessMemoryMetricsEmitter::FetchAndEmitProcessMemoryMetrics() {
  MarkServiceRequestsInProgress();

  // The callback keeps this object alive until the callback is invoked.
  auto callback =
      base::Bind(&ProcessMemoryMetricsEmitter::ReceivedMemoryDump, this);
  std::vector<std::string> mad_list;
  for (const auto& metric : kAllocatorDumpNamesForMetrics)
    mad_list.push_back(metric.dump_name);
  memory_instrumentation::MemoryInstrumentation::GetInstance()
      ->RequestGlobalDump(mad_list, callback);

  // The callback keeps this object alive until the callback is invoked.
  if (IsResourceCoordinatorEnabled()) {
    service_manager::Connector* connector =
        content::ServiceManagerConnection::GetForProcess()->GetConnector();
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
    std::unique_ptr<GlobalMemoryDump> dump) {
  memory_dump_in_progress_ = false;
  if (!success)
    return;
  global_dump_ = std::move(dump);
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
  uint32_t shared_footprint_total_kb = 0;
  base::Time now = base::Time::Now();
  for (const auto& pmd : global_dump_->process_dumps()) {
    private_footprint_total_kb += pmd.os_dump().private_footprint_kb;
    shared_footprint_total_kb += pmd.os_dump().shared_footprint_kb;
    switch (pmd.process_type()) {
      case memory_instrumentation::mojom::ProcessType::BROWSER: {
        EmitBrowserMemoryMetrics(pmd, ukm::UkmRecorder::GetNewSourceID(),
                                 GetUkmRecorder(),
                                 GetProcessUptime(now, pmd.pid()));
        break;
      }
      case memory_instrumentation::mojom::ProcessType::RENDERER: {
        resource_coordinator::mojom::PageInfoPtr page_info;
        // If there is more than one frame being hosted in a renderer, don't
        // emit any URLs. This is not ideal, but UKM does not support
        // multiple-URLs per entry, and we must have one entry per process.
        if (process_infos_.find(pmd.pid()) != process_infos_.end()) {
          const resource_coordinator::mojom::ProcessInfoPtr& process_info =
              process_infos_[pmd.pid()];
          if (process_info->page_infos.size() == 1) {
            page_info = std::move(process_info->page_infos[0]);
          }
        }

        int number_of_extensions = GetNumberOfExtensions(pmd.pid());
        EmitRendererMemoryMetrics(pmd, page_info, GetUkmRecorder(),
                                  number_of_extensions,
                                  GetProcessUptime(now, pmd.pid()));
        break;
      }
      case memory_instrumentation::mojom::ProcessType::GPU: {
        EmitGpuMemoryMetrics(pmd, ukm::UkmRecorder::GetNewSourceID(),
                             GetUkmRecorder(),
                             GetProcessUptime(now, pmd.pid()));
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
  UMA_HISTOGRAM_MEMORY_LARGE_MB("Memory.Total.SharedMemoryFootprint",
                                shared_footprint_total_kb / 1024);

  Memory_Experimental(ukm::UkmRecorder::GetNewSourceID())
      .SetTotal2_PrivateMemoryFootprint(private_footprint_total_kb / 1024)
      .SetTotal2_SharedMemoryFootprint(shared_footprint_total_kb / 1024)
      .Record(GetUkmRecorder());
}
