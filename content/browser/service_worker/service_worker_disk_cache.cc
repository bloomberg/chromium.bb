// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_disk_cache.h"

namespace content {

scoped_ptr<ServiceWorkerDiskCache>
ServiceWorkerDiskCache::CreateWithBlockFileBackend() {
  return make_scoped_ptr(
      new ServiceWorkerDiskCache(false /* use_simple_cache*/));
}

scoped_ptr<ServiceWorkerDiskCache>
ServiceWorkerDiskCache::CreateWithSimpleBackend() {
  return make_scoped_ptr(
      new ServiceWorkerDiskCache(true /* use_simple_cache */));
}

ServiceWorkerDiskCache::ServiceWorkerDiskCache(bool use_simple_cache)
    : AppCacheDiskCache(use_simple_cache) {
}

ServiceWorkerResponseReader::ServiceWorkerResponseReader(
    int64 resource_id,
    ServiceWorkerDiskCache* disk_cache)
    : AppCacheResponseReader(resource_id, 0, disk_cache) {}

ServiceWorkerResponseWriter::ServiceWorkerResponseWriter(
    int64 resource_id,
    ServiceWorkerDiskCache* disk_cache)
    : AppCacheResponseWriter(resource_id, 0, disk_cache) {}

ServiceWorkerResponseMetadataWriter::ServiceWorkerResponseMetadataWriter(
    int64 resource_id,
    ServiceWorkerDiskCache* disk_cache)
    : AppCacheResponseMetadataWriter(resource_id, 0, disk_cache) {}

}  // namespace content
