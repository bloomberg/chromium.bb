// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_IN_PROCESS_WEBKIT_INDEXED_DB_CONTEXT_H_
#define CONTENT_BROWSER_IN_PROCESS_WEBKIT_INDEXED_DB_CONTEXT_H_
#pragma once

#include <map>
#include <set>
#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "googleurl/src/gurl.h"
#include "webkit/quota/quota_types.h"

class GURL;
class FilePath;
class WebKitContext;

namespace WebKit {
class WebIDBFactory;
}

namespace base {
class MessageLoopProxy;
}

namespace quota {
class QuotaManagerProxy;
class SpecialStoragePolicy;
}

class CONTENT_EXPORT IndexedDBContext
    : public base::RefCountedThreadSafe<IndexedDBContext> {
 public:
  IndexedDBContext(WebKitContext* webkit_context,
                   quota::SpecialStoragePolicy* special_storage_policy,
                   quota::QuotaManagerProxy* quota_manager_proxy,
                   base::MessageLoopProxy* webkit_thread_loop);

  ~IndexedDBContext();

  WebKit::WebIDBFactory* GetIDBFactory();

  // The indexed db directory.
  static const FilePath::CharType kIndexedDBDirectory[];

  // The indexed db file extension.
  static const FilePath::CharType kIndexedDBExtension[];

  void set_clear_local_state_on_exit(bool clear_local_state) {
    clear_local_state_on_exit_ = clear_local_state;
  }

  // Deletes all indexed db files for the given origin.
  void DeleteIndexedDBForOrigin(const GURL& origin_url);

  // Methods used in response to QuotaManager requests.
  void GetAllOrigins(std::vector<GURL>* origins);
  int64 GetOriginDiskUsage(const GURL& origin_url);
  base::Time GetOriginLastModified(const GURL& origin_url);

  // Methods called by IndexedDBDispatcherHost for quota support.
  void ConnectionOpened(const GURL& origin_url);
  void ConnectionClosed(const GURL& origin_url);
  void TransactionComplete(const GURL& origin_url);
  bool WouldBeOverQuota(const GURL& origin_url, int64 additional_bytes);
  bool IsOverQuota(const GURL& origin_url);

  quota::QuotaManagerProxy* quota_manager_proxy();

  // For unit tests allow to override the |data_path_|.
  void set_data_path_for_testing(const FilePath& data_path) {
    data_path_ = data_path;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(ExtensionServiceTest, ClearExtensionData);
  FRIEND_TEST_ALL_PREFIXES(ExtensionServiceTest, ClearAppData);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBBrowserTest, ClearLocalState);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBBrowserTest, ClearSessionOnlyDatabases);
  friend class IndexedDBQuotaClientTest;

  typedef std::map<GURL, int64> OriginToSizeMap;
  class IndexedDBGetUsageAndQuotaCallback;

  FilePath GetIndexedDBFilePath(const string16& origin_id) const;
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
  FilePath data_path_;
  bool clear_local_state_on_exit_;
  scoped_refptr<quota::SpecialStoragePolicy> special_storage_policy_;
  scoped_refptr<quota::QuotaManagerProxy> quota_manager_proxy_;
  scoped_ptr<std::set<GURL> > origin_set_;
  OriginToSizeMap origin_size_map_;
  OriginToSizeMap space_available_map_;
  std::map<GURL, unsigned int> connection_count_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBContext);
};

#endif  // CONTENT_BROWSER_IN_PROCESS_WEBKIT_INDEXED_DB_CONTEXT_H_
