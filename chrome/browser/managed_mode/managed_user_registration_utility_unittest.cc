// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/managed_mode/managed_user_refresh_token_fetcher.h"
#include "chrome/browser/managed_mode/managed_user_registration_utility.h"
#include "chrome/browser/managed_mode/managed_user_shared_settings_service.h"
#include "chrome/browser/managed_mode/managed_user_shared_settings_service_factory.h"
#include "chrome/browser/managed_mode/managed_user_sync_service.h"
#include "chrome/browser/managed_mode/managed_user_sync_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "sync/api/attachments/attachment_id.h"
#include "sync/api/attachments/attachment_service_proxy_for_test.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_error_factory_mock.h"
#include "sync/protocol/sync.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

using sync_pb::ManagedUserSpecifics;
using syncer::MANAGED_USERS;
using syncer::SyncChange;
using syncer::SyncChangeList;
using syncer::SyncChangeProcessor;
using syncer::SyncData;
using syncer::SyncDataList;
using syncer::SyncError;
using syncer::SyncErrorFactory;
using syncer::SyncMergeResult;

namespace {

const char kManagedUserToken[] = "managedusertoken";

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

class MockManagedUserRefreshTokenFetcher
    : public ManagedUserRefreshTokenFetcher {
 public:
  MockManagedUserRefreshTokenFetcher() {}
  virtual ~MockManagedUserRefreshTokenFetcher() {}

  // ManagedUserRefreshTokenFetcher implementation:
  virtual void Start(const std::string& managed_user_id,
                     const std::string& device_name,
                     const TokenCallback& callback) OVERRIDE {
    GoogleServiceAuthError error(GoogleServiceAuthError::NONE);
    callback.Run(error, kManagedUserToken);
  }
};

}  // namespace

class ManagedUserRegistrationUtilityTest : public ::testing::Test {
 public:
  ManagedUserRegistrationUtilityTest();
  virtual ~ManagedUserRegistrationUtilityTest();

  virtual void TearDown() OVERRIDE;

 protected:
  scoped_ptr<SyncChangeProcessor> CreateChangeProcessor();
  scoped_ptr<SyncErrorFactory> CreateErrorFactory();
  SyncData CreateRemoteData(const std::string& id, const std::string& name);

  SyncMergeResult StartInitialSync();

  ManagedUserRegistrationUtility::RegistrationCallback
      GetRegistrationCallback();

  ManagedUserRegistrationUtility* GetRegistrationUtility();

  void Acknowledge();

  PrefService* prefs() { return profile_.GetTestingPrefService(); }
  ManagedUserSyncService* service() { return service_; }
  ManagedUserSharedSettingsService* shared_settings_service() {
    return shared_settings_service_;
  }
  MockChangeProcessor* change_processor() { return change_processor_; }

  bool received_callback() const { return received_callback_; }
  const GoogleServiceAuthError& error() const { return error_; }
  const std::string& token() const { return token_; }

 private:
  void OnManagedUserRegistered(const GoogleServiceAuthError& error,
                               const std::string& token);

  base::MessageLoop message_loop_;
  base::RunLoop run_loop_;
  TestingProfile profile_;
  ManagedUserSyncService* service_;
  ManagedUserSharedSettingsService* shared_settings_service_;
  scoped_ptr<ManagedUserRegistrationUtility> registration_utility_;

  // Owned by the ManagedUserSyncService.
  MockChangeProcessor* change_processor_;

  // A unique ID for creating "remote" Sync data.
  int64 sync_data_id_;

  // Whether OnManagedUserRegistered has been called.
  bool received_callback_;

  // Hold the registration result (either an error, or a token).
  GoogleServiceAuthError error_;
  std::string token_;

  base::WeakPtrFactory<ManagedUserRegistrationUtilityTest> weak_ptr_factory_;
};

ManagedUserRegistrationUtilityTest::ManagedUserRegistrationUtilityTest()
    : change_processor_(NULL),
      sync_data_id_(0),
      received_callback_(false),
      error_(GoogleServiceAuthError::NUM_STATES),
      weak_ptr_factory_(this) {
  service_ = ManagedUserSyncServiceFactory::GetForProfile(&profile_);
  shared_settings_service_ =
      ManagedUserSharedSettingsServiceFactory::GetForBrowserContext(&profile_);
}

ManagedUserRegistrationUtilityTest::~ManagedUserRegistrationUtilityTest() {
  EXPECT_FALSE(weak_ptr_factory_.HasWeakPtrs());
}

void ManagedUserRegistrationUtilityTest::TearDown() {
  content::BrowserThread::GetBlockingPool()->FlushForTesting();
  base::RunLoop().RunUntilIdle();
}

scoped_ptr<SyncChangeProcessor>
ManagedUserRegistrationUtilityTest::CreateChangeProcessor() {
  EXPECT_FALSE(change_processor_);
  change_processor_ = new MockChangeProcessor();
  return scoped_ptr<SyncChangeProcessor>(change_processor_);
}

scoped_ptr<SyncErrorFactory>
ManagedUserRegistrationUtilityTest::CreateErrorFactory() {
  return scoped_ptr<SyncErrorFactory>(new syncer::SyncErrorFactoryMock());
}

SyncMergeResult ManagedUserRegistrationUtilityTest::StartInitialSync() {
  SyncDataList initial_sync_data;
  SyncMergeResult result =
      service()->MergeDataAndStartSyncing(MANAGED_USERS,
                                          initial_sync_data,
                                          CreateChangeProcessor(),
                                          CreateErrorFactory());
  EXPECT_FALSE(result.error().IsSet());
  return result;
}

ManagedUserRegistrationUtility::RegistrationCallback
ManagedUserRegistrationUtilityTest::GetRegistrationCallback() {
  return base::Bind(
      &ManagedUserRegistrationUtilityTest::OnManagedUserRegistered,
      weak_ptr_factory_.GetWeakPtr());
}

ManagedUserRegistrationUtility*
ManagedUserRegistrationUtilityTest::GetRegistrationUtility() {
  if (registration_utility_.get())
    return registration_utility_.get();

  scoped_ptr<ManagedUserRefreshTokenFetcher> token_fetcher(
      new MockManagedUserRefreshTokenFetcher);
  registration_utility_.reset(
      ManagedUserRegistrationUtility::CreateImpl(prefs(),
                                                 token_fetcher.Pass(),
                                                 service(),
                                                 shared_settings_service()));
  return registration_utility_.get();
}

void ManagedUserRegistrationUtilityTest::Acknowledge() {
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

void ManagedUserRegistrationUtilityTest::OnManagedUserRegistered(
    const GoogleServiceAuthError& error,
    const std::string& token) {
  received_callback_ = true;
  error_ = error;
  token_ = token;
  run_loop_.Quit();
}

TEST_F(ManagedUserRegistrationUtilityTest, Register) {
  StartInitialSync();
  GetRegistrationUtility()->Register(
      ManagedUserRegistrationUtility::GenerateNewManagedUserId(),
      ManagedUserRegistrationInfo(base::ASCIIToUTF16("Dug"), 0),
      GetRegistrationCallback());
  EXPECT_EQ(1u, prefs()->GetDictionary(prefs::kManagedUsers)->size());
  Acknowledge();

  EXPECT_TRUE(received_callback());
  EXPECT_EQ(GoogleServiceAuthError::NONE, error().state());
  EXPECT_FALSE(token().empty());
}

TEST_F(ManagedUserRegistrationUtilityTest, RegisterBeforeInitialSync) {
  GetRegistrationUtility()->Register(
      ManagedUserRegistrationUtility::GenerateNewManagedUserId(),
      ManagedUserRegistrationInfo(base::ASCIIToUTF16("Nemo"), 5),
      GetRegistrationCallback());
  EXPECT_EQ(1u, prefs()->GetDictionary(prefs::kManagedUsers)->size());
  StartInitialSync();
  Acknowledge();

  EXPECT_TRUE(received_callback());
  EXPECT_EQ(GoogleServiceAuthError::NONE, error().state());
  EXPECT_FALSE(token().empty());
}

TEST_F(ManagedUserRegistrationUtilityTest, SyncServiceShutdownBeforeRegFinish) {
  StartInitialSync();
  GetRegistrationUtility()->Register(
      ManagedUserRegistrationUtility::GenerateNewManagedUserId(),
      ManagedUserRegistrationInfo(base::ASCIIToUTF16("Remy"), 12),
      GetRegistrationCallback());
  EXPECT_EQ(1u, prefs()->GetDictionary(prefs::kManagedUsers)->size());
  service()->Shutdown();
  EXPECT_EQ(0u, prefs()->GetDictionary(prefs::kManagedUsers)->size());
  EXPECT_TRUE(received_callback());
  EXPECT_EQ(GoogleServiceAuthError::REQUEST_CANCELED, error().state());
  EXPECT_EQ(std::string(), token());
}

TEST_F(ManagedUserRegistrationUtilityTest, StopSyncingBeforeRegFinish) {
  StartInitialSync();
  GetRegistrationUtility()->Register(
      ManagedUserRegistrationUtility::GenerateNewManagedUserId(),
      ManagedUserRegistrationInfo(base::ASCIIToUTF16("Mike"), 17),
      GetRegistrationCallback());
  EXPECT_EQ(1u, prefs()->GetDictionary(prefs::kManagedUsers)->size());
  service()->StopSyncing(MANAGED_USERS);
  EXPECT_EQ(0u, prefs()->GetDictionary(prefs::kManagedUsers)->size());
  EXPECT_TRUE(received_callback());
  EXPECT_EQ(GoogleServiceAuthError::REQUEST_CANCELED, error().state());
  EXPECT_EQ(std::string(), token());
}
