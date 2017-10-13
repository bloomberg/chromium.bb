// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/dom_storage/local_storage_cached_areas.h"

#include "base/metrics/histogram_macros.h"
#include "base/sys_info.h"
#include "content/renderer/dom_storage/local_storage_cached_area.h"

namespace content {
namespace {
const size_t kTotalCacheLimitInBytesLowEnd = 1 * 1024 * 1024;
const size_t kTotalCacheLimitInBytes = 5 * 1024 * 1024;
}  // namespace

LocalStorageCachedAreas::LocalStorageCachedAreas(
    mojom::StoragePartitionService* storage_partition_service)
    : storage_partition_service_(storage_partition_service),
      total_cache_limit_(base::SysInfo::IsLowEndDevice()
                             ? kTotalCacheLimitInBytesLowEnd
                             : kTotalCacheLimitInBytes) {}

LocalStorageCachedAreas::~LocalStorageCachedAreas() {}

scoped_refptr<LocalStorageCachedArea> LocalStorageCachedAreas::GetCachedArea(
    const url::Origin& origin) {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class CacheMetrics {
    kMiss = 0,    // Area not in cache.
    kHit = 1,     // Area with refcount = 0 loaded from cache.
    kUnused = 2,  // Cache was not used. Area had refcount > 0.
    kMaxValue
  };

  auto it = cached_areas_.find(origin);
  if (it != cached_areas_.end()) {
    if (it->second->HasOneRef()) {
      UMA_HISTOGRAM_ENUMERATION("LocalStorage.RendererAreaCacheHit",
                                CacheMetrics::kHit, CacheMetrics::kMaxValue);
    } else {
      UMA_HISTOGRAM_ENUMERATION("LocalStorage.RendererAreaCacheHit",
                                CacheMetrics::kUnused, CacheMetrics::kMaxValue);
    }
  } else {
    UMA_HISTOGRAM_ENUMERATION("LocalStorage.RendererAreaCacheHit",
                              CacheMetrics::kMiss, CacheMetrics::kMaxValue);
  }

  if (it == cached_areas_.end()) {
    ClearAreasIfNeeded();
    it = cached_areas_
             .emplace(origin, new LocalStorageCachedArea(
                                  origin, storage_partition_service_, this))
             .first;
  }
  return it->second;
}

size_t LocalStorageCachedAreas::TotalCacheSize() const {
  size_t total = 0;
  for (const auto& it : cached_areas_)
    total += it.second.get()->memory_used();
  return total;
}

void LocalStorageCachedAreas::ClearAreasIfNeeded() {
  if (TotalCacheSize() < total_cache_limit_)
    return;
  auto it = cached_areas_.begin();
  while (it != cached_areas_.end()) {
    if (it->second->HasOneRef())
      it = cached_areas_.erase(it);
    else
      ++it;
  }
}

}  // namespace content
