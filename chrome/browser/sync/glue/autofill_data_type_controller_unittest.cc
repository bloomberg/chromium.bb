// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_timeouts.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/sync/glue/autofill_data_type_controller.h"
#include "chrome/browser/sync/glue/data_type_controller_mock.h"
#include "chrome/browser/sync/profile_sync_components_factory_mock.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/test/base/profile_mock.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"

using base::WaitableEvent;
using browser_sync::AutofillDataTypeController;
using browser_sync::DataTypeController;
using browser_sync::StartCallback;
using content::BrowserThread;
using testing::_;
using testing::DoAll;
using testing::Invoke;
using testing::Return;
using testing::SetArgumentPointee;

namespace {

ACTION_P(WaitOnEvent, event) {
  event->Wait();
}

ACTION_P(SignalEvent, event) {
  event->Signal();
}

ACTION_P(GetAutocompleteSyncComponents, wds) {
  return base::WeakPtr<SyncableService>();
}

ACTION_P(RaiseSignal, data_controller_mock) {
  data_controller_mock->RaiseStartSignal();
}

// This class mocks PersonalDataManager and provides a factory method to
// serve back the mocked object to callers of
// |PersonalDataManagerFactory::GetForProfile()|.
class PersonalDataManagerMock : public PersonalDataManager {
 public:
  PersonalDataManagerMock() : PersonalDataManager() {}
  virtual ~PersonalDataManagerMock() {}

  static ProfileKeyedService* Build(Profile* profile) {
    return new PersonalDataManagerMock;
  }

  MOCK_CONST_METHOD0(IsDataLoaded, bool());

 private:
  DISALLOW_COPY_AND_ASSIGN(PersonalDataManagerMock);
};

class WebDataServiceFake : public WebDataService {
 public:
  explicit WebDataServiceFake() {}

  MOCK_METHOD0(IsDatabaseLoaded, bool());

 private:
  DISALLOW_COPY_AND_ASSIGN(WebDataServiceFake);
};

class AutofillDataTypeControllerMock : public AutofillDataTypeController {
 public:
  AutofillDataTypeControllerMock(
      ProfileSyncComponentsFactory* profile_sync_factory,
      Profile* profile)
      : AutofillDataTypeController(profile_sync_factory, profile),
        start_called_(false, false) {}

  MOCK_METHOD0(StartAssociation, void());

  void RaiseStartSignal() {
    start_called_.Signal();
  }

  void WaitUntilStartCalled() {
    start_called_.Wait();
  }

 private:
  friend class AutofillDataTypeControllerTest;
  FRIEND_TEST_ALL_PREFIXES(AutofillDataTypeControllerTest, StartWDSReady);
  FRIEND_TEST_ALL_PREFIXES(AutofillDataTypeControllerTest, StartWDSNotReady);

  WaitableEvent start_called_;

  DISALLOW_COPY_AND_ASSIGN(AutofillDataTypeControllerMock);
};

class AutofillDataTypeControllerTest : public testing::Test {
 public:
  AutofillDataTypeControllerTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        db_thread_(BrowserThread::DB) {}

  virtual void SetUp() {
    db_thread_.Start();
    db_notification_service_ = new ThreadNotificationService(
        db_thread_.DeprecatedGetThreadObject());
    db_notification_service_->Init();
    web_data_service_ = new WebDataServiceFake();
    EXPECT_CALL(profile_, GetProfileSyncService()).WillRepeatedly(
        Return(&service_));
    autofill_dtc_ =
        new AutofillDataTypeControllerMock(&profile_sync_factory_, &profile_);
    EXPECT_CALL(profile_, GetWebDataService(_)).
        WillRepeatedly(Return(web_data_service_.get()));
  }

  virtual void TearDown() {
    db_notification_service_->TearDown();
    db_thread_.Stop();
  }

 protected:
  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread db_thread_;
  scoped_refptr<ThreadNotificationService> db_notification_service_;
  scoped_refptr<AutofillDataTypeControllerMock> autofill_dtc_;
  ProfileSyncComponentsFactoryMock profile_sync_factory_;
  ProfileMock profile_;
  ProfileSyncServiceMock service_;
  scoped_refptr<WebDataServiceFake> web_data_service_;
};

TEST_F(AutofillDataTypeControllerTest, StartWDSReady) {
  EXPECT_CALL(*(web_data_service_.get()), IsDatabaseLoaded()).
      WillOnce(Return(true));
  EXPECT_CALL(*(autofill_dtc_.get()), StartAssociation()).Times(0);

  autofill_dtc_->set_state(DataTypeController::MODEL_STARTING);
  EXPECT_TRUE(autofill_dtc_->StartModels());
}

TEST_F(AutofillDataTypeControllerTest, StartWDSNotReady) {
  EXPECT_CALL(*(web_data_service_.get()), IsDatabaseLoaded()).
      WillOnce(Return(false));
  EXPECT_CALL(*(autofill_dtc_.get()), StartAssociation()).
      WillOnce(RaiseSignal(autofill_dtc_.get()));

  autofill_dtc_->set_state(DataTypeController::MODEL_STARTING);
  EXPECT_FALSE(autofill_dtc_->StartModels());

  scoped_refptr<ThreadNotifier> notifier(new ThreadNotifier(
      ui_thread_.DeprecatedGetThreadObject()));
  autofill_dtc_->Observe(
      chrome::NOTIFICATION_WEB_DATABASE_LOADED,
      content::Source<WebDataService>(web_data_service_.get()),
      content::NotificationService::NoDetails());
  autofill_dtc_->WaitUntilStartCalled();
  EXPECT_EQ(DataTypeController::ASSOCIATING, autofill_dtc_->state());
}

}  // namespace
