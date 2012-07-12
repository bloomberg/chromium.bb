// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/storage_partition_map.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/file_path.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/browser/resource_context_impl.h"
#include "content/browser/storage_partition.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_constants.h"

namespace content {

// Dirname for storing persistent data for renderers with isolated storage.
const FilePath::CharType kStoragePartitionDirName[] =
    FILE_PATH_LITERAL("Storage Partitions");

StoragePartitionMap::StoragePartitionMap(
    BrowserContext* browser_context)
    : browser_context_(browser_context) {
}

StoragePartitionMap::~StoragePartitionMap() {
  STLDeleteContainerPairSecondPointers(partitions_.begin(),
                                       partitions_.end());
}

StoragePartition* StoragePartitionMap::Get(const std::string& partition_id) {
  // Find the previously created partition if it's available.
  std::map<std::string, StoragePartition*>::const_iterator it =
      partitions_.find(partition_id);
  if (it != partitions_.end())
    return it->second;

  // There was no previous partition, so let's make a new one.
  FilePath partition_path = browser_context_->GetPath();
  if (!partition_id.empty()) {
    // TODO(ajwong): This should check the pth is valid?
    CHECK(IsStringASCII(partition_id));
    partition_path = partition_path.Append(kStoragePartitionDirName)
        .AppendASCII(partition_id);
  }

  StoragePartition* storage_partition =
      StoragePartition::Create(browser_context_, partition_path);
  partitions_[partition_id] = storage_partition;

  PostCreateInitialization(storage_partition, partition_path);

  // TODO(ajwong): We need to remove this conditional by making
  // InitializeResourceContext() understand having different partition data
  // based on the renderer_id.
  if (partition_id.empty()) {
    InitializeResourceContext(browser_context_);
  }

  return storage_partition;
}

void StoragePartitionMap::ForEach(
    const base::Callback<void(StoragePartition*)>& callback) {
  for (std::map<std::string, StoragePartition*>::const_iterator it =
           partitions_.begin();
       it != partitions_.end();
       ++it) {
    callback.Run(it->second);
  }
}

void StoragePartitionMap::PostCreateInitialization(
    StoragePartition* partition,
    const FilePath& partition_path) {
  // Check first to avoid memory leak in unittests.
  if (BrowserThread::IsMessageLoopValid(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&ChromeAppCacheService::InitializeOnIOThread,
                   partition->appcache_service(),
                   browser_context_->IsOffTheRecord() ? FilePath() :
                       partition_path.Append(kAppCacheDirname),
                       browser_context_->GetResourceContext(),
                   make_scoped_refptr(
                       browser_context_->GetSpecialStoragePolicy())));
  }
}

}  // namespace content
