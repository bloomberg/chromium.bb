// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_STORAGE_PARTITION_H_
#define CONTENT_PUBLIC_BROWSER_STORAGE_PARTITION_H_

#include <string>

#include "base/basictypes.h"
#include "base/file_path.h"

namespace appcache {
class AppCacheService;
}

namespace fileapi {
class FileSystemContext;
}

namespace quota {
class QuotaManager;
}

namespace webkit_database {
class DatabaseTracker;
}

namespace content {

class BrowserContext;
class IndexedDBContext;
class DOMStorageContext;

// Defines what persistent state a child process can access.
//
// The StoragePartition defines the view each child process has of the
// persistent state inside the BrowserContext. This is used to implement
// isolated storage where a renderer with isolated storage cannot see
// the cookies, localStorage, etc., that normal web renderers have access to.
class StoragePartition {
 public:
  virtual FilePath GetPath() = 0;
  virtual quota::QuotaManager* GetQuotaManager() = 0;
  virtual appcache::AppCacheService* GetAppCacheService() = 0;
  virtual fileapi::FileSystemContext* GetFileSystemContext() = 0;
  virtual webkit_database::DatabaseTracker* GetDatabaseTracker() = 0;
  virtual DOMStorageContext* GetDOMStorageContext() = 0;
  virtual IndexedDBContext* GetIndexedDBContext() = 0;

  // Returns the relative path from the profile's base directory, to the
  // directory that holds all the state for storage contexts in |partition_id|.
  //
  // TODO(ajwong): Remove this function from the public API once
  // URLRequestContextGetter's creation is moved into StoragePartition.
  static CONTENT_EXPORT FilePath GetPartitionPath(
      const std::string& partition_id);

 protected:
  virtual ~StoragePartition() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_STORAGE_PARTITION_H_
