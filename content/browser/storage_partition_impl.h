// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_STORAGE_PARTITION_IMPL_H_
#define CONTENT_BROWSER_STORAGE_PARTITION_IMPL_H_

#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/browser/dom_storage/dom_storage_context_impl.h"
#include "content/browser/in_process_webkit/indexed_db_context_impl.h"
#include "content/public/browser/storage_partition.h"

namespace content {

class StoragePartitionImpl : public StoragePartition {
 public:
  virtual ~StoragePartitionImpl();

  // StoragePartition interface.
  virtual FilePath GetPath() OVERRIDE;
  virtual net::URLRequestContextGetter* GetURLRequestContext() OVERRIDE;
  virtual net::URLRequestContextGetter* GetMediaURLRequestContext() OVERRIDE;
  virtual quota::QuotaManager* GetQuotaManager() OVERRIDE;
  virtual ChromeAppCacheService* GetAppCacheService() OVERRIDE;
  virtual fileapi::FileSystemContext* GetFileSystemContext() OVERRIDE;
  virtual webkit_database::DatabaseTracker* GetDatabaseTracker() OVERRIDE;
  virtual DOMStorageContextImpl* GetDOMStorageContext() OVERRIDE;
  virtual IndexedDBContextImpl* GetIndexedDBContext() OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionConfigTest, OperatorLess);
  friend class StoragePartitionImplMap;

  // Each StoragePartition is uniquely identified by which partition domain
  // it belongs to (such as an app or the browser itself), the user supplied
  // partition name and the bit indicating whether it should be persisted on
  // disk or not. This structure contains those elements and is used as
  // uniqueness key to lookup StoragePartition objects in the global map.
  //
  // TODO(nasko): It is equivalent, though not identical to the same structure
  // that lives in chrome profiles. The difference is that this one has
  // partition_domain and partition_name separate, while the latter one has
  // the path produced by combining the two pieces together.
  // The fix for http://crbug.com/159193 will remove the chrome version.
  struct StoragePartitionConfig {
    const std::string partition_domain;
    const std::string partition_name;
    const bool in_memory;

    StoragePartitionConfig(const std::string& domain,
                               const std::string& partition,
                               const bool& in_memory_only)
      : partition_domain(domain),
        partition_name(partition),
        in_memory(in_memory_only) {}
  };

  // Functor for operator <.
  struct StoragePartitionConfigLess {
    bool operator()(const StoragePartitionConfig& lhs,
                    const StoragePartitionConfig& rhs) const {
      if (lhs.partition_domain != rhs.partition_domain)
        return lhs.partition_domain < rhs.partition_domain;
      else if (lhs.partition_name != rhs.partition_name)
        return lhs.partition_name < rhs.partition_name;
      else if (lhs.in_memory != rhs.in_memory)
        return lhs.in_memory < rhs.in_memory;
      else
        return false;
    }
  };

  // TODO(ajwong): Break the direct dependency on |context|. We only
  // need 3 pieces of info from it.
  static StoragePartitionImpl* Create(
      BrowserContext* context,
      const StoragePartitionConfig& partition_id,
      const FilePath& profile_path);

  // Returns the relative path from the profile's base directory, to the
  // directory that holds all the state for storage contexts in
  // |partition_config|. If any of the strings in |partition_config| contain
  // embedded nuls, the values will be truncated and only the portion prior to
  // the nul will be used.
  static FilePath GetStoragePartitionPath(
      const StoragePartitionConfig& partition_config);

  StoragePartitionImpl(
      const FilePath& partition_path,
      quota::QuotaManager* quota_manager,
      ChromeAppCacheService* appcache_service,
      fileapi::FileSystemContext* filesystem_context,
      webkit_database::DatabaseTracker* database_tracker,
      DOMStorageContextImpl* dom_storage_context,
      IndexedDBContextImpl* indexed_db_context);

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

  FilePath partition_path_;
  scoped_refptr<net::URLRequestContextGetter> url_request_context_;
  scoped_refptr<net::URLRequestContextGetter> media_url_request_context_;
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
