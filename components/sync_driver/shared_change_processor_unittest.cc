// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/shared_change_processor.h"

#include <cstddef>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "components/sync_driver/data_type_error_handler_mock.h"
#include "components/sync_driver/fake_sync_client.h"
#include "components/sync_driver/generic_change_processor.h"
#include "components/sync_driver/generic_change_processor_factory.h"
#include "components/sync_driver/local_device_info_provider.h"
#include "components/sync_driver/sync_api_component_factory.h"
#include "sync/api/attachments/attachment_id.h"
#include "sync/api/attachments/attachment_store.h"
#include "sync/api/fake_syncable_service.h"
#include "sync/internal_api/public/attachments/attachment_service_impl.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/test/test_user_share.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_driver {

namespace {

using ::testing::NiceMock;
using ::testing::StrictMock;

class TestSyncApiComponentFactory : public SyncApiComponentFactory {
 public:
  TestSyncApiComponentFactory() {}
  ~TestSyncApiComponentFactory() override {}

  // SyncApiComponentFactory implementation.
  void RegisterDataTypes(
      sync_driver::SyncService* sync_service,
      const RegisterDataTypesMethod& register_platform_types_method) override {}
  sync_driver::DataTypeManager* CreateDataTypeManager(
      const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>&
          debug_info_listener,
      const sync_driver::DataTypeController::TypeMap* controllers,
      const sync_driver::DataTypeEncryptionHandler* encryption_handler,
      browser_sync::SyncBackendHost* backend,
      sync_driver::DataTypeManagerObserver* observer) override {
    return nullptr;
  }
  browser_sync::SyncBackendHost* CreateSyncBackendHost(
      const std::string& name,
      invalidation::InvalidationService* invalidator,
      const base::WeakPtr<sync_driver::SyncPrefs>& sync_prefs,
      const base::FilePath& sync_folder) override {
    return nullptr;
  }
  std::unique_ptr<sync_driver::LocalDeviceInfoProvider>
  CreateLocalDeviceInfoProvider() override {
    return nullptr;
  }
  SyncApiComponentFactory::SyncComponents CreateBookmarkSyncComponents(
      sync_driver::SyncService* sync_service,
      sync_driver::DataTypeErrorHandler* error_handler) override {
    return SyncApiComponentFactory::SyncComponents(nullptr, nullptr);
  }
  std::unique_ptr<syncer::AttachmentService> CreateAttachmentService(
      std::unique_ptr<syncer::AttachmentStoreForSync> attachment_store,
      const syncer::UserShare& user_share,
      const std::string& store_birthday,
      syncer::ModelType model_type,
      syncer::AttachmentService::Delegate* delegate) override {
    return syncer::AttachmentServiceImpl::CreateForTest();
  }
};

class SyncSharedChangeProcessorTest :
    public testing::Test,
    public FakeSyncClient {
 public:
  SyncSharedChangeProcessorTest()
      : FakeSyncClient(&factory_),
        backend_thread_("dbthread"),
        did_connect_(false),
        has_attachment_service_(false) {}

  ~SyncSharedChangeProcessorTest() override {
    EXPECT_FALSE(db_syncable_service_.get());
  }

  // FakeSyncClient override.
  base::WeakPtr<syncer::SyncableService> GetSyncableServiceForType(
      syncer::ModelType type) override {
    return db_syncable_service_->AsWeakPtr();
  }

 protected:
  void SetUp() override {
    test_user_share_.SetUp();
    shared_change_processor_ = new SharedChangeProcessor();
    ASSERT_TRUE(backend_thread_.Start());
    ASSERT_TRUE(backend_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&SyncSharedChangeProcessorTest::SetUpDBSyncableService,
                   base::Unretained(this))));
  }

  void TearDown() override {
    EXPECT_TRUE(backend_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&SyncSharedChangeProcessorTest::TearDownDBSyncableService,
                   base::Unretained(this))));
    // This must happen before the DB thread is stopped since
    // |shared_change_processor_| may post tasks to delete its members
    // on the correct thread.
    //
    // TODO(akalin): Write deterministic tests for the destruction of
    // |shared_change_processor_| on the UI and DB threads.
    shared_change_processor_ = NULL;
    backend_thread_.Stop();

    // Note: Stop() joins the threads, and that barrier prevents this read
    // from being moved (e.g by compiler optimization) in such a way that it
    // would race with the write in ConnectOnDBThread (because by this time,
    // everything that could have run on |backend_thread_| has done so).
    ASSERT_TRUE(did_connect_);
    test_user_share_.TearDown();
  }

  // Connect |shared_change_processor_| on the DB thread.
  void Connect() {
    EXPECT_TRUE(backend_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&SyncSharedChangeProcessorTest::ConnectOnDBThread,
                   base::Unretained(this), shared_change_processor_)));
  }

  void SetAttachmentStore() {
    EXPECT_TRUE(backend_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&SyncSharedChangeProcessorTest::SetAttachmentStoreOnDBThread,
                   base::Unretained(this))));
  }

  bool HasAttachmentService() {
    base::WaitableEvent event(false, false);
    EXPECT_TRUE(backend_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::Bind(
            &SyncSharedChangeProcessorTest::CheckAttachmentServiceOnDBThread,
            base::Unretained(this), base::Unretained(&event))));
    event.Wait();
    return has_attachment_service_;
  }

 private:
  // Used by SetUp().
  void SetUpDBSyncableService() {
    DCHECK(backend_thread_.task_runner()->BelongsToCurrentThread());
    DCHECK(!db_syncable_service_.get());
    db_syncable_service_.reset(new syncer::FakeSyncableService());
  }

  // Used by TearDown().
  void TearDownDBSyncableService() {
    DCHECK(backend_thread_.task_runner()->BelongsToCurrentThread());
    DCHECK(db_syncable_service_.get());
    db_syncable_service_.reset();
  }

  void SetAttachmentStoreOnDBThread() {
    DCHECK(backend_thread_.task_runner()->BelongsToCurrentThread());
    DCHECK(db_syncable_service_.get());
    db_syncable_service_->set_attachment_store(
        syncer::AttachmentStore::CreateInMemoryStore());
  }

  // Used by Connect().  The SharedChangeProcessor is passed in
  // because we modify |shared_change_processor_| on the main thread
  // (in TearDown()).
  void ConnectOnDBThread(
      const scoped_refptr<SharedChangeProcessor>& shared_change_processor) {
    DCHECK(backend_thread_.task_runner()->BelongsToCurrentThread());
    EXPECT_TRUE(shared_change_processor->Connect(
        this, &processor_factory_, test_user_share_.user_share(),
        &error_handler_, syncer::AUTOFILL,
        base::WeakPtr<syncer::SyncMergeResult>()));
    did_connect_ = true;
  }

  void CheckAttachmentServiceOnDBThread(base::WaitableEvent* event) {
    DCHECK(backend_thread_.task_runner()->BelongsToCurrentThread());
    DCHECK(db_syncable_service_.get());
    has_attachment_service_ = !!db_syncable_service_->attachment_service();
    event->Signal();
  }

  base::MessageLoop frontend_loop_;
  base::Thread backend_thread_;
  syncer::TestUserShare test_user_share_;
  TestSyncApiComponentFactory factory_;

  scoped_refptr<SharedChangeProcessor> shared_change_processor_;
  StrictMock<DataTypeErrorHandlerMock> error_handler_;

  GenericChangeProcessorFactory processor_factory_;
  bool did_connect_;
  bool has_attachment_service_;

  // Used only on DB thread.
  std::unique_ptr<syncer::FakeSyncableService> db_syncable_service_;
};

// Simply connect the shared change processor.  It should succeed, and
// nothing further should happen.
TEST_F(SyncSharedChangeProcessorTest, Basic) {
  Connect();
}

// Connect the shared change processor to a syncable service with
// AttachmentStore. Verify that shared change processor implementation
// creates AttachmentService and passes it back to the syncable service.
TEST_F(SyncSharedChangeProcessorTest, ConnectWithAttachmentStore) {
  SetAttachmentStore();
  Connect();
  EXPECT_TRUE(HasAttachmentService());
}

}  // namespace

}  // namespace sync_driver
