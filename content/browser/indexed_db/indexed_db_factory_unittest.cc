// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_factory.h"

#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/mock_indexed_db_callbacks.h"
#include "content/browser/indexed_db/mock_indexed_db_database_callbacks.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebIDBDatabaseException.h"
#include "third_party/WebKit/public/platform/WebIDBTypes.h"
#include "url/gurl.h"
#include "webkit/common/database/database_identifier.h"

namespace content {

namespace {

class IndexedDBFactoryTest : public testing::Test {
 public:
  IndexedDBFactoryTest() {}

 protected:
  // For timers to post events.
  base::MessageLoop loop_;

 private:
  DISALLOW_COPY_AND_ASSIGN(IndexedDBFactoryTest);
};

class MockIDBFactory : public IndexedDBFactory {
 public:
  MockIDBFactory() : IndexedDBFactory(NULL) {}
  scoped_refptr<IndexedDBBackingStore> TestOpenBackingStore(
      const GURL& origin,
      const base::FilePath& data_directory) {
    WebKit::WebIDBCallbacks::DataLoss data_loss =
        WebKit::WebIDBCallbacks::DataLossNone;
    std::string data_loss_message;
    bool disk_full;
    scoped_refptr<IndexedDBBackingStore> backing_store =
        OpenBackingStore(origin,
                         data_directory,
                         &data_loss,
                         &data_loss_message,
                         &disk_full);
    EXPECT_EQ(WebKit::WebIDBCallbacks::DataLossNone, data_loss);
    return backing_store;
  }

  void TestCloseBackingStore(IndexedDBBackingStore* backing_store) {
    CloseBackingStore(backing_store->origin_url());
  }

  void TestReleaseBackingStore(IndexedDBBackingStore* backing_store,
                               bool immediate) {
    ReleaseBackingStore(backing_store->origin_url(), immediate);
  }

 private:
  virtual ~MockIDBFactory() {}
};

TEST_F(IndexedDBFactoryTest, BackingStoreLifetime) {
  GURL origin1("http://localhost:81");
  GURL origin2("http://localhost:82");

  scoped_refptr<MockIDBFactory> factory = new MockIDBFactory();

  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());
  scoped_refptr<IndexedDBBackingStore> disk_store1 =
      factory->TestOpenBackingStore(origin1, temp_directory.path());

  scoped_refptr<IndexedDBBackingStore> disk_store2 =
      factory->TestOpenBackingStore(origin1, temp_directory.path());
  EXPECT_EQ(disk_store1.get(), disk_store2.get());

  scoped_refptr<IndexedDBBackingStore> disk_store3 =
      factory->TestOpenBackingStore(origin2, temp_directory.path());

  factory->TestCloseBackingStore(disk_store1);
  factory->TestCloseBackingStore(disk_store3);

  EXPECT_FALSE(disk_store1->HasOneRef());
  EXPECT_FALSE(disk_store2->HasOneRef());
  EXPECT_TRUE(disk_store3->HasOneRef());

  disk_store2 = NULL;
  EXPECT_TRUE(disk_store1->HasOneRef());
}

TEST_F(IndexedDBFactoryTest, BackingStoreLazyClose) {
  GURL origin("http://localhost:81");

  scoped_refptr<MockIDBFactory> factory = new MockIDBFactory();

  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());
  scoped_refptr<IndexedDBBackingStore> store =
      factory->TestOpenBackingStore(origin, temp_directory.path());

  // Give up the local refptr so that the factory has the only
  // outstanding reference.
  IndexedDBBackingStore* store_ptr = store.get();
  store = NULL;
  EXPECT_FALSE(store_ptr->close_timer()->IsRunning());
  factory->TestReleaseBackingStore(store_ptr, false);
  EXPECT_TRUE(store_ptr->close_timer()->IsRunning());

  factory->TestOpenBackingStore(origin, temp_directory.path());
  EXPECT_FALSE(store_ptr->close_timer()->IsRunning());
  factory->TestReleaseBackingStore(store_ptr, false);
  EXPECT_TRUE(store_ptr->close_timer()->IsRunning());

  // Take back a ref ptr and ensure that the actual close
  // stops a running timer.
  store = store_ptr;
  factory->TestCloseBackingStore(store_ptr);
  EXPECT_FALSE(store_ptr->close_timer()->IsRunning());
}

TEST_F(IndexedDBFactoryTest, MemoryBackingStoreLifetime) {
  GURL origin1("http://localhost:81");
  GURL origin2("http://localhost:82");

  scoped_refptr<MockIDBFactory> factory = new MockIDBFactory();
  scoped_refptr<IndexedDBBackingStore> mem_store1 =
      factory->TestOpenBackingStore(origin1, base::FilePath());

  scoped_refptr<IndexedDBBackingStore> mem_store2 =
      factory->TestOpenBackingStore(origin1, base::FilePath());
  EXPECT_EQ(mem_store1.get(), mem_store2.get());

  scoped_refptr<IndexedDBBackingStore> mem_store3 =
      factory->TestOpenBackingStore(origin2, base::FilePath());

  factory->TestCloseBackingStore(mem_store1);
  factory->TestCloseBackingStore(mem_store3);

  EXPECT_FALSE(mem_store1->HasOneRef());
  EXPECT_FALSE(mem_store2->HasOneRef());
  EXPECT_FALSE(mem_store3->HasOneRef());

  factory = NULL;
  EXPECT_FALSE(mem_store1->HasOneRef());  // mem_store1 and 2
  EXPECT_FALSE(mem_store2->HasOneRef());  // mem_store1 and 2
  EXPECT_TRUE(mem_store3->HasOneRef());

  mem_store2 = NULL;
  EXPECT_TRUE(mem_store1->HasOneRef());
}

TEST_F(IndexedDBFactoryTest, RejectLongOrigins) {
  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());
  const base::FilePath base_path = temp_directory.path();
  scoped_refptr<MockIDBFactory> factory = new MockIDBFactory();

  int limit = file_util::GetMaximumPathComponentLength(base_path);
  EXPECT_GT(limit, 0);

  std::string origin(limit + 1, 'x');
  GURL too_long_origin("http://" + origin + ":81/");
  scoped_refptr<IndexedDBBackingStore> diskStore1 =
      factory->TestOpenBackingStore(too_long_origin, base_path);
  EXPECT_FALSE(diskStore1);

  GURL ok_origin("http://someorigin.com:82/");
  scoped_refptr<IndexedDBBackingStore> diskStore2 =
      factory->TestOpenBackingStore(ok_origin, base_path);
  EXPECT_TRUE(diskStore2);
}

class DiskFullFactory : public IndexedDBFactory {
 public:
  DiskFullFactory() : IndexedDBFactory(NULL) {}

 private:
  virtual ~DiskFullFactory() {}
  virtual scoped_refptr<IndexedDBBackingStore> OpenBackingStore(
      const GURL& origin_url,
      const base::FilePath& data_directory,
      WebKit::WebIDBCallbacks::DataLoss* data_loss,
      std::string* data_loss_message,
      bool* disk_full) OVERRIDE {
    *disk_full = true;
    return scoped_refptr<IndexedDBBackingStore>();
  }
};

class LookingForQuotaErrorMockCallbacks : public IndexedDBCallbacks {
 public:
  LookingForQuotaErrorMockCallbacks()
      : IndexedDBCallbacks(NULL, 0, 0), error_called_(false) {}
  virtual void OnError(const IndexedDBDatabaseError& error) OVERRIDE {
    error_called_ = true;
    EXPECT_EQ(WebKit::WebIDBDatabaseExceptionQuotaError, error.code());
  }

 private:
  virtual ~LookingForQuotaErrorMockCallbacks() { EXPECT_TRUE(error_called_); }
  bool error_called_;
};

TEST_F(IndexedDBFactoryTest, QuotaErrorOnDiskFull) {
  const GURL origin("http://localhost:81");

  scoped_refptr<DiskFullFactory> factory = new DiskFullFactory;
  scoped_refptr<LookingForQuotaErrorMockCallbacks> callbacks =
      new LookingForQuotaErrorMockCallbacks;
  scoped_refptr<IndexedDBDatabaseCallbacks> dummy_database_callbacks =
      new IndexedDBDatabaseCallbacks(NULL, 0, 0);
  const string16 name(ASCIIToUTF16("name"));
  factory->Open(name,
                1, /* version */
                2, /* transaction_id */
                callbacks,
                dummy_database_callbacks,
                origin,
                base::FilePath(FILE_PATH_LITERAL("/dummy")));
}

TEST_F(IndexedDBFactoryTest, BackingStoreReleasedOnForcedClose) {
  GURL origin("http://localhost:81");

  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());

  scoped_refptr<IndexedDBFactory> factory = new IndexedDBFactory(NULL);

  scoped_refptr<MockIndexedDBCallbacks> callbacks(new MockIndexedDBCallbacks());
  scoped_refptr<MockIndexedDBDatabaseCallbacks> db_callbacks(
      new MockIndexedDBDatabaseCallbacks());
  const int64 transaction_id = 1;
  factory->Open(ASCIIToUTF16("db"),
                IndexedDBDatabaseMetadata::DEFAULT_INT_VERSION,
                transaction_id,
                callbacks,
                db_callbacks,
                origin,
                temp_directory.path());

  EXPECT_TRUE(callbacks->connection());

  EXPECT_TRUE(factory->IsBackingStoreOpenForTesting(origin));
  callbacks->connection()->ForceClose();
  EXPECT_FALSE(factory->IsBackingStoreOpenForTesting(origin));
}

TEST_F(IndexedDBFactoryTest, BackingStoreReleaseDelayedOnClose) {
  GURL origin("http://localhost:81");

  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());

  scoped_refptr<IndexedDBFactory> factory = new IndexedDBFactory(NULL);

  scoped_refptr<MockIndexedDBCallbacks> callbacks(new MockIndexedDBCallbacks());
  scoped_refptr<MockIndexedDBDatabaseCallbacks> db_callbacks(
      new MockIndexedDBDatabaseCallbacks());
  const int64 transaction_id = 1;
  factory->Open(ASCIIToUTF16("db"),
                IndexedDBDatabaseMetadata::DEFAULT_INT_VERSION,
                transaction_id,
                callbacks,
                db_callbacks,
                origin,
                temp_directory.path());

  EXPECT_TRUE(callbacks->connection());
  IndexedDBBackingStore* store =
      callbacks->connection()->database()->backing_store();
  EXPECT_FALSE(store->HasOneRef());  // Factory and database.

  EXPECT_TRUE(factory->IsBackingStoreOpenForTesting(origin));
  callbacks->connection()->Close();
  EXPECT_TRUE(store->HasOneRef());  // Factory.
  EXPECT_TRUE(factory->IsBackingStoreOpenForTesting(origin));
  EXPECT_TRUE(store->close_timer()->IsRunning());

  // Take a ref so it won't be destroyed out from under the test.
  scoped_refptr<IndexedDBBackingStore> store_ref = store;
  // Now simulate shutdown, which should stop the timer.
  factory->ContextDestroyed();
  EXPECT_TRUE(store->HasOneRef());  // Local.
  EXPECT_FALSE(store->close_timer()->IsRunning());
  EXPECT_FALSE(factory->IsBackingStoreOpenForTesting(origin));
}

}  // namespace

}  // namespace content
