// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_STORAGE_PARTITION_MAP_H_
#define CONTENT_BROWSER_STORAGE_PARTITION_MAP_H_

#include <map>
#include <string>

#include "base/callback_forward.h"
#include "base/supports_user_data.h"
#include "content/public/browser/browser_context.h"

class FilePath;

namespace content {

class BrowserContext;
class StoragePartitionImpl;

// A std::string to StoragePartition map for use with SupportsUserData APIs.
class StoragePartitionImplMap : public base::SupportsUserData::Data {
 public:
  explicit StoragePartitionImplMap(BrowserContext* browser_context);

  virtual ~StoragePartitionImplMap();

  // This map retains ownership of the returned StoragePartition objects.
  StoragePartitionImpl* Get(const std::string& partition_id);

  void ForEach(const BrowserContext::StoragePartitionCallback& callback);

 private:
  // This must always be called *after* |partition| has been added to the
  // partitions_.
  //
  // TODO(ajwong): Is there a way to make it so that Get()'s implementation
  // doesn't need to be aware of this ordering?  Revisit when refactoring
  // ResourceContext and AppCache to respect storage partitions.
  void PostCreateInitialization(StoragePartitionImpl* partition,
                                net::URLRequestContextGetter* request_context);

  BrowserContext* browser_context_;  // Not Owned.
  std::map<std::string, StoragePartitionImpl*> partitions_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_STORAGE_PARTITION_MAP_H_
