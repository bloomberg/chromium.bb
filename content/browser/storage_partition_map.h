// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_STORAGE_PARTITION_MAP_H_
#define CONTENT_BROWSER_STORAGE_PARTITION_MAP_H_

#include <map>
#include <string>

#include "base/callback_forward.h"
#include "base/supports_user_data.h"

class FilePath;

namespace content {

class BrowserContext;
class StoragePartition;

// A std::string to StoragePartition map for use with SupportsUserData APIs.
class StoragePartitionMap : public base::SupportsUserData::Data {
 public:
  explicit StoragePartitionMap(BrowserContext* browser_context);

  virtual ~StoragePartitionMap();

  // This map retains ownership of the returned StoragePartition objects.
  StoragePartition* Get(const std::string& partition_id);

  void ForEach(const base::Callback<void(StoragePartition*)>& callback);

 private:
  // This must always be called *after* |partition| has been added to the
  // partitions_.
  //
  // TODO(ajwong): Is there a way to make it so that Get()'s implementation
  // doesn't need to be aware of this ordering?  Revisit when refactoring
  // ResourceContext and AppCache to respect storage partitions.
  void PostCreateInitialization(StoragePartition* partition,
                                const FilePath& partition_path);

  BrowserContext* browser_context_;  // Not Owned.
  std::map<std::string, StoragePartition*> partitions_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_STORAGE_PARTITION_MAP_H_
