// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_STORAGE_PARTITION_H_
#define CONTENT_PUBLIC_BROWSER_STORAGE_PARTITION_H_

#include <string>

#include "base/basictypes.h"
#include "base/files/file_path.h"

class GURL;

namespace appcache {
class AppCacheService;
}

namespace fileapi {
class FileSystemContext;
}

namespace net {
class URLRequestContextGetter;
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
  virtual base::FilePath GetPath() = 0;
  virtual net::URLRequestContextGetter* GetURLRequestContext() = 0;
  virtual net::URLRequestContextGetter* GetMediaURLRequestContext() = 0;
  virtual quota::QuotaManager* GetQuotaManager() = 0;
  virtual appcache::AppCacheService* GetAppCacheService() = 0;
  virtual fileapi::FileSystemContext* GetFileSystemContext() = 0;
  virtual webkit_database::DatabaseTracker* GetDatabaseTracker() = 0;
  virtual DOMStorageContext* GetDOMStorageContext() = 0;
  virtual IndexedDBContext* GetIndexedDBContext() = 0;

  enum StorageMask {
    kCookies = 1 << 0,

    // Corresponds to quota::kStorageTypeTemporary.
    kQuotaManagedTemporaryStorage = 1 << 1,

    // Corresponds to quota::kStorageTypePersistent.
    kQuotaManagedPersistentStorage = 1 << 2,

    // Local dom storage.
    kLocalDomStorage = 1 << 3,
    kSessionDomStorage = 1 << 4,

    kAllStorage = -1,
  };

  // Starts an asynchronous task that does a best-effort clear the data
  // corresonding to the given |storage_mask| inside this StoragePartition for
  // the given |storage_origin|. Note kSessionDomStorage is not cleared and the
  // mask is ignored.
  //
  // TODO(ajwong): Right now, the embedder may have some
  // URLRequestContextGetter objects that the StoragePartition does not know
  // about.  This will no longer be the case when we resolve
  // http://crbug.com/159193. Remove |request_context_getter| when that bug
  // is fixed.
  virtual void AsyncClearDataForOrigin(
      uint32 storage_mask,
      const GURL& storage_origin,
      net::URLRequestContextGetter* request_context_getter) = 0;

  // Similar to AsyncClearDataForOrigin(), but deletes all data out of the
  // StoragePartition rather than just the data related to this origin.
  virtual void AsyncClearData(uint32 storage_mask) = 0;

 protected:
  virtual ~StoragePartition() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_STORAGE_PARTITION_H_
