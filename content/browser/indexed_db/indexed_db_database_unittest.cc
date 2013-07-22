// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_database.h"

#include "base/auto_reset.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/indexed_db/indexed_db.h"
#include "content/browser/indexed_db/indexed_db_backing_store.h"
#include "content/browser/indexed_db/indexed_db_callbacks.h"
#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/indexed_db_cursor.h"
#include "content/browser/indexed_db/indexed_db_database.h"
#include "content/browser/indexed_db/indexed_db_database_callbacks.h"
#include "content/browser/indexed_db/indexed_db_factory.h"
#include "content/browser/indexed_db/indexed_db_fake_backing_store.h"
#include "content/browser/indexed_db/indexed_db_transaction.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TEST(IndexedDBDatabaseTest, BackingStoreRetention) {
  scoped_refptr<IndexedDBFakeBackingStore> backing_store =
      new IndexedDBFakeBackingStore();
  EXPECT_TRUE(backing_store->HasOneRef());

  IndexedDBFactory* factory = 0;
  scoped_refptr<IndexedDBDatabase> db = IndexedDBDatabase::Create(
      ASCIIToUTF16("db"),
      backing_store,
      factory,
      IndexedDBDatabase::Identifier());
  EXPECT_FALSE(backing_store->HasOneRef());  // local and db
  db = NULL;
  EXPECT_TRUE(backing_store->HasOneRef());  // local
}

class MockOpenCallbacks : public IndexedDBCallbacks {
 public:
  MockOpenCallbacks() : IndexedDBCallbacks(NULL, 0, 0) {}

  virtual void OnSuccess(scoped_ptr<IndexedDBConnection> connection,
                         const IndexedDBDatabaseMetadata& metadata) OVERRIDE {
    connection_ = connection.Pass();
  }

  IndexedDBConnection* connection() { return connection_.get(); }

 private:
  virtual ~MockOpenCallbacks() { EXPECT_TRUE(connection_); }
  scoped_ptr<IndexedDBConnection> connection_;
};

class FakeDatabaseCallbacks : public IndexedDBDatabaseCallbacks {
 public:
  FakeDatabaseCallbacks() : IndexedDBDatabaseCallbacks(NULL, 0, 0) {}

  virtual void OnVersionChange(int64 old_version, int64 new_version) OVERRIDE {}
  virtual void OnForcedClose() OVERRIDE {}
  virtual void OnAbort(int64 transaction_id,
                       const IndexedDBDatabaseError& error) OVERRIDE {}
  virtual void OnComplete(int64 transaction_id) OVERRIDE {}

 private:
  virtual ~FakeDatabaseCallbacks() {}
};

TEST(IndexedDBDatabaseTest, ConnectionLifecycle) {
  scoped_refptr<IndexedDBFakeBackingStore> backing_store =
      new IndexedDBFakeBackingStore();
  EXPECT_TRUE(backing_store->HasOneRef());  // local

  IndexedDBFactory* factory = 0;
  scoped_refptr<IndexedDBDatabase> db =
      IndexedDBDatabase::Create(ASCIIToUTF16("db"),
                                backing_store,
                                factory,
                                IndexedDBDatabase::Identifier());

  EXPECT_FALSE(backing_store->HasOneRef());  // local and db

  scoped_refptr<MockOpenCallbacks> request1(new MockOpenCallbacks());
  scoped_refptr<FakeDatabaseCallbacks> callbacks1(new FakeDatabaseCallbacks());
  const int64 transaction_id1 = 1;
  db->OpenConnection(request1,
                     callbacks1,
                     transaction_id1,
                     IndexedDBDatabaseMetadata::DEFAULT_INT_VERSION);

  EXPECT_FALSE(backing_store->HasOneRef());  // db, connection count > 0

  scoped_refptr<MockOpenCallbacks> request2(new MockOpenCallbacks());
  scoped_refptr<FakeDatabaseCallbacks> callbacks2(new FakeDatabaseCallbacks());
  const int64 transaction_id2 = 2;
  db->OpenConnection(request2,
                     callbacks2,
                     transaction_id2,
                     IndexedDBDatabaseMetadata::DEFAULT_INT_VERSION);

  EXPECT_FALSE(backing_store->HasOneRef());  // local and connection

  db->Close(request1->connection());

  EXPECT_FALSE(backing_store->HasOneRef());  // local and connection

  db->Close(request2->connection());
  EXPECT_TRUE(backing_store->HasOneRef());
  EXPECT_FALSE(db->BackingStore());

  db = NULL;
}

class MockAbortCallbacks : public IndexedDBDatabaseCallbacks {
 public:
  MockAbortCallbacks()
      : IndexedDBDatabaseCallbacks(NULL, 0, 0), abort_called_(false) {}

  virtual void OnAbort(int64 transaction_id,
                       const IndexedDBDatabaseError& error) OVERRIDE {
    abort_called_ = true;
  }

 private:
  virtual ~MockAbortCallbacks() { EXPECT_TRUE(abort_called_); }
  bool abort_called_;
};

TEST(IndexedDBDatabaseTest, ForcedClose) {
  scoped_refptr<IndexedDBFakeBackingStore> backing_store =
      new IndexedDBFakeBackingStore();
  EXPECT_TRUE(backing_store->HasOneRef());

  IndexedDBFactory* factory = 0;
  scoped_refptr<IndexedDBDatabase> database =
      IndexedDBDatabase::Create(ASCIIToUTF16("db"),
                                backing_store,
                                factory,
                                IndexedDBDatabase::Identifier());

  EXPECT_FALSE(backing_store->HasOneRef());  // local and db

  scoped_refptr<MockAbortCallbacks> callbacks(new MockAbortCallbacks());
  scoped_refptr<MockOpenCallbacks> request(new MockOpenCallbacks());
  const int64 upgrade_transaction_id = 3;
  database->OpenConnection(request,
                           callbacks,
                           upgrade_transaction_id,
                           IndexedDBDatabaseMetadata::DEFAULT_INT_VERSION);
  EXPECT_EQ(database, request->connection()->database());

  const int64 transaction_id = 123;
  const std::vector<int64> scope;
  database->CreateTransaction(transaction_id,
                              request->connection(),
                              scope,
                              indexed_db::TRANSACTION_READ_ONLY);

  request->connection()->ForceClose();

  EXPECT_TRUE(backing_store->HasOneRef());  // local
}

class MockDeleteCallbacks : public IndexedDBCallbacks {
 public:
  MockDeleteCallbacks()
      : IndexedDBCallbacks(NULL, 0, 0),
        blocked_called_(false),
        success_void_called_(false) {}

  virtual void OnBlocked(int64 existing_version) OVERRIDE {
    blocked_called_ = true;
  }
  virtual void OnSuccess() OVERRIDE { success_void_called_ = true; }

  bool blocked_called() const { return blocked_called_; }

 private:
  virtual ~MockDeleteCallbacks() { EXPECT_TRUE(success_void_called_); }

  scoped_ptr<IndexedDBConnection> connection_;
  bool blocked_called_;
  bool success_void_called_;
};

TEST(IndexedDBDatabaseTest, PendingDelete) {
  scoped_refptr<IndexedDBFakeBackingStore> backing_store =
      new IndexedDBFakeBackingStore();
  EXPECT_TRUE(backing_store->HasOneRef());  // local

  IndexedDBFactory* factory = 0;
  scoped_refptr<IndexedDBDatabase> db =
      IndexedDBDatabase::Create(ASCIIToUTF16("db"),
                                backing_store,
                                factory,
                                IndexedDBDatabase::Identifier());

  EXPECT_FALSE(backing_store->HasOneRef());  // local and db

  scoped_refptr<MockOpenCallbacks> request1(new MockOpenCallbacks());
  scoped_refptr<FakeDatabaseCallbacks> callbacks1(new FakeDatabaseCallbacks());
  const int64 transaction_id1 = 1;
  db->OpenConnection(request1,
                     callbacks1,
                     transaction_id1,
                     IndexedDBDatabaseMetadata::DEFAULT_INT_VERSION);

  EXPECT_FALSE(backing_store->HasOneRef());  // local and db

  scoped_refptr<MockDeleteCallbacks> request2(new MockDeleteCallbacks());
  db->DeleteDatabase(request2);

  EXPECT_TRUE(request2->blocked_called());
  EXPECT_FALSE(backing_store->HasOneRef());  // local and db

  db->Close(request1->connection());

  EXPECT_FALSE(db->BackingStore());
  EXPECT_TRUE(backing_store->HasOneRef());  // local
}

}  // namespace content
