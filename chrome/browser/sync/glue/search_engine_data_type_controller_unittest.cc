// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/tracked_objects.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory_test_util.h"
#include "chrome/browser/sync/glue/search_engine_data_type_controller.h"
#include "chrome/browser/sync/profile_sync_components_factory_mock.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "chrome/test/base/profile_mock.h"
#include "components/sync_driver/data_type_controller_mock.h"
#include "components/sync_driver/fake_generic_change_processor.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "sync/api/fake_syncable_service.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::DoAll;
using testing::InvokeWithoutArgs;
using testing::Return;
using testing::SetArgumentPointee;

namespace browser_sync {
namespace {

class SyncSearchEngineDataTypeControllerTest : public testing::Test {
 public:
  SyncSearchEngineDataTypeControllerTest() : test_util_(&profile_) { }

  virtual void SetUp() {
    service_.reset(new ProfileSyncServiceMock(&profile_));
    profile_sync_factory_.reset(new ProfileSyncComponentsFactoryMock());
    // Feed the DTC the profile so it is reused later.
    // This allows us to control the associated TemplateURLService.
    search_engine_dtc_ = new SearchEngineDataTypeController(
        profile_sync_factory_.get(), &profile_,
        sync_driver::DataTypeController::DisableTypeCallback());
  }

  virtual void TearDown() {
    // Must be done before we pump the loop.
    syncable_service_.StopSyncing(syncer::SEARCH_ENGINES);
    search_engine_dtc_ = NULL;
    service_.reset();
  }

 protected:
  // Pre-emptively get the TURL Service and make sure it is loaded.
  void PreloadTemplateURLService() {
    test_util_.VerifyLoad();
  }

  void SetStartExpectations() {
    search_engine_dtc_->SetGenericChangeProcessorFactoryForTest(
        make_scoped_ptr<sync_driver::GenericChangeProcessorFactory>(
            new sync_driver::FakeGenericChangeProcessorFactory(
                make_scoped_ptr(new sync_driver::FakeGenericChangeProcessor(
                    profile_sync_factory_.get())))));
    EXPECT_CALL(model_load_callback_, Run(_, _));
    EXPECT_CALL(*profile_sync_factory_,
                GetSyncableServiceForType(syncer::SEARCH_ENGINES)).
        WillOnce(Return(syncable_service_.AsWeakPtr()));
  }

  void Start() {
    search_engine_dtc_->LoadModels(
        base::Bind(&sync_driver::ModelLoadCallbackMock::Run,
                   base::Unretained(&model_load_callback_)));
    search_engine_dtc_->StartAssociating(
        base::Bind(&sync_driver::StartCallbackMock::Run,
                   base::Unretained(&start_callback_)));
  }

  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  TemplateURLServiceFactoryTestUtil test_util_;
  scoped_refptr<SearchEngineDataTypeController> search_engine_dtc_;
  scoped_ptr<ProfileSyncComponentsFactoryMock> profile_sync_factory_;
  scoped_ptr<ProfileSyncServiceMock> service_;
  syncer::FakeSyncableService syncable_service_;
  sync_driver::StartCallbackMock start_callback_;
  sync_driver::ModelLoadCallbackMock model_load_callback_;
};

TEST_F(SyncSearchEngineDataTypeControllerTest, StartURLServiceReady) {
  SetStartExpectations();
  // We want to start ready.
  PreloadTemplateURLService();
  EXPECT_CALL(start_callback_, Run(sync_driver::DataTypeController::OK, _, _));

  EXPECT_EQ(sync_driver::DataTypeController::NOT_RUNNING,
            search_engine_dtc_->state());
  EXPECT_FALSE(syncable_service_.syncing());
  Start();
  EXPECT_EQ(sync_driver::DataTypeController::RUNNING,
            search_engine_dtc_->state());
  EXPECT_TRUE(syncable_service_.syncing());
}

TEST_F(SyncSearchEngineDataTypeControllerTest, StartURLServiceNotReady) {
  EXPECT_CALL(model_load_callback_, Run(_, _));
  EXPECT_FALSE(syncable_service_.syncing());
  search_engine_dtc_->LoadModels(
      base::Bind(&sync_driver::ModelLoadCallbackMock::Run,
                 base::Unretained(&model_load_callback_)));
  EXPECT_TRUE(search_engine_dtc_->GetSubscriptionForTesting());
  EXPECT_EQ(sync_driver::DataTypeController::MODEL_STARTING,
            search_engine_dtc_->state());
  EXPECT_FALSE(syncable_service_.syncing());

  // Send the notification that the TemplateURLService has started.
  PreloadTemplateURLService();
  EXPECT_EQ(NULL, search_engine_dtc_->GetSubscriptionForTesting());
  EXPECT_EQ(sync_driver::DataTypeController::MODEL_LOADED,
            search_engine_dtc_->state());

  // Wait until WebDB is loaded before we shut it down.
  base::RunLoop().RunUntilIdle();
}

TEST_F(SyncSearchEngineDataTypeControllerTest, StartAssociationFailed) {
  SetStartExpectations();
  PreloadTemplateURLService();
  EXPECT_CALL(start_callback_,
              Run(sync_driver::DataTypeController::ASSOCIATION_FAILED, _, _));
  syncable_service_.set_merge_data_and_start_syncing_error(
      syncer::SyncError(FROM_HERE,
                        syncer::SyncError::DATATYPE_ERROR,
                        "Error",
                        syncer::SEARCH_ENGINES));

  Start();
  EXPECT_EQ(sync_driver::DataTypeController::DISABLED,
            search_engine_dtc_->state());
  EXPECT_FALSE(syncable_service_.syncing());
  search_engine_dtc_->Stop();
  EXPECT_EQ(sync_driver::DataTypeController::NOT_RUNNING,
            search_engine_dtc_->state());
  EXPECT_FALSE(syncable_service_.syncing());
}

TEST_F(SyncSearchEngineDataTypeControllerTest, Stop) {
  SetStartExpectations();
  PreloadTemplateURLService();
  EXPECT_CALL(start_callback_, Run(sync_driver::DataTypeController::OK, _, _));

  EXPECT_EQ(sync_driver::DataTypeController::NOT_RUNNING,
            search_engine_dtc_->state());
  EXPECT_FALSE(syncable_service_.syncing());
  Start();
  EXPECT_EQ(sync_driver::DataTypeController::RUNNING,
            search_engine_dtc_->state());
  EXPECT_TRUE(syncable_service_.syncing());
  search_engine_dtc_->Stop();
  EXPECT_EQ(sync_driver::DataTypeController::NOT_RUNNING,
            search_engine_dtc_->state());
  EXPECT_FALSE(syncable_service_.syncing());
}

TEST_F(SyncSearchEngineDataTypeControllerTest, StopBeforeLoaded) {
  EXPECT_CALL(model_load_callback_, Run(_, _));
  EXPECT_FALSE(syncable_service_.syncing());
  search_engine_dtc_->LoadModels(
      base::Bind(&sync_driver::ModelLoadCallbackMock::Run,
                 base::Unretained(&model_load_callback_)));
  EXPECT_TRUE(search_engine_dtc_->GetSubscriptionForTesting());
  EXPECT_EQ(sync_driver::DataTypeController::MODEL_STARTING,
            search_engine_dtc_->state());
  EXPECT_FALSE(syncable_service_.syncing());
  search_engine_dtc_->Stop();
  EXPECT_EQ(NULL, search_engine_dtc_->GetSubscriptionForTesting());
  EXPECT_EQ(sync_driver::DataTypeController::NOT_RUNNING,
            search_engine_dtc_->state());
  EXPECT_FALSE(syncable_service_.syncing());
}

}  // namespace
}  // namespace browser_sync
