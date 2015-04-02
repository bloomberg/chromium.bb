// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_STORAGE_PARTITION_HTTP_CACHE_DATA_REMOVER_H_
#define COMPONENTS_BROWSING_DATA_STORAGE_PARTITION_HTTP_CACHE_DATA_REMOVER_H_

#include "base/callback.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/time/time.h"

namespace content {
class StoragePartition;
}

namespace disk_cache {
class Backend;
}

namespace net {
class URLRequestContextGetter;
}

namespace browsing_data {

// Helper to remove http cache data from a StoragePartition.
class StoragePartitionHttpCacheDataRemover {
 public:
  static StoragePartitionHttpCacheDataRemover* CreateForRange(
      content::StoragePartition* storage_partition,
      base::Time delete_begin,
      base::Time delete_end);

  // Calls |done_callback| upon completion and also destroys itself.
  void Remove(const base::Closure& done_callback);

 private:
  enum CacheState {
    STATE_NONE,
    STATE_CREATE_MAIN,
    STATE_CREATE_MEDIA,
    STATE_DELETE_MAIN,
    STATE_DELETE_MEDIA,
    STATE_DONE
  };

  StoragePartitionHttpCacheDataRemover(
      base::Time delete_begin,
      base::Time delete_end,
      net::URLRequestContextGetter* main_context_getter,
      net::URLRequestContextGetter* media_context_getter);

  // StoragePartitionHttpCacheDataRemover deletes itself (using DeleteHelper)
  // and is not supposed to be deleted by other objects so make destructor
  // private and DeleteHelper a friend.
  friend class base::DeleteHelper<StoragePartitionHttpCacheDataRemover>;

  ~StoragePartitionHttpCacheDataRemover();

  void ClearHttpCacheOnIOThread();
  void ClearedHttpCache();
  // Performs the actual work to delete the cache.
  void DoClearCache(int rv);

  const base::Time delete_begin_;
  const base::Time delete_end_;

  const scoped_refptr<net::URLRequestContextGetter> main_context_getter_;
  const scoped_refptr<net::URLRequestContextGetter> media_context_getter_;

  base::Closure done_callback_;

  // IO.
  int next_cache_state_;
  disk_cache::Backend* cache_;

  DISALLOW_COPY_AND_ASSIGN(StoragePartitionHttpCacheDataRemover);
};

}  // namespace browsing_data

#endif  // COMPONENTS_BROWSING_DATA_STORAGE_PARTITION_HTTP_CACHE_DATA_REMOVER_H_
