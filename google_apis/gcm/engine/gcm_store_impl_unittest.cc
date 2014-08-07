// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/engine/gcm_store_impl.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "google_apis/gcm/base/fake_encryptor.h"
#include "google_apis/gcm/base/mcs_message.h"
#include "google_apis/gcm/base/mcs_util.h"
#include "google_apis/gcm/protocol/mcs.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {

namespace {

// Number of persistent ids to use in tests.
const int kNumPersistentIds = 10;

// Number of per-app messages in tests.
const int kNumMessagesPerApp = 20;

// App name for testing.
const char kAppName[] = "my_app";

// Category name for testing.
const char kCategoryName[] = "my_category";

const uint64 kDeviceId = 22;
const uint64 kDeviceToken = 55;

class GCMStoreImplTest : public testing::Test {
 public:
  GCMStoreImplTest();
  virtual ~GCMStoreImplTest();

  scoped_ptr<GCMStore> BuildGCMStore();

  std::string GetNextPersistentId();

  void PumpLoop();

  void LoadCallback(scoped_ptr<GCMStore::LoadResult>* result_dst,
                    scoped_ptr<GCMStore::LoadResult> result);
  void UpdateCallback(bool success);

 protected:
  base::MessageLoop message_loop_;
  base::ScopedTempDir temp_directory_;
  bool expected_success_;
  uint64 next_persistent_id_;
  scoped_ptr<base::RunLoop> run_loop_;
};

GCMStoreImplTest::GCMStoreImplTest()
    : expected_success_(true),
      next_persistent_id_(base::Time::Now().ToInternalValue()) {
  EXPECT_TRUE(temp_directory_.CreateUniqueTempDir());
  run_loop_.reset(new base::RunLoop());
}

GCMStoreImplTest::~GCMStoreImplTest() {}

scoped_ptr<GCMStore> GCMStoreImplTest::BuildGCMStore() {
  return scoped_ptr<GCMStore>(new GCMStoreImpl(
      temp_directory_.path(),
      message_loop_.message_loop_proxy(),
      make_scoped_ptr<Encryptor>(new FakeEncryptor)));
}

std::string GCMStoreImplTest::GetNextPersistentId() {
  return base::Uint64ToString(next_persistent_id_++);
}

void GCMStoreImplTest::PumpLoop() { message_loop_.RunUntilIdle(); }

void GCMStoreImplTest::LoadCallback(
    scoped_ptr<GCMStore::LoadResult>* result_dst,
    scoped_ptr<GCMStore::LoadResult> result) {
  ASSERT_TRUE(result->success);
  *result_dst = result.Pass();
  run_loop_->Quit();
  run_loop_.reset(new base::RunLoop());
}

void GCMStoreImplTest::UpdateCallback(bool success) {
  ASSERT_EQ(expected_success_, success);
}

// Verify creating a new database and loading it.
TEST_F(GCMStoreImplTest, LoadNew) {
  scoped_ptr<GCMStore> gcm_store(BuildGCMStore());
  scoped_ptr<GCMStore::LoadResult> load_result;
  gcm_store->Load(base::Bind(
      &GCMStoreImplTest::LoadCallback, base::Unretained(this), &load_result));
  PumpLoop();

  EXPECT_EQ(0U, load_result->device_android_id);
  EXPECT_EQ(0U, load_result->device_security_token);
  EXPECT_TRUE(load_result->incoming_messages.empty());
  EXPECT_TRUE(load_result->outgoing_messages.empty());
  EXPECT_TRUE(load_result->gservices_settings.empty());
  EXPECT_EQ(base::Time::FromInternalValue(0LL), load_result->last_checkin_time);
}

TEST_F(GCMStoreImplTest, DeviceCredentials) {
  scoped_ptr<GCMStore> gcm_store(BuildGCMStore());
  scoped_ptr<GCMStore::LoadResult> load_result;
  gcm_store->Load(base::Bind(
      &GCMStoreImplTest::LoadCallback, base::Unretained(this), &load_result));
  PumpLoop();

  gcm_store->SetDeviceCredentials(
      kDeviceId,
      kDeviceToken,
      base::Bind(&GCMStoreImplTest::UpdateCallback, base::Unretained(this)));
  PumpLoop();

  gcm_store = BuildGCMStore().Pass();
  gcm_store->Load(base::Bind(
      &GCMStoreImplTest::LoadCallback, base::Unretained(this), &load_result));
  PumpLoop();

  ASSERT_EQ(kDeviceId, load_result->device_android_id);
  ASSERT_EQ(kDeviceToken, load_result->device_security_token);
}

TEST_F(GCMStoreImplTest, LastCheckinInfo) {
  scoped_ptr<GCMStore> gcm_store(BuildGCMStore());
  scoped_ptr<GCMStore::LoadResult> load_result;
  gcm_store->Load(base::Bind(
      &GCMStoreImplTest::LoadCallback, base::Unretained(this), &load_result));
  PumpLoop();

  base::Time last_checkin_time = base::Time::Now();
  std::set<std::string> accounts;
  accounts.insert("test_user1@gmail.com");
  accounts.insert("test_user2@gmail.com");

  gcm_store->SetLastCheckinInfo(
      last_checkin_time,
      accounts,
      base::Bind(&GCMStoreImplTest::UpdateCallback, base::Unretained(this)));
  PumpLoop();

  gcm_store = BuildGCMStore().Pass();
  gcm_store->Load(base::Bind(
      &GCMStoreImplTest::LoadCallback, base::Unretained(this), &load_result));
  PumpLoop();
  ASSERT_EQ(last_checkin_time, load_result->last_checkin_time);
  ASSERT_EQ(accounts, load_result->last_checkin_accounts);
}

TEST_F(GCMStoreImplTest, GServicesSettings_ProtocolV2) {
  scoped_ptr<GCMStore> gcm_store(BuildGCMStore());
  scoped_ptr<GCMStore::LoadResult> load_result;
  gcm_store->Load(base::Bind(
      &GCMStoreImplTest::LoadCallback, base::Unretained(this), &load_result));
  PumpLoop();

  std::map<std::string, std::string> settings;
  settings["checkin_interval"] = "12345";
  settings["mcs_port"] = "438";
  settings["checkin_url"] = "http://checkin.google.com";
  std::string digest = "digest1";

  gcm_store->SetGServicesSettings(
      settings,
      digest,
      base::Bind(&GCMStoreImplTest::UpdateCallback, base::Unretained(this)));
  PumpLoop();

  gcm_store = BuildGCMStore().Pass();
  gcm_store->Load(base::Bind(
      &GCMStoreImplTest::LoadCallback, base::Unretained(this), &load_result));
  PumpLoop();

  ASSERT_EQ(settings, load_result->gservices_settings);
  ASSERT_EQ(digest, load_result->gservices_digest);

  // Remove some, and add some.
  settings.clear();
  settings["checkin_interval"] = "54321";
  settings["registration_url"] = "http://registration.google.com";
  digest = "digest2";

  gcm_store->SetGServicesSettings(
      settings,
      digest,
      base::Bind(&GCMStoreImplTest::UpdateCallback, base::Unretained(this)));
  PumpLoop();

  gcm_store = BuildGCMStore().Pass();
  gcm_store->Load(base::Bind(
      &GCMStoreImplTest::LoadCallback, base::Unretained(this), &load_result));
  PumpLoop();

  ASSERT_EQ(settings, load_result->gservices_settings);
  ASSERT_EQ(digest, load_result->gservices_digest);
}

TEST_F(GCMStoreImplTest, Registrations) {
  scoped_ptr<GCMStore> gcm_store(BuildGCMStore());
  scoped_ptr<GCMStore::LoadResult> load_result;
  gcm_store->Load(base::Bind(
      &GCMStoreImplTest::LoadCallback, base::Unretained(this), &load_result));
  PumpLoop();

  // Add one registration with one sender.
  linked_ptr<RegistrationInfo> registration1(new RegistrationInfo);
  registration1->sender_ids.push_back("sender1");
  registration1->registration_id = "registration1";
  gcm_store->AddRegistration(
      "app1",
      registration1,
      base::Bind(&GCMStoreImplTest::UpdateCallback, base::Unretained(this)));
  PumpLoop();

  // Add one registration with multiple senders.
  linked_ptr<RegistrationInfo> registration2(new RegistrationInfo);
  registration2->sender_ids.push_back("sender2_1");
  registration2->sender_ids.push_back("sender2_2");
  registration2->registration_id = "registration2";
  gcm_store->AddRegistration(
      "app2",
      registration2,
      base::Bind(&GCMStoreImplTest::UpdateCallback, base::Unretained(this)));
  PumpLoop();

  gcm_store = BuildGCMStore().Pass();
  gcm_store->Load(base::Bind(
      &GCMStoreImplTest::LoadCallback, base::Unretained(this), &load_result));
  PumpLoop();

  ASSERT_EQ(2u, load_result->registrations.size());
  ASSERT_TRUE(load_result->registrations.find("app1") !=
              load_result->registrations.end());
  EXPECT_EQ(registration1->registration_id,
            load_result->registrations["app1"]->registration_id);
  ASSERT_EQ(1u, load_result->registrations["app1"]->sender_ids.size());
  EXPECT_EQ(registration1->sender_ids[0],
            load_result->registrations["app1"]->sender_ids[0]);
  ASSERT_TRUE(load_result->registrations.find("app2") !=
              load_result->registrations.end());
  EXPECT_EQ(registration2->registration_id,
            load_result->registrations["app2"]->registration_id);
  ASSERT_EQ(2u, load_result->registrations["app2"]->sender_ids.size());
  EXPECT_EQ(registration2->sender_ids[0],
            load_result->registrations["app2"]->sender_ids[0]);
  EXPECT_EQ(registration2->sender_ids[1],
            load_result->registrations["app2"]->sender_ids[1]);
}

// Verify saving some incoming messages, reopening the directory, and then
// removing those incoming messages.
TEST_F(GCMStoreImplTest, IncomingMessages) {
  scoped_ptr<GCMStore> gcm_store(BuildGCMStore());
  scoped_ptr<GCMStore::LoadResult> load_result;
  gcm_store->Load(base::Bind(
      &GCMStoreImplTest::LoadCallback, base::Unretained(this), &load_result));
  PumpLoop();

  std::vector<std::string> persistent_ids;
  for (int i = 0; i < kNumPersistentIds; ++i) {
    persistent_ids.push_back(GetNextPersistentId());
    gcm_store->AddIncomingMessage(
        persistent_ids.back(),
        base::Bind(&GCMStoreImplTest::UpdateCallback, base::Unretained(this)));
    PumpLoop();
  }

  gcm_store = BuildGCMStore().Pass();
  gcm_store->Load(base::Bind(
      &GCMStoreImplTest::LoadCallback, base::Unretained(this), &load_result));
  PumpLoop();

  ASSERT_EQ(persistent_ids, load_result->incoming_messages);
  ASSERT_TRUE(load_result->outgoing_messages.empty());

  gcm_store->RemoveIncomingMessages(
      persistent_ids,
      base::Bind(&GCMStoreImplTest::UpdateCallback, base::Unretained(this)));
  PumpLoop();

  gcm_store = BuildGCMStore().Pass();
  load_result->incoming_messages.clear();
  gcm_store->Load(base::Bind(
      &GCMStoreImplTest::LoadCallback, base::Unretained(this), &load_result));
  PumpLoop();

  ASSERT_TRUE(load_result->incoming_messages.empty());
  ASSERT_TRUE(load_result->outgoing_messages.empty());
}

// Verify saving some outgoing messages, reopening the directory, and then
// removing those outgoing messages.
TEST_F(GCMStoreImplTest, OutgoingMessages) {
  scoped_ptr<GCMStore> gcm_store(BuildGCMStore());
  scoped_ptr<GCMStore::LoadResult> load_result;
  gcm_store->Load(base::Bind(
      &GCMStoreImplTest::LoadCallback, base::Unretained(this), &load_result));
  PumpLoop();

  std::vector<std::string> persistent_ids;
  const int kNumPersistentIds = 10;
  for (int i = 0; i < kNumPersistentIds; ++i) {
    persistent_ids.push_back(GetNextPersistentId());
    mcs_proto::DataMessageStanza message;
    message.set_from(kAppName + persistent_ids.back());
    message.set_category(kCategoryName + persistent_ids.back());
    gcm_store->AddOutgoingMessage(
        persistent_ids.back(),
        MCSMessage(message),
        base::Bind(&GCMStoreImplTest::UpdateCallback, base::Unretained(this)));
    PumpLoop();
  }

  gcm_store = BuildGCMStore().Pass();
  gcm_store->Load(base::Bind(
      &GCMStoreImplTest::LoadCallback, base::Unretained(this), &load_result));
  PumpLoop();

  ASSERT_TRUE(load_result->incoming_messages.empty());
  ASSERT_EQ(load_result->outgoing_messages.size(), persistent_ids.size());
  for (int i = 0; i < kNumPersistentIds; ++i) {
    std::string id = persistent_ids[i];
    ASSERT_TRUE(load_result->outgoing_messages[id].get());
    const mcs_proto::DataMessageStanza* message =
        reinterpret_cast<mcs_proto::DataMessageStanza*>(
            load_result->outgoing_messages[id].get());
    ASSERT_EQ(message->from(), kAppName + id);
    ASSERT_EQ(message->category(), kCategoryName + id);
  }

  gcm_store->RemoveOutgoingMessages(
      persistent_ids,
      base::Bind(&GCMStoreImplTest::UpdateCallback, base::Unretained(this)));
  PumpLoop();

  gcm_store = BuildGCMStore().Pass();
  load_result->outgoing_messages.clear();
  gcm_store->Load(base::Bind(
      &GCMStoreImplTest::LoadCallback, base::Unretained(this), &load_result));
  PumpLoop();

  ASSERT_TRUE(load_result->incoming_messages.empty());
  ASSERT_TRUE(load_result->outgoing_messages.empty());
}

// Verify incoming and outgoing messages don't conflict.
TEST_F(GCMStoreImplTest, IncomingAndOutgoingMessages) {
  scoped_ptr<GCMStore> gcm_store(BuildGCMStore());
  scoped_ptr<GCMStore::LoadResult> load_result;
  gcm_store->Load(base::Bind(
      &GCMStoreImplTest::LoadCallback, base::Unretained(this), &load_result));
  PumpLoop();

  std::vector<std::string> persistent_ids;
  const int kNumPersistentIds = 10;
  for (int i = 0; i < kNumPersistentIds; ++i) {
    persistent_ids.push_back(GetNextPersistentId());
    gcm_store->AddIncomingMessage(
        persistent_ids.back(),
        base::Bind(&GCMStoreImplTest::UpdateCallback, base::Unretained(this)));
    PumpLoop();

    mcs_proto::DataMessageStanza message;
    message.set_from(kAppName + persistent_ids.back());
    message.set_category(kCategoryName + persistent_ids.back());
    gcm_store->AddOutgoingMessage(
        persistent_ids.back(),
        MCSMessage(message),
        base::Bind(&GCMStoreImplTest::UpdateCallback, base::Unretained(this)));
    PumpLoop();
  }

  gcm_store = BuildGCMStore().Pass();
  gcm_store->Load(base::Bind(
      &GCMStoreImplTest::LoadCallback, base::Unretained(this), &load_result));
  PumpLoop();

  ASSERT_EQ(persistent_ids, load_result->incoming_messages);
  ASSERT_EQ(load_result->outgoing_messages.size(), persistent_ids.size());
  for (int i = 0; i < kNumPersistentIds; ++i) {
    std::string id = persistent_ids[i];
    ASSERT_TRUE(load_result->outgoing_messages[id].get());
    const mcs_proto::DataMessageStanza* message =
        reinterpret_cast<mcs_proto::DataMessageStanza*>(
            load_result->outgoing_messages[id].get());
    ASSERT_EQ(message->from(), kAppName + id);
    ASSERT_EQ(message->category(), kCategoryName + id);
  }

  gcm_store->RemoveIncomingMessages(
      persistent_ids,
      base::Bind(&GCMStoreImplTest::UpdateCallback, base::Unretained(this)));
  PumpLoop();
  gcm_store->RemoveOutgoingMessages(
      persistent_ids,
      base::Bind(&GCMStoreImplTest::UpdateCallback, base::Unretained(this)));
  PumpLoop();

  gcm_store = BuildGCMStore().Pass();
  load_result->incoming_messages.clear();
  load_result->outgoing_messages.clear();
  gcm_store->Load(base::Bind(
      &GCMStoreImplTest::LoadCallback, base::Unretained(this), &load_result));
  PumpLoop();

  ASSERT_TRUE(load_result->incoming_messages.empty());
  ASSERT_TRUE(load_result->outgoing_messages.empty());
}

// Test that per-app message limits are enforced, persisted across restarts,
// and updated as messages are removed.
TEST_F(GCMStoreImplTest, PerAppMessageLimits) {
  scoped_ptr<GCMStore> gcm_store(BuildGCMStore());
  scoped_ptr<GCMStore::LoadResult> load_result;
  gcm_store->Load(base::Bind(&GCMStoreImplTest::LoadCallback,
                             base::Unretained(this),
                             &load_result));

  // Add the initial (below app limit) messages.
  for (int i = 0; i < kNumMessagesPerApp; ++i) {
    mcs_proto::DataMessageStanza message;
    message.set_from(kAppName);
    message.set_category(kCategoryName);
    EXPECT_TRUE(gcm_store->AddOutgoingMessage(
                    base::IntToString(i),
                    MCSMessage(message),
                    base::Bind(&GCMStoreImplTest::UpdateCallback,
                               base::Unretained(this))));
    PumpLoop();
  }

  // Attempting to add some more should fail.
  for (int i = 0; i < kNumMessagesPerApp; ++i) {
    mcs_proto::DataMessageStanza message;
    message.set_from(kAppName);
    message.set_category(kCategoryName);
    EXPECT_FALSE(gcm_store->AddOutgoingMessage(
                     base::IntToString(i + kNumMessagesPerApp),
                     MCSMessage(message),
                     base::Bind(&GCMStoreImplTest::UpdateCallback,
                                base::Unretained(this))));
    PumpLoop();
  }

  // Tear down and restore the database.
  gcm_store = BuildGCMStore().Pass();
  gcm_store->Load(base::Bind(&GCMStoreImplTest::LoadCallback,
                             base::Unretained(this),
                             &load_result));
  PumpLoop();

  // Adding more messages should still fail.
  for (int i = 0; i < kNumMessagesPerApp; ++i) {
    mcs_proto::DataMessageStanza message;
    message.set_from(kAppName);
    message.set_category(kCategoryName);
    EXPECT_FALSE(gcm_store->AddOutgoingMessage(
                     base::IntToString(i + kNumMessagesPerApp),
                     MCSMessage(message),
                     base::Bind(&GCMStoreImplTest::UpdateCallback,
                                base::Unretained(this))));
    PumpLoop();
  }

  // Remove the existing messages.
  for (int i = 0; i < kNumMessagesPerApp; ++i) {
    gcm_store->RemoveOutgoingMessage(
        base::IntToString(i),
        base::Bind(&GCMStoreImplTest::UpdateCallback,
                   base::Unretained(this)));
    PumpLoop();
  }

  // Successfully add new messages.
  for (int i = 0; i < kNumMessagesPerApp; ++i) {
    mcs_proto::DataMessageStanza message;
    message.set_from(kAppName);
    message.set_category(kCategoryName);
    EXPECT_TRUE(gcm_store->AddOutgoingMessage(
                    base::IntToString(i + kNumMessagesPerApp),
                    MCSMessage(message),
                    base::Bind(&GCMStoreImplTest::UpdateCallback,
                               base::Unretained(this))));
    PumpLoop();
  }
}

TEST_F(GCMStoreImplTest, AccountMapping) {
  scoped_ptr<GCMStore> gcm_store(BuildGCMStore());
  scoped_ptr<GCMStore::LoadResult> load_result;
  gcm_store->Load(base::Bind(
      &GCMStoreImplTest::LoadCallback, base::Unretained(this), &load_result));

  // Add account mappings.
  AccountMapping account_mapping1;
  account_mapping1.account_id = "account_id_1";
  account_mapping1.email = "account_id_1@gmail.com";
  account_mapping1.access_token = "account_token1";
  account_mapping1.status = AccountMapping::ADDING;
  account_mapping1.status_change_timestamp = base::Time();
  account_mapping1.last_message_type = AccountMapping::MSG_ADD;
  account_mapping1.last_message_id = "message_1";

  AccountMapping account_mapping2;
  account_mapping2.account_id = "account_id_2";
  account_mapping2.email = "account_id_2@gmail.com";
  account_mapping2.access_token = "account_token1";
  account_mapping2.status = AccountMapping::REMOVING;
  account_mapping2.status_change_timestamp =
      base::Time::FromInternalValue(1305734521259935LL);
  account_mapping2.last_message_type = AccountMapping::MSG_REMOVE;
  account_mapping2.last_message_id = "message_2";

  gcm_store->AddAccountMapping(
      account_mapping1,
      base::Bind(&GCMStoreImplTest::UpdateCallback, base::Unretained(this)));
  PumpLoop();
  gcm_store->AddAccountMapping(
      account_mapping2,
      base::Bind(&GCMStoreImplTest::UpdateCallback, base::Unretained(this)));
  PumpLoop();

  gcm_store = BuildGCMStore().Pass();
  gcm_store->Load(base::Bind(
      &GCMStoreImplTest::LoadCallback, base::Unretained(this), &load_result));
  PumpLoop();

  EXPECT_EQ(2UL, load_result->account_mappings.size());
  GCMStore::AccountMappingMap::iterator iter =
      load_result->account_mappings.begin();
  EXPECT_EQ("account_id_1", iter->first);
  EXPECT_EQ(account_mapping1.account_id, iter->second.account_id);
  EXPECT_EQ(account_mapping1.email, iter->second.email);
  EXPECT_TRUE(iter->second.access_token.empty());
  EXPECT_EQ(AccountMapping::ADDING, iter->second.status);
  EXPECT_EQ(account_mapping1.status_change_timestamp,
            iter->second.status_change_timestamp);
  EXPECT_EQ(account_mapping1.last_message_type, iter->second.last_message_type);
  EXPECT_EQ(account_mapping1.last_message_id, iter->second.last_message_id);
  ++iter;
  EXPECT_EQ("account_id_2", iter->first);
  EXPECT_EQ(account_mapping2.account_id, iter->second.account_id);
  EXPECT_EQ(account_mapping2.email, iter->second.email);
  EXPECT_TRUE(iter->second.access_token.empty());
  EXPECT_EQ(AccountMapping::REMOVING, iter->second.status);
  EXPECT_EQ(account_mapping2.status_change_timestamp,
            iter->second.status_change_timestamp);
  EXPECT_EQ(account_mapping2.last_message_type, iter->second.last_message_type);
  EXPECT_EQ(account_mapping2.last_message_id, iter->second.last_message_id);

  gcm_store->RemoveAccountMapping(
      account_mapping1.account_id,
      base::Bind(&GCMStoreImplTest::UpdateCallback, base::Unretained(this)));
  PumpLoop();

  gcm_store = BuildGCMStore().Pass();
  gcm_store->Load(base::Bind(
      &GCMStoreImplTest::LoadCallback, base::Unretained(this), &load_result));
  PumpLoop();

  EXPECT_EQ(1UL, load_result->account_mappings.size());
  iter = load_result->account_mappings.begin();
  EXPECT_EQ("account_id_2", iter->first);
  EXPECT_EQ(account_mapping2.account_id, iter->second.account_id);
  EXPECT_EQ(account_mapping2.email, iter->second.email);
  EXPECT_TRUE(iter->second.access_token.empty());
  EXPECT_EQ(AccountMapping::REMOVING, iter->second.status);
  EXPECT_EQ(account_mapping2.status_change_timestamp,
            iter->second.status_change_timestamp);
  EXPECT_EQ(account_mapping2.last_message_type, iter->second.last_message_type);
  EXPECT_EQ(account_mapping2.last_message_id, iter->second.last_message_id);
}

// When the database is destroyed, all database updates should fail. At the
// same time, they per-app message counts should not go up, as failures should
// result in decrementing the counts.
TEST_F(GCMStoreImplTest, AddMessageAfterDestroy) {
  scoped_ptr<GCMStore> gcm_store(BuildGCMStore());
  scoped_ptr<GCMStore::LoadResult> load_result;
  gcm_store->Load(base::Bind(&GCMStoreImplTest::LoadCallback,
                             base::Unretained(this),
                             &load_result));
  PumpLoop();
  gcm_store->Destroy(base::Bind(&GCMStoreImplTest::UpdateCallback,
                               base::Unretained(this)));
  PumpLoop();

  expected_success_ = false;
  for (int i = 0; i < kNumMessagesPerApp * 2; ++i) {
    mcs_proto::DataMessageStanza message;
    message.set_from(kAppName);
    message.set_category(kCategoryName);
    // Because all adds are failing, none should hit the per-app message limits.
    EXPECT_TRUE(gcm_store->AddOutgoingMessage(
                    base::IntToString(i),
                    MCSMessage(message),
                    base::Bind(&GCMStoreImplTest::UpdateCallback,
                               base::Unretained(this))));
    PumpLoop();
  }
}

TEST_F(GCMStoreImplTest, ReloadAfterClose) {
  scoped_ptr<GCMStore> gcm_store(BuildGCMStore());
  scoped_ptr<GCMStore::LoadResult> load_result;
  gcm_store->Load(base::Bind(&GCMStoreImplTest::LoadCallback,
                             base::Unretained(this),
                             &load_result));
  PumpLoop();

  gcm_store->Close();
  PumpLoop();

  gcm_store->Load(base::Bind(&GCMStoreImplTest::LoadCallback,
                             base::Unretained(this),
                             &load_result));
  PumpLoop();
}

}  // namespace

}  // namespace gcm
