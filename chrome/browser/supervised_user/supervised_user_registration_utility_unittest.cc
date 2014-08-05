// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/supervised_user/supervised_user_refresh_token_fetcher.h"
#include "chrome/browser/supervised_user/supervised_user_registration_utility.h"
#include "chrome/browser/supervised_user/supervised_user_shared_settings_service.h"
#include "chrome/browser/supervised_user/supervised_user_shared_settings_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_sync_service.h"
#include "chrome/browser/supervised_user/supervised_user_sync_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "sync/api/attachments/attachment_id.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_error_factory_mock.h"
#include "sync/internal_api/public/attachments/attachment_service_proxy_for_test.h"
#include "sync/protocol/sync.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

using sync_pb::ManagedUserSpecifics;
using syncer::SUPERVISED_USERS;
using syncer::SyncChange;
using syncer::SyncChangeList;
using syncer::SyncChangeProcessor;
using syncer::SyncData;
using syncer::SyncDataList;
using syncer::SyncError;
using syncer::SyncErrorFactory;
using syncer::SyncMergeResult;

namespace {

const char kSupervisedUserToken[] = "supervisedusertoken";

class MockChangeProcessor : public SyncChangeProcessor {
 public:
  MockChangeProcessor() {}
  virtual ~MockChangeProcessor() {}

  // SyncChangeProcessor implementation:
  virtual SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const SyncChangeList& change_list) OVERRIDE;

  virtual SyncDataList GetAllSyncData(syncer::ModelType type) const
      OVERRIDE {
    return SyncDataList();
  }

  const SyncChangeList& changes() const { return change_list_; }

 private:
  SyncChangeList change_list_;
};

SyncError MockChangeProcessor::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const SyncChangeList& change_list) {
  change_list_ = change_list;
  return SyncError();
}

class MockSupervisedUserRefreshTokenFetcher
    : public SupervisedUserRefreshTokenFetcher {
 public:
  MockSupervisedUserRefreshTokenFetcher() {}
  virtual ~MockSupervisedUserRefreshTokenFetcher() {}

  // SupervisedUserRefreshTokenFetcher implementation:
  virtual void Start(const std::string& supervised_user_id,
                     const std::string& device_name,
                     const TokenCallback& callback) OVERRIDE {
    GoogleServiceAuthError error(GoogleServiceAuthError::NONE);
    callback.Run(error, kSupervisedUserToken);
  }
};

}  // namespace

class SupervisedUserRegistrationUtilityTest : public ::testing::Test {
 public:
  SupervisedUserRegistrationUtilityTest();
  virtual ~SupervisedUserRegistrationUtilityTest();

  virtual void TearDown() OVERRIDE;

 protected:
  scoped_ptr<SyncChangeProcessor> CreateChangeProcessor();
  scoped_ptr<SyncErrorFactory> CreateErrorFactory();
  SyncData CreateRemoteData(const std::string& id, const std::string& name);

  SyncMergeResult StartInitialSync();

  SupervisedUserRegistrationUtility::RegistrationCallback
      GetRegistrationCallback();

  SupervisedUserRegistrationUtility* GetRegistrationUtility();

  void Acknowledge();

  PrefService* prefs() { return profile_.GetTestingPrefService(); }
  SupervisedUserSyncService* service() { return service_; }
  SupervisedUserSharedSettingsService* shared_settings_service() {
    return shared_settings_service_;
  }
  MockChangeProcessor* change_processor() { return change_processor_; }

  bool received_callback() const { return received_callback_; }
  const GoogleServiceAuthError& error() const { return error_; }
  const std::string& token() const { return token_; }

 private:
  void OnSupervisedUserRegistered(const GoogleServiceAuthError& error,
                                  const std::string& token);

  base::MessageLoop message_loop_;
  base::RunLoop run_loop_;
  TestingProfile profile_;
  SupervisedUserSyncService* service_;
  SupervisedUserSharedSettingsService* shared_settings_service_;
  scoped_ptr<SupervisedUserRegistrationUtility> registration_utility_;

  // Owned by the SupervisedUserSyncService.
  MockChangeProcessor* change_processor_;

  // A unique ID for creating "remote" Sync data.
  int64 sync_data_id_;

  // Whether OnSupervisedUserRegistered has been called.
  bool received_callback_;

  // Hold the registration result (either an error, or a token).
  GoogleServiceAuthError error_;
  std::string token_;

  base::WeakPtrFactory<SupervisedUserRegistrationUtilityTest> weak_ptr_factory_;
};

SupervisedUserRegistrationUtilityTest::SupervisedUserRegistrationUtilityTest()
    : change_processor_(NULL),
      sync_data_id_(0),
      received_callback_(false),
      error_(GoogleServiceAuthError::NUM_STATES),
      weak_ptr_factory_(this) {
  service_ = SupervisedUserSyncServiceFactory::GetForProfile(&profile_);
  shared_settings_service_ =
      SupervisedUserSharedSettingsServiceFactory::GetForBrowserContext(
          &profile_);
}

SupervisedUserRegistrationUtilityTest::
    ~SupervisedUserRegistrationUtilityTest() {
  EXPECT_FALSE(weak_ptr_factory_.HasWeakPtrs());
}

void SupervisedUserRegistrationUtilityTest::TearDown() {
  content::BrowserThread::GetBlockingPool()->FlushForTesting();
  base::RunLoop().RunUntilIdle();
}

scoped_ptr<SyncChangeProcessor>
SupervisedUserRegistrationUtilityTest::CreateChangeProcessor() {
  EXPECT_FALSE(change_processor_);
  change_processor_ = new MockChangeProcessor();
  return scoped_ptr<SyncChangeProcessor>(change_processor_);
}

scoped_ptr<SyncErrorFactory>
SupervisedUserRegistrationUtilityTest::CreateErrorFactory() {
  return scoped_ptr<SyncErrorFactory>(new syncer::SyncErrorFactoryMock());
}

SyncMergeResult SupervisedUserRegistrationUtilityTest::StartInitialSync() {
  SyncDataList initial_sync_data;
  SyncMergeResult result =
      service()->MergeDataAndStartSyncing(SUPERVISED_USERS,
                                          initial_sync_data,
                                          CreateChangeProcessor(),
                                          CreateErrorFactory());
  EXPECT_FALSE(result.error().IsSet());
  return result;
}

SupervisedUserRegistrationUtility::RegistrationCallback
SupervisedUserRegistrationUtilityTest::GetRegistrationCallback() {
  return base::Bind(
      &SupervisedUserRegistrationUtilityTest::OnSupervisedUserRegistered,
      weak_ptr_factory_.GetWeakPtr());
}

SupervisedUserRegistrationUtility*
SupervisedUserRegistrationUtilityTest::GetRegistrationUtility() {
  if (registration_utility_.get())
    return registration_utility_.get();

  scoped_ptr<SupervisedUserRefreshTokenFetcher> token_fetcher(
      new MockSupervisedUserRefreshTokenFetcher);
  registration_utility_.reset(
      SupervisedUserRegistrationUtility::CreateImpl(prefs(),
                                                    token_fetcher.Pass(),
                                                    service(),
                                                    shared_settings_service()));
  return registration_utility_.get();
}

void SupervisedUserRegistrationUtilityTest::Acknowledge() {
  SyncChangeList new_changes;
  const SyncChangeList& changes = change_processor()->changes();
  for (SyncChangeList::const_iterator it = changes.begin(); it != changes.end();
       ++it) {
    EXPECT_EQ(SyncChange::ACTION_ADD, it->change_type());
    ::sync_pb::EntitySpecifics specifics = it->sync_data().GetSpecifics();
    EXPECT_FALSE(specifics.managed_user().acknowledged());
    specifics.mutable_managed_user()->set_acknowledged(true);
    new_changes.push_back(
        SyncChange(FROM_HERE,
                   SyncChange::ACTION_UPDATE,
                   SyncData::CreateRemoteData(
                       ++sync_data_id_,
                       specifics,
                       base::Time(),
                       syncer::AttachmentIdList(),
                       syncer::AttachmentServiceProxyForTest::Create())));
  }
  service()->ProcessSyncChanges(FROM_HERE, new_changes);

  run_loop_.Run();
}

void SupervisedUserRegistrationUtilityTest::OnSupervisedUserRegistered(
    const GoogleServiceAuthError& error,
    const std::string& token) {
  received_callback_ = true;
  error_ = error;
  token_ = token;
  run_loop_.Quit();
}

TEST_F(SupervisedUserRegistrationUtilityTest, Register) {
  StartInitialSync();
  GetRegistrationUtility()->Register(
      SupervisedUserRegistrationUtility::GenerateNewSupervisedUserId(),
      SupervisedUserRegistrationInfo(base::ASCIIToUTF16("Dug"), 0),
      GetRegistrationCallback());
  EXPECT_EQ(1u, prefs()->GetDictionary(prefs::kSupervisedUsers)->size());
  Acknowledge();

  EXPECT_TRUE(received_callback());
  EXPECT_EQ(GoogleServiceAuthError::NONE, error().state());
  EXPECT_FALSE(token().empty());
}

TEST_F(SupervisedUserRegistrationUtilityTest, RegisterBeforeInitialSync) {
  GetRegistrationUtility()->Register(
      SupervisedUserRegistrationUtility::GenerateNewSupervisedUserId(),
      SupervisedUserRegistrationInfo(base::ASCIIToUTF16("Nemo"), 5),
      GetRegistrationCallback());
  EXPECT_EQ(1u, prefs()->GetDictionary(prefs::kSupervisedUsers)->size());
  StartInitialSync();
  Acknowledge();

  EXPECT_TRUE(received_callback());
  EXPECT_EQ(GoogleServiceAuthError::NONE, error().state());
  EXPECT_FALSE(token().empty());
}

TEST_F(SupervisedUserRegistrationUtilityTest,
       SyncServiceShutdownBeforeRegFinish) {
  StartInitialSync();
  GetRegistrationUtility()->Register(
      SupervisedUserRegistrationUtility::GenerateNewSupervisedUserId(),
      SupervisedUserRegistrationInfo(base::ASCIIToUTF16("Remy"), 12),
      GetRegistrationCallback());
  EXPECT_EQ(1u, prefs()->GetDictionary(prefs::kSupervisedUsers)->size());
  service()->Shutdown();
  EXPECT_EQ(0u, prefs()->GetDictionary(prefs::kSupervisedUsers)->size());
  EXPECT_TRUE(received_callback());
  EXPECT_EQ(GoogleServiceAuthError::REQUEST_CANCELED, error().state());
  EXPECT_EQ(std::string(), token());
}

TEST_F(SupervisedUserRegistrationUtilityTest, StopSyncingBeforeRegFinish) {
  StartInitialSync();
  GetRegistrationUtility()->Register(
      SupervisedUserRegistrationUtility::GenerateNewSupervisedUserId(),
      SupervisedUserRegistrationInfo(base::ASCIIToUTF16("Mike"), 17),
      GetRegistrationCallback());
  EXPECT_EQ(1u, prefs()->GetDictionary(prefs::kSupervisedUsers)->size());
  service()->StopSyncing(SUPERVISED_USERS);
  EXPECT_EQ(0u, prefs()->GetDictionary(prefs::kSupervisedUsers)->size());
  EXPECT_TRUE(received_callback());
  EXPECT_EQ(GoogleServiceAuthError::REQUEST_CANCELED, error().state());
  EXPECT_EQ(std::string(), token());
}
