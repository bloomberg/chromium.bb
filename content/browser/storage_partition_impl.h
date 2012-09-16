// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_STORAGE_PARTITION_IMPL_H_
#define CONTENT_BROWSER_STORAGE_PARTITION_IMPL_H_

#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/browser/dom_storage/dom_storage_context_impl.h"
#include "content/browser/in_process_webkit/indexed_db_context_impl.h"
#include "content/public/browser/storage_partition.h"

namespace content {

class StoragePartitionImpl : public StoragePartition {
 public:
  virtual ~StoragePartitionImpl();

  // TODO(ajwong): Break the direct dependency on |context|. We only
  // need 3 pieces of info from it.
  static StoragePartitionImpl* Create(BrowserContext* context,
                                      const std::string& partition_id,
                                      const FilePath& partition_path);

  // StoragePartition interface.
  virtual FilePath GetPath() OVERRIDE;
  virtual quota::QuotaManager* GetQuotaManager() OVERRIDE;
  virtual ChromeAppCacheService* GetAppCacheService() OVERRIDE;
  virtual fileapi::FileSystemContext* GetFileSystemContext() OVERRIDE;
  virtual webkit_database::DatabaseTracker* GetDatabaseTracker() OVERRIDE;
  virtual DOMStorageContextImpl* GetDOMStorageContext() OVERRIDE;
  virtual IndexedDBContextImpl* GetIndexedDBContext() OVERRIDE;

 private:
  StoragePartitionImpl(const FilePath& partition_path,
                       quota::QuotaManager* quota_manager,
                       ChromeAppCacheService* appcache_service,
                       fileapi::FileSystemContext* filesystem_context,
                       webkit_database::DatabaseTracker* database_tracker,
                       DOMStorageContextImpl* dom_storage_context,
                       IndexedDBContextImpl* indexed_db_context);

  FilePath partition_path_;
  scoped_refptr<quota::QuotaManager> quota_manager_;
  scoped_refptr<ChromeAppCacheService> appcache_service_;
  scoped_refptr<fileapi::FileSystemContext> filesystem_context_;
  scoped_refptr<webkit_database::DatabaseTracker> database_tracker_;
  scoped_refptr<DOMStorageContextImpl> dom_storage_context_;
  scoped_refptr<IndexedDBContextImpl> indexed_db_context_;

  DISALLOW_COPY_AND_ASSIGN(StoragePartitionImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_STORAGE_PARTITION_IMPL_H_
