// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/gcm_account_mapper.h"

#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "components/gcm_driver/fake_gcm_driver.h"
#include "google_apis/gcm/engine/account_mapping.h"
#include "google_apis/gcm/engine/gcm_store.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {

namespace {

const char kGCMAccountMapperSenderId[] = "745476177629";
const char kGCMAccountMapperAppId[] = "com.google.android.gms";
const char kRegistrationId[] = "reg_id";

AccountMapping MakeAccountMapping(const std::string& account_id,
                                  AccountMapping::MappingStatus status,
                                  const base::Time& status_change_timestamp,
                                  const std::string& last_message_id) {
  AccountMapping account_mapping;
  account_mapping.account_id = account_id;
  account_mapping.email = account_id + "@gmail.com";
  // account_mapping.access_token intentionally left empty.
  account_mapping.status = status;
  account_mapping.status_change_timestamp = status_change_timestamp;
  account_mapping.last_message_id = last_message_id;
  return account_mapping;
}

GCMClient::AccountTokenInfo MakeAccountTokenInfo(
    const std::string& account_id) {
  GCMClient::AccountTokenInfo account_token;
  account_token.account_id = account_id;
  account_token.email = account_id + "@gmail.com";
  account_token.access_token = account_id + "_token";
  return account_token;
}

class CustomFakeGCMDriver : public FakeGCMDriver {
 public:
  enum LastMessageAction {
    NONE,
    SEND_STARTED,
    SEND_FINISHED,
    SEND_ACKNOWLEDGED
  };

  CustomFakeGCMDriver();
  virtual ~CustomFakeGCMDriver();

  virtual void UpdateAccountMapping(
      const AccountMapping& account_mapping) OVERRIDE;
  virtual void RemoveAccountMapping(const std::string& account_id) OVERRIDE;
  virtual void AddAppHandler(const std::string& app_id,
                             GCMAppHandler* handler) OVERRIDE;
  virtual void RemoveAppHandler(const std::string& app_id) OVERRIDE;

  void CompleteSend(const std::string& message_id, GCMClient::Result result);
  void AcknowledgeSend(const std::string& message_id);
  void MessageSendError(const std::string& message_id);

  void CompleteSendAllMessages();
  void AcknowledgeSendAllMessages();

  void SetLastMessageAction(const std::string& message_id,
                            LastMessageAction action);
  void Clear();

  const AccountMapping& last_account_mapping() const {
    return account_mapping_;
  }
  const std::string& last_message_id() const { return last_message_id_; }
  const std::string& last_removed_account_id() const {
    return last_removed_account_id_;
  }
  LastMessageAction last_action() const { return last_action_; }

 protected:
  virtual void SendImpl(const std::string& app_id,
                        const std::string& receiver_id,
                        const GCMClient::OutgoingMessage& message) OVERRIDE;

 private:
  AccountMapping account_mapping_;
  std::string last_message_id_;
  std::string last_removed_account_id_;
  LastMessageAction last_action_;
  std::map<std::string, LastMessageAction> all_messages_;
};

CustomFakeGCMDriver::CustomFakeGCMDriver() : last_action_(NONE) {
}

CustomFakeGCMDriver::~CustomFakeGCMDriver() {
}

void CustomFakeGCMDriver::UpdateAccountMapping(
    const AccountMapping& account_mapping) {
  account_mapping_.email = account_mapping.email;
  account_mapping_.account_id = account_mapping.account_id;
  account_mapping_.access_token = account_mapping.access_token;
  account_mapping_.status = account_mapping.status;
  account_mapping_.status_change_timestamp =
      account_mapping.status_change_timestamp;
  account_mapping_.last_message_id = account_mapping.last_message_id;
}

void CustomFakeGCMDriver::RemoveAccountMapping(const std::string& account_id) {
  last_removed_account_id_ = account_id;
}

void CustomFakeGCMDriver::AddAppHandler(const std::string& app_id,
                                        GCMAppHandler* handler) {
  GCMDriver::AddAppHandler(app_id, handler);
}

void CustomFakeGCMDriver::RemoveAppHandler(const std::string& app_id) {
  GCMDriver::RemoveAppHandler(app_id);
}

void CustomFakeGCMDriver::CompleteSend(const std::string& message_id,
                                       GCMClient::Result result) {
  SendFinished(kGCMAccountMapperAppId, message_id, result);
  SetLastMessageAction(message_id, SEND_FINISHED);
}

void CustomFakeGCMDriver::AcknowledgeSend(const std::string& message_id) {
  GetAppHandler(kGCMAccountMapperAppId)
      ->OnSendAcknowledged(kGCMAccountMapperAppId, message_id);
  SetLastMessageAction(message_id, SEND_ACKNOWLEDGED);
}

void CustomFakeGCMDriver::MessageSendError(const std::string& message_id) {
  GCMClient::SendErrorDetails send_error;
  send_error.message_id = message_id;
  send_error.result = GCMClient::TTL_EXCEEDED;
  GetAppHandler(kGCMAccountMapperAppId)
      ->OnSendError(kGCMAccountMapperAppId, send_error);
}

void CustomFakeGCMDriver::SendImpl(const std::string& app_id,
                                   const std::string& receiver_id,
                                   const GCMClient::OutgoingMessage& message) {
  DCHECK_EQ(kGCMAccountMapperAppId, app_id);
  DCHECK_EQ(kGCMAccountMapperSenderId, receiver_id);

  SetLastMessageAction(message.id, SEND_STARTED);
}

void CustomFakeGCMDriver::CompleteSendAllMessages() {
  for (std::map<std::string, LastMessageAction>::const_iterator iter =
           all_messages_.begin();
       iter != all_messages_.end();
       ++iter) {
    if (iter->second == SEND_STARTED)
      CompleteSend(iter->first, GCMClient::SUCCESS);
  }
}

void CustomFakeGCMDriver::AcknowledgeSendAllMessages() {
  for (std::map<std::string, LastMessageAction>::const_iterator iter =
           all_messages_.begin();
       iter != all_messages_.end();
       ++iter) {
    if (iter->second == SEND_FINISHED)
      AcknowledgeSend(iter->first);
  }
}

void CustomFakeGCMDriver::Clear() {
  account_mapping_ = AccountMapping();
  last_message_id_.clear();
  last_removed_account_id_.clear();
  last_action_ = NONE;
}

void CustomFakeGCMDriver::SetLastMessageAction(const std::string& message_id,
                                               LastMessageAction action) {
  last_action_ = action;
  last_message_id_ = message_id;
  all_messages_[message_id] = action;
}

}  // namespace

class GCMAccountMapperTest : public testing::Test {
 public:
  GCMAccountMapperTest();
  virtual ~GCMAccountMapperTest();

  void Restart();

  const GCMAccountMapper::AccountMappings& GetAccounts() const {
    return account_mapper_->accounts_;
  }

  GCMAccountMapper* mapper() { return account_mapper_.get(); }

  CustomFakeGCMDriver& gcm_driver() { return gcm_driver_; }

  base::SimpleTestClock* clock() { return clock_; }

 private:
  CustomFakeGCMDriver gcm_driver_;
  scoped_ptr<GCMAccountMapper> account_mapper_;
  base::SimpleTestClock* clock_;
};

GCMAccountMapperTest::GCMAccountMapperTest() {
  Restart();
}

GCMAccountMapperTest::~GCMAccountMapperTest() {
}

void GCMAccountMapperTest::Restart() {
  if (account_mapper_)
    account_mapper_->ShutdownHandler();
  account_mapper_.reset(new GCMAccountMapper(&gcm_driver_));
  scoped_ptr<base::SimpleTestClock> clock(new base::SimpleTestClock);
  clock_ = clock.get();
  account_mapper_->SetClockForTesting(clock.PassAs<base::Clock>());
}

// Tests the initialization of account mappings (from the store) when empty.
TEST_F(GCMAccountMapperTest, InitializeAccountMappingsEmpty) {
  GCMAccountMapper::AccountMappings account_mappings;
  mapper()->Initialize(account_mappings, "");
  EXPECT_TRUE(GetAccounts().empty());
}

// Tests the initialization of account mappings (from the store).
TEST_F(GCMAccountMapperTest, InitializeAccountMappings) {
  GCMAccountMapper::AccountMappings account_mappings;
  AccountMapping account_mapping1 = MakeAccountMapping("acc_id1",
                                                       AccountMapping::MAPPED,
                                                       base::Time::Now(),
                                                       std::string());
  AccountMapping account_mapping2 = MakeAccountMapping("acc_id2",
                                                       AccountMapping::ADDING,
                                                       base::Time::Now(),
                                                       "add_message_1");
  account_mappings.push_back(account_mapping1);
  account_mappings.push_back(account_mapping2);

  mapper()->Initialize(account_mappings, "");

  GCMAccountMapper::AccountMappings mappings = GetAccounts();
  EXPECT_EQ(2UL, mappings.size());
  GCMAccountMapper::AccountMappings::const_iterator iter = mappings.begin();

  EXPECT_EQ(account_mapping1.account_id, iter->account_id);
  EXPECT_EQ(account_mapping1.email, iter->email);
  EXPECT_TRUE(account_mapping1.access_token.empty());
  EXPECT_EQ(account_mapping1.status, iter->status);
  EXPECT_EQ(account_mapping1.status_change_timestamp,
            iter->status_change_timestamp);
  EXPECT_TRUE(account_mapping1.last_message_id.empty());

  ++iter;
  EXPECT_EQ(account_mapping2.account_id, iter->account_id);
  EXPECT_EQ(account_mapping2.email, iter->email);
  EXPECT_TRUE(account_mapping2.access_token.empty());
  EXPECT_EQ(account_mapping2.status, iter->status);
  EXPECT_EQ(account_mapping2.status_change_timestamp,
            iter->status_change_timestamp);
  EXPECT_EQ(account_mapping2.last_message_id, iter->last_message_id);
}

// Tests the part where a new account is added with a token, to the point when
// GCM message is sent.
TEST_F(GCMAccountMapperTest, AddMappingToMessageSent) {
  mapper()->Initialize(GCMAccountMapper::AccountMappings(), kRegistrationId);

  std::vector<GCMClient::AccountTokenInfo> account_tokens;
  GCMClient::AccountTokenInfo account_token = MakeAccountTokenInfo("acc_id");
  account_tokens.push_back(account_token);
  mapper()->SetAccountTokens(account_tokens);

  GCMAccountMapper::AccountMappings mappings = GetAccounts();
  EXPECT_EQ(1UL, mappings.size());
  GCMAccountMapper::AccountMappings::const_iterator iter = mappings.begin();
  EXPECT_EQ("acc_id", iter->account_id);
  EXPECT_EQ("acc_id@gmail.com", iter->email);
  EXPECT_EQ("acc_id_token", iter->access_token);
  EXPECT_EQ(AccountMapping::NEW, iter->status);
  EXPECT_EQ(base::Time(), iter->status_change_timestamp);

  EXPECT_TRUE(!gcm_driver().last_message_id().empty());
}

// Tests the part where GCM message is successfully queued.
TEST_F(GCMAccountMapperTest, AddMappingMessageQueued) {
  mapper()->Initialize(GCMAccountMapper::AccountMappings(), kRegistrationId);

  std::vector<GCMClient::AccountTokenInfo> account_tokens;
  GCMClient::AccountTokenInfo account_token = MakeAccountTokenInfo("acc_id");
  account_tokens.push_back(account_token);
  mapper()->SetAccountTokens(account_tokens);

  clock()->SetNow(base::Time::Now());
  gcm_driver().CompleteSend(gcm_driver().last_message_id(), GCMClient::SUCCESS);

  EXPECT_EQ(account_token.email, gcm_driver().last_account_mapping().email);
  EXPECT_EQ(account_token.account_id,
            gcm_driver().last_account_mapping().account_id);
  EXPECT_EQ(account_token.access_token,
            gcm_driver().last_account_mapping().access_token);
  EXPECT_EQ(AccountMapping::ADDING, gcm_driver().last_account_mapping().status);
  EXPECT_EQ(clock()->Now(),
            gcm_driver().last_account_mapping().status_change_timestamp);
  EXPECT_EQ(gcm_driver().last_message_id(),
            gcm_driver().last_account_mapping().last_message_id);

  GCMAccountMapper::AccountMappings mappings = GetAccounts();
  GCMAccountMapper::AccountMappings::const_iterator iter = mappings.begin();
  EXPECT_EQ(account_token.email, iter->email);
  EXPECT_EQ(account_token.account_id, iter->account_id);
  EXPECT_EQ(account_token.access_token, iter->access_token);
  EXPECT_EQ(AccountMapping::ADDING, iter->status);
  EXPECT_EQ(clock()->Now(), iter->status_change_timestamp);
  EXPECT_EQ(gcm_driver().last_message_id(), iter->last_message_id);
}

// Tests status change from ADDING to MAPPED (Message is acknowledged).
TEST_F(GCMAccountMapperTest, AddMappingMessageAcknowledged) {
  mapper()->Initialize(GCMAccountMapper::AccountMappings(), kRegistrationId);

  std::vector<GCMClient::AccountTokenInfo> account_tokens;
  GCMClient::AccountTokenInfo account_token = MakeAccountTokenInfo("acc_id");
  account_tokens.push_back(account_token);
  mapper()->SetAccountTokens(account_tokens);

  clock()->SetNow(base::Time::Now());
  gcm_driver().CompleteSend(gcm_driver().last_message_id(), GCMClient::SUCCESS);
  clock()->SetNow(base::Time::Now());
  gcm_driver().AcknowledgeSend(gcm_driver().last_message_id());

  EXPECT_EQ(account_token.email, gcm_driver().last_account_mapping().email);
  EXPECT_EQ(account_token.account_id,
            gcm_driver().last_account_mapping().account_id);
  EXPECT_EQ(account_token.access_token,
            gcm_driver().last_account_mapping().access_token);
  EXPECT_EQ(AccountMapping::MAPPED, gcm_driver().last_account_mapping().status);
  EXPECT_EQ(clock()->Now(),
            gcm_driver().last_account_mapping().status_change_timestamp);
  EXPECT_TRUE(gcm_driver().last_account_mapping().last_message_id.empty());

  GCMAccountMapper::AccountMappings mappings = GetAccounts();
  GCMAccountMapper::AccountMappings::const_iterator iter = mappings.begin();
  EXPECT_EQ(account_token.email, iter->email);
  EXPECT_EQ(account_token.account_id, iter->account_id);
  EXPECT_EQ(account_token.access_token, iter->access_token);
  EXPECT_EQ(AccountMapping::MAPPED, iter->status);
  EXPECT_EQ(clock()->Now(), iter->status_change_timestamp);
  EXPECT_TRUE(iter->last_message_id.empty());
}

// Tests status change form ADDING to MAPPED (When message was acknowledged,
// after Chrome was restarted).
TEST_F(GCMAccountMapperTest, AddMappingMessageAckedAfterRestart) {
  mapper()->Initialize(GCMAccountMapper::AccountMappings(), kRegistrationId);

  std::vector<GCMClient::AccountTokenInfo> account_tokens;
  GCMClient::AccountTokenInfo account_token = MakeAccountTokenInfo("acc_id");
  account_tokens.push_back(account_token);
  mapper()->SetAccountTokens(account_tokens);

  clock()->SetNow(base::Time::Now());
  gcm_driver().CompleteSend(gcm_driver().last_message_id(), GCMClient::SUCCESS);

  Restart();
  GCMAccountMapper::AccountMappings stored_mappings;
  stored_mappings.push_back(gcm_driver().last_account_mapping());
  mapper()->Initialize(stored_mappings, kRegistrationId);

  clock()->SetNow(base::Time::Now());
  gcm_driver().AcknowledgeSend(gcm_driver().last_message_id());

  EXPECT_EQ(account_token.email, gcm_driver().last_account_mapping().email);
  EXPECT_EQ(account_token.account_id,
            gcm_driver().last_account_mapping().account_id);
  EXPECT_EQ(account_token.access_token,
            gcm_driver().last_account_mapping().access_token);
  EXPECT_EQ(AccountMapping::MAPPED, gcm_driver().last_account_mapping().status);
  EXPECT_EQ(clock()->Now(),
            gcm_driver().last_account_mapping().status_change_timestamp);
  EXPECT_TRUE(gcm_driver().last_account_mapping().last_message_id.empty());

  GCMAccountMapper::AccountMappings mappings = GetAccounts();
  GCMAccountMapper::AccountMappings::const_iterator iter = mappings.begin();
  EXPECT_EQ(account_token.email, iter->email);
  EXPECT_EQ(account_token.account_id, iter->account_id);
  EXPECT_EQ(account_token.access_token, iter->access_token);
  EXPECT_EQ(AccountMapping::MAPPED, iter->status);
  EXPECT_EQ(clock()->Now(), iter->status_change_timestamp);
  EXPECT_TRUE(iter->last_message_id.empty());
}

// Tests a case when ADD message times out for a new account.
TEST_F(GCMAccountMapperTest, AddMappingMessageSendErrorForNewAccount) {
  mapper()->Initialize(GCMAccountMapper::AccountMappings(), kRegistrationId);

  std::vector<GCMClient::AccountTokenInfo> account_tokens;
  GCMClient::AccountTokenInfo account_token = MakeAccountTokenInfo("acc_id");
  account_tokens.push_back(account_token);
  mapper()->SetAccountTokens(account_tokens);

  clock()->SetNow(base::Time::Now());
  gcm_driver().CompleteSend(gcm_driver().last_message_id(), GCMClient::SUCCESS);

  clock()->SetNow(base::Time::Now());
  std::string old_message_id = gcm_driver().last_message_id();
  gcm_driver().MessageSendError(old_message_id);

  // No new message is sent because of the send error, as the token is stale.
  // Because the account was new, the entry should be deleted.
  EXPECT_EQ(old_message_id, gcm_driver().last_message_id());
  EXPECT_EQ(account_token.account_id, gcm_driver().last_removed_account_id());
  EXPECT_TRUE(GetAccounts().empty());
}

/// Tests a case when ADD message times out for a MAPPED account.
TEST_F(GCMAccountMapperTest, AddMappingMessageSendErrorForMappedAccount) {
  // Start with one account that is mapped.
  base::Time status_change_timestamp = base::Time::Now();
  AccountMapping mapping = MakeAccountMapping("acc_id",
                                              AccountMapping::MAPPED,
                                              status_change_timestamp,
                                              "add_message_id");

  GCMAccountMapper::AccountMappings stored_mappings;
  stored_mappings.push_back(mapping);
  mapper()->Initialize(stored_mappings, kRegistrationId);

  clock()->SetNow(base::Time::Now());
  gcm_driver().MessageSendError("add_message_id");

  // No new message is sent because of the send error, as the token is stale.
  // Because the account was new, the entry should be deleted.
  EXPECT_TRUE(gcm_driver().last_message_id().empty());

  GCMAccountMapper::AccountMappings mappings = GetAccounts();
  GCMAccountMapper::AccountMappings::const_iterator iter = mappings.begin();
  EXPECT_EQ(mapping.email, iter->email);
  EXPECT_EQ(mapping.account_id, iter->account_id);
  EXPECT_EQ(mapping.access_token, iter->access_token);
  EXPECT_EQ(AccountMapping::MAPPED, iter->status);
  EXPECT_EQ(status_change_timestamp, iter->status_change_timestamp);
  EXPECT_TRUE(iter->last_message_id.empty());
}

// Tests that a missing token for an account will trigger removing of that
// account. This test goes only until the message is passed to GCM.
TEST_F(GCMAccountMapperTest, RemoveMappingToMessageSent) {
  // Start with one account that is mapped.
  AccountMapping mapping = MakeAccountMapping("acc_id",
                                              AccountMapping::MAPPED,
                                              base::Time::Now(),
                                              std::string());

  GCMAccountMapper::AccountMappings stored_mappings;
  stored_mappings.push_back(mapping);
  mapper()->Initialize(stored_mappings, kRegistrationId);
  clock()->SetNow(base::Time::Now());

  mapper()->SetAccountTokens(std::vector<GCMClient::AccountTokenInfo>());

  EXPECT_EQ(mapping.account_id, gcm_driver().last_account_mapping().account_id);
  EXPECT_EQ(mapping.email, gcm_driver().last_account_mapping().email);
  EXPECT_EQ(AccountMapping::REMOVING,
            gcm_driver().last_account_mapping().status);
  EXPECT_EQ(clock()->Now(),
            gcm_driver().last_account_mapping().status_change_timestamp);
  EXPECT_TRUE(gcm_driver().last_account_mapping().last_message_id.empty());

  GCMAccountMapper::AccountMappings mappings = GetAccounts();
  GCMAccountMapper::AccountMappings::const_iterator iter = mappings.begin();
  EXPECT_EQ(mapping.email, iter->email);
  EXPECT_EQ(mapping.account_id, iter->account_id);
  EXPECT_EQ(mapping.access_token, iter->access_token);
  EXPECT_EQ(AccountMapping::REMOVING, iter->status);
  EXPECT_EQ(clock()->Now(), iter->status_change_timestamp);
  EXPECT_TRUE(iter->last_message_id.empty());
}

// Tests that a missing token for an account will trigger removing of that
// account. This test goes until the message is queued by GCM.
TEST_F(GCMAccountMapperTest, RemoveMappingMessageQueued) {
  // Start with one account that is mapped.
  AccountMapping mapping = MakeAccountMapping("acc_id",
                                              AccountMapping::MAPPED,
                                              base::Time::Now(),
                                              std::string());

  GCMAccountMapper::AccountMappings stored_mappings;
  stored_mappings.push_back(mapping);
  mapper()->Initialize(stored_mappings, kRegistrationId);
  clock()->SetNow(base::Time::Now());
  base::Time status_change_timestamp = clock()->Now();

  mapper()->SetAccountTokens(std::vector<GCMClient::AccountTokenInfo>());
  clock()->SetNow(base::Time::Now());
  gcm_driver().CompleteSend(gcm_driver().last_message_id(), GCMClient::SUCCESS);

  EXPECT_EQ(mapping.account_id, gcm_driver().last_account_mapping().account_id);
  EXPECT_EQ(mapping.email, gcm_driver().last_account_mapping().email);
  EXPECT_EQ(AccountMapping::REMOVING,
            gcm_driver().last_account_mapping().status);
  EXPECT_EQ(status_change_timestamp,
            gcm_driver().last_account_mapping().status_change_timestamp);
  EXPECT_TRUE(!gcm_driver().last_account_mapping().last_message_id.empty());

  GCMAccountMapper::AccountMappings mappings = GetAccounts();
  GCMAccountMapper::AccountMappings::const_iterator iter = mappings.begin();
  EXPECT_EQ(mapping.email, iter->email);
  EXPECT_EQ(mapping.account_id, iter->account_id);
  EXPECT_EQ(mapping.access_token, iter->access_token);
  EXPECT_EQ(AccountMapping::REMOVING, iter->status);
  EXPECT_EQ(status_change_timestamp, iter->status_change_timestamp);
  EXPECT_EQ(gcm_driver().last_account_mapping().last_message_id,
            iter->last_message_id);
}

// Tests that a missing token for an account will trigger removing of that
// account. This test goes until the message is acknowledged by GCM.
// This is a complete success scenario for account removal, and it end with
// account mapping being completely gone.
TEST_F(GCMAccountMapperTest, RemoveMappingMessageAcknowledged) {
  // Start with one account that is mapped.
  AccountMapping mapping = MakeAccountMapping("acc_id",
                                              AccountMapping::MAPPED,
                                              base::Time::Now(),
                                              std::string());

  GCMAccountMapper::AccountMappings stored_mappings;
  stored_mappings.push_back(mapping);
  mapper()->Initialize(stored_mappings, kRegistrationId);
  clock()->SetNow(base::Time::Now());

  mapper()->SetAccountTokens(std::vector<GCMClient::AccountTokenInfo>());
  gcm_driver().CompleteSend(gcm_driver().last_message_id(), GCMClient::SUCCESS);
  gcm_driver().AcknowledgeSend(gcm_driver().last_message_id());

  EXPECT_EQ(mapping.account_id, gcm_driver().last_removed_account_id());

  GCMAccountMapper::AccountMappings mappings = GetAccounts();
  EXPECT_TRUE(mappings.empty());
}

// Tests that account removing proceeds, when a removing message is acked after
// Chrome was restarted.
TEST_F(GCMAccountMapperTest, RemoveMappingMessageAckedAfterRestart) {
  // Start with one account that is mapped.
  AccountMapping mapping = MakeAccountMapping("acc_id",
                                              AccountMapping::REMOVING,
                                              base::Time::Now(),
                                              "remove_message_id");

  GCMAccountMapper::AccountMappings stored_mappings;
  stored_mappings.push_back(mapping);
  mapper()->Initialize(stored_mappings, kRegistrationId);

  gcm_driver().AcknowledgeSend("remove_message_id");

  EXPECT_EQ(mapping.account_id, gcm_driver().last_removed_account_id());

  GCMAccountMapper::AccountMappings mappings = GetAccounts();
  EXPECT_TRUE(mappings.empty());
}

// Tests that account removing proceeds, when a removing message is acked after
// Chrome was restarted.
TEST_F(GCMAccountMapperTest, RemoveMappingMessageSendError) {
  // Start with one account that is mapped.
  base::Time status_change_timestamp = base::Time::Now();
  AccountMapping mapping = MakeAccountMapping("acc_id",
                                              AccountMapping::REMOVING,
                                              status_change_timestamp,
                                              "remove_message_id");

  GCMAccountMapper::AccountMappings stored_mappings;
  stored_mappings.push_back(mapping);
  mapper()->Initialize(stored_mappings, kRegistrationId);

  clock()->SetNow(base::Time::Now());
  gcm_driver().MessageSendError("remove_message_id");

  EXPECT_TRUE(gcm_driver().last_removed_account_id().empty());

  EXPECT_EQ(mapping.account_id, gcm_driver().last_account_mapping().account_id);
  EXPECT_EQ(mapping.email, gcm_driver().last_account_mapping().email);
  EXPECT_EQ(AccountMapping::REMOVING,
            gcm_driver().last_account_mapping().status);
  EXPECT_EQ(status_change_timestamp,
            gcm_driver().last_account_mapping().status_change_timestamp);
  // Message is not persisted, until send is completed.
  EXPECT_TRUE(gcm_driver().last_account_mapping().last_message_id.empty());

  GCMAccountMapper::AccountMappings mappings = GetAccounts();
  GCMAccountMapper::AccountMappings::const_iterator iter = mappings.begin();
  EXPECT_EQ(mapping.email, iter->email);
  EXPECT_EQ(mapping.account_id, iter->account_id);
  EXPECT_TRUE(iter->access_token.empty());
  EXPECT_EQ(AccountMapping::REMOVING, iter->status);
  EXPECT_EQ(status_change_timestamp, iter->status_change_timestamp);
  EXPECT_TRUE(iter->last_message_id.empty());
}

// Tests that, if a new token arrives when the adding message is in progress
// no new message is sent and account mapper still waits for the first one to
// complete.
TEST_F(GCMAccountMapperTest, TokenIsRefreshedWhenAdding) {
  mapper()->Initialize(GCMAccountMapper::AccountMappings(), kRegistrationId);

  clock()->SetNow(base::Time::Now());
  std::vector<GCMClient::AccountTokenInfo> account_tokens;
  GCMClient::AccountTokenInfo account_token = MakeAccountTokenInfo("acc_id");
  account_tokens.push_back(account_token);
  mapper()->SetAccountTokens(account_tokens);
  DCHECK_EQ(CustomFakeGCMDriver::SEND_STARTED, gcm_driver().last_action());

  clock()->SetNow(base::Time::Now());
  gcm_driver().CompleteSend(gcm_driver().last_message_id(), GCMClient::SUCCESS);
  DCHECK_EQ(CustomFakeGCMDriver::SEND_FINISHED, gcm_driver().last_action());

  // Providing another token and clearing status.
  gcm_driver().Clear();
  mapper()->SetAccountTokens(account_tokens);
  DCHECK_EQ(CustomFakeGCMDriver::NONE, gcm_driver().last_action());
}

// Tests that, if a new token arrives when a removing message is in progress
// a new adding message is sent and while account mapping status is changed to
// mapped. If the original Removing message arrives it is discarded.
TEST_F(GCMAccountMapperTest, TokenIsRefreshedWhenRemoving) {
  // Start with one account that is mapped.
  AccountMapping mapping = MakeAccountMapping(
      "acc_id", AccountMapping::MAPPED, base::Time::Now(), std::string());

  GCMAccountMapper::AccountMappings stored_mappings;
  stored_mappings.push_back(mapping);
  mapper()->Initialize(stored_mappings, kRegistrationId);
  clock()->SetNow(base::Time::Now());

  // Remove the token to trigger a remove message to be sent
  mapper()->SetAccountTokens(std::vector<GCMClient::AccountTokenInfo>());
  EXPECT_EQ(CustomFakeGCMDriver::SEND_STARTED, gcm_driver().last_action());
  gcm_driver().CompleteSend(gcm_driver().last_message_id(), GCMClient::SUCCESS);
  EXPECT_EQ(CustomFakeGCMDriver::SEND_FINISHED, gcm_driver().last_action());

  std::string remove_message_id = gcm_driver().last_message_id();
  gcm_driver().Clear();

  // The account mapping for acc_id is now in status REMOVING.
  // Adding the token for that account.
  clock()->SetNow(base::Time::Now());
  std::vector<GCMClient::AccountTokenInfo> account_tokens;
  GCMClient::AccountTokenInfo account_token = MakeAccountTokenInfo("acc_id");
  account_tokens.push_back(account_token);
  mapper()->SetAccountTokens(account_tokens);
  DCHECK_EQ(CustomFakeGCMDriver::SEND_STARTED, gcm_driver().last_action());
  gcm_driver().CompleteSend(gcm_driver().last_message_id(), GCMClient::SUCCESS);
  EXPECT_EQ(CustomFakeGCMDriver::SEND_FINISHED, gcm_driver().last_action());

  std::string add_message_id = gcm_driver().last_message_id();

  // A remove message confirmation arrives now, but should be ignored.
  gcm_driver().AcknowledgeSend(remove_message_id);

  GCMAccountMapper::AccountMappings mappings = GetAccounts();
  GCMAccountMapper::AccountMappings::const_iterator iter = mappings.begin();
  EXPECT_EQ(mapping.email, iter->email);
  EXPECT_EQ(mapping.account_id, iter->account_id);
  EXPECT_FALSE(iter->access_token.empty());
  EXPECT_EQ(AccountMapping::MAPPED, iter->status);
  // Status change timestamp is set to very long time ago, to make sure the next
  // round of mapping picks it up.
  EXPECT_EQ(base::Time(), iter->status_change_timestamp);
  EXPECT_EQ(add_message_id, iter->last_message_id);
}

// Tests adding/removing works for multiple accounts, after a restart and when
// tokens are periodically delierverd.
TEST_F(GCMAccountMapperTest, MultipleAccountMappings) {
  clock()->SetNow(base::Time::Now());
  base::Time half_hour_ago = clock()->Now() - base::TimeDelta::FromMinutes(30);
  GCMAccountMapper::AccountMappings stored_mappings;
  stored_mappings.push_back(MakeAccountMapping(
      "acc_id_0", AccountMapping::ADDING, half_hour_ago, "acc_id_0_msg"));
  stored_mappings.push_back(MakeAccountMapping(
      "acc_id_1", AccountMapping::MAPPED, half_hour_ago, "acc_id_1_msg"));
  stored_mappings.push_back(MakeAccountMapping(
      "acc_id_2", AccountMapping::REMOVING, half_hour_ago, "acc_id_2_msg"));

  mapper()->Initialize(stored_mappings, kRegistrationId);

  GCMAccountMapper::AccountMappings expected_mappings(stored_mappings);

  // Finish messages after a restart.
  clock()->SetNow(base::Time::Now());
  gcm_driver().AcknowledgeSend(expected_mappings[0].last_message_id);
  expected_mappings[0].status_change_timestamp = clock()->Now();
  expected_mappings[0].status = AccountMapping::MAPPED;
  expected_mappings[0].last_message_id.clear();

  clock()->SetNow(base::Time::Now());
  gcm_driver().AcknowledgeSend(expected_mappings[1].last_message_id);
  expected_mappings[1].status_change_timestamp = clock()->Now();
  expected_mappings[1].status = AccountMapping::MAPPED;
  expected_mappings[1].last_message_id.clear();

  // Upon success last element is removed.
  clock()->SetNow(base::Time::Now());
  gcm_driver().AcknowledgeSend(expected_mappings[2].last_message_id);
  expected_mappings.pop_back();

  GCMAccountMapper::AccountMappings mappings = GetAccounts();

  EXPECT_EQ(expected_mappings.size(), mappings.size());
  GCMAccountMapper::AccountMappings::const_iterator expected_iter =
      expected_mappings.begin();
  GCMAccountMapper::AccountMappings::const_iterator actual_iter =
      mappings.begin();

  for (; expected_iter != expected_mappings.end() &&
             actual_iter != mappings.end();
       ++expected_iter, ++actual_iter) {
    EXPECT_EQ(expected_iter->email, actual_iter->email);
    EXPECT_EQ(expected_iter->account_id, actual_iter->account_id);
    EXPECT_EQ(expected_iter->status, actual_iter->status);
    EXPECT_EQ(expected_iter->status_change_timestamp,
              actual_iter->status_change_timestamp);
  }

  // One of accounts gets removed.
  std::vector<GCMClient::AccountTokenInfo> account_tokens;
  account_tokens.push_back(MakeAccountTokenInfo("acc_id_0"));

  // Advance a day to make sure existing mappings will be reported.
  clock()->SetNow(clock()->Now() + base::TimeDelta::FromDays(1));
  mapper()->SetAccountTokens(account_tokens);

  expected_mappings[0].status = AccountMapping::MAPPED;
  expected_mappings[1].status = AccountMapping::REMOVING;
  expected_mappings[1].status_change_timestamp = clock()->Now();

  gcm_driver().CompleteSendAllMessages();

  for (; expected_iter != expected_mappings.end() &&
             actual_iter != mappings.end();
       ++expected_iter, ++actual_iter) {
    EXPECT_EQ(expected_iter->email, actual_iter->email);
    EXPECT_EQ(expected_iter->account_id, actual_iter->account_id);
    EXPECT_EQ(expected_iter->status, actual_iter->status);
    EXPECT_EQ(expected_iter->status_change_timestamp,
              actual_iter->status_change_timestamp);
  }

  clock()->SetNow(clock()->Now() + base::TimeDelta::FromSeconds(5));
  gcm_driver().AcknowledgeSendAllMessages();

  expected_mappings[0].status_change_timestamp = clock()->Now();
  expected_mappings.pop_back();

  for (; expected_iter != expected_mappings.end() &&
             actual_iter != mappings.end();
       ++expected_iter, ++actual_iter) {
    EXPECT_EQ(expected_iter->email, actual_iter->email);
    EXPECT_EQ(expected_iter->account_id, actual_iter->account_id);
    EXPECT_EQ(expected_iter->status, actual_iter->status);
    EXPECT_EQ(expected_iter->status_change_timestamp,
              actual_iter->status_change_timestamp);
  }

  account_tokens.clear();
  account_tokens.push_back(MakeAccountTokenInfo("acc_id_0"));
  account_tokens.push_back(MakeAccountTokenInfo("acc_id_3"));
  account_tokens.push_back(MakeAccountTokenInfo("acc_id_4"));

  // Advance a day to make sure existing mappings will be reported.
  clock()->SetNow(clock()->Now() + base::TimeDelta::FromDays(1));
  mapper()->SetAccountTokens(account_tokens);

  // Mapping from acc_id_0 still in position 0
  expected_mappings.push_back(MakeAccountMapping(
      "acc_id_3", AccountMapping::ADDING, base::Time(), std::string()));
  expected_mappings.push_back(MakeAccountMapping(
      "acc_id_4", AccountMapping::ADDING, base::Time(), std::string()));

  for (; expected_iter != expected_mappings.end() &&
             actual_iter != mappings.end();
       ++expected_iter, ++actual_iter) {
    EXPECT_EQ(expected_iter->email, actual_iter->email);
    EXPECT_EQ(expected_iter->account_id, actual_iter->account_id);
    EXPECT_EQ(expected_iter->status, actual_iter->status);
    EXPECT_EQ(expected_iter->status_change_timestamp,
              actual_iter->status_change_timestamp);
  }

  clock()->SetNow(clock()->Now() + base::TimeDelta::FromSeconds(1));
  gcm_driver().CompleteSendAllMessages();

  expected_mappings[1].status_change_timestamp = clock()->Now();
  expected_mappings[2].status_change_timestamp = clock()->Now();

  for (; expected_iter != expected_mappings.end() &&
             actual_iter != mappings.end();
       ++expected_iter, ++actual_iter) {
    EXPECT_EQ(expected_iter->email, actual_iter->email);
    EXPECT_EQ(expected_iter->account_id, actual_iter->account_id);
    EXPECT_EQ(expected_iter->status, actual_iter->status);
    EXPECT_EQ(expected_iter->status_change_timestamp,
              actual_iter->status_change_timestamp);
  }

  clock()->SetNow(clock()->Now() + base::TimeDelta::FromSeconds(5));
  gcm_driver().AcknowledgeSendAllMessages();

  expected_mappings[0].status_change_timestamp = clock()->Now();
  expected_mappings[1].status_change_timestamp = clock()->Now();
  expected_mappings[1].status = AccountMapping::MAPPED;
  expected_mappings[2].status_change_timestamp = clock()->Now();
  expected_mappings[2].status = AccountMapping::MAPPED;

  for (; expected_iter != expected_mappings.end() &&
             actual_iter != mappings.end();
       ++expected_iter, ++actual_iter) {
    EXPECT_EQ(expected_iter->email, actual_iter->email);
    EXPECT_EQ(expected_iter->account_id, actual_iter->account_id);
    EXPECT_EQ(expected_iter->status, actual_iter->status);
    EXPECT_EQ(expected_iter->status_change_timestamp,
              actual_iter->status_change_timestamp);
  }
}

}  // namespace gcm
