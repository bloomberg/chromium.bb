// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_STORAGE_PARTITION_H_
#define CONTENT_BROWSER_STORAGE_PARTITION_H_

#include "base/file_path.h"
#include "base/memory/ref_counted.h"

namespace fileapi {
class FileSystemContext;
}

namespace quota {
class QuotaManager;
}

namespace webkit_database {
class DatabaseTracker;
}

class ChromeAppCacheService;
class DOMStorageContextImpl;

namespace content {

class BrowserContext;
class IndexedDBContext;

// Defines the what persistent state a child process can access.
//
// The StoragePartition defines the view each child process has of the
// persistent state inside the BrowserContext. This is used to implement
// isolated storage where a renderer with isolated storage cannot see
// the cookies, localStorage, etc., that normal web renderers have access to.
class StoragePartition {
 public:
  ~StoragePartition();

  // TODO(ajwong): Break the direct dependency on |context|. We only
  // need 3 pieces of info from it.
  static StoragePartition* Create(BrowserContext* context,
                                  const FilePath& partition_path);

  quota::QuotaManager* quota_manager() { return quota_manager_; }
  ChromeAppCacheService* appcache_service() { return appcache_service_; }
  fileapi::FileSystemContext* filesystem_context() {
    return filesystem_context_;
  }
  webkit_database::DatabaseTracker* database_tracker() {
    return database_tracker_;
  }
  DOMStorageContextImpl* dom_storage_context() { return dom_storage_context_; }
  IndexedDBContext* indexed_db_context() { return indexed_db_context_; }

 private:
  StoragePartition(const FilePath& partition_path,
                   quota::QuotaManager* quota_manager,
                   ChromeAppCacheService* appcache_service,
                   fileapi::FileSystemContext* filesystem_context,
                   webkit_database::DatabaseTracker* database_tracker,
                   DOMStorageContextImpl* dom_storage_context,
                   IndexedDBContext* indexed_db_context);

  FilePath partition_path_;
  scoped_refptr<quota::QuotaManager> quota_manager_;
  scoped_refptr<ChromeAppCacheService> appcache_service_;
  scoped_refptr<fileapi::FileSystemContext> filesystem_context_;
  scoped_refptr<webkit_database::DatabaseTracker> database_tracker_;
  scoped_refptr<DOMStorageContextImpl> dom_storage_context_;
  scoped_refptr<IndexedDBContext> indexed_db_context_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_STORAGE_PARTITION_H_
