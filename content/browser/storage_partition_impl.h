// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_STORAGE_PARTITION_IMPL_H_
#define CONTENT_BROWSER_STORAGE_PARTITION_IMPL_H_

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/browser/dom_storage/dom_storage_context_wrapper.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/media/webrtc_identity_store.h"
#include "content/common/content_export.h"
#include "content/public/browser/storage_partition.h"

namespace content {

class StoragePartitionImpl : public StoragePartition {
 public:
  CONTENT_EXPORT virtual ~StoragePartitionImpl();

  // StoragePartition interface.
  virtual base::FilePath GetPath() OVERRIDE;
  virtual net::URLRequestContextGetter* GetURLRequestContext() OVERRIDE;
  virtual net::URLRequestContextGetter* GetMediaURLRequestContext() OVERRIDE;
  virtual quota::QuotaManager* GetQuotaManager() OVERRIDE;
  virtual ChromeAppCacheService* GetAppCacheService() OVERRIDE;
  virtual fileapi::FileSystemContext* GetFileSystemContext() OVERRIDE;
  virtual webkit_database::DatabaseTracker* GetDatabaseTracker() OVERRIDE;
  virtual DOMStorageContextWrapper* GetDOMStorageContext() OVERRIDE;
  virtual IndexedDBContextImpl* GetIndexedDBContext() OVERRIDE;

  virtual void ClearDataForOrigin(
      uint32 remove_mask,
      uint32 quota_storage_remove_mask,
      const GURL& storage_origin,
      net::URLRequestContextGetter* request_context_getter) OVERRIDE;
  virtual void ClearDataForUnboundedRange(
      uint32 remove_mask,
      uint32 quota_storage_remove_mask) OVERRIDE;
  virtual void ClearDataForRange(uint32 remove_mask,
                                 uint32 quota_storage_remove_mask,
                                 const base::Time& begin,
                                 const base::Time& end,
                                 const base::Closure& callback) OVERRIDE;

  WebRTCIdentityStore* GetWebRTCIdentityStore();

  struct DataDeletionHelper;
  struct QuotaManagedDataDeletionHelper;

 private:
  friend class StoragePartitionImplMap;
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionShaderClearTest, ClearShaderCache);

  // The |partition_path| is the absolute path to the root of this
  // StoragePartition's on-disk storage.
  //
  // If |in_memory| is true, the |partition_path| is (ab)used as a way of
  // distinguishing different in-memory partitions, but nothing is persisted
  // on to disk.
  static StoragePartitionImpl* Create(BrowserContext* context,
                                      bool in_memory,
                                      const base::FilePath& profile_path);

  // Quota managed data uses a different bitmask for types than
  // StoragePartition uses. This method generates that mask.
  static int GenerateQuotaClientMask(uint32 remove_mask);

  CONTENT_EXPORT StoragePartitionImpl(
      const base::FilePath& partition_path,
      quota::QuotaManager* quota_manager,
      ChromeAppCacheService* appcache_service,
      fileapi::FileSystemContext* filesystem_context,
      webkit_database::DatabaseTracker* database_tracker,
      DOMStorageContextWrapper* dom_storage_context,
      IndexedDBContextImpl* indexed_db_context,
      WebRTCIdentityStore* webrtc_identity_store);

  void ClearDataImpl(uint32 remove_mask,
                     uint32 quota_storage_remove_mask,
                     const GURL& remove_origin,
                     net::URLRequestContextGetter* rq_context,
                     const base::Time begin,
                     const base::Time end,
                     const base::Closure& callback);

  // Used by StoragePartitionImplMap.
  //
  // TODO(ajwong): These should be taken in the constructor and in Create() but
  // because the URLRequestContextGetter still lives in Profile with a tangled
  // initialization, if we try to retrieve the URLRequestContextGetter()
  // before the default StoragePartition is created, we end up reentering the
  // construction and double-initializing.  For now, we retain the legacy
  // behavior while allowing StoragePartitionImpl to expose these accessors by
  // letting StoragePartitionImplMap call these two private settings at the
  // appropriate time.  These should move back into the constructor once
  // URLRequestContextGetter's lifetime is sorted out. We should also move the
  // PostCreateInitialization() out of StoragePartitionImplMap.
  void SetURLRequestContext(net::URLRequestContextGetter* url_request_context);
  void SetMediaURLRequestContext(
      net::URLRequestContextGetter* media_url_request_context);

  base::FilePath partition_path_;
  scoped_refptr<net::URLRequestContextGetter> url_request_context_;
  scoped_refptr<net::URLRequestContextGetter> media_url_request_context_;
  scoped_refptr<quota::QuotaManager> quota_manager_;
  scoped_refptr<ChromeAppCacheService> appcache_service_;
  scoped_refptr<fileapi::FileSystemContext> filesystem_context_;
  scoped_refptr<webkit_database::DatabaseTracker> database_tracker_;
  scoped_refptr<DOMStorageContextWrapper> dom_storage_context_;
  scoped_refptr<IndexedDBContextImpl> indexed_db_context_;
  scoped_refptr<WebRTCIdentityStore> webrtc_identity_store_;

  DISALLOW_COPY_AND_ASSIGN(StoragePartitionImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_STORAGE_PARTITION_IMPL_H_
