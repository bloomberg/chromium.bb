// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CONTEXT_IMPL_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CONTEXT_IMPL_H_

#include <map>
#include <set>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/indexed_db_context.h"
#include "googleurl/src/gurl.h"
#include "webkit/quota/quota_types.h"

class GURL;

namespace WebKit {
class WebIDBDatabase;
class WebIDBFactory;
}

namespace base {
class FilePath;
class MessageLoopProxy;
}

namespace quota {
class QuotaManagerProxy;
class SpecialStoragePolicy;
}

namespace content {

class CONTENT_EXPORT IndexedDBContextImpl
    : NON_EXPORTED_BASE(public IndexedDBContext) {
 public:
  // If |data_path| is empty, nothing will be saved to disk.
  IndexedDBContextImpl(const base::FilePath& data_path,
                       quota::SpecialStoragePolicy* special_storage_policy,
                       quota::QuotaManagerProxy* quota_manager_proxy,
                       base::MessageLoopProxy* webkit_thread_loop);

  WebKit::WebIDBFactory* GetIDBFactory();

  // The indexed db directory.
  static const base::FilePath::CharType kIndexedDBDirectory[];

  // The indexed db file extension.
  static const base::FilePath::CharType kIndexedDBExtension[];

  // Disables the exit-time deletion of session-only data.
  void SetForceKeepSessionState() {
    force_keep_session_state_ = true;
  }

  // IndexedDBContext implementation:
  virtual std::vector<GURL> GetAllOrigins() OVERRIDE;
  virtual int64 GetOriginDiskUsage(const GURL& origin_url) OVERRIDE;
  virtual base::Time GetOriginLastModified(const GURL& origin_url) OVERRIDE;
  virtual void DeleteForOrigin(const GURL& origin_url) OVERRIDE;
  virtual base::FilePath GetFilePathForTesting(
      const string16& origin_id) const OVERRIDE;

  // Methods called by IndexedDBDispatcherHost for quota support.
  void ConnectionOpened(const GURL& origin_url, WebKit::WebIDBDatabase*);
  void ConnectionClosed(const GURL& origin_url, WebKit::WebIDBDatabase*);
  void TransactionComplete(const GURL& origin_url);
  bool WouldBeOverQuota(const GURL& origin_url, int64 additional_bytes);
  bool IsOverQuota(const GURL& origin_url);

  quota::QuotaManagerProxy* quota_manager_proxy();

  base::FilePath data_path() const { return data_path_; }

  // For unit tests allow to override the |data_path_|.
  void set_data_path_for_testing(const base::FilePath& data_path) {
    data_path_ = data_path;
  }

 protected:
  virtual ~IndexedDBContextImpl();

 private:
  FRIEND_TEST_ALL_PREFIXES(IndexedDBTest, ClearLocalState);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBTest, ClearSessionOnlyDatabases);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBTest, SetForceKeepSessionState);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBTest, ForceCloseOpenDatabasesOnDelete);
  friend class IndexedDBQuotaClientTest;

  typedef std::map<GURL, int64> OriginToSizeMap;
  class IndexedDBGetUsageAndQuotaCallback;

  base::FilePath GetIndexedDBFilePath(const string16& origin_id) const;
  int64 ReadUsageFromDisk(const GURL& origin_url) const;
  void EnsureDiskUsageCacheInitialized(const GURL& origin_url);
  void QueryDiskAndUpdateQuotaUsage(const GURL& origin_url);
  void GotUsageAndQuota(const GURL& origin_url, quota::QuotaStatusCode,
                        int64 usage, int64 quota);
  void GotUpdatedQuota(const GURL& origin_url, int64 usage, int64 quota);
  void QueryAvailableQuota(const GURL& origin_url);

  std::set<GURL>* GetOriginSet();
  bool AddToOriginSet(const GURL& origin_url) {
    return GetOriginSet()->insert(origin_url).second;
  }
  void RemoveFromOriginSet(const GURL& origin_url) {
    GetOriginSet()->erase(origin_url);
  }
  bool IsInOriginSet(const GURL& origin_url) {
    std::set<GURL>* set = GetOriginSet();
    return set->find(origin_url) != set->end();
  }

  // Only for testing.
  void ResetCaches();

  scoped_ptr<WebKit::WebIDBFactory> idb_factory_;
  base::FilePath data_path_;
  // If true, nothing (not even session-only data) should be deleted on exit.
  bool force_keep_session_state_;
  scoped_refptr<quota::SpecialStoragePolicy> special_storage_policy_;
  scoped_refptr<quota::QuotaManagerProxy> quota_manager_proxy_;
  scoped_ptr<std::set<GURL> > origin_set_;
  OriginToSizeMap origin_size_map_;
  OriginToSizeMap space_available_map_;
  typedef std::set<WebKit::WebIDBDatabase*> ConnectionSet;
  std::map<GURL, ConnectionSet> connections_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBContextImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CONTEXT_IMPL_H_
