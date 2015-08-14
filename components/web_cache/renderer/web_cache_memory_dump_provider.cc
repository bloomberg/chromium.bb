// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_cache/renderer/web_cache_memory_dump_provider.h"

#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "third_party/WebKit/public/web/WebCache.h"

namespace web_cache {
namespace {

base::LazyInstance<WebCacheMemoryDumpProvider>::Leaky g_wcmdp_instance =
    LAZY_INSTANCE_INITIALIZER;

void DumpResourceStats(const blink::WebCache::ResourceTypeStat& resource_stat,
                       const std::string& resource_name,
                       base::trace_event::ProcessMemoryDump* pmd) {
  base::trace_event::MemoryAllocatorDump* allocator_dump =
      pmd->CreateAllocatorDump(resource_name);
  allocator_dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                            base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                            resource_stat.size);
  allocator_dump->AddScalar(
      base::trace_event::MemoryAllocatorDump::kNameObjectsCount,
      base::trace_event::MemoryAllocatorDump::kUnitsObjects,
      resource_stat.count);
  allocator_dump->AddScalar("live_size",
                            base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                            resource_stat.liveSize);
  allocator_dump->AddScalar("decoded_size",
                            base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                            resource_stat.decodedSize);
  allocator_dump->AddScalar("purged_size",
                            base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                            resource_stat.purgedSize);
  allocator_dump->AddScalar("purgeable_size",
                            base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                            resource_stat.purgeableSize);
}

}  // namespace

// static
WebCacheMemoryDumpProvider* WebCacheMemoryDumpProvider::GetInstance() {
  return g_wcmdp_instance.Pointer();
}

WebCacheMemoryDumpProvider::WebCacheMemoryDumpProvider() {}

WebCacheMemoryDumpProvider::~WebCacheMemoryDumpProvider() {}

bool WebCacheMemoryDumpProvider::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  blink::WebCache::UsageStats memory_stats;
  blink::WebCache::getUsageStats(&memory_stats);

  const std::string dump_name("web_cache");
  base::trace_event::MemoryAllocatorDump* allocator_dump =
      pmd->CreateAllocatorDump(dump_name);

  allocator_dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                            base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                            memory_stats.liveSize + memory_stats.deadSize);
  allocator_dump->AddScalar("live_size",
                            base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                            memory_stats.liveSize);
  allocator_dump->AddScalar("dead_size",
                            base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                            memory_stats.deadSize);

  blink::WebCache::ResourceTypeStats resource_stats;
  blink::WebCache::getResourceTypeStats(&resource_stats);
  DumpResourceStats(resource_stats.images, dump_name + "/images", pmd);
  DumpResourceStats(resource_stats.cssStyleSheets,
                    dump_name + "/css_style_sheets", pmd);
  DumpResourceStats(resource_stats.scripts, dump_name + "/scripts", pmd);
  DumpResourceStats(resource_stats.xslStyleSheets,
                    dump_name + "/xsl_style_sheets", pmd);
  DumpResourceStats(resource_stats.fonts, dump_name + "/fonts", pmd);
  DumpResourceStats(resource_stats.other, dump_name + "/other", pmd);

  return true;
}

}  // namespace web_cache
