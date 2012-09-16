// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/storage_partition_impl_map.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/file_path.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/browser/resource_context_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_constants.h"
#include "net/url_request/url_request_context_getter.h"

namespace content {

StoragePartitionImplMap::StoragePartitionImplMap(
    BrowserContext* browser_context)
    : browser_context_(browser_context) {
}

StoragePartitionImplMap::~StoragePartitionImplMap() {
  STLDeleteContainerPairSecondPointers(partitions_.begin(),
                                       partitions_.end());
}

StoragePartitionImpl* StoragePartitionImplMap::Get(
    const std::string& partition_id) {
  // Find the previously created partition if it's available.
  std::map<std::string, StoragePartitionImpl*>::const_iterator it =
      partitions_.find(partition_id);
  if (it != partitions_.end())
    return it->second;

  // There was no previous partition, so let's make a new one.
  StoragePartitionImpl* storage_partition =
      StoragePartitionImpl::Create(browser_context_,
                                   partition_id,
                                   browser_context_->GetPath());
  partitions_[partition_id] = storage_partition;

  net::URLRequestContextGetter* request_context = partition_id.empty() ?
      browser_context_->GetRequestContext() :
      browser_context_->GetRequestContextForStoragePartition(partition_id);

  PostCreateInitialization(storage_partition, request_context);

  // TODO(ajwong): We need to remove this conditional by making
  // InitializeResourceContext() understand having different partition data
  // based on the renderer_id.
  if (partition_id.empty()) {
    InitializeResourceContext(browser_context_);
  }

  return storage_partition;
}

void StoragePartitionImplMap::ForEach(
    const BrowserContext::StoragePartitionCallback& callback) {
  for (std::map<std::string, StoragePartitionImpl*>::const_iterator it =
           partitions_.begin();
       it != partitions_.end();
       ++it) {
    callback.Run(it->first, it->second);
  }
}

void StoragePartitionImplMap::PostCreateInitialization(
    StoragePartitionImpl* partition,
    net::URLRequestContextGetter* request_context_getter) {
  // Check first to avoid memory leak in unittests.
  if (BrowserThread::IsMessageLoopValid(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&ChromeAppCacheService::InitializeOnIOThread,
                   partition->GetAppCacheService(),
                   browser_context_->IsOffTheRecord() ? FilePath() :
                       partition->GetPath().Append(kAppCacheDirname),
                   browser_context_->GetResourceContext(),
                   make_scoped_refptr(request_context_getter),
                   make_scoped_refptr(
                       browser_context_->GetSpecialStoragePolicy())));
  }
}

}  // namespace content
