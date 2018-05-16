// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_CACHE_STORAGE_CACHE_STORAGE_TYPES_H_
#define CONTENT_COMMON_CACHE_STORAGE_CACHE_STORAGE_TYPES_H_

#include <map>
#include <string>

#include "base/strings/nullable_string16.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_types.h"

// This file is to have common definitions that are to be shared by
// browser and child process.

namespace content {

// Controls how requests are matched in the Cache API.
struct CONTENT_EXPORT CacheStorageCacheQueryParams {
  CacheStorageCacheQueryParams();

  bool ignore_search = false;
  bool ignore_method = false;
  bool ignore_vary = false;
  base::NullableString16 cache_name;
};

// The type of a single batch operation in the Cache API.
enum CacheStorageCacheOperationType {
  CACHE_STORAGE_CACHE_OPERATION_TYPE_UNDEFINED,
  CACHE_STORAGE_CACHE_OPERATION_TYPE_PUT,
  CACHE_STORAGE_CACHE_OPERATION_TYPE_DELETE,
  CACHE_STORAGE_CACHE_OPERATION_TYPE_LAST =
      CACHE_STORAGE_CACHE_OPERATION_TYPE_DELETE
};

// A single batch operation for the Cache API.
struct CONTENT_EXPORT CacheStorageBatchOperation {
  CacheStorageBatchOperation();
  CacheStorageBatchOperation(const CacheStorageBatchOperation& other);

  CacheStorageCacheOperationType operation_type;
  ServiceWorkerFetchRequest request;
  ServiceWorkerResponse response;
  CacheStorageCacheQueryParams match_params;
};

}  // namespace content

#endif  // CONTENT_COMMON_CACHE_STORAGE_CACHE_STORAGE_TYPES_H_
