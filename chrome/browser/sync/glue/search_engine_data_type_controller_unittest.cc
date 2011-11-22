// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/task.h"
#include "base/tracked_objects.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_test_util.h"
#include "chrome/browser/sync/glue/change_processor_mock.h"
#include "chrome/browser/sync/glue/data_type_controller_mock.h"
#include "chrome/browser/sync/glue/model_associator_mock.h"
#include "chrome/browser/sync/glue/search_engine_data_type_controller.h"
#include "chrome/browser/sync/profile_sync_components_factory_mock.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/test/base/profile_mock.h"
#include "content/public/browser/notification_service.h"
#include "testing/gtest/include/gtest/gtest.h"

using browser_sync::ChangeProcessorMock;
using browser_sync::DataTypeController;
using browser_sync::ModelAssociatorMock;
using browser_sync::SearchEngineDataTypeController;
using browser_sync::StartCallback;
using testing::_;
using testing::DoAll;
using testing::InvokeWithoutArgs;
using testing::Return;
using testing::SetArgumentPointee;

class SearchEngineDataTypeControllerTest : public testing::Test {
 public:
  SearchEngineDataTypeControllerTest() {}

  virtual void SetUp() {
    test_util_.SetUp();
    model_associator_ = new ModelAssociatorMock();
    change_processor_ = new ChangeProcessorMock();
    profile_sync_factory_.reset(
        new ProfileSyncComponentsFactoryMock(model_associator_,
                                   change_processor_));
    // Feed the DTC test_util_'s profile so it is reused later.
    // This allows us to control the associated TemplateURLService.
    search_engine_dtc_ =
        new SearchEngineDataTypeController(profile_sync_factory_.get(),
                                           test_util_.profile(),
                                           &service_);
  }

  virtual void TearDown() {
    test_util_.TearDown();
  }

 protected:
  // Pre-emptively get the TURL Service and make sure it is loaded.
  void PreloadTemplateURLService() {
    test_util_.VerifyLoad();
  }

  void SetAssociateExpectations() {
    EXPECT_CALL(*model_associator_, CryptoReadyIfNecessary()).
        WillRepeatedly(Return(true));
    EXPECT_CALL(*profile_sync_factory_, CreateSearchEngineSyncComponents(_, _));
    EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
        WillRepeatedly(DoAll(SetArgumentPointee<0>(true), Return(true)));
    EXPECT_CALL(*model_associator_, AssociateModels(_)).
        WillRepeatedly(Return(true));
    EXPECT_CALL(service_, ActivateDataType(_, _, _));
  }

  void SetStopExpectations() {
    EXPECT_CALL(service_, DeactivateDataType(_));
    EXPECT_CALL(*model_associator_, DisassociateModels(_));
  }

  // This also manages a BrowserThread and MessageLoop for us. Note that this
  // must be declared here as the destruction order of the BrowserThread
  // matters - we could leak if this is declared below.
  TemplateURLServiceTestUtil test_util_;
  scoped_refptr<SearchEngineDataTypeController> search_engine_dtc_;
  scoped_ptr<ProfileSyncComponentsFactoryMock> profile_sync_factory_;
  ProfileSyncServiceMock service_;
  ModelAssociatorMock* model_associator_;
  ChangeProcessorMock* change_processor_;
  StartCallback start_callback_;
};

TEST_F(SearchEngineDataTypeControllerTest, StartURLServiceReady) {
  // We want to start ready.
  PreloadTemplateURLService();
  SetAssociateExpectations();

  EXPECT_EQ(DataTypeController::NOT_RUNNING, search_engine_dtc_->state());

  EXPECT_CALL(start_callback_, Run(DataTypeController::OK, _));
  search_engine_dtc_->Start(NewCallback(&start_callback_, &StartCallback::Run));
  EXPECT_EQ(DataTypeController::RUNNING, search_engine_dtc_->state());
}

TEST_F(SearchEngineDataTypeControllerTest, StartURLServiceNotReady) {
  SetAssociateExpectations();

  EXPECT_CALL(start_callback_, Run(DataTypeController::OK, _));
  search_engine_dtc_->Start(NewCallback(&start_callback_, &StartCallback::Run));
  EXPECT_EQ(DataTypeController::MODEL_STARTING, search_engine_dtc_->state());

  // Send the notification that the TemplateURLService has started.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_TEMPLATE_URL_SERVICE_LOADED,
      content::Source<TemplateURLService>(test_util_.model()),
      content::NotificationService::NoDetails());
  EXPECT_EQ(DataTypeController::RUNNING, search_engine_dtc_->state());
}

TEST_F(SearchEngineDataTypeControllerTest, StartFirstRun) {
  PreloadTemplateURLService();
  SetAssociateExpectations();
  EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
      WillRepeatedly(DoAll(SetArgumentPointee<0>(false), Return(true)));
  EXPECT_CALL(start_callback_, Run(DataTypeController::OK_FIRST_RUN, _));
  search_engine_dtc_->Start(NewCallback(&start_callback_, &StartCallback::Run));
}

TEST_F(SearchEngineDataTypeControllerTest, StartOk) {
  PreloadTemplateURLService();
  SetAssociateExpectations();
  EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
      WillRepeatedly(DoAll(SetArgumentPointee<0>(true), Return(true)));

  EXPECT_CALL(start_callback_, Run(DataTypeController::OK, _));
  search_engine_dtc_->Start(NewCallback(&start_callback_, &StartCallback::Run));
}

TEST_F(SearchEngineDataTypeControllerTest, StartAssociationFailed) {
  PreloadTemplateURLService();
  EXPECT_CALL(*profile_sync_factory_, CreateSearchEngineSyncComponents(_, _));
  EXPECT_CALL(*model_associator_, CryptoReadyIfNecessary()).
      WillRepeatedly(Return(true));
  EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
      WillRepeatedly(DoAll(SetArgumentPointee<0>(true), Return(true)));
  EXPECT_CALL(*model_associator_, AssociateModels(_)).
      WillRepeatedly(DoAll(browser_sync::SetSyncError(syncable::SEARCH_ENGINES),
                           Return(false)));

  EXPECT_CALL(start_callback_, Run(DataTypeController::ASSOCIATION_FAILED, _));
  search_engine_dtc_->Start(NewCallback(&start_callback_, &StartCallback::Run));
  EXPECT_EQ(DataTypeController::DISABLED, search_engine_dtc_->state());
}

TEST_F(SearchEngineDataTypeControllerTest,
       StartAssociationTriggersUnrecoverableError) {
  PreloadTemplateURLService();
  // Set up association to fail with an unrecoverable error.
  EXPECT_CALL(*profile_sync_factory_, CreateSearchEngineSyncComponents(_, _));
  EXPECT_CALL(*model_associator_, CryptoReadyIfNecessary()).
      WillRepeatedly(Return(true));
  EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
      WillRepeatedly(DoAll(SetArgumentPointee<0>(false), Return(false)));
  EXPECT_CALL(start_callback_, Run(DataTypeController::UNRECOVERABLE_ERROR, _));
  search_engine_dtc_->Start(NewCallback(&start_callback_, &StartCallback::Run));
  EXPECT_EQ(DataTypeController::NOT_RUNNING, search_engine_dtc_->state());
}

TEST_F(SearchEngineDataTypeControllerTest, Stop) {
  PreloadTemplateURLService();
  SetAssociateExpectations();
  SetStopExpectations();

  EXPECT_EQ(DataTypeController::NOT_RUNNING, search_engine_dtc_->state());

  EXPECT_CALL(start_callback_, Run(DataTypeController::OK, _));
  search_engine_dtc_->Start(NewCallback(&start_callback_, &StartCallback::Run));
  EXPECT_EQ(DataTypeController::RUNNING, search_engine_dtc_->state());
  search_engine_dtc_->Stop();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, search_engine_dtc_->state());
}

TEST_F(SearchEngineDataTypeControllerTest, OnUnrecoverableError) {
  PreloadTemplateURLService();
  SetAssociateExpectations();
  EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
      WillRepeatedly(DoAll(SetArgumentPointee<0>(true), Return(true)));
  EXPECT_CALL(service_, OnUnrecoverableError(_, _)).
      WillOnce(InvokeWithoutArgs(search_engine_dtc_.get(),
                                 &SearchEngineDataTypeController::Stop));
  SetStopExpectations();

  EXPECT_CALL(start_callback_, Run(DataTypeController::OK, _));
  search_engine_dtc_->Start(NewCallback(&start_callback_, &StartCallback::Run));
  // This should cause search_engine_dtc_->Stop() to be called.
  search_engine_dtc_->OnUnrecoverableError(FROM_HERE, "Test");
}

