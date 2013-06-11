// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_database.h"

#include <gtest/gtest.h>

#include "base/auto_reset.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/in_process_webkit/indexed_db_database_callbacks.h"
#include "content/browser/indexed_db/indexed_db.h"
#include "content/browser/indexed_db/indexed_db_backing_store.h"
#include "content/browser/indexed_db/indexed_db_callbacks_wrapper.h"
#include "content/browser/indexed_db/indexed_db_cursor.h"
#include "content/browser/indexed_db/indexed_db_database.h"
#include "content/browser/indexed_db/indexed_db_factory.h"
#include "content/browser/indexed_db/indexed_db_fake_backing_store.h"
#include "content/browser/indexed_db/indexed_db_transaction.h"
#include "content/browser/indexed_db/webidbdatabase_impl.h"

using WebKit::WebIDBDatabaseError;

namespace content {

TEST(IndexedDBDatabaseTest, BackingStoreRetention) {
  scoped_refptr<IndexedDBFakeBackingStore> backing_store =
      new IndexedDBFakeBackingStore();
  EXPECT_TRUE(backing_store->HasOneRef());

  IndexedDBFactory* factory = 0;
  scoped_refptr<IndexedDBDatabase> db =
      IndexedDBDatabase::Create(ASCIIToUTF16("db"),
                                    backing_store.get(),
                                    factory,
                                    ASCIIToUTF16("uniqueid"));
  EXPECT_FALSE(backing_store->HasOneRef());  // local and db
  db = NULL;
  EXPECT_TRUE(backing_store->HasOneRef());  // local
}

class MockIDBCallbacks : public IndexedDBCallbacksWrapper {
 public:
  static scoped_refptr<MockIDBCallbacks> Create() {
    return make_scoped_refptr(new MockIDBCallbacks());
  }
  virtual void OnError(const IndexedDBDatabaseError& error) OVERRIDE {}
  virtual void OnSuccess(const std::vector<string16>& value) OVERRIDE {}
  virtual void OnSuccess(scoped_refptr<IndexedDBCursor> cursor,
                         const IndexedDBKey& key,
                         const IndexedDBKey& primary_key,
                         std::vector<char>* value) OVERRIDE {}
  virtual void OnSuccess(scoped_refptr<IndexedDBDatabase> db,
                         const IndexedDBDatabaseMetadata& metadata) OVERRIDE {
    was_success_db_called_ = true;
  }
  virtual void OnSuccess(const IndexedDBKey& key) OVERRIDE {}
  virtual void OnSuccess(std::vector<char>* value) OVERRIDE {}
  virtual void OnSuccess(std::vector<char>* value,
                         const IndexedDBKey& key,
                         const IndexedDBKeyPath& key_path) OVERRIDE {}
  virtual void OnSuccess(int64 value) OVERRIDE {}
  virtual void OnSuccess() OVERRIDE {}
  virtual void OnSuccess(const IndexedDBKey& key,
                         const IndexedDBKey& primary_key,
                         std::vector<char>* value) OVERRIDE {}
  virtual void OnSuccessWithPrefetch(
      const std::vector<IndexedDBKey>& keys,
      const std::vector<IndexedDBKey>& primary_keys,
      const std::vector<std::vector<char> >& values) OVERRIDE {}

 private:
  virtual ~MockIDBCallbacks() { EXPECT_TRUE(was_success_db_called_); }
  MockIDBCallbacks()
      : IndexedDBCallbacksWrapper(NULL), was_success_db_called_(false) {}
  bool was_success_db_called_;
};

class FakeIDBDatabaseCallbacks : public IndexedDBDatabaseCallbacksWrapper {
 public:
  static scoped_refptr<FakeIDBDatabaseCallbacks> Create() {
    return make_scoped_refptr(new FakeIDBDatabaseCallbacks());
  }
  virtual void OnVersionChange(int64 old_version, int64 new_version) OVERRIDE {}
  virtual void OnForcedClose() OVERRIDE {}
  virtual void OnAbort(int64 transaction_id,
                       const IndexedDBDatabaseError& error) OVERRIDE {}
  virtual void OnComplete(int64 transaction_id) OVERRIDE {}

 private:
  friend class base::RefCounted<FakeIDBDatabaseCallbacks>;
  virtual ~FakeIDBDatabaseCallbacks() {}
  FakeIDBDatabaseCallbacks() : IndexedDBDatabaseCallbacksWrapper(NULL) {}
};

TEST(IndexedDBDatabaseTest, ConnectionLifecycle) {
  scoped_refptr<IndexedDBFakeBackingStore> backing_store =
      new IndexedDBFakeBackingStore();
  EXPECT_TRUE(backing_store->HasOneRef());  // local

  IndexedDBFactory* factory = 0;
  scoped_refptr<IndexedDBDatabase> db =
      IndexedDBDatabase::Create(ASCIIToUTF16("db"),
                                    backing_store.get(),
                                    factory,
                                    ASCIIToUTF16("uniqueid"));

  EXPECT_FALSE(backing_store->HasOneRef());  // local and db

  scoped_refptr<MockIDBCallbacks> request1 = MockIDBCallbacks::Create();
  scoped_refptr<FakeIDBDatabaseCallbacks> connection1 =
      FakeIDBDatabaseCallbacks::Create();
  const int64 transaction_id1 = 1;
  db->OpenConnection(request1,
                     connection1,
                     transaction_id1,
                     IndexedDBDatabaseMetadata::DEFAULT_INT_VERSION);

  EXPECT_FALSE(backing_store->HasOneRef());  // db, connection count > 0

  scoped_refptr<MockIDBCallbacks> request2 = MockIDBCallbacks::Create();
  scoped_refptr<FakeIDBDatabaseCallbacks> connection2 =
      FakeIDBDatabaseCallbacks::Create();
  const int64 transaction_id2 = 2;
  db->OpenConnection(request2,
                     connection2,
                     transaction_id2,
                     IndexedDBDatabaseMetadata::DEFAULT_INT_VERSION);

  EXPECT_FALSE(backing_store->HasOneRef());  // local and connection

  db->Close(connection1);

  EXPECT_FALSE(backing_store->HasOneRef());  // local and connection

  db->Close(connection2);
  EXPECT_TRUE(backing_store->HasOneRef());
  EXPECT_FALSE(db->BackingStore().get());

  db = NULL;
}

class MockIDBDatabaseCallbacks : public IndexedDBDatabaseCallbacksWrapper {
 public:
  static scoped_refptr<MockIDBDatabaseCallbacks> Create() {
    return make_scoped_refptr(new MockIDBDatabaseCallbacks());
  }
  virtual void OnVersionChange(int64 old_version, int64 new_version) OVERRIDE {}
  virtual void OnForcedClose() OVERRIDE {}
  virtual void OnAbort(int64 transaction_id,
                       const IndexedDBDatabaseError& error) OVERRIDE {
    was_abort_called_ = true;
  }
  virtual void OnComplete(int64 transaction_id) OVERRIDE {}

 private:
  MockIDBDatabaseCallbacks()
      : IndexedDBDatabaseCallbacksWrapper(NULL), was_abort_called_(false) {}
  virtual ~MockIDBDatabaseCallbacks() { EXPECT_TRUE(was_abort_called_); }
  bool was_abort_called_;
};

TEST(IndexedDBDatabaseTest, ForcedClose) {
  scoped_refptr<IndexedDBFakeBackingStore> backing_store =
      new IndexedDBFakeBackingStore();
  EXPECT_TRUE(backing_store->HasOneRef());

  IndexedDBFactory* factory = 0;
  scoped_refptr<IndexedDBDatabase> backend =
      IndexedDBDatabase::Create(ASCIIToUTF16("db"),
                                    backing_store.get(),
                                    factory,
                                    ASCIIToUTF16("uniqueid"));

  EXPECT_FALSE(backing_store->HasOneRef());  // local and db

  scoped_refptr<MockIDBDatabaseCallbacks> connection =
      MockIDBDatabaseCallbacks::Create();
  WebIDBDatabaseImpl web_database(backend, connection);

  scoped_refptr<MockIDBCallbacks> request = MockIDBCallbacks::Create();
  const int64 upgrade_transaction_id = 3;
  backend->OpenConnection(request,
                          connection,
                          upgrade_transaction_id,
                          IndexedDBDatabaseMetadata::DEFAULT_INT_VERSION);

  const int64 transaction_id = 123;
  const std::vector<int64> scope;
  web_database.createTransaction(
      transaction_id, 0, scope, indexed_db::TRANSACTION_READ_ONLY);

  web_database.forceClose();

  EXPECT_TRUE(backing_store->HasOneRef());  // local
}

}  // namespace content
