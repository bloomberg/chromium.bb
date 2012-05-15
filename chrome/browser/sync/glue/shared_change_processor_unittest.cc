// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/shared_change_processor.h"

#include <cstddef>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "chrome/browser/sync/glue/data_type_error_handler_mock.h"
#include "chrome/browser/sync/profile_sync_components_factory_impl.h"
#include "chrome/browser/sync/profile_sync_components_factory_mock.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "content/test/test_browser_thread.h"
#include "sync/api/fake_syncable_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {

namespace {

using content::BrowserThread;
using ::testing::NiceMock;
using ::testing::StrictMock;

ACTION_P(GetWeakPtrToSyncableService, syncable_service) {
  // Have to do this within an Action to ensure it's not evaluated on the wrong
  // thread.
  return syncable_service->AsWeakPtr();
}

class SyncSharedChangeProcessorTest : public testing::Test {
 public:
  SyncSharedChangeProcessorTest()
      : ui_thread_(BrowserThread::UI, &ui_loop_),
        db_thread_(BrowserThread::DB),
        db_syncable_service_(NULL) {}

  virtual ~SyncSharedChangeProcessorTest() {
    EXPECT_FALSE(db_syncable_service_.get());
  }

 protected:
  virtual void SetUp() OVERRIDE {
    shared_change_processor_ = new SharedChangeProcessor();
    db_thread_.Start();
    EXPECT_TRUE(BrowserThread::PostTask(
        BrowserThread::DB,
        FROM_HERE,
        base::Bind(&SyncSharedChangeProcessorTest::SetUpDBSyncableService,
                   base::Unretained(this))));
  }

  virtual void TearDown() OVERRIDE {
    EXPECT_TRUE(BrowserThread::PostTask(
        BrowserThread::DB,
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
    db_thread_.Stop();
  }

  // Connect |shared_change_processor_| on the DB thread.
  void Connect() {
    EXPECT_TRUE(BrowserThread::PostTask(
        BrowserThread::DB,
        FROM_HERE,
        base::Bind(&SyncSharedChangeProcessorTest::ConnectOnDBThread,
                   base::Unretained(this),
                   shared_change_processor_)));
  }

 private:
  // Used by SetUp().
  void SetUpDBSyncableService() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
    DCHECK(!db_syncable_service_.get());
    db_syncable_service_.reset(new FakeSyncableService());
  }

  // Used by TearDown().
  void TearDownDBSyncableService() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
    DCHECK(db_syncable_service_.get());
    db_syncable_service_.reset();
  }

  // Used by Connect().  The SharedChangeProcessor is passed in
  // because we modify |shared_change_processor_| on the main thread
  // (in TearDown()).
  void ConnectOnDBThread(
      const scoped_refptr<SharedChangeProcessor>& shared_change_processor) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
    EXPECT_CALL(sync_factory_, GetSyncableServiceForType(syncable::AUTOFILL)).
        WillOnce(GetWeakPtrToSyncableService(db_syncable_service_.get()));
    EXPECT_TRUE(shared_change_processor->Connect(&sync_factory_,
                                                 &sync_service_,
                                                 &error_handler_,
                                                 syncable::AUTOFILL));
  }

  MessageLoopForUI ui_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread db_thread_;

  scoped_refptr<SharedChangeProcessor> shared_change_processor_;
  NiceMock<ProfileSyncComponentsFactoryMock> sync_factory_;
  NiceMock<ProfileSyncServiceMock> sync_service_;
  StrictMock<DataTypeErrorHandlerMock> error_handler_;

  // Used only on DB thread.
  scoped_ptr<FakeSyncableService> db_syncable_service_;
};

// Simply connect the shared change processor.  It should succeed, and
// nothing further should happen.
TEST_F(SyncSharedChangeProcessorTest, Basic) {
  Connect();
}

}  // namespace

}  // namespace browser_sync
