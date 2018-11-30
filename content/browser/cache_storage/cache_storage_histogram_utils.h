// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_HISTOGRAM_UTILS_H_
#define CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_HISTOGRAM_UTILS_H_

#include "base/metrics/histogram_macros.h"
#include "content/browser/cache_storage/cache_storage_scheduler_types.h"
#include "third_party/blink/public/mojom/cache_storage/cache_storage.mojom.h"

namespace content {

// This enum gets recorded as a histogram.  Do not renumber the values.
enum class ErrorStorageType {
  kDidCreateNullCache = 0,
  kDeleteCacheFailed = 1,
  kMatchBackendClosed = 2,
  kMatchAllBackendClosed = 3,
  kWriteSideDataBackendClosed = 4,
  kBatchBackendClosed = 5,
  kBatchInvalidSpace = 6,
  kBatchDidGetUsageAndQuotaInvalidSpace = 7,
  kBatchDidGetUsageAndQuotaUndefinedOp = 8,
  kKeysBackendClosed = 9,
  kQueryCacheBackendClosed = 10,
  kQueryCacheFilterEntryFailed = 11,
  kQueryCacheDidReadMetadataNullBlobContext = 12,
  kStorageMatchAllBackendClosed = 13,
  kWriteSideDataImplBackendClosed = 14,
  kPutImplBackendClosed = 15,
  kPutDidDeleteEntryBackendClosed = 16,
  kMetadataSerializationFailed = 17,
  kPutDidWriteHeadersWrongBytes = 18,
  kPutDidWriteBlobToCacheFailed = 19,
  kDeleteImplBackendClosed = 20,
  kKeysImplBackendClosed = 21,
  kCreateBackendDidCreateFailed = 22,
  kStorageGetAllMatchedEntriesBackendClosed = 23,
  kMaxValue = kStorageGetAllMatchedEntriesBackendClosed,
};

blink::mojom::CacheStorageError MakeErrorStorage(ErrorStorageType type);

// Metrics to make it easier to write histograms for several clients.
#define CACHE_STORAGE_SCHEDULER_UMA_THUNK2(uma_type, args) \
  UMA_HISTOGRAM_##uma_type args

#define CACHE_STORAGE_SCHEDULER_UMA_THUNK(uma_type, op_type, histogram_name, \
                                          ...)                               \
  CACHE_STORAGE_SCHEDULER_UMA_THUNK2(uma_type,                               \
                                     (histogram_name, ##__VA_ARGS__));       \
  do {                                                                       \
    switch (op_type) {                                                       \
      case CacheStorageSchedulerOp::kBackgroundSync:                         \
        /* do not record op-specific histograms for background sync */       \
        break;                                                               \
      case CacheStorageSchedulerOp::kClose:                                  \
        CACHE_STORAGE_SCHEDULER_UMA_THUNK2(                                  \
            uma_type, (histogram_name ".Close", ##__VA_ARGS__));             \
        break;                                                               \
      case CacheStorageSchedulerOp::kDelete:                                 \
        CACHE_STORAGE_SCHEDULER_UMA_THUNK2(                                  \
            uma_type, (histogram_name ".Delete", ##__VA_ARGS__));            \
        break;                                                               \
      case CacheStorageSchedulerOp::kGetAllMatched:                          \
        CACHE_STORAGE_SCHEDULER_UMA_THUNK2(                                  \
            uma_type, (histogram_name ".GetAllMatched", ##__VA_ARGS__));     \
        break;                                                               \
      case CacheStorageSchedulerOp::kHas:                                    \
        CACHE_STORAGE_SCHEDULER_UMA_THUNK2(                                  \
            uma_type, (histogram_name ".Has", ##__VA_ARGS__));               \
        break;                                                               \
      case CacheStorageSchedulerOp::kInit:                                   \
        CACHE_STORAGE_SCHEDULER_UMA_THUNK2(                                  \
            uma_type, (histogram_name ".Init", ##__VA_ARGS__));              \
        break;                                                               \
      case CacheStorageSchedulerOp::kKeys:                                   \
        CACHE_STORAGE_SCHEDULER_UMA_THUNK2(                                  \
            uma_type, (histogram_name ".Keys", ##__VA_ARGS__));              \
        break;                                                               \
      case CacheStorageSchedulerOp::kMatch:                                  \
        CACHE_STORAGE_SCHEDULER_UMA_THUNK2(                                  \
            uma_type, (histogram_name ".Match", ##__VA_ARGS__));             \
        break;                                                               \
      case CacheStorageSchedulerOp::kMatchAll:                               \
        CACHE_STORAGE_SCHEDULER_UMA_THUNK2(                                  \
            uma_type, (histogram_name ".MatchAll", ##__VA_ARGS__));          \
        break;                                                               \
      case CacheStorageSchedulerOp::kOpen:                                   \
        CACHE_STORAGE_SCHEDULER_UMA_THUNK2(                                  \
            uma_type, (histogram_name ".Open", ##__VA_ARGS__));              \
        break;                                                               \
      case CacheStorageSchedulerOp::kPut:                                    \
        CACHE_STORAGE_SCHEDULER_UMA_THUNK2(                                  \
            uma_type, (histogram_name ".Put", ##__VA_ARGS__));               \
        break;                                                               \
      case CacheStorageSchedulerOp::kSize:                                   \
        CACHE_STORAGE_SCHEDULER_UMA_THUNK2(                                  \
            uma_type, (histogram_name ".Size", ##__VA_ARGS__));              \
        break;                                                               \
      case CacheStorageSchedulerOp::kSizeThenClose:                          \
        CACHE_STORAGE_SCHEDULER_UMA_THUNK2(                                  \
            uma_type, (histogram_name ".SizeThenClose", ##__VA_ARGS__));     \
        break;                                                               \
      case CacheStorageSchedulerOp::kTest:                                   \
        /* do not record op-specific histograms for test ops */              \
        break;                                                               \
      case CacheStorageSchedulerOp::kWriteIndex:                             \
        CACHE_STORAGE_SCHEDULER_UMA_THUNK2(                                  \
            uma_type, (histogram_name ".WriteIndex", ##__VA_ARGS__));        \
        break;                                                               \
      case CacheStorageSchedulerOp::kWriteSideData:                          \
        CACHE_STORAGE_SCHEDULER_UMA_THUNK2(                                  \
            uma_type, (histogram_name ".WriteSideData", ##__VA_ARGS__));     \
        break;                                                               \
    }                                                                        \
  } while (0)

#define CACHE_STORAGE_SCHEDULER_UMA(uma_type, uma_name, client_type, op_type,  \
                                    ...)                                       \
  /* Only the Cache and CacheStorage clients should have specific op types */  \
  DCHECK(client_type != CacheStorageSchedulerClient::kBackgroundSync ||        \
         op_type == CacheStorageSchedulerOp::kBackgroundSync);                 \
  do {                                                                         \
    switch (client_type) {                                                     \
      case CacheStorageSchedulerClient::kStorage:                              \
        CACHE_STORAGE_SCHEDULER_UMA_THUNK(                                     \
            uma_type, op_type,                                                 \
            "ServiceWorkerCache.CacheStorage.Scheduler." uma_name,             \
            ##__VA_ARGS__);                                                    \
        break;                                                                 \
      case CacheStorageSchedulerClient::kCache:                                \
        CACHE_STORAGE_SCHEDULER_UMA_THUNK(                                     \
            uma_type, op_type, "ServiceWorkerCache.Cache.Scheduler." uma_name, \
            ##__VA_ARGS__);                                                    \
        break;                                                                 \
      case CacheStorageSchedulerClient::kBackgroundSync:                       \
        CACHE_STORAGE_SCHEDULER_UMA_THUNK(                                     \
            uma_type, op_type,                                                 \
            "ServiceWorkerCache.BackgroundSyncManager.Scheduler." uma_name,    \
            ##__VA_ARGS__);                                                    \
        break;                                                                 \
    }                                                                          \
  } while (0)

}  // namespace content

#endif  // CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_HISTOGRAM_UTILS_H_
