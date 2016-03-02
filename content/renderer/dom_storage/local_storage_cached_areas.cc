// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/dom_storage/local_storage_cached_areas.h"

#include "content/renderer/dom_storage/local_storage_cached_area.h"

namespace content {

LocalStorageCachedAreas::LocalStorageCachedAreas(
    StoragePartitionService* storage_partition_service)
    : storage_partition_service_(storage_partition_service) {
}

LocalStorageCachedAreas::~LocalStorageCachedAreas() {
}

scoped_refptr<LocalStorageCachedArea>
LocalStorageCachedAreas::GetLocalStorageCachedArea(
    const url::Origin& origin) {
  if (cached_areas_.find(origin) == cached_areas_.end()) {
    cached_areas_[origin] = new LocalStorageCachedArea(
        origin, storage_partition_service_, this);
  }

  return make_scoped_refptr(cached_areas_[origin]);
}

void LocalStorageCachedAreas::LocalStorageCacheAreaClosed(
    LocalStorageCachedArea* cached_area) {
  DCHECK(cached_areas_.find(cached_area->origin()) != cached_areas_.end());
  cached_areas_.erase(cached_area->origin());
}

}  // namespace content
