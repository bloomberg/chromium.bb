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
#include "content/browser/indexed_db/indexed_db_factory.h"
#include "content/browser/indexed_db/indexed_db_fake_backing_store.h"
#include "content/browser/indexed_db/indexed_db_transaction.h"
#include "content/browser/indexed_db/mock_indexed_db_callbacks.h"
#include "content/browser/indexed_db/mock_indexed_db_database_callbacks.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

namespace {
const int FAKE_CHILD_PROCESS_ID = 0;
}

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

  scoped_refptr<MockIndexedDBCallbacks> request1(new MockIndexedDBCallbacks());
  scoped_refptr<MockIndexedDBDatabaseCallbacks> callbacks1(
      new MockIndexedDBDatabaseCallbacks());
  const int64 transaction_id1 = 1;
  IndexedDBPendingConnection connection1(
      request1,
      callbacks1,
      FAKE_CHILD_PROCESS_ID,
      transaction_id1,
      IndexedDBDatabaseMetadata::DEFAULT_INT_VERSION);
  db->OpenConnection(connection1);

  EXPECT_FALSE(backing_store->HasOneRef());  // db, connection count > 0

  scoped_refptr<MockIndexedDBCallbacks> request2(new MockIndexedDBCallbacks());
  scoped_refptr<MockIndexedDBDatabaseCallbacks> callbacks2(
      new MockIndexedDBDatabaseCallbacks());
  const int64 transaction_id2 = 2;
  IndexedDBPendingConnection connection2(
      request2,
      callbacks2,
      FAKE_CHILD_PROCESS_ID,
      transaction_id2,
      IndexedDBDatabaseMetadata::DEFAULT_INT_VERSION);
  db->OpenConnection(connection2);

  EXPECT_FALSE(backing_store->HasOneRef());  // local and connection

  request1->connection()->ForceClose();
  EXPECT_FALSE(request1->connection()->IsConnected());

  EXPECT_FALSE(backing_store->HasOneRef());  // local and connection

  request2->connection()->ForceClose();
  EXPECT_FALSE(request2->connection()->IsConnected());

  EXPECT_TRUE(backing_store->HasOneRef());
  EXPECT_FALSE(db->backing_store());

  db = NULL;
}

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

  scoped_refptr<MockIndexedDBDatabaseCallbacks> callbacks(
      new MockIndexedDBDatabaseCallbacks());
  scoped_refptr<MockIndexedDBCallbacks> request(new MockIndexedDBCallbacks());
  const int64 upgrade_transaction_id = 3;
  IndexedDBPendingConnection connection(
      request,
      callbacks,
      FAKE_CHILD_PROCESS_ID,
      upgrade_transaction_id,
      IndexedDBDatabaseMetadata::DEFAULT_INT_VERSION);
  database->OpenConnection(connection);
  EXPECT_EQ(database, request->connection()->database());

  const int64 transaction_id = 123;
  const std::vector<int64> scope;
  database->CreateTransaction(transaction_id,
                              request->connection(),
                              scope,
                              indexed_db::TRANSACTION_READ_ONLY);

  request->connection()->ForceClose();

  EXPECT_TRUE(backing_store->HasOneRef());  // local
  EXPECT_TRUE(callbacks->abort_called());
}

class MockDeleteCallbacks : public IndexedDBCallbacks {
 public:
  MockDeleteCallbacks()
      : IndexedDBCallbacks(NULL, 0, 0),
        blocked_called_(false),
        success_called_(false) {}

  virtual void OnBlocked(int64 existing_version) OVERRIDE {
    blocked_called_ = true;
  }
  virtual void OnSuccess(int64) OVERRIDE { success_called_ = true; }

  bool blocked_called() const { return blocked_called_; }

 private:
  virtual ~MockDeleteCallbacks() { EXPECT_TRUE(success_called_); }

  bool blocked_called_;
  bool success_called_;
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

  scoped_refptr<MockIndexedDBCallbacks> request1(new MockIndexedDBCallbacks());
  scoped_refptr<MockIndexedDBDatabaseCallbacks> callbacks1(
      new MockIndexedDBDatabaseCallbacks());
  const int64 transaction_id1 = 1;
  IndexedDBPendingConnection connection(
      request1,
      callbacks1,
      FAKE_CHILD_PROCESS_ID,
      transaction_id1,
      IndexedDBDatabaseMetadata::DEFAULT_INT_VERSION);
  db->OpenConnection(connection);

  EXPECT_FALSE(backing_store->HasOneRef());  // local and db

  scoped_refptr<MockDeleteCallbacks> request2(new MockDeleteCallbacks());
  db->DeleteDatabase(request2);

  EXPECT_TRUE(request2->blocked_called());
  EXPECT_FALSE(backing_store->HasOneRef());  // local and db

  db->Close(request1->connection(), true /* forced */);

  EXPECT_FALSE(db->backing_store());
  EXPECT_TRUE(backing_store->HasOneRef());  // local
}

}  // namespace content
