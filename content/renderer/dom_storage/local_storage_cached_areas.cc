// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/dom_storage/local_storage_cached_areas.h"

#include "base/metrics/histogram_macros.h"
#include "base/sys_info.h"
#include "content/common/dom_storage/dom_storage_types.h"
#include "content/renderer/dom_storage/local_storage_cached_area.h"
#include "content/renderer/render_thread_impl.h"

namespace content {
namespace {
const size_t kTotalCacheLimitInBytesLowEnd = 1 * 1024 * 1024;
const size_t kTotalCacheLimitInBytes = 5 * 1024 * 1024;
}  // namespace

LocalStorageCachedAreas::LocalStorageCachedAreas(
    mojom::StoragePartitionService* storage_partition_service,
    blink::scheduler::RendererScheduler* renderer_scheduler)
    : storage_partition_service_(storage_partition_service),
      total_cache_limit_(base::SysInfo::IsLowEndDevice()
                             ? kTotalCacheLimitInBytesLowEnd
                             : kTotalCacheLimitInBytes),
      renderer_scheduler_(renderer_scheduler) {}

LocalStorageCachedAreas::~LocalStorageCachedAreas() {}

scoped_refptr<LocalStorageCachedArea> LocalStorageCachedAreas::GetCachedArea(
    const url::Origin& origin) {
  return GetCachedArea(kLocalStorageNamespaceId, origin, renderer_scheduler_);
}

scoped_refptr<LocalStorageCachedArea>
LocalStorageCachedAreas::GetSessionStorageArea(int64_t namespace_id,
                                               const url::Origin& origin) {
  DCHECK_NE(kLocalStorageNamespaceId, kInvalidSessionStorageNamespaceId);
  return GetCachedArea(namespace_id, origin, renderer_scheduler_);
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

scoped_refptr<LocalStorageCachedArea> LocalStorageCachedAreas::GetCachedArea(
    int64_t namespace_id,
    const url::Origin& origin,
    blink::scheduler::RendererScheduler* scheduler) {
  AreaKey key(namespace_id, origin);

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class CacheMetrics {
    kMiss = 0,    // Area not in cache.
    kHit = 1,     // Area with refcount = 0 loaded from cache.
    kUnused = 2,  // Cache was not used. Area had refcount > 0.
    kMaxValue
  };

  auto it = cached_areas_.find(key);
  CacheMetrics metric;
  if (it != cached_areas_.end()) {
    if (it->second->HasOneRef()) {
      metric = CacheMetrics::kHit;
    } else {
      metric = CacheMetrics::kUnused;
    }
  } else {
    metric = CacheMetrics::kMiss;
  }
  if (namespace_id == kLocalStorageNamespaceId) {
    UMA_HISTOGRAM_ENUMERATION("LocalStorage.RendererAreaCacheHit", metric,
                              CacheMetrics::kMaxValue);
  } else {
    LOCAL_HISTOGRAM_ENUMERATION("SessionStorage.RendererAreaCacheHit", metric,
                                CacheMetrics::kMaxValue);
  }

  if (it == cached_areas_.end()) {
    ClearAreasIfNeeded();
    scoped_refptr<LocalStorageCachedArea> area;
    if (namespace_id == kLocalStorageNamespaceId) {
      area = base::MakeRefCounted<LocalStorageCachedArea>(
          origin, storage_partition_service_, this, scheduler);
    } else {
      area = base::MakeRefCounted<LocalStorageCachedArea>(
          namespace_id, origin, storage_partition_service_, this, scheduler);
    }
    it = cached_areas_.emplace(key, std::move(area)).first;
  }
  return it->second;
}

}  // namespace content
