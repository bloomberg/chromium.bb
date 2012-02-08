// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/shared_change_processor.h"

#include <cstddef>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "chrome/browser/sync/api/syncable_service_mock.h"
#include "chrome/browser/sync/profile_sync_components_factory_impl.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "content/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {

namespace {

using content::BrowserThread;
using ::testing::NiceMock;

class SyncSharedChangeProcessorTest : public testing::Test {
 public:
  SyncSharedChangeProcessorTest()
      : ui_thread_(BrowserThread::UI, &ui_loop_),
        db_thread_(BrowserThread::DB),
        sync_factory_(NULL, NULL),
        db_syncable_service_(NULL) {}

  virtual ~SyncSharedChangeProcessorTest() {
    EXPECT_FALSE(db_syncable_service_);
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
    db_thread_.Stop();
    shared_change_processor_ = NULL;
  }

  // Connect |shared_change_processor_| on the DB thread.
  void Connect() {
    EXPECT_TRUE(BrowserThread::PostTask(
        BrowserThread::DB,
        FROM_HERE,
        base::Bind(&SyncSharedChangeProcessorTest::ConnectOnDBThread,
                   base::Unretained(this))));
  }

 private:
  // Used by SetUp().
  void SetUpDBSyncableService() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
    DCHECK(!db_syncable_service_);
    db_syncable_service_ = new NiceMock<SyncableServiceMock>();
  }

  // Used by TearDown().
  void TearDownDBSyncableService() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
    DCHECK(db_syncable_service_);
    delete db_syncable_service_;
    db_syncable_service_ = NULL;
  }

  // Used by Connect().
  void ConnectOnDBThread() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
    EXPECT_TRUE(
        shared_change_processor_->Connect(&sync_factory_,
                                          &sync_service_,
                                          &sync_service_,
                                          db_syncable_service_->AsWeakPtr()));
  }

  MessageLoopForUI ui_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread db_thread_;

  scoped_refptr<SharedChangeProcessor> shared_change_processor_;
  ProfileSyncComponentsFactoryImpl sync_factory_;
  NiceMock<ProfileSyncServiceMock> sync_service_;

  // Used only on DB thread.
  NiceMock<SyncableServiceMock>* db_syncable_service_;
};

// Simply connect the shared change processor.  It should succeed, and
// nothing further should happen.
TEST_F(SyncSharedChangeProcessorTest, Basic) {
  Connect();
}

}  // namespace

}  // namespace browser_sync
