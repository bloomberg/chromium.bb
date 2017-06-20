// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <utility>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_factory_impl.h"
#include "content/browser/indexed_db/mock_indexed_db_callbacks.h"
#include "content/browser/indexed_db/mock_indexed_db_database_callbacks.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

using url::Origin;

namespace content {

class IndexedDBTest : public testing::Test {
 public:
  const Origin kNormalOrigin;
  const Origin kSessionOnlyOrigin;

  IndexedDBTest()
      : kNormalOrigin(GURL("http://normal/")),
        kSessionOnlyOrigin(GURL("http://session-only/")),
        task_runner_(new base::TestSimpleTaskRunner),
        special_storage_policy_(new MockSpecialStoragePolicy),
        quota_manager_proxy_(new MockQuotaManagerProxy(nullptr, nullptr)) {
    special_storage_policy_->AddSessionOnly(kSessionOnlyOrigin.GetURL());
  }
  ~IndexedDBTest() override {
    quota_manager_proxy_->SimulateQuotaManagerDestroyed();
  }

 protected:
  void FlushIndexedDBTaskRunner() { task_runner_->RunUntilIdle(); }

  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  scoped_refptr<MockSpecialStoragePolicy> special_storage_policy_;
  scoped_refptr<MockQuotaManagerProxy> quota_manager_proxy_;

 private:
  TestBrowserThreadBundle thread_bundle_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBTest);
};

TEST_F(IndexedDBTest, ClearSessionOnlyDatabases) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath normal_path;
  base::FilePath session_only_path;

  // Create the scope which will ensure we run the destructor of the context
  // which should trigger the clean up.
  {
    scoped_refptr<IndexedDBContextImpl> idb_context = new IndexedDBContextImpl(
        temp_dir.GetPath(), special_storage_policy_.get(),
        quota_manager_proxy_.get(), task_runner_.get());

    normal_path = idb_context->GetFilePathForTesting(kNormalOrigin);
    session_only_path = idb_context->GetFilePathForTesting(kSessionOnlyOrigin);
    ASSERT_TRUE(base::CreateDirectory(normal_path));
    ASSERT_TRUE(base::CreateDirectory(session_only_path));
    FlushIndexedDBTaskRunner();
    base::RunLoop().RunUntilIdle();
    quota_manager_proxy_->SimulateQuotaManagerDestroyed();
  }

  FlushIndexedDBTaskRunner();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(base::DirectoryExists(normal_path));
  EXPECT_FALSE(base::DirectoryExists(session_only_path));
}

TEST_F(IndexedDBTest, SetForceKeepSessionState) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath normal_path;
  base::FilePath session_only_path;

  // Create the scope which will ensure we run the destructor of the context.
  {
    // Create some indexedDB paths.
    // With the levelDB backend, these are directories.
    scoped_refptr<IndexedDBContextImpl> idb_context = new IndexedDBContextImpl(
        temp_dir.GetPath(), special_storage_policy_.get(),
        quota_manager_proxy_.get(), task_runner_.get());

    // Save session state. This should bypass the destruction-time deletion.
    idb_context->SetForceKeepSessionState();

    normal_path = idb_context->GetFilePathForTesting(kNormalOrigin);
    session_only_path = idb_context->GetFilePathForTesting(kSessionOnlyOrigin);
    ASSERT_TRUE(base::CreateDirectory(normal_path));
    ASSERT_TRUE(base::CreateDirectory(session_only_path));
    base::RunLoop().RunUntilIdle();
  }

  // Make sure we wait until the destructor has run.
  base::RunLoop().RunUntilIdle();

  // No data was cleared because of SetForceKeepSessionState.
  EXPECT_TRUE(base::DirectoryExists(normal_path));
  EXPECT_TRUE(base::DirectoryExists(session_only_path));
}

class ForceCloseDBCallbacks : public IndexedDBCallbacks {
 public:
  ForceCloseDBCallbacks(scoped_refptr<IndexedDBContextImpl> idb_context,
                        const Origin& origin)
      : IndexedDBCallbacks(nullptr,
                           origin,
                           nullptr,
                           base::ThreadTaskRunnerHandle::Get()),
        idb_context_(idb_context),
        origin_(origin) {}

  void OnSuccess() override {}
  void OnSuccess(const std::vector<base::string16>&) override {}
  void OnSuccess(std::unique_ptr<IndexedDBConnection> connection,
                 const IndexedDBDatabaseMetadata& metadata) override {
    connection_ = std::move(connection);
    idb_context_->ConnectionOpened(origin_, connection_.get());
  }

  IndexedDBConnection* connection() { return connection_.get(); }

 protected:
  ~ForceCloseDBCallbacks() override {}

 private:
  scoped_refptr<IndexedDBContextImpl> idb_context_;
  Origin origin_;
  std::unique_ptr<IndexedDBConnection> connection_;
  DISALLOW_COPY_AND_ASSIGN(ForceCloseDBCallbacks);
};

TEST_F(IndexedDBTest, ForceCloseOpenDatabasesOnDelete) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  scoped_refptr<MockIndexedDBDatabaseCallbacks> open_db_callbacks(
      new MockIndexedDBDatabaseCallbacks());
  scoped_refptr<MockIndexedDBDatabaseCallbacks> closed_db_callbacks(
      new MockIndexedDBDatabaseCallbacks());

  base::FilePath test_path;

  // Create the scope which will ensure we run the destructor of the context.
  {
    TestBrowserContext browser_context;

    const Origin kTestOrigin(GURL("http://test/"));

    scoped_refptr<IndexedDBContextImpl> idb_context = new IndexedDBContextImpl(
        temp_dir.GetPath(), special_storage_policy_.get(),
        quota_manager_proxy_.get(), task_runner_.get());

    scoped_refptr<ForceCloseDBCallbacks> open_callbacks =
        new ForceCloseDBCallbacks(idb_context, kTestOrigin);

    scoped_refptr<ForceCloseDBCallbacks> closed_callbacks =
        new ForceCloseDBCallbacks(idb_context, kTestOrigin);

    IndexedDBFactory* factory = idb_context->GetIDBFactory();

    test_path = idb_context->GetFilePathForTesting(kTestOrigin);

    std::unique_ptr<IndexedDBPendingConnection> open_connection(
        base::MakeUnique<IndexedDBPendingConnection>(
            open_callbacks, open_db_callbacks, 0 /* child_process_id */,
            0 /* host_transaction_id */, 0 /* version */));
    factory->Open(base::ASCIIToUTF16("opendb"), std::move(open_connection),
                  nullptr /* request_context */, Origin(kTestOrigin),
                  idb_context->data_path());
    std::unique_ptr<IndexedDBPendingConnection> closed_connection(
        base::MakeUnique<IndexedDBPendingConnection>(
            closed_callbacks, closed_db_callbacks, 0 /* child_process_id */,
            0 /* host_transaction_id */, 0 /* version */));
    factory->Open(base::ASCIIToUTF16("closeddb"), std::move(closed_connection),
                  nullptr /* request_context */, Origin(kTestOrigin),
                  idb_context->data_path());

    closed_callbacks->connection()->Close();

    // TODO(jsbell): Remove static_cast<> when overloads are eliminated.
    idb_context->TaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            static_cast<void (IndexedDBContextImpl::*)(const Origin&)>(
                &IndexedDBContextImpl::DeleteForOrigin),
            idb_context, kTestOrigin));
    FlushIndexedDBTaskRunner();
    base::RunLoop().RunUntilIdle();
  }

  // Make sure we wait until the destructor has run.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(open_db_callbacks->forced_close_called());
  EXPECT_FALSE(closed_db_callbacks->forced_close_called());
  EXPECT_FALSE(base::DirectoryExists(test_path));
}

TEST_F(IndexedDBTest, DeleteFailsIfDirectoryLocked) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const Origin kTestOrigin(GURL("http://test/"));

  scoped_refptr<IndexedDBContextImpl> idb_context = new IndexedDBContextImpl(
      temp_dir.GetPath(), special_storage_policy_.get(),
      quota_manager_proxy_.get(), task_runner_.get());

  base::FilePath test_path = idb_context->GetFilePathForTesting(kTestOrigin);
  ASSERT_TRUE(base::CreateDirectory(test_path));

  std::unique_ptr<LevelDBLock> lock =
      LevelDBDatabase::LockForTesting(test_path);
  ASSERT_TRUE(lock);

  // TODO(jsbell): Remove static_cast<> when overloads are eliminated.
  void (IndexedDBContextImpl::* delete_for_origin)(const Origin&) =
      &IndexedDBContextImpl::DeleteForOrigin;
  idb_context->TaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(delete_for_origin, idb_context, kTestOrigin));
  FlushIndexedDBTaskRunner();

  EXPECT_TRUE(base::DirectoryExists(test_path));
}

TEST_F(IndexedDBTest, ForceCloseOpenDatabasesOnCommitFailure) {
  const Origin kTestOrigin(GURL("http://test/"));

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  scoped_refptr<IndexedDBContextImpl> context = new IndexedDBContextImpl(
      temp_dir.GetPath(), special_storage_policy_.get(),
      quota_manager_proxy_.get(), task_runner_.get());

  scoped_refptr<IndexedDBFactoryImpl> factory =
      static_cast<IndexedDBFactoryImpl*>(context->GetIDBFactory());

  scoped_refptr<MockIndexedDBCallbacks> callbacks(new MockIndexedDBCallbacks());
  scoped_refptr<MockIndexedDBDatabaseCallbacks> db_callbacks(
      new MockIndexedDBDatabaseCallbacks());
  const int64_t transaction_id = 1;
  std::unique_ptr<IndexedDBPendingConnection> connection(
      base::MakeUnique<IndexedDBPendingConnection>(
          callbacks, db_callbacks, 0 /* child_process_id */, transaction_id,
          IndexedDBDatabaseMetadata::DEFAULT_VERSION));
  factory->Open(base::ASCIIToUTF16("db"), std::move(connection),
                nullptr /* request_context */, Origin(kTestOrigin),
                temp_dir.GetPath());

  EXPECT_TRUE(callbacks->connection());

  // ConnectionOpened() is usually called by the dispatcher.
  context->ConnectionOpened(kTestOrigin, callbacks->connection());

  EXPECT_TRUE(factory->IsBackingStoreOpen(kTestOrigin));

  // Simulate the write failure.
  leveldb::Status status = leveldb::Status::IOError("Simulated failure");
  context->GetIDBFactory()->HandleBackingStoreFailure(kTestOrigin);

  EXPECT_TRUE(db_callbacks->forced_close_called());
  EXPECT_FALSE(factory->IsBackingStoreOpen(kTestOrigin));
}

}  // namespace content
