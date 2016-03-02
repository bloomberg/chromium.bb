// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_DOM_STORAGE_LOCAL_STORAGE_CACHED_AREAS_H_
#define CONTENT_RENDERER_DOM_STORAGE_LOCAL_STORAGE_CACHED_AREAS_H_

#include <map>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "url/origin.h"

namespace content {
class LocalStorageCachedArea;
class StoragePartitionService;

// Owns all the LocalStorageCachedArea objects in a renderer. This is needed
// because we can have n LocalStorageArea objects for the same origin but we
// want just one LocalStorageCachedArea to service them (no point in having
// multiple caches of the same data in the same process).
class LocalStorageCachedAreas {
 public:
  explicit LocalStorageCachedAreas(
      StoragePartitionService* storage_partition_service);
  ~LocalStorageCachedAreas();

  scoped_refptr<LocalStorageCachedArea> GetLocalStorageCachedArea(
      const url::Origin& origin);

  // Called by LocalStorageCachedArea on destruction.
  void LocalStorageCacheAreaClosed(LocalStorageCachedArea* cached_area);

 private:
  StoragePartitionService* const storage_partition_service_;

  // Maps from an origin to its LocalStorageCachedArea object. The object owns
  // itself.
  std::map<url::Origin, LocalStorageCachedArea*> cached_areas_;

  DISALLOW_COPY_AND_ASSIGN(LocalStorageCachedAreas);
};

}  // namespace content

#endif  // CONTENT_RENDERER_DOM_STORAGE_LOCAL_STORAGE_CACHED_AREAS_H_
