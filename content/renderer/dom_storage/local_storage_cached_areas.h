// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_DOM_STORAGE_LOCAL_STORAGE_CACHED_AREAS_H_
#define CONTENT_RENDERER_DOM_STORAGE_LOCAL_STORAGE_CACHED_AREAS_H_

#include <map>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "url/origin.h"

namespace blink {
namespace scheduler {
class RendererScheduler;
}
}  // namespace blink

namespace content {
class LocalStorageCachedArea;

namespace mojom {
class StoragePartitionService;
}

// Keeps a map of all the LocalStorageCachedArea objects in a renderer. This is
// needed because we can have n LocalStorageArea objects for the same origin but
// we want just one LocalStorageCachedArea to service them (no point in having
// multiple caches of the same data in the same process).
// TODO(dmurph): Rename to remove LocalStorage.
class CONTENT_EXPORT LocalStorageCachedAreas {
 public:
  LocalStorageCachedAreas(
      mojom::StoragePartitionService* storage_partition_service,
      blink::scheduler::RendererScheduler* renderer_schedule);
  ~LocalStorageCachedAreas();

  // Returns, creating if necessary, a cached storage area for the given origin.
  scoped_refptr<LocalStorageCachedArea>
      GetCachedArea(const url::Origin& origin);

  scoped_refptr<LocalStorageCachedArea> GetSessionStorageArea(
      int64_t namespace_id,
      const url::Origin& origin);

  size_t TotalCacheSize() const;

  void set_cache_limit_for_testing(size_t limit) { total_cache_limit_ = limit; }

 private:
  void ClearAreasIfNeeded();

  scoped_refptr<LocalStorageCachedArea> GetCachedArea(
      int64_t namespace_id,
      const url::Origin& origin,
      blink::scheduler::RendererScheduler* scheduler);

  mojom::StoragePartitionService* const storage_partition_service_;

  // Maps from a namespace + origin to its LocalStorageCachedArea object. When
  // this map is the only reference to the object, it can be deleted by the
  // cache.
  using AreaKey = std::pair<int64_t, url::Origin>;
  std::map<AreaKey, scoped_refptr<LocalStorageCachedArea>> cached_areas_;
  size_t total_cache_limit_;

  blink::scheduler::RendererScheduler* renderer_scheduler_;  // NOT OWNED

  DISALLOW_COPY_AND_ASSIGN(LocalStorageCachedAreas);
};

}  // namespace content

#endif  // CONTENT_RENDERER_DOM_STORAGE_LOCAL_STORAGE_CACHED_AREAS_H_
