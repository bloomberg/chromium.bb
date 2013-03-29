// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/run_loop.h"
#include "chrome/browser/sync/glue/autofill_data_type_controller.h"
#include "chrome/browser/sync/glue/data_type_controller_mock.h"
#include "chrome/browser/sync/glue/shared_change_processor_mock.h"
#include "chrome/browser/sync/profile_sync_components_factory_mock.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "chrome/browser/webdata/autocomplete_syncable_service.h"
#include "chrome/browser/webdata/autofill_web_data_service.h"
#include "chrome/browser/webdata/web_data_service_factory.h"
#include "chrome/browser/webdata/web_data_service_test_util.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/test/base/profile_mock.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/test/test_browser_thread.h"
#include "sync/api/sync_error.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {

namespace {

using content::BrowserThread;
using testing::_;
using testing::NiceMock;
using testing::Return;

// Fake WebDataService implementation that stubs out the database loading.
class FakeWebDataService : public AutofillWebDataService {
 public:
  FakeWebDataService()
      : AutofillWebDataService(
            NULL, WebDataServiceBase::ProfileErrorCallback()),
        is_database_loaded_(false) {}

  // Mark the database as loaded and send out the appropriate notification.
  void LoadDatabase() {
    StartSyncableService();
    is_database_loaded_ = true;
    NotifyDatabaseLoadedOnUIThread();
  }

  virtual bool IsDatabaseLoaded() OVERRIDE {
    return is_database_loaded_;
  }

  virtual void ShutdownOnUIThread() OVERRIDE {
    // The storage for syncable services must be destructed on the DB
    // thread.
    base::RunLoop run_loop;
    BrowserThread::PostTaskAndReply(BrowserThread::DB, FROM_HERE,
        base::Bind(&FakeWebDataService::ShutdownOnDBThread,
                   base::Unretained(this)), run_loop.QuitClosure());
    run_loop.Run();
  }

  void StartSyncableService() {
    // The |autofill_profile_syncable_service_| must be constructed on the DB
    // thread.
    base::RunLoop run_loop;
    BrowserThread::PostTaskAndReply(BrowserThread::DB, FROM_HERE,
        base::Bind(&FakeWebDataService::CreateSyncableService,
                   base::Unretained(this)), run_loop.QuitClosure());
    run_loop.Run();
  }

  void GetAutofillCullingValue(bool* result) {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::DB));
    *result = AutocompleteSyncableService::FromWebDataService(
        this)->cull_expired_entries();
  }

  bool CheckAutofillCullingValue() {
    bool result = false;
    base::RunLoop run_loop;
    BrowserThread::PostTaskAndReply(BrowserThread::DB, FROM_HERE,
        base::Bind(&FakeWebDataService::GetAutofillCullingValue,
                   base::Unretained(this), &result), run_loop.QuitClosure());
    run_loop.Run();
    return result;
  }

 private:
  virtual ~FakeWebDataService() {
  }

  void CreateSyncableService() {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::DB));
    // These services are deleted in DestroySyncableService().
    AutocompleteSyncableService::CreateForWebDataService(this);
  }

  bool is_database_loaded_;

  DISALLOW_COPY_AND_ASSIGN(FakeWebDataService);
};

class MockWebDataServiceWrapperSyncable : public MockWebDataServiceWrapper {
 public:
  static ProfileKeyedService* Build(Profile* profile) {
    return new MockWebDataServiceWrapperSyncable();
  }

  MockWebDataServiceWrapperSyncable()
      : MockWebDataServiceWrapper(NULL, new FakeWebDataService()) {
  }

  void Shutdown() OVERRIDE {
    static_cast<FakeWebDataService*>(
        fake_autofill_web_data_.get())->ShutdownOnUIThread();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockWebDataServiceWrapperSyncable);
};

class SyncAutofillDataTypeControllerTest : public testing::Test {
 public:
  SyncAutofillDataTypeControllerTest()
      : weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
        ui_thread_(BrowserThread::UI, &message_loop_),
        db_thread_(BrowserThread::DB),
        last_start_result_(DataTypeController::OK) {}

  virtual ~SyncAutofillDataTypeControllerTest() {}

  virtual void SetUp() {
    change_processor_ = new NiceMock<SharedChangeProcessorMock>();

    EXPECT_CALL(profile_sync_factory_,
                CreateSharedChangeProcessor()).
        WillRepeatedly(Return(change_processor_.get()));

    WebDataServiceFactory::GetInstance()->SetTestingFactory(
        &profile_, MockWebDataServiceWrapperSyncable::Build);

    autofill_dtc_ =
        new AutofillDataTypeController(&profile_sync_factory_,
                                       &profile_,
                                       &service_);

    db_thread_.Start();
  }

  // Passed to AutofillDTC::Start().
  void OnStartFinished(DataTypeController::StartResult result,
                       const syncer::SyncMergeResult& local_merge_result,
                       const syncer::SyncMergeResult& syncer_merge_result) {
    last_start_result_ = result;
    last_start_error_ = local_merge_result.error();
  }

  void OnLoadFinished(syncer::ModelType type, syncer::SyncError error) {
    EXPECT_FALSE(error.IsSet());
    EXPECT_EQ(type, syncer::AUTOFILL);
  }

  virtual void TearDown() {
    autofill_dtc_ = NULL;
    change_processor_ = NULL;
  }

  void BlockForDBThread() {
    base::RunLoop run_loop;
    ASSERT_TRUE(BrowserThread::PostTaskAndReply(BrowserThread::DB, FROM_HERE,
        base::Bind(&base::DoNothing), run_loop.QuitClosure()));
    run_loop.Run();
  }

 protected:
  base::WeakPtrFactory<SyncAutofillDataTypeControllerTest> weak_ptr_factory_;
  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread db_thread_;

  scoped_refptr<NiceMock<SharedChangeProcessorMock> > change_processor_;
  ProfileSyncComponentsFactoryMock profile_sync_factory_;
  ProfileSyncServiceMock service_;
  ProfileMock profile_;
  scoped_refptr<AutofillDataTypeController> autofill_dtc_;

  // Stores arguments of most recent call of OnStartFinished().
  DataTypeController::StartResult last_start_result_;
  syncer::SyncError last_start_error_;
};

// Load the WDS's database, then start the Autofill DTC.  It should
// immediately try to start association and fail (due to missing DB
// thread).
TEST_F(SyncAutofillDataTypeControllerTest, StartWDSReady) {
  FakeWebDataService* web_db =
      static_cast<FakeWebDataService*>(
          AutofillWebDataService::FromBrowserContext(&profile_).get());
  web_db->LoadDatabase();
  autofill_dtc_->LoadModels(
    base::Bind(&SyncAutofillDataTypeControllerTest::OnLoadFinished,
               weak_ptr_factory_.GetWeakPtr()));

  EXPECT_CALL(*change_processor_, Connect(_,_,_,_,_))
      .WillOnce(Return(base::WeakPtr<syncer::SyncableService>()));
  autofill_dtc_->StartAssociating(
      base::Bind(&SyncAutofillDataTypeControllerTest::OnStartFinished,
                 weak_ptr_factory_.GetWeakPtr()));
  BlockForDBThread();

  EXPECT_EQ(DataTypeController::ASSOCIATION_FAILED, last_start_result_);
  EXPECT_TRUE(last_start_error_.IsSet());
  EXPECT_EQ(DataTypeController::DISABLED, autofill_dtc_->state());
}

// Start the autofill DTC without the WDS's database loaded, then
// start the DB.  The Autofill DTC should be in the MODEL_STARTING
// state until the database in loaded, when it should try to start
// association and fail (due to missing DB thread).
TEST_F(SyncAutofillDataTypeControllerTest, StartWDSNotReady) {
  autofill_dtc_->LoadModels(
    base::Bind(&SyncAutofillDataTypeControllerTest::OnLoadFinished,
               weak_ptr_factory_.GetWeakPtr()));

  EXPECT_EQ(DataTypeController::OK, last_start_result_);
  EXPECT_FALSE(last_start_error_.IsSet());
  EXPECT_EQ(DataTypeController::MODEL_STARTING, autofill_dtc_->state());

  FakeWebDataService* web_db =
      static_cast<FakeWebDataService*>(
        AutofillWebDataService::FromBrowserContext(&profile_).get());
  web_db->LoadDatabase();

  EXPECT_CALL(*change_processor_, Connect(_,_,_,_,_))
      .WillOnce(Return(base::WeakPtr<syncer::SyncableService>()));
  autofill_dtc_->StartAssociating(
      base::Bind(&SyncAutofillDataTypeControllerTest::OnStartFinished,
                 weak_ptr_factory_.GetWeakPtr()));
  BlockForDBThread();

  EXPECT_EQ(DataTypeController::ASSOCIATION_FAILED, last_start_result_);
  EXPECT_TRUE(last_start_error_.IsSet());

  EXPECT_EQ(DataTypeController::DISABLED, autofill_dtc_->state());
}

TEST_F(SyncAutofillDataTypeControllerTest, UpdateAutofillCullingSettings) {
  FakeWebDataService* web_db =
      static_cast<FakeWebDataService*>(
          AutofillWebDataService::FromBrowserContext(&profile_).get());

  // Set up the experiments state.
  ProfileSyncService* sync = ProfileSyncServiceFactory::GetForProfile(
      &profile_);
  syncer::Experiments experiments;
  experiments.autofill_culling = true;
  sync->OnExperimentsChanged(experiments);

  web_db->LoadDatabase();
  autofill_dtc_->LoadModels(
    base::Bind(&SyncAutofillDataTypeControllerTest::OnLoadFinished,
               weak_ptr_factory_.GetWeakPtr()));

  EXPECT_FALSE(web_db->CheckAutofillCullingValue());

  EXPECT_CALL(*change_processor_, Connect(_,_,_,_,_))
      .WillOnce(Return(base::WeakPtr<syncer::SyncableService>()));
  autofill_dtc_->StartAssociating(
      base::Bind(&SyncAutofillDataTypeControllerTest::OnStartFinished,
                 weak_ptr_factory_.GetWeakPtr()));
  BlockForDBThread();

  EXPECT_TRUE(web_db->CheckAutofillCullingValue());
}

}  // namespace

}  // namespace browser_sync
