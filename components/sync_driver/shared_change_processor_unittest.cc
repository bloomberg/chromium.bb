// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/shared_change_processor.h"

#include <cstddef>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread.h"
#include "components/sync_driver/data_type_error_handler_mock.h"
#include "components/sync_driver/generic_change_processor.h"
#include "components/sync_driver/generic_change_processor_factory.h"
#include "components/sync_driver/sync_api_component_factory.h"
#include "sync/api/fake_syncable_service.h"
#include "sync/internal_api/public/attachments/attachment_service_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_driver {

namespace {

using ::testing::NiceMock;
using ::testing::StrictMock;

class SyncSharedChangeProcessorTest :
    public testing::Test,
    public SyncApiComponentFactory {
 public:
  SyncSharedChangeProcessorTest() : backend_thread_("dbthread"),
                                    did_connect_(false) {}

  virtual ~SyncSharedChangeProcessorTest() {
    EXPECT_FALSE(db_syncable_service_.get());
  }

  virtual base::WeakPtr<syncer::SyncableService> GetSyncableServiceForType(
      syncer::ModelType type) OVERRIDE {
    return db_syncable_service_->AsWeakPtr();
  }

  virtual scoped_ptr<syncer::AttachmentService> CreateAttachmentService(
      const scoped_refptr<syncer::AttachmentStore>& attachment_store,
      const syncer::UserShare& user_share,
      syncer::AttachmentService::Delegate* delegate) OVERRIDE {
    return syncer::AttachmentServiceImpl::CreateForTest();
  }

 protected:
  virtual void SetUp() OVERRIDE {
    shared_change_processor_ = new SharedChangeProcessor();
    ASSERT_TRUE(backend_thread_.Start());
    ASSERT_TRUE(backend_thread_.message_loop_proxy()->PostTask(
        FROM_HERE,
        base::Bind(&SyncSharedChangeProcessorTest::SetUpDBSyncableService,
                   base::Unretained(this))));
  }

  virtual void TearDown() OVERRIDE {
    EXPECT_TRUE(backend_thread_.message_loop_proxy()->PostTask(
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
  }

  // Connect |shared_change_processor_| on the DB thread.
  void Connect() {
    EXPECT_TRUE(backend_thread_.message_loop_proxy()->PostTask(
        FROM_HERE,
        base::Bind(&SyncSharedChangeProcessorTest::ConnectOnDBThread,
                   base::Unretained(this),
                   shared_change_processor_)));
  }

 private:
  // Used by SetUp().
  void SetUpDBSyncableService() {
    DCHECK(backend_thread_.message_loop_proxy()->BelongsToCurrentThread());
    DCHECK(!db_syncable_service_.get());
    db_syncable_service_.reset(new syncer::FakeSyncableService());
  }

  // Used by TearDown().
  void TearDownDBSyncableService() {
    DCHECK(backend_thread_.message_loop_proxy()->BelongsToCurrentThread());
    DCHECK(db_syncable_service_.get());
    db_syncable_service_.reset();
  }

  // Used by Connect().  The SharedChangeProcessor is passed in
  // because we modify |shared_change_processor_| on the main thread
  // (in TearDown()).
  void ConnectOnDBThread(
      const scoped_refptr<SharedChangeProcessor>& shared_change_processor) {
    DCHECK(backend_thread_.message_loop_proxy()->BelongsToCurrentThread());
    syncer::UserShare share;
    EXPECT_TRUE(shared_change_processor->Connect(
        this,
        &processor_factory_,
        &share,
        &error_handler_,
        syncer::AUTOFILL,
        base::WeakPtr<syncer::SyncMergeResult>()));
    did_connect_ = true;
  }

  base::MessageLoop frontend_loop_;
  base::Thread backend_thread_;

  scoped_refptr<SharedChangeProcessor> shared_change_processor_;
  StrictMock<DataTypeErrorHandlerMock> error_handler_;

  GenericChangeProcessorFactory processor_factory_;
  bool did_connect_;

  // Used only on DB thread.
  scoped_ptr<syncer::FakeSyncableService> db_syncable_service_;
};

// Simply connect the shared change processor.  It should succeed, and
// nothing further should happen.
TEST_F(SyncSharedChangeProcessorTest, Basic) {
  Connect();
}

}  // namespace

}  // namespace sync_driver
