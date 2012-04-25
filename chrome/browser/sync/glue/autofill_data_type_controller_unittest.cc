// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "chrome/browser/sync/api/sync_error.h"
#include "chrome/browser/sync/glue/autofill_data_type_controller.h"
#include "chrome/browser/sync/glue/data_type_controller_mock.h"
#include "chrome/browser/sync/glue/shared_change_processor_mock.h"
#include "chrome/browser/sync/profile_sync_components_factory_mock.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/test/base/profile_mock.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {

namespace {

using content::BrowserThread;
using testing::_;
using testing::NiceMock;
using testing::Return;

// Fake WebDataService implementation that stubs out the database
// loading.
class FakeWebDataService : public WebDataService {
 public:
  FakeWebDataService() : is_database_loaded_(false) {}

  // Mark the database as loaded and send out the appropriate
  // notification.
  void LoadDatabase() {
    is_database_loaded_ = true;
    // TODO(akalin): Expose WDS::NotifyDatabaseLoadedOnUIThread() and
    // use that instead of sending this notification manually.
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_WEB_DATABASE_LOADED,
        content::Source<WebDataService>(this),
        content::NotificationService::NoDetails());
  }

  virtual bool IsDatabaseLoaded() OVERRIDE {
    return is_database_loaded_;
  }

 private:
  virtual ~FakeWebDataService() {}

  bool is_database_loaded_;

  DISALLOW_COPY_AND_ASSIGN(FakeWebDataService);
};

class SyncAutofillDataTypeControllerTest : public testing::Test {
 public:
  SyncAutofillDataTypeControllerTest()
      : weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
        ui_thread_(BrowserThread::UI, &message_loop_),
        last_start_result_(DataTypeController::OK) {}

  virtual ~SyncAutofillDataTypeControllerTest() {}

  // We deliberately do not set up a DB thread so that we always stop
  // with an association failure.

  virtual void SetUp() {
    change_processor_ = new NiceMock<SharedChangeProcessorMock>();

    EXPECT_CALL(profile_sync_factory_,
                CreateSharedChangeProcessor()).
        WillRepeatedly(Return(change_processor_.get()));

    web_data_service_ = new FakeWebDataService();

    EXPECT_CALL(profile_, GetWebDataService(_)).
        WillRepeatedly(Return(web_data_service_.get()));

    autofill_dtc_ =
        new AutofillDataTypeController(&profile_sync_factory_,
                                       &profile_,
                                       &service_);
  }

  // Passed to AutofillDTC::Start().
  void OnStartFinished(DataTypeController::StartResult result,
                       const SyncError& error) {
    last_start_result_ = result;
    last_start_error_ = error;
  }

  virtual void TearDown() {
    autofill_dtc_ = NULL;
    web_data_service_ = NULL;
    change_processor_ = NULL;
  }

 protected:
  base::WeakPtrFactory<SyncAutofillDataTypeControllerTest> weak_ptr_factory_;
  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;

  scoped_refptr<NiceMock<SharedChangeProcessorMock> > change_processor_;
  ProfileSyncComponentsFactoryMock profile_sync_factory_;
  ProfileSyncServiceMock service_;
  ProfileMock profile_;
  scoped_refptr<FakeWebDataService> web_data_service_;
  scoped_refptr<AutofillDataTypeController> autofill_dtc_;

  // Stores arguments of most recent call of OnStartFinished().
  DataTypeController::StartResult last_start_result_;
  SyncError last_start_error_;
};

// Load the WDS's database, then start the Autofill DTC.  It should
// immediately try to start association and fail (due to missing DB
// thread).
TEST_F(SyncAutofillDataTypeControllerTest, StartWDSReady) {
  web_data_service_->LoadDatabase();
  autofill_dtc_->Start(
      base::Bind(&SyncAutofillDataTypeControllerTest::OnStartFinished,
                 weak_ptr_factory_.GetWeakPtr()));

  EXPECT_EQ(DataTypeController::ASSOCIATION_FAILED, last_start_result_);
  EXPECT_TRUE(last_start_error_.IsSet());
  EXPECT_EQ(DataTypeController::NOT_RUNNING, autofill_dtc_->state());
}

// Start the autofill DTC without the WDS's database loaded, then
// start the DB.  The Autofill DTC should be in the MODEL_STARTING
// state until the database in loaded, when it should try to start
// association and fail (due to missing DB thread).
TEST_F(SyncAutofillDataTypeControllerTest, StartWDSNotReady) {
  autofill_dtc_->Start(
      base::Bind(&SyncAutofillDataTypeControllerTest::OnStartFinished,
                 weak_ptr_factory_.GetWeakPtr()));

  EXPECT_EQ(DataTypeController::OK, last_start_result_);
  EXPECT_FALSE(last_start_error_.IsSet());
  EXPECT_EQ(DataTypeController::MODEL_STARTING, autofill_dtc_->state());

  web_data_service_->LoadDatabase();

  EXPECT_EQ(DataTypeController::ASSOCIATION_FAILED, last_start_result_);
  EXPECT_TRUE(last_start_error_.IsSet());
  // There's a TODO for
  // NonFrontendDataTypeController::StartAssociationAsync() that, when
  // done, will make this consistent with the previous test.
  EXPECT_EQ(DataTypeController::DISABLED, autofill_dtc_->state());
}

}  // namespace

}  // namespace browser_sync
