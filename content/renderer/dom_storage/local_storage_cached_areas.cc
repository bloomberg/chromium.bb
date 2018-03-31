// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/dom_storage/local_storage_cached_areas.h"

#include "base/metrics/histogram_macros.h"
#include "base/sys_info.h"
#include "content/common/dom_storage/dom_storage_types.h"
#include "content/public/common/content_features.h"
#include "content/renderer/dom_storage/local_storage_cached_area.h"
#include "content/renderer/render_thread_impl.h"

namespace content {
namespace {
const size_t kTotalCacheLimitInBytesLowEnd = 1 * 1024 * 1024;
const size_t kTotalCacheLimitInBytes = 5 * 1024 * 1024;

// An empty namespace is the local storage namespace.
constexpr const char kLocalStorageNamespaceId[] = "";
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
LocalStorageCachedAreas::GetSessionStorageArea(const std::string& namespace_id,
                                               const url::Origin& origin) {
  DCHECK_NE(namespace_id, kLocalStorageNamespaceId);
  return GetCachedArea(namespace_id, origin, renderer_scheduler_);
}

void LocalStorageCachedAreas::CloneNamespace(
    const std::string& source_namespace,
    const std::string& destination_namespace) {
  DCHECK(base::FeatureList::IsEnabled(features::kMojoSessionStorage));
  auto namespace_it = cached_namespaces_.find(source_namespace);
  if (namespace_it != cached_namespaces_.end()) {
    namespace_it->second.session_storage_namespace->Clone(
        destination_namespace);
    return;
  }
  // The clone call still has to be sent, as we can have a case where the
  // storage is never opened but there is still data there (from a restore or
  // an earlier clone).
  mojom::SessionStorageNamespacePtr session_storage_namespace;
  storage_partition_service_->OpenSessionStorage(
      source_namespace, mojo::MakeRequest(&session_storage_namespace));
  session_storage_namespace->Clone(destination_namespace);
}

size_t LocalStorageCachedAreas::TotalCacheSize() const {
  size_t total = 0;
  for (const auto& it : cached_namespaces_)
    total += it.second.TotalCacheSize();
  return total;
}

void LocalStorageCachedAreas::ClearAreasIfNeeded() {
  if (TotalCacheSize() < total_cache_limit_)
    return;

  base::EraseIf(cached_namespaces_,
                [](auto& pair) { return pair.second.CleanUpUnusedAreas(); });
}

scoped_refptr<LocalStorageCachedArea> LocalStorageCachedAreas::GetCachedArea(
    const std::string& namespace_id,
    const url::Origin& origin,
    blink::scheduler::RendererScheduler* scheduler) {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class CacheMetrics {
    kMiss = 0,    // Area not in cache.
    kHit = 1,     // Area with refcount = 0 loaded from cache.
    kUnused = 2,  // Cache was not used. Area had refcount > 0.
    kMaxValue = kUnused,
  };

  auto namespace_it = cached_namespaces_.find(namespace_id);
  CacheMetrics metric;
  scoped_refptr<LocalStorageCachedArea> result;
  DOMStorageNamespace* dom_namespace = nullptr;
  if (namespace_it == cached_namespaces_.end()) {
    metric = CacheMetrics::kMiss;
  } else {
    dom_namespace = &namespace_it->second;
    auto cache_it = dom_namespace->cached_areas.find(origin);
    if (cache_it == dom_namespace->cached_areas.end()) {
      metric = CacheMetrics::kMiss;
    } else {
      if (cache_it->second->HasOneRef()) {
        metric = CacheMetrics::kHit;
      } else {
        metric = CacheMetrics::kUnused;
      }
      result = cache_it->second;
    }
  }
  if (namespace_id == kLocalStorageNamespaceId)
    UMA_HISTOGRAM_ENUMERATION("LocalStorage.RendererAreaCacheHit", metric);
  else
    LOCAL_HISTOGRAM_ENUMERATION("SessionStorage.RendererAreaCacheHit", metric);

  if (!result) {
    ClearAreasIfNeeded();
    if (!dom_namespace) {
      dom_namespace = &cached_namespaces_[namespace_id];
    }
    if (namespace_id == kLocalStorageNamespaceId) {
      result = base::MakeRefCounted<LocalStorageCachedArea>(
          origin, storage_partition_service_, this, scheduler);
    } else {
      DCHECK(base::FeatureList::IsEnabled(features::kMojoSessionStorage));
      storage_partition_service_->OpenSessionStorage(
          namespace_id,
          mojo::MakeRequest(&dom_namespace->session_storage_namespace));
      result = base::MakeRefCounted<LocalStorageCachedArea>(
          namespace_id, origin, dom_namespace->session_storage_namespace.get(),
          this, scheduler);
    }
    dom_namespace->cached_areas.emplace(origin, result);
  }
  return result;
}

LocalStorageCachedAreas::DOMStorageNamespace::DOMStorageNamespace() {}
LocalStorageCachedAreas::DOMStorageNamespace::~DOMStorageNamespace() {}
LocalStorageCachedAreas::DOMStorageNamespace::DOMStorageNamespace(
    LocalStorageCachedAreas::DOMStorageNamespace&& other) = default;

size_t LocalStorageCachedAreas::DOMStorageNamespace::TotalCacheSize() const {
  size_t total = 0;
  for (const auto& it : cached_areas)
    total += it.second.get()->memory_used();
  return total;
}

bool LocalStorageCachedAreas::DOMStorageNamespace::CleanUpUnusedAreas() {
  base::EraseIf(cached_areas,
                [](auto& pair) { return pair.second->HasOneRef(); });
  return cached_areas.empty();
}

}  // namespace content
