// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_dispatcher_host.h"

#include "base/barrier_closure.h"
#include "base/callback.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_offset_string_conversions.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/indexed_db/indexed_db_callbacks.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_database_callbacks.h"
#include "content/browser/indexed_db/indexed_db_factory.h"
#include "content/browser/indexed_db/indexed_db_pending_connection.h"
#include "content/browser/indexed_db/mock_mojo_indexed_db_callbacks.h"
#include "content/browser/indexed_db/mock_mojo_indexed_db_database_callbacks.h"
#include "content/common/indexed_db/indexed_db.mojom.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/associated_interface_ptr.h"
#include "mojo/public/cpp/bindings/strong_associated_binding.h"
#include "net/url_request/url_request_test_util.h"
#include "storage/browser/test/mock_quota_manager.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBDatabaseException.h"
#include "url/origin.h"

using indexed_db::mojom::Callbacks;
using indexed_db::mojom::CallbacksAssociatedPtrInfo;
using indexed_db::mojom::DatabaseAssociatedPtr;
using indexed_db::mojom::DatabaseAssociatedPtrInfo;
using indexed_db::mojom::DatabaseAssociatedRequest;
using indexed_db::mojom::DatabaseCallbacks;
using indexed_db::mojom::DatabaseCallbacksAssociatedPtrInfo;
using indexed_db::mojom::Factory;
using indexed_db::mojom::FactoryAssociatedPtr;
using indexed_db::mojom::FactoryAssociatedRequest;
using indexed_db::mojom::KeyPath;
using indexed_db::mojom::Value;
using indexed_db::mojom::ValuePtr;
using mojo::StrongAssociatedBindingPtr;
using testing::_;
using testing::StrictMock;

namespace content {
namespace {

ACTION_TEMPLATE(MoveArg,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(out)) {
  *out = std::move(*::testing::get<k>(args));
};

ACTION_P(RunClosure, closure) {
  closure.Run();
}

MATCHER_P(IsAssociatedInterfacePtrInfoValid,
          tf,
          std::string(negation ? "isn't" : "is") + " " +
              std::string(tf ? "valid" : "invalid")) {
  return tf == arg->is_valid();
}

MATCHER_P(MatchesIDBKey, key, "") {
  return arg.Equals(key);
}

typedef void (base::Closure::*ClosureRunFcn)() const &;

static const char kDatabaseName[] = "db";
static const char kOrigin[] = "https://www.example.com";
static const int kFakeProcessId = 2;
static const int64_t kTemporaryQuota = 50 * 1024 * 1024;

base::FilePath CreateAndReturnTempDir(base::ScopedTempDir* temp_dir) {
  CHECK(temp_dir->CreateUniqueTempDir());
  return temp_dir->GetPath();
}

// Stores data specific to a connection.
struct TestDatabaseConnection {
  TestDatabaseConnection(url::Origin origin,
                         base::string16 db_name,
                         int64_t version,
                         int64_t upgrade_txn_id)
      : origin(std::move(origin)),
        db_name(std::move(db_name)),
        version(version),
        upgrade_txn_id(upgrade_txn_id),
        open_callbacks(new StrictMock<MockMojoIndexedDBCallbacks>()),
        connection_callbacks(
            new StrictMock<MockMojoIndexedDBDatabaseCallbacks>()){};
  ~TestDatabaseConnection() {}

  void Open(Factory* factory) {
    factory->Open(open_callbacks->CreateInterfacePtrAndBind(),
                  connection_callbacks->CreateInterfacePtrAndBind(), origin,
                  db_name, version, upgrade_txn_id);
  }

  url::Origin origin;
  base::string16 db_name;
  int64_t version;
  int64_t upgrade_txn_id;

  DatabaseAssociatedPtr database;

  std::unique_ptr<MockMojoIndexedDBCallbacks> open_callbacks;
  std::unique_ptr<MockMojoIndexedDBDatabaseCallbacks> connection_callbacks;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestDatabaseConnection);
};

void StatusCallback(const base::Closure& callback,
                    ::indexed_db::mojom::Status* status_out,
                    ::indexed_db::mojom::Status status) {
  *status_out = status;
  callback.Run();
}

}  // namespace

class IndexedDBDispatcherHostTest : public testing::Test {
 public:
  IndexedDBDispatcherHostTest()
      : thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP),
        special_storage_policy_(
            base::MakeRefCounted<MockSpecialStoragePolicy>()),
        quota_manager_(base::MakeRefCounted<MockQuotaManager>(
            false /*is_incognito*/,
            browser_context_.GetPath(),
            BrowserThread::GetTaskRunnerForThread(BrowserThread::IO),
            base::ThreadTaskRunnerHandle::Get().get(),
            special_storage_policy_)),
        context_impl_(base::MakeRefCounted<IndexedDBContextImpl>(
            CreateAndReturnTempDir(&temp_dir_),
            special_storage_policy_,
            quota_manager_->proxy())),
        host_(new IndexedDBDispatcherHost(
            kFakeProcessId,
            base::MakeRefCounted<net::TestURLRequestContextGetter>(
                BrowserThread::GetTaskRunnerForThread(BrowserThread::IO)),
            context_impl_,
            ChromeBlobStorageContext::GetFor(&browser_context_))) {
    quota_manager_->SetQuota(GURL(kOrigin), storage::kStorageTypeTemporary,
                             kTemporaryQuota);
  }

  void TearDown() override {
    host_.reset();
    context_impl_ = nullptr;
    quota_manager_ = nullptr;
    RunAllBlockingPoolTasksUntilIdle();
    // File are leaked if this doesn't return true.
    ASSERT_TRUE(temp_dir_.Delete());
  }

  void SetUp() override {
    FactoryAssociatedRequest request =
        ::mojo::MakeIsolatedRequest(&idb_mojo_factory_);
    host_->AddBinding(std::move(request));
  }

 protected:
  TestBrowserThreadBundle thread_bundle_;
  TestBrowserContext browser_context_;

  base::ScopedTempDir temp_dir_;
  scoped_refptr<MockSpecialStoragePolicy> special_storage_policy_;
  scoped_refptr<MockQuotaManager> quota_manager_;
  scoped_refptr<IndexedDBContextImpl> context_impl_;
  std::unique_ptr<IndexedDBDispatcherHost, BrowserThread::DeleteOnIOThread>
      host_;
  FactoryAssociatedPtr idb_mojo_factory_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBDispatcherHostTest);
};

TEST_F(IndexedDBDispatcherHostTest, CloseConnectionBeforeUpgrade) {
  const int64_t kDBVersion = 1;
  const int64_t kTransactionId = 1;

  TestDatabaseConnection connection(url::Origin(GURL(kOrigin)),
                                    base::UTF8ToUTF16(kDatabaseName),
                                    kDBVersion, kTransactionId);
  IndexedDBDatabaseMetadata metadata;
  DatabaseAssociatedPtrInfo database_info;
  base::RunLoop loop;
  EXPECT_CALL(
      *connection.open_callbacks,
      MockedUpgradeNeeded(IsAssociatedInterfacePtrInfoValid(true),
                          IndexedDBDatabaseMetadata::NO_VERSION,
                          blink::kWebIDBDataLossNone, std::string(""), _))
      .WillOnce(testing::DoAll(MoveArg<0>(&database_info),
                               testing::SaveArg<4>(&metadata),
                               RunClosure(loop.QuitClosure())));

  connection.Open(idb_mojo_factory_.get());
  loop.Run();

  EXPECT_TRUE(database_info.is_valid());
  EXPECT_EQ(connection.version, metadata.version);
  EXPECT_EQ(connection.db_name, metadata.name);
}

TEST_F(IndexedDBDispatcherHostTest, CloseAfterUpgrade) {
  const int64_t kDBVersion = 1;
  const int64_t kTransactionId = 1;
  const int64_t kObjectStoreId = 10;
  const char kObjectStoreName[] = "os";

  // Open connection.
  TestDatabaseConnection connection(url::Origin(GURL(kOrigin)),
                                    base::UTF8ToUTF16(kDatabaseName),
                                    kDBVersion, kTransactionId);

  IndexedDBDatabaseMetadata metadata;
  DatabaseAssociatedPtrInfo database_info;
  {
    base::RunLoop loop;
    EXPECT_CALL(
        *connection.open_callbacks,
        MockedUpgradeNeeded(IsAssociatedInterfacePtrInfoValid(true),
                            IndexedDBDatabaseMetadata::NO_VERSION,
                            blink::kWebIDBDataLossNone, std::string(""), _))
        .WillOnce(testing::DoAll(MoveArg<0>(&database_info),
                                 testing::SaveArg<4>(&metadata),
                                 RunClosure(loop.QuitClosure())));

    // Queue open request message.
    connection.Open(idb_mojo_factory_.get());
    loop.Run();
  }

  ASSERT_TRUE(database_info.is_valid());
  EXPECT_EQ(connection.version, metadata.version);
  EXPECT_EQ(connection.db_name, metadata.name);

  connection.database.Bind(std::move(database_info));

  {
    ::testing::InSequence dummy;
    base::RunLoop loop;
    base::Closure quit_closure = base::BarrierClosure(2, loop.QuitClosure());
    EXPECT_CALL(*connection.connection_callbacks, Complete(kTransactionId))
        .Times(1)
        .WillOnce(RunClosure(quit_closure));
    EXPECT_CALL(
        *connection.open_callbacks,
        MockedSuccessDatabase(IsAssociatedInterfacePtrInfoValid(false), _))
        .Times(1)
        .WillOnce(RunClosure(quit_closure));

    ASSERT_TRUE(connection.database.is_bound());
    connection.database->CreateObjectStore(kTransactionId, kObjectStoreId,
                                           base::UTF8ToUTF16(kObjectStoreName),
                                           content::IndexedDBKeyPath(), false);
    connection.database->Commit(kTransactionId);
    loop.Run();
  }
}

TEST_F(IndexedDBDispatcherHostTest, OpenNewConnectionWhileUpgrading) {
  const int64_t kDBVersion = 1;
  const int64_t kTransactionId = 1;
  const int64_t kObjectStoreId = 10;
  const char kObjectStoreName[] = "os";

  // Open connection 1, and expect the upgrade needed.
  TestDatabaseConnection connection1(url::Origin(GURL(kOrigin)),
                                     base::UTF8ToUTF16(kDatabaseName),
                                     kDBVersion, kTransactionId);
  DatabaseAssociatedPtrInfo database_info1;
  {
    base::RunLoop loop;
    IndexedDBDatabaseMetadata metadata;
    EXPECT_CALL(
        *connection1.open_callbacks,
        MockedUpgradeNeeded(IsAssociatedInterfacePtrInfoValid(true),
                            IndexedDBDatabaseMetadata::NO_VERSION,
                            blink::kWebIDBDataLossNone, std::string(""), _))
        .WillOnce(testing::DoAll(MoveArg<0>(&database_info1),
                                 testing::SaveArg<4>(&metadata),
                                 RunClosure(loop.QuitClosure())));

    // Queue open request message.
    connection1.Open(idb_mojo_factory_.get());
    loop.Run();
  }
  connection1.database.Bind(std::move(database_info1));

  // Open connection 2, but expect that we won't be called back.
  DatabaseAssociatedPtrInfo database_info2;
  IndexedDBDatabaseMetadata metadata2;
  TestDatabaseConnection connection2(url::Origin(GURL(kOrigin)),
                                     base::UTF8ToUTF16(kDatabaseName),
                                     kDBVersion, 0);
  connection2.Open(idb_mojo_factory_.get());

  // Check that we're called in order and the second connection gets it's
  // database after the first connection completes.
  {
    ::testing::InSequence dummy;
    base::RunLoop loop;
    base::Closure quit_closure = base::BarrierClosure(3, loop.QuitClosure());
    EXPECT_CALL(*connection1.connection_callbacks, Complete(kTransactionId))
        .Times(1)
        .WillOnce(RunClosure(quit_closure));
    EXPECT_CALL(
        *connection1.open_callbacks,
        MockedSuccessDatabase(IsAssociatedInterfacePtrInfoValid(false), _))
        .Times(1)
        .WillOnce(RunClosure(quit_closure));
    EXPECT_CALL(
        *connection2.open_callbacks,
        MockedSuccessDatabase(IsAssociatedInterfacePtrInfoValid(true), _))
        .WillOnce(testing::DoAll(MoveArg<0>(&database_info2),
                                 testing::SaveArg<1>(&metadata2),
                                 RunClosure(quit_closure)));

    // Create object store.
    ASSERT_TRUE(connection1.database.is_bound());
    connection1.database->CreateObjectStore(kTransactionId, kObjectStoreId,
                                            base::UTF8ToUTF16(kObjectStoreName),
                                            content::IndexedDBKeyPath(), false);
    connection1.database->Commit(kTransactionId);
    loop.Run();
  }

  EXPECT_TRUE(database_info2.is_valid());
  EXPECT_EQ(connection2.version, metadata2.version);
  EXPECT_EQ(connection2.db_name, metadata2.name);
}

TEST_F(IndexedDBDispatcherHostTest, PutWithInvalidBlob) {
  const int64_t kDBVersion = 1;
  const int64_t kTransactionId = 1;
  const int64_t kObjectStoreId = 10;
  const char kObjectStoreName[] = "os";

  // Open connection.
  TestDatabaseConnection connection(url::Origin(GURL(kOrigin)),
                                    base::UTF8ToUTF16(kDatabaseName),
                                    kDBVersion, kTransactionId);

  IndexedDBDatabaseMetadata metadata;
  DatabaseAssociatedPtrInfo database_info;
  {
    base::RunLoop loop;
    EXPECT_CALL(
        *connection.open_callbacks,
        MockedUpgradeNeeded(IsAssociatedInterfacePtrInfoValid(true),
                            IndexedDBDatabaseMetadata::NO_VERSION,
                            blink::kWebIDBDataLossNone, std::string(""), _))
        .WillOnce(testing::DoAll(MoveArg<0>(&database_info),
                                 testing::SaveArg<4>(&metadata),
                                 RunClosure(loop.QuitClosure())));

    // Queue open request message.
    connection.Open(idb_mojo_factory_.get());
    loop.Run();
  }

  ASSERT_TRUE(database_info.is_valid());
  EXPECT_EQ(connection.version, metadata.version);
  EXPECT_EQ(connection.db_name, metadata.name);

  connection.database.Bind(std::move(database_info));

  {
    ::testing::InSequence dummy;
    base::RunLoop loop;
    base::Closure quit_closure = base::BarrierClosure(3, loop.QuitClosure());

    auto put_callbacks =
        base::MakeUnique<StrictMock<MockMojoIndexedDBCallbacks>>();

    EXPECT_CALL(*put_callbacks,
                Error(blink::kWebIDBDatabaseExceptionUnknownError, _))
        .Times(1)
        .WillOnce(RunClosure(quit_closure));

    EXPECT_CALL(
        *connection.connection_callbacks,
        Abort(kTransactionId, blink::kWebIDBDatabaseExceptionUnknownError, _))
        .Times(1)
        .WillOnce(RunClosure(quit_closure));

    EXPECT_CALL(*connection.open_callbacks,
                Error(blink::kWebIDBDatabaseExceptionAbortError, _))
        .Times(1)
        .WillOnce(RunClosure(quit_closure));

    ASSERT_TRUE(connection.database.is_bound());
    connection.database->CreateObjectStore(kTransactionId, kObjectStoreId,
                                           base::UTF8ToUTF16(kObjectStoreName),
                                           content::IndexedDBKeyPath(), false);
    // Call Put with an invalid blob.
    std::vector<::indexed_db::mojom::BlobInfoPtr> blobs;
    blobs.push_back(::indexed_db::mojom::BlobInfo::New(
        "fakeUUID", base::string16(), 100, nullptr));
    connection.database->Put(kTransactionId, kObjectStoreId,
                             Value::New("hello", std::move(blobs)),
                             content::IndexedDBKey(base::UTF8ToUTF16("hello")),
                             blink::kWebIDBPutModeAddOnly,
                             std::vector<content::IndexedDBIndexKeys>(),
                             put_callbacks->CreateInterfacePtrAndBind());
    connection.database->Commit(kTransactionId);
    loop.Run();
  }
}

TEST_F(IndexedDBDispatcherHostTest, CompactDatabaseWithConnection) {
  const int64_t kDBVersion = 1;
  const int64_t kTransactionId = 1;

  // Open connection.
  TestDatabaseConnection connection(url::Origin(GURL(kOrigin)),
                                    base::UTF8ToUTF16(kDatabaseName),
                                    kDBVersion, kTransactionId);
  IndexedDBDatabaseMetadata metadata;
  DatabaseAssociatedPtrInfo database_info;
  {
    base::RunLoop loop;
    EXPECT_CALL(
        *connection.open_callbacks,
        MockedUpgradeNeeded(IsAssociatedInterfacePtrInfoValid(true),
                            IndexedDBDatabaseMetadata::NO_VERSION,
                            blink::kWebIDBDataLossNone, std::string(), _))
        .WillOnce(testing::DoAll(MoveArg<0>(&database_info),
                                 testing::SaveArg<4>(&metadata),
                                 RunClosure(loop.QuitClosure())));

    // Queue open request message.
    connection.Open(idb_mojo_factory_.get());
    loop.Run();
  }

  EXPECT_TRUE(database_info.is_valid());
  EXPECT_EQ(connection.version, metadata.version);
  EXPECT_EQ(connection.db_name, metadata.name);

  connection.database.Bind(std::move(database_info));

  ::indexed_db::mojom::Status callback_result =
      ::indexed_db::mojom::Status::IOError;
  {
    ::testing::InSequence dummy;
    base::RunLoop loop;
    base::Closure quit_closure = base::BarrierClosure(3, loop.QuitClosure());
    const url::Origin origin = url::Origin(GURL(kOrigin));

    EXPECT_CALL(*connection.connection_callbacks, Complete(kTransactionId))
        .Times(1)
        .WillOnce(RunClosure(quit_closure));
    EXPECT_CALL(
        *connection.open_callbacks,
        MockedSuccessDatabase(IsAssociatedInterfacePtrInfoValid(false), _))
        .Times(1)
        .WillOnce(RunClosure(quit_closure));

    connection.database->Commit(kTransactionId);
    idb_mojo_factory_->AbortTransactionsAndCompactDatabase(
        origin, base::BindOnce(&StatusCallback, std::move(quit_closure),
                               &callback_result));

    loop.Run();
  }
  EXPECT_EQ(::indexed_db::mojom::Status::OK, callback_result);
}

TEST_F(IndexedDBDispatcherHostTest, CompactDatabaseWhileDoingTransaction) {
  const int64_t kDBVersion = 1;
  const int64_t kTransactionId = 1;
  const int64_t kObjectStoreId = 10;
  const char kObjectStoreName[] = "os";

  // Open connection.
  TestDatabaseConnection connection(url::Origin(GURL(kOrigin)),
                                    base::UTF8ToUTF16(kDatabaseName),
                                    kDBVersion, kTransactionId);
  IndexedDBDatabaseMetadata metadata;
  DatabaseAssociatedPtrInfo database_info;
  {
    base::RunLoop loop;
    EXPECT_CALL(
        *connection.open_callbacks,
        MockedUpgradeNeeded(IsAssociatedInterfacePtrInfoValid(true),
                            IndexedDBDatabaseMetadata::NO_VERSION,
                            blink::kWebIDBDataLossNone, std::string(), _))
        .WillOnce(testing::DoAll(MoveArg<0>(&database_info),
                                 testing::SaveArg<4>(&metadata),
                                 RunClosure(loop.QuitClosure())));

    // Queue open request message.
    connection.Open(idb_mojo_factory_.get());
    loop.Run();
  }

  EXPECT_TRUE(database_info.is_valid());
  EXPECT_EQ(connection.version, metadata.version);
  EXPECT_EQ(connection.db_name, metadata.name);

  connection.database.Bind(std::move(database_info));

  ::indexed_db::mojom::Status callback_result =
      ::indexed_db::mojom::Status::IOError;
  {
    ::testing::InSequence dummy;
    base::RunLoop loop;
    base::Closure quit_closure = base::BarrierClosure(4, loop.QuitClosure());
    const url::Origin origin = url::Origin(GURL(kOrigin));

    EXPECT_CALL(
        *connection.connection_callbacks,
        Abort(kTransactionId, blink::kWebIDBDatabaseExceptionUnknownError, _))
        .Times(1)
        .WillOnce(RunClosure(quit_closure));
    EXPECT_CALL(*connection.open_callbacks,
                Error(blink::kWebIDBDatabaseExceptionAbortError, _))
        .Times(1)
        .WillOnce(RunClosure(quit_closure));
    EXPECT_CALL(*connection.connection_callbacks, ForcedClose())
        .Times(1)
        .WillOnce(RunClosure(quit_closure));

    ASSERT_TRUE(connection.database.is_bound());
    connection.database->CreateObjectStore(kTransactionId, kObjectStoreId,
                                           base::UTF8ToUTF16(kObjectStoreName),
                                           content::IndexedDBKeyPath(), false);
    idb_mojo_factory_->AbortTransactionsAndCompactDatabase(
        origin, base::BindOnce(&StatusCallback, std::move(quit_closure),
                               &callback_result));

    loop.Run();
  }
  EXPECT_EQ(::indexed_db::mojom::Status::OK, callback_result);
}

TEST_F(IndexedDBDispatcherHostTest, CompactDatabaseWhileUpgrading) {
  const int64_t kDBVersion = 1;
  const int64_t kTransactionId = 1;

  // Open connection.
  TestDatabaseConnection connection(url::Origin(GURL(kOrigin)),
                                    base::UTF8ToUTF16(kDatabaseName),
                                    kDBVersion, kTransactionId);
  IndexedDBDatabaseMetadata metadata;
  DatabaseAssociatedPtrInfo database_info;
  {
    base::RunLoop loop;
    EXPECT_CALL(
        *connection.open_callbacks,
        MockedUpgradeNeeded(IsAssociatedInterfacePtrInfoValid(true),
                            IndexedDBDatabaseMetadata::NO_VERSION,
                            blink::kWebIDBDataLossNone, std::string(), _))
        .WillOnce(testing::DoAll(MoveArg<0>(&database_info),
                                 testing::SaveArg<4>(&metadata),
                                 RunClosure(loop.QuitClosure())));

    // Queue open request message.
    connection.Open(idb_mojo_factory_.get());
    loop.Run();
  }

  EXPECT_TRUE(database_info.is_valid());
  EXPECT_EQ(connection.version, metadata.version);
  EXPECT_EQ(connection.db_name, metadata.name);

  connection.database.Bind(std::move(database_info));

  ::indexed_db::mojom::Status callback_result =
      ::indexed_db::mojom::Status::IOError;
  {
    ::testing::InSequence dummy;
    base::RunLoop loop;
    base::Closure quit_closure = base::BarrierClosure(4, loop.QuitClosure());
    const url::Origin origin = url::Origin(GURL(kOrigin));

    EXPECT_CALL(
        *connection.connection_callbacks,
        Abort(kTransactionId, blink::kWebIDBDatabaseExceptionUnknownError, _))
        .Times(1)
        .WillOnce(RunClosure(quit_closure));
    EXPECT_CALL(*connection.open_callbacks,
                Error(blink::kWebIDBDatabaseExceptionAbortError, _))
        .Times(1)
        .WillOnce(RunClosure(quit_closure));
    EXPECT_CALL(*connection.connection_callbacks, ForcedClose())
        .Times(1)
        .WillOnce(RunClosure(quit_closure));

    ASSERT_TRUE(connection.database.is_bound());
    idb_mojo_factory_->AbortTransactionsAndCompactDatabase(
        origin, base::BindOnce(&StatusCallback, std::move(quit_closure),
                               &callback_result));

    loop.Run();
  }
  EXPECT_EQ(::indexed_db::mojom::Status::OK, callback_result);
}

TEST_F(IndexedDBDispatcherHostTest,
       AbortTransactionsAfterCompletingTransaction) {
  const int64_t kDBVersion = 1;
  const int64_t kTransactionId = 1;

  // Open connection.
  TestDatabaseConnection connection(url::Origin(GURL(kOrigin)),
                                    base::UTF8ToUTF16(kDatabaseName),
                                    kDBVersion, kTransactionId);
  IndexedDBDatabaseMetadata metadata;
  DatabaseAssociatedPtrInfo database_info;
  {
    base::RunLoop loop;
    EXPECT_CALL(
        *connection.open_callbacks,
        MockedUpgradeNeeded(IsAssociatedInterfacePtrInfoValid(true),
                            IndexedDBDatabaseMetadata::NO_VERSION,
                            blink::kWebIDBDataLossNone, std::string(), _))
        .WillOnce(testing::DoAll(MoveArg<0>(&database_info),
                                 testing::SaveArg<4>(&metadata),
                                 RunClosure(loop.QuitClosure())));

    // Queue open request message.
    connection.Open(idb_mojo_factory_.get());
    loop.Run();
  }

  EXPECT_TRUE(database_info.is_valid());
  EXPECT_EQ(connection.version, metadata.version);
  EXPECT_EQ(connection.db_name, metadata.name);

  connection.database.Bind(std::move(database_info));

  ::indexed_db::mojom::Status callback_result =
      ::indexed_db::mojom::Status::IOError;
  {
    ::testing::InSequence dummy;
    base::RunLoop loop;
    base::Closure quit_closure = base::BarrierClosure(4, loop.QuitClosure());
    const url::Origin origin = url::Origin(GURL(kOrigin));

    EXPECT_CALL(*connection.connection_callbacks, Complete(kTransactionId))
        .Times(1)
        .WillOnce(RunClosure(quit_closure));
    EXPECT_CALL(
        *connection.open_callbacks,
        MockedSuccessDatabase(IsAssociatedInterfacePtrInfoValid(false), _))
        .Times(1)
        .WillOnce(RunClosure(quit_closure));
    EXPECT_CALL(*connection.connection_callbacks, ForcedClose())
        .Times(1)
        .WillOnce(RunClosure(quit_closure));

    connection.database->Commit(kTransactionId);
    idb_mojo_factory_->AbortTransactionsForDatabase(
        origin, base::BindOnce(&StatusCallback, std::move(quit_closure),
                               &callback_result));

    loop.Run();
  }
  EXPECT_EQ(::indexed_db::mojom::Status::OK, callback_result);
}

TEST_F(IndexedDBDispatcherHostTest, AbortTransactionsWhileDoingTransaction) {
  const int64_t kDBVersion = 1;
  const int64_t kTransactionId = 1;
  const int64_t kObjectStoreId = 10;
  const char kObjectStoreName[] = "os";

  // Open connection.
  TestDatabaseConnection connection(url::Origin(GURL(kOrigin)),
                                    base::UTF8ToUTF16(kDatabaseName),
                                    kDBVersion, kTransactionId);
  IndexedDBDatabaseMetadata metadata;
  DatabaseAssociatedPtrInfo database_info;
  {
    base::RunLoop loop;
    EXPECT_CALL(
        *connection.open_callbacks,
        MockedUpgradeNeeded(IsAssociatedInterfacePtrInfoValid(true),
                            IndexedDBDatabaseMetadata::NO_VERSION,
                            blink::kWebIDBDataLossNone, std::string(), _))
        .WillOnce(testing::DoAll(MoveArg<0>(&database_info),
                                 testing::SaveArg<4>(&metadata),
                                 RunClosure(loop.QuitClosure())));

    // Queue open request message.
    connection.Open(idb_mojo_factory_.get());
    loop.Run();
  }

  EXPECT_TRUE(database_info.is_valid());
  EXPECT_EQ(connection.version, metadata.version);
  EXPECT_EQ(connection.db_name, metadata.name);

  connection.database.Bind(std::move(database_info));

  ::indexed_db::mojom::Status callback_result =
      ::indexed_db::mojom::Status::IOError;
  {
    ::testing::InSequence dummy;
    base::RunLoop loop;
    base::Closure quit_closure = base::BarrierClosure(4, loop.QuitClosure());
    const url::Origin origin = url::Origin(GURL(kOrigin));

    EXPECT_CALL(
        *connection.connection_callbacks,
        Abort(kTransactionId, blink::kWebIDBDatabaseExceptionUnknownError, _))
        .Times(1)
        .WillOnce(RunClosure(quit_closure));
    EXPECT_CALL(*connection.open_callbacks,
                Error(blink::kWebIDBDatabaseExceptionAbortError, _))
        .Times(1)
        .WillOnce(RunClosure(quit_closure));
    EXPECT_CALL(*connection.connection_callbacks, ForcedClose())
        .Times(1)
        .WillOnce(RunClosure(quit_closure));

    ASSERT_TRUE(connection.database.is_bound());
    connection.database->CreateObjectStore(kTransactionId, kObjectStoreId,
                                           base::UTF8ToUTF16(kObjectStoreName),
                                           content::IndexedDBKeyPath(), false);
    idb_mojo_factory_->AbortTransactionsForDatabase(
        origin, base::BindOnce(&StatusCallback, std::move(quit_closure),
                               &callback_result));

    loop.Run();
  }
  EXPECT_EQ(::indexed_db::mojom::Status::OK, callback_result);
}

TEST_F(IndexedDBDispatcherHostTest, AbortTransactionsWhileUpgrading) {
  const int64_t kDBVersion = 1;
  const int64_t kTransactionId = 1;

  // Open connection.
  TestDatabaseConnection connection(url::Origin(GURL(kOrigin)),
                                    base::UTF8ToUTF16(kDatabaseName),
                                    kDBVersion, kTransactionId);
  IndexedDBDatabaseMetadata metadata;
  DatabaseAssociatedPtrInfo database_info;
  {
    base::RunLoop loop;
    EXPECT_CALL(
        *connection.open_callbacks,
        MockedUpgradeNeeded(IsAssociatedInterfacePtrInfoValid(true),
                            IndexedDBDatabaseMetadata::NO_VERSION,
                            blink::kWebIDBDataLossNone, std::string(), _))
        .WillOnce(testing::DoAll(MoveArg<0>(&database_info),
                                 testing::SaveArg<4>(&metadata),
                                 RunClosure(loop.QuitClosure())));

    // Queue open request message.
    connection.Open(idb_mojo_factory_.get());
    loop.Run();
  }

  EXPECT_TRUE(database_info.is_valid());
  EXPECT_EQ(connection.version, metadata.version);
  EXPECT_EQ(connection.db_name, metadata.name);

  connection.database.Bind(std::move(database_info));

  ::indexed_db::mojom::Status callback_result =
      ::indexed_db::mojom::Status::IOError;
  {
    ::testing::InSequence dummy;
    base::RunLoop loop;
    base::Closure quit_closure = base::BarrierClosure(4, loop.QuitClosure());
    const url::Origin origin = url::Origin(GURL(kOrigin));

    EXPECT_CALL(
        *connection.connection_callbacks,
        Abort(kTransactionId, blink::kWebIDBDatabaseExceptionUnknownError, _))
        .Times(1)
        .WillOnce(RunClosure(quit_closure));
    EXPECT_CALL(*connection.open_callbacks,
                Error(blink::kWebIDBDatabaseExceptionAbortError, _))
        .Times(1)
        .WillOnce(RunClosure(quit_closure));
    EXPECT_CALL(*connection.connection_callbacks, ForcedClose())
        .Times(1)
        .WillOnce(RunClosure(quit_closure));

    ASSERT_TRUE(connection.database.is_bound());
    idb_mojo_factory_->AbortTransactionsForDatabase(
        origin, base::BindOnce(&StatusCallback, std::move(quit_closure),
                               &callback_result));

    loop.Run();
  }
  EXPECT_EQ(::indexed_db::mojom::Status::OK, callback_result);
}

}  // namespace content
