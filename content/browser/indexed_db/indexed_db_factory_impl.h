// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_FACTORY_IMPL_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_FACTORY_IMPL_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <set>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "content/browser/indexed_db/indexed_db_factory.h"

namespace url {
class Origin;
}

namespace content {

class IndexedDBContextImpl;

class CONTENT_EXPORT IndexedDBFactoryImpl : public IndexedDBFactory {
 public:
  explicit IndexedDBFactoryImpl(IndexedDBContextImpl* context);

  // content::IndexedDBFactory overrides:
  void ReleaseDatabase(const IndexedDBDatabase::Identifier& identifier,
                       bool forced_close) override;

  void GetDatabaseNames(scoped_refptr<IndexedDBCallbacks> callbacks,
                        const url::Origin& origin,
                        const base::FilePath& data_directory,
                        scoped_refptr<net::URLRequestContextGetter>
                            request_context_getter) override;
  void Open(const base::string16& name,
            std::unique_ptr<IndexedDBPendingConnection> connection,
            scoped_refptr<net::URLRequestContextGetter> request_context_getter,
            const url::Origin& origin,
            const base::FilePath& data_directory) override;

  void DeleteDatabase(
      const base::string16& name,
      scoped_refptr<net::URLRequestContextGetter> request_context_getter,
      scoped_refptr<IndexedDBCallbacks> callbacks,
      const url::Origin& origin,
      const base::FilePath& data_directory,
      bool force_close) override;

  void AbortTransactionsAndCompactDatabase(
      base::OnceCallback<void(leveldb::Status)> callback,
      const url::Origin& origin) override;
  void AbortTransactionsForDatabase(
      base::OnceCallback<void(leveldb::Status)> callback,
      const url::Origin& origin) override;

  void HandleBackingStoreFailure(const url::Origin& origin) override;
  void HandleBackingStoreCorruption(
      const url::Origin& origin,
      const IndexedDBDatabaseError& error) override;

  OriginDBs GetOpenDatabasesForOrigin(const url::Origin& origin) const override;

  void ForceClose(const url::Origin& origin) override;

  // Called by the IndexedDBContext destructor so the factory can do cleanup.
  void ContextDestroyed() override;

  // Called by the IndexedDBActiveBlobRegistry.
  void ReportOutstandingBlobs(const url::Origin& origin,
                              bool blobs_outstanding) override;

  // Called by an IndexedDBDatabase when it is actually deleted.
  void DatabaseDeleted(
      const IndexedDBDatabase::Identifier& identifier) override;

  // Called by IndexedDBBackingStore when blob files have been cleaned.
  void BlobFilesCleaned(const url::Origin& origin) override;

  size_t GetConnectionCount(const url::Origin& origin) const override;

  void NotifyIndexedDBContentChanged(
      const url::Origin& origin,
      const base::string16& database_name,
      const base::string16& object_store_name) override;

 protected:
  ~IndexedDBFactoryImpl() override;

  scoped_refptr<IndexedDBBackingStore> OpenBackingStore(
      const url::Origin& origin,
      const base::FilePath& data_directory,
      scoped_refptr<net::URLRequestContextGetter> request_context_getter,
      IndexedDBDataLossInfo* data_loss_info,
      bool* disk_full,
      leveldb::Status* s) override;

  scoped_refptr<IndexedDBBackingStore> OpenBackingStoreHelper(
      const url::Origin& origin,
      const base::FilePath& data_directory,
      scoped_refptr<net::URLRequestContextGetter> request_context_getter,
      IndexedDBDataLossInfo* data_loss_info,
      bool* disk_full,
      bool first_time,
      leveldb::Status* s) override;

  void ReleaseBackingStore(const url::Origin& origin, bool immediate);
  void CloseBackingStore(const url::Origin& origin);
  IndexedDBContextImpl* context() const { return context_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(IndexedDBFactoryTest,
                           BackingStoreReleasedOnForcedClose);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBFactoryTest,
                           BackingStoreReleaseDelayedOnClose);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBFactoryTest, DatabaseFailedOpen);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBFactoryTest,
                           DeleteDatabaseClosesBackingStore);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBFactoryTest,
                           ForceCloseReleasesBackingStore);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBFactoryTest,
                           GetDatabaseNamesClosesBackingStore);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBTest,
                           ForceCloseOpenDatabasesOnCommitFailure);

  leveldb::Status AbortTransactions(const url::Origin& origin);

  // Called internally after a database is closed, with some delay. If this
  // factory has the last reference it will start running pre-close tasks.
  void MaybeStartPreCloseTasks(const url::Origin& origin);
  // Called internally after pre-close tasks. If this factory has the last
  // reference it will be released.
  void MaybeCloseBackingStore(const url::Origin& origin);
  bool HasLastBackingStoreReference(const url::Origin& origin) const;

  // Testing helpers, so unit tests don't need to grovel through internal state.
  bool IsDatabaseOpen(const url::Origin& origin,
                      const base::string16& name) const;
  bool IsBackingStoreOpen(const url::Origin& origin) const;
  bool IsBackingStorePendingClose(const url::Origin& origin) const;
  void RemoveDatabaseFromMaps(const IndexedDBDatabase::Identifier& identifier);

  IndexedDBContextImpl* context_;

  std::map<IndexedDBDatabase::Identifier, IndexedDBDatabase*> database_map_;
  OriginDBMap origin_dbs_;
  std::map<url::Origin, scoped_refptr<IndexedDBBackingStore>>
      backing_store_map_;

  std::set<scoped_refptr<IndexedDBBackingStore> > session_only_backing_stores_;
  std::map<url::Origin, scoped_refptr<IndexedDBBackingStore>>
      backing_stores_with_active_blobs_;
  std::set<url::Origin> backends_opened_since_boot_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBFactoryImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_FACTORY_IMPL_H_
