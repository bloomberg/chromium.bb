// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <utility>

#include "base/auto_reset.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_data_format_version.h"
#include "content/browser/indexed_db/indexed_db_factory_impl.h"
#include "content/browser/indexed_db/mock_indexed_db_callbacks.h"
#include "content/browser/indexed_db/mock_indexed_db_database_callbacks.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBDatabaseException.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBTypes.h"
#include "url/gurl.h"
#include "url/origin.h"

using base::ASCIIToUTF16;
using url::Origin;

namespace content {

namespace {

class MockIDBFactory : public IndexedDBFactoryImpl {
 public:
  explicit MockIDBFactory(IndexedDBContextImpl* context)
      : IndexedDBFactoryImpl(context) {}
  scoped_refptr<IndexedDBBackingStore> TestOpenBackingStore(
      const Origin& origin,
      const base::FilePath& data_directory) {
    IndexedDBDataLossInfo data_loss_info;
    bool disk_full;
    leveldb::Status s;
    scoped_refptr<IndexedDBBackingStore> backing_store =
        OpenBackingStore(origin, data_directory, nullptr /* request_context */,
                         &data_loss_info, &disk_full, &s);
    EXPECT_EQ(blink::kWebIDBDataLossNone, data_loss_info.status);
    return backing_store;
  }

  void TestCloseBackingStore(IndexedDBBackingStore* backing_store) {
    CloseBackingStore(backing_store->origin());
  }

  void TestReleaseBackingStore(IndexedDBBackingStore* backing_store,
                               bool immediate) {
    ReleaseBackingStore(backing_store->origin(), immediate);
  }

 private:
  ~MockIDBFactory() override {}

  DISALLOW_COPY_AND_ASSIGN(MockIDBFactory);
};

}  // namespace

class IndexedDBFactoryTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    task_runner_ = new base::TestSimpleTaskRunner();
    quota_manager_proxy_ = new MockQuotaManagerProxy(nullptr, nullptr);
    context_ = new IndexedDBContextImpl(
        temp_dir_.GetPath(), nullptr /* special_storage_policy */,
        quota_manager_proxy_.get(), task_runner_.get());
    idb_factory_ = new MockIDBFactory(context_.get());
  }

  void TearDown() override {
    quota_manager_proxy_->SimulateQuotaManagerDestroyed();
  }

 protected:
  IndexedDBFactoryTest() {}
  MockIDBFactory* factory() const { return idb_factory_.get(); }
  void clear_factory() { idb_factory_ = nullptr; }
  IndexedDBContextImpl* context() const { return context_.get(); }

 private:
  base::ScopedTempDir temp_dir_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  scoped_refptr<IndexedDBContextImpl> context_;
  scoped_refptr<MockIDBFactory> idb_factory_;
  scoped_refptr<MockQuotaManagerProxy> quota_manager_proxy_;
  TestBrowserThreadBundle thread_bundle_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBFactoryTest);
};

TEST_F(IndexedDBFactoryTest, BackingStoreLifetime) {
  const Origin origin1(GURL("http://localhost:81"));
  const Origin origin2(GURL("http://localhost:82"));

  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());
  scoped_refptr<IndexedDBBackingStore> disk_store1 =
      factory()->TestOpenBackingStore(origin1, temp_directory.GetPath());

  scoped_refptr<IndexedDBBackingStore> disk_store2 =
      factory()->TestOpenBackingStore(origin1, temp_directory.GetPath());
  EXPECT_EQ(disk_store1.get(), disk_store2.get());

  scoped_refptr<IndexedDBBackingStore> disk_store3 =
      factory()->TestOpenBackingStore(origin2, temp_directory.GetPath());

  factory()->TestCloseBackingStore(disk_store1.get());
  factory()->TestCloseBackingStore(disk_store3.get());

  EXPECT_FALSE(disk_store1->HasOneRef());
  EXPECT_FALSE(disk_store2->HasOneRef());
  EXPECT_TRUE(disk_store3->HasOneRef());

  disk_store2 = nullptr;
  EXPECT_TRUE(disk_store1->HasOneRef());
}

TEST_F(IndexedDBFactoryTest, BackingStoreLazyClose) {
  const Origin origin(GURL("http://localhost:81"));

  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());
  scoped_refptr<IndexedDBBackingStore> store =
      factory()->TestOpenBackingStore(origin, temp_directory.GetPath());

  // Give up the local refptr so that the factory has the only
  // outstanding reference.
  IndexedDBBackingStore* store_ptr = store.get();
  store = nullptr;
  EXPECT_FALSE(store_ptr->close_timer()->IsRunning());
  factory()->TestReleaseBackingStore(store_ptr, false);
  EXPECT_TRUE(store_ptr->close_timer()->IsRunning());

  factory()->TestOpenBackingStore(origin, temp_directory.GetPath());
  EXPECT_FALSE(store_ptr->close_timer()->IsRunning());
  factory()->TestReleaseBackingStore(store_ptr, false);
  EXPECT_TRUE(store_ptr->close_timer()->IsRunning());

  // Take back a ref ptr and ensure that the actual close
  // stops a running timer.
  store = store_ptr;
  factory()->TestCloseBackingStore(store_ptr);
  EXPECT_FALSE(store_ptr->close_timer()->IsRunning());
}

TEST_F(IndexedDBFactoryTest, MemoryBackingStoreLifetime) {
  const Origin origin1(GURL("http://localhost:81"));
  const Origin origin2(GURL("http://localhost:82"));

  scoped_refptr<IndexedDBBackingStore> mem_store1 =
      factory()->TestOpenBackingStore(origin1, base::FilePath());

  scoped_refptr<IndexedDBBackingStore> mem_store2 =
      factory()->TestOpenBackingStore(origin1, base::FilePath());
  EXPECT_EQ(mem_store1.get(), mem_store2.get());

  scoped_refptr<IndexedDBBackingStore> mem_store3 =
      factory()->TestOpenBackingStore(origin2, base::FilePath());

  factory()->TestCloseBackingStore(mem_store1.get());
  factory()->TestCloseBackingStore(mem_store3.get());

  EXPECT_FALSE(mem_store1->HasOneRef());
  EXPECT_FALSE(mem_store2->HasOneRef());
  EXPECT_FALSE(mem_store3->HasOneRef());

  clear_factory();
  EXPECT_FALSE(mem_store1->HasOneRef());  // mem_store1 and 2
  EXPECT_FALSE(mem_store2->HasOneRef());  // mem_store1 and 2
  EXPECT_TRUE(mem_store3->HasOneRef());

  mem_store2 = nullptr;
  EXPECT_TRUE(mem_store1->HasOneRef());
}

TEST_F(IndexedDBFactoryTest, RejectLongOrigins) {
  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());
  const base::FilePath base_path = temp_directory.GetPath();

  int limit = base::GetMaximumPathComponentLength(base_path);
  EXPECT_GT(limit, 0);

  std::string origin(limit + 1, 'x');
  Origin too_long_origin(GURL("http://" + origin + ":81/"));
  scoped_refptr<IndexedDBBackingStore> diskStore1 =
      factory()->TestOpenBackingStore(too_long_origin, base_path);
  EXPECT_FALSE(diskStore1.get());

  Origin ok_origin(GURL("http://someorigin.com:82/"));
  scoped_refptr<IndexedDBBackingStore> diskStore2 =
      factory()->TestOpenBackingStore(ok_origin, base_path);
  EXPECT_TRUE(diskStore2.get());
  // We need a manual close or Windows can't delete the temp directory.
  factory()->TestCloseBackingStore(diskStore2.get());
}

class DiskFullFactory : public IndexedDBFactoryImpl {
 public:
  explicit DiskFullFactory(IndexedDBContextImpl* context)
      : IndexedDBFactoryImpl(context) {}

 private:
  ~DiskFullFactory() override {}
  scoped_refptr<IndexedDBBackingStore> OpenBackingStore(
      const Origin& origin,
      const base::FilePath& data_directory,
      scoped_refptr<net::URLRequestContextGetter> request_context_getter,
      IndexedDBDataLossInfo* data_loss_info,
      bool* disk_full,
      leveldb::Status* s) override {
    *disk_full = true;
    *s = leveldb::Status::IOError("Disk is full");
    return scoped_refptr<IndexedDBBackingStore>();
  }

  DISALLOW_COPY_AND_ASSIGN(DiskFullFactory);
};

class LookingForQuotaErrorMockCallbacks : public IndexedDBCallbacks {
 public:
  LookingForQuotaErrorMockCallbacks()
      : IndexedDBCallbacks(nullptr,
                           url::Origin(),
                           nullptr,
                           base::ThreadTaskRunnerHandle::Get()),
        error_called_(false) {}
  void OnError(const IndexedDBDatabaseError& error) override {
    error_called_ = true;
    EXPECT_EQ(blink::kWebIDBDatabaseExceptionQuotaError, error.code());
  }
  bool error_called() const { return error_called_; }

 private:
  ~LookingForQuotaErrorMockCallbacks() override {}
  bool error_called_;

  DISALLOW_COPY_AND_ASSIGN(LookingForQuotaErrorMockCallbacks);
};

TEST_F(IndexedDBFactoryTest, QuotaErrorOnDiskFull) {
  const Origin origin(GURL("http://localhost:81"));
  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());

  scoped_refptr<DiskFullFactory> factory = new DiskFullFactory(context());
  scoped_refptr<LookingForQuotaErrorMockCallbacks> callbacks =
      new LookingForQuotaErrorMockCallbacks;
  scoped_refptr<IndexedDBDatabaseCallbacks> dummy_database_callbacks =
      new IndexedDBDatabaseCallbacks(nullptr, nullptr);
  const base::string16 name(ASCIIToUTF16("name"));
  std::unique_ptr<IndexedDBPendingConnection> connection(
      base::MakeUnique<IndexedDBPendingConnection>(
          callbacks, dummy_database_callbacks, 0 /* child_process_id */,
          2 /* transaction_id */, 1 /* version */));
  factory->Open(name, std::move(connection), nullptr /* request_context */,
                origin, temp_directory.GetPath());
  EXPECT_TRUE(callbacks->error_called());
}

TEST_F(IndexedDBFactoryTest, BackingStoreReleasedOnForcedClose) {
  const Origin origin(GURL("http://localhost:81"));

  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());

  scoped_refptr<MockIndexedDBCallbacks> callbacks(new MockIndexedDBCallbacks());
  scoped_refptr<MockIndexedDBDatabaseCallbacks> db_callbacks(
      new MockIndexedDBDatabaseCallbacks());
  const int64_t transaction_id = 1;
  std::unique_ptr<IndexedDBPendingConnection> connection(
      base::MakeUnique<IndexedDBPendingConnection>(
          callbacks, db_callbacks, 0 /* child_process_id */, transaction_id,
          IndexedDBDatabaseMetadata::DEFAULT_VERSION));
  factory()->Open(ASCIIToUTF16("db"), std::move(connection),
                  nullptr /* request_context */, origin,
                  temp_directory.GetPath());

  EXPECT_TRUE(callbacks->connection());

  EXPECT_TRUE(factory()->IsBackingStoreOpen(origin));
  EXPECT_FALSE(factory()->IsBackingStorePendingClose(origin));

  callbacks->connection()->ForceClose();

  EXPECT_FALSE(factory()->IsBackingStoreOpen(origin));
  EXPECT_FALSE(factory()->IsBackingStorePendingClose(origin));
}

TEST_F(IndexedDBFactoryTest, BackingStoreReleaseDelayedOnClose) {
  const Origin origin(GURL("http://localhost:81"));

  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());

  scoped_refptr<MockIndexedDBCallbacks> callbacks(new MockIndexedDBCallbacks());
  scoped_refptr<MockIndexedDBDatabaseCallbacks> db_callbacks(
      new MockIndexedDBDatabaseCallbacks());
  const int64_t transaction_id = 1;
  std::unique_ptr<IndexedDBPendingConnection> connection(
      base::MakeUnique<IndexedDBPendingConnection>(
          callbacks, db_callbacks, 0 /* child_process_id */, transaction_id,
          IndexedDBDatabaseMetadata::DEFAULT_VERSION));
  factory()->Open(ASCIIToUTF16("db"), std::move(connection),
                  nullptr /* request_context */, origin,
                  temp_directory.GetPath());

  EXPECT_TRUE(callbacks->connection());
  IndexedDBBackingStore* store =
      callbacks->connection()->database()->backing_store();
  EXPECT_FALSE(store->HasOneRef());  // Factory and database.

  EXPECT_TRUE(factory()->IsBackingStoreOpen(origin));
  callbacks->connection()->Close();
  EXPECT_TRUE(store->HasOneRef());  // Factory.
  EXPECT_TRUE(factory()->IsBackingStoreOpen(origin));
  EXPECT_TRUE(factory()->IsBackingStorePendingClose(origin));
  EXPECT_TRUE(store->close_timer()->IsRunning());

  // Take a ref so it won't be destroyed out from under the test.
  scoped_refptr<IndexedDBBackingStore> store_ref = store;
  // Now simulate shutdown, which should stop the timer.
  factory()->ContextDestroyed();
  EXPECT_TRUE(store->HasOneRef());  // Local.
  EXPECT_FALSE(store->close_timer()->IsRunning());
  EXPECT_FALSE(factory()->IsBackingStoreOpen(origin));
  EXPECT_FALSE(factory()->IsBackingStorePendingClose(origin));
}

TEST_F(IndexedDBFactoryTest, DeleteDatabaseClosesBackingStore) {
  const Origin origin(GURL("http://localhost:81"));

  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());

  EXPECT_FALSE(factory()->IsBackingStoreOpen(origin));

  const bool expect_connection = false;
  scoped_refptr<MockIndexedDBCallbacks> callbacks(
      new MockIndexedDBCallbacks(expect_connection));
  factory()->DeleteDatabase(ASCIIToUTF16("db"), nullptr /* request_context */,
                            callbacks, origin, temp_directory.GetPath(),
                            false /* force_close */);

  EXPECT_TRUE(factory()->IsBackingStoreOpen(origin));
  EXPECT_TRUE(factory()->IsBackingStorePendingClose(origin));

  // Now simulate shutdown, which should stop the timer.
  factory()->ContextDestroyed();

  EXPECT_FALSE(factory()->IsBackingStoreOpen(origin));
  EXPECT_FALSE(factory()->IsBackingStorePendingClose(origin));
}

TEST_F(IndexedDBFactoryTest, GetDatabaseNamesClosesBackingStore) {
  const Origin origin(GURL("http://localhost:81"));

  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());

  EXPECT_FALSE(factory()->IsBackingStoreOpen(origin));

  const bool expect_connection = false;
  scoped_refptr<MockIndexedDBCallbacks> callbacks(
      new MockIndexedDBCallbacks(expect_connection));
  factory()->GetDatabaseNames(callbacks, origin, temp_directory.GetPath(),
                              nullptr /* request_context */);

  EXPECT_TRUE(factory()->IsBackingStoreOpen(origin));
  EXPECT_TRUE(factory()->IsBackingStorePendingClose(origin));

  // Now simulate shutdown, which should stop the timer.
  factory()->ContextDestroyed();

  EXPECT_FALSE(factory()->IsBackingStoreOpen(origin));
  EXPECT_FALSE(factory()->IsBackingStorePendingClose(origin));
}

TEST_F(IndexedDBFactoryTest, ForceCloseReleasesBackingStore) {
  const Origin origin(GURL("http://localhost:81"));

  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());

  scoped_refptr<MockIndexedDBCallbacks> callbacks(new MockIndexedDBCallbacks());
  scoped_refptr<MockIndexedDBDatabaseCallbacks> db_callbacks(
      new MockIndexedDBDatabaseCallbacks());
  const int64_t transaction_id = 1;
  std::unique_ptr<IndexedDBPendingConnection> connection(
      base::MakeUnique<IndexedDBPendingConnection>(
          callbacks, db_callbacks, 0 /* child_process_id */, transaction_id,
          IndexedDBDatabaseMetadata::DEFAULT_VERSION));
  factory()->Open(ASCIIToUTF16("db"), std::move(connection),
                  nullptr /* request_context */, origin,
                  temp_directory.GetPath());

  EXPECT_TRUE(callbacks->connection());
  EXPECT_TRUE(factory()->IsBackingStoreOpen(origin));
  EXPECT_FALSE(factory()->IsBackingStorePendingClose(origin));

  callbacks->connection()->Close();

  EXPECT_TRUE(factory()->IsBackingStoreOpen(origin));
  EXPECT_TRUE(factory()->IsBackingStorePendingClose(origin));

  factory()->ForceClose(origin);

  EXPECT_FALSE(factory()->IsBackingStoreOpen(origin));
  EXPECT_FALSE(factory()->IsBackingStorePendingClose(origin));

  // Ensure it is safe if the store is not open.
  factory()->ForceClose(origin);
}

class UpgradeNeededCallbacks : public MockIndexedDBCallbacks {
 public:
  UpgradeNeededCallbacks() {}

  void OnSuccess(std::unique_ptr<IndexedDBConnection> connection,
                 const IndexedDBDatabaseMetadata& metadata) override {
    EXPECT_TRUE(connection_.get());
    EXPECT_FALSE(connection.get());
  }

  void OnUpgradeNeeded(int64_t old_version,
                       std::unique_ptr<IndexedDBConnection> connection,
                       const content::IndexedDBDatabaseMetadata& metadata,
                       const IndexedDBDataLossInfo& data_loss_info) override {
    connection_ = std::move(connection);
  }

 protected:
  ~UpgradeNeededCallbacks() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(UpgradeNeededCallbacks);
};

class ErrorCallbacks : public MockIndexedDBCallbacks {
 public:
  ErrorCallbacks() : MockIndexedDBCallbacks(false), saw_error_(false) {}

  void OnError(const IndexedDBDatabaseError& error) override {
    saw_error_ = true;
  }
  bool saw_error() const { return saw_error_; }

 private:
  ~ErrorCallbacks() override {}
  bool saw_error_;

  DISALLOW_COPY_AND_ASSIGN(ErrorCallbacks);
};

TEST_F(IndexedDBFactoryTest, DatabaseFailedOpen) {
  const Origin origin(GURL("http://localhost:81"));

  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());

  const base::string16 db_name(ASCIIToUTF16("db"));
  const int64_t db_version = 2;
  const int64_t transaction_id = 1;
  scoped_refptr<IndexedDBDatabaseCallbacks> db_callbacks(
      new MockIndexedDBDatabaseCallbacks());

  // Open at version 2, then close.
  {
    scoped_refptr<UpgradeNeededCallbacks> callbacks(
        new UpgradeNeededCallbacks());
    std::unique_ptr<IndexedDBPendingConnection> connection(
        base::MakeUnique<IndexedDBPendingConnection>(
            callbacks, db_callbacks, 0 /* child_process_id */, transaction_id,
            db_version));
    factory()->Open(db_name, std::move(connection),
                    nullptr /* request_context */, origin,
                    temp_directory.GetPath());
    EXPECT_TRUE(factory()->IsDatabaseOpen(origin, db_name));

    // Pump the message loop so the upgrade transaction can run.
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(callbacks->connection());
    callbacks->connection()->database()->Commit(
        callbacks->connection()->GetTransaction(transaction_id));

    callbacks->connection()->Close();
    EXPECT_FALSE(factory()->IsDatabaseOpen(origin, db_name));
  }

  // Open at version < 2, which will fail; ensure factory doesn't retain
  // the database object.
  {
    scoped_refptr<ErrorCallbacks> callbacks(new ErrorCallbacks());
    std::unique_ptr<IndexedDBPendingConnection> connection(
        base::MakeUnique<IndexedDBPendingConnection>(
            callbacks, db_callbacks, 0 /* child_process_id */, transaction_id,
            db_version - 1));
    factory()->Open(db_name, std::move(connection),
                    nullptr /* request_context */, origin,
                    temp_directory.GetPath());
    EXPECT_TRUE(callbacks->saw_error());
    EXPECT_FALSE(factory()->IsDatabaseOpen(origin, db_name));
  }

  // Terminate all pending-close timers.
  factory()->ForceClose(origin);
}

namespace {

class DataLossCallbacks final : public MockIndexedDBCallbacks {
 public:
  blink::WebIDBDataLoss data_loss() const { return data_loss_; }
  void OnSuccess(std::unique_ptr<IndexedDBConnection> connection,
                 const IndexedDBDatabaseMetadata& metadata) override {
    if (!connection_)
      connection_ = std::move(connection);
  }
  void OnError(const IndexedDBDatabaseError& error) final {
    ADD_FAILURE() << "Unexpected IDB error: " << error.message();
  }
  void OnUpgradeNeeded(int64_t old_version,
                       std::unique_ptr<IndexedDBConnection> connection,
                       const content::IndexedDBDatabaseMetadata& metadata,
                       const IndexedDBDataLossInfo& data_loss) final {
    connection_ = std::move(connection);
    data_loss_ = data_loss.status;
  }

 private:
  ~DataLossCallbacks() final {}
  blink::WebIDBDataLoss data_loss_ = blink::kWebIDBDataLossNone;
};

TEST_F(IndexedDBFactoryTest, DataFormatVersion) {
  auto try_open = [this](const Origin& origin,
                         const IndexedDBDataFormatVersion& version) {
    base::AutoReset<IndexedDBDataFormatVersion> override_version(
        &IndexedDBDataFormatVersion::GetMutableCurrentForTesting(), version);
    auto db_callbacks = base::MakeRefCounted<MockIndexedDBDatabaseCallbacks>();
    auto callbacks = base::MakeRefCounted<DataLossCallbacks>();
    const int64_t transaction_id = 1;
    factory()->Open(ASCIIToUTF16("test_db"),
                    base::MakeUnique<IndexedDBPendingConnection>(
                        callbacks, db_callbacks, 0 /* child_process_id */,
                        transaction_id, 1 /* version */),
                    nullptr /* request_context */, origin,
                    context()->data_path());
    base::RunLoop().RunUntilIdle();
    auto* connection = callbacks->connection();
    EXPECT_TRUE(connection);
    connection->database()->Commit(connection->GetTransaction(transaction_id));
    connection->Close();
    factory()->ForceClose(origin);
    return callbacks->data_loss();
  };

  using blink::kWebIDBDataLossNone;
  using blink::kWebIDBDataLossTotal;
  static const struct {
    const char* origin;
    IndexedDBDataFormatVersion open_version_1;
    IndexedDBDataFormatVersion open_version_2;
    blink::WebIDBDataLoss expected_data_loss;
  } kTestCases[] = {
      {"http://same-version.com/", {3, 4}, {3, 4}, kWebIDBDataLossNone},
      {"http://blink-upgrade.com/", {3, 4}, {3, 5}, kWebIDBDataLossNone},
      {"http://v8-upgrade.com/", {3, 4}, {4, 4}, kWebIDBDataLossNone},
      {"http://both-upgrade.com/", {3, 4}, {4, 5}, kWebIDBDataLossNone},
      {"http://blink-downgrade.com/", {3, 4}, {3, 3}, kWebIDBDataLossTotal},
      {"http://v8-downgrade.com/", {3, 4}, {2, 4}, kWebIDBDataLossTotal},
      {"http://both-downgrade.com/", {3, 4}, {2, 3}, kWebIDBDataLossTotal},
      {"http://v8-up-blink-down.com/", {3, 4}, {4, 2}, kWebIDBDataLossTotal},
      {"http://v8-down-blink-up.com/", {3, 4}, {2, 5}, kWebIDBDataLossTotal},
  };
  for (const auto& test : kTestCases) {
    SCOPED_TRACE(test.origin);
    const Origin origin(GURL(test.origin));
    ASSERT_EQ(kWebIDBDataLossNone, try_open(origin, test.open_version_1));
    EXPECT_EQ(test.expected_data_loss, try_open(origin, test.open_version_2));
  }
}

}  // namespace

}  // namespace content
