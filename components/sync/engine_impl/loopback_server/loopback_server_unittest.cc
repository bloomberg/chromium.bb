// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "components/sync/base/cancelation_signal.h"
#include "components/sync/engine_impl/loopback_server/loopback_connection_manager.h"
#include "components/sync/engine_impl/syncer_proto_util.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync/test/engine/test_directory_setter_upper.h"
#include "testing/gtest/include/gtest/gtest.h"

using sync_pb::ClientToServerMessage;

namespace syncer {

class LoopbackServerTest : public testing::Test {
 public:
  void SetUp() override {
    base::CreateTemporaryFile(&persistent_file_);
    lcm_ =
        base::MakeUnique<LoopbackConnectionManager>(&signal_, persistent_file_);
  }

  static bool CallPostAndProcessHeaders(
      ServerConnectionManager* scm,
      SyncCycle* cycle,
      const sync_pb::ClientToServerMessage& msg,
      sync_pb::ClientToServerResponse* response) {
    return SyncerProtoUtil::PostAndProcessHeaders(scm, cycle, msg, response);
  }

 protected:
  ClientToServerMessage CreateCommitMessage() {
    ClientToServerMessage msg;
    SyncerProtoUtil::SetProtocolVersion(&msg);
    msg.set_share("required");
    msg.set_message_contents(ClientToServerMessage::COMMIT);
    msg.set_invalidator_client_id("client_id");
    auto* commit = msg.mutable_commit();
    commit->set_cache_guid("cache_guid");
    auto* entry = commit->add_entries();
    // Not quite well formed but enough to fool the server.
    entry->set_parent_id_string("bookmark_bar");
    entry->set_id_string("id_string");
    entry->set_version(0);
    entry->set_name("google");
    entry->mutable_specifics()->mutable_bookmark()->set_url("http://google.de");
    return msg;
  }

  CancelationSignal signal_;
  base::FilePath persistent_file_;
  std::unique_ptr<LoopbackConnectionManager> lcm_;
};

TEST_F(LoopbackServerTest, WrongBirthday) {
  ClientToServerMessage msg;
  SyncerProtoUtil::SetProtocolVersion(&msg);
  msg.set_share("required");
  msg.set_store_birthday("not_your_birthday");
  msg.set_message_contents(ClientToServerMessage::GET_UPDATES);
  msg.mutable_get_updates()->add_from_progress_marker()->set_data_type_id(
      sync_pb::EntitySpecifics::kBookmarkFieldNumber);
  sync_pb::ClientToServerResponse response;

  EXPECT_TRUE(CallPostAndProcessHeaders(lcm_.get(), NULL, msg, &response));
  EXPECT_EQ(sync_pb::SyncEnums::NOT_MY_BIRTHDAY, response.error_code());
}

TEST_F(LoopbackServerTest, GetUpdateCommand) {
  ClientToServerMessage msg;
  SyncerProtoUtil::SetProtocolVersion(&msg);
  msg.set_share("required");
  msg.set_message_contents(ClientToServerMessage::GET_UPDATES);
  msg.mutable_get_updates()->add_from_progress_marker()->set_data_type_id(
      sync_pb::EntitySpecifics::kBookmarkFieldNumber);
  sync_pb::ClientToServerResponse response;

  EXPECT_TRUE(CallPostAndProcessHeaders(lcm_.get(), NULL, msg, &response));
  EXPECT_EQ(sync_pb::SyncEnums::SUCCESS, response.error_code());
  ASSERT_TRUE(response.has_get_updates());
  // Expect to see the three top-level folders in this update already.
  EXPECT_EQ(3, response.get_updates().entries_size());
}

TEST_F(LoopbackServerTest, ClearServerDataCommand) {
  ClientToServerMessage msg;
  SyncerProtoUtil::SetProtocolVersion(&msg);
  msg.set_share("required");
  msg.set_message_contents(ClientToServerMessage::CLEAR_SERVER_DATA);
  sync_pb::ClientToServerResponse response;

  EXPECT_TRUE(CallPostAndProcessHeaders(lcm_.get(), NULL, msg, &response));
  EXPECT_EQ(sync_pb::SyncEnums::SUCCESS, response.error_code());
  EXPECT_TRUE(response.has_clear_server_data());
}

TEST_F(LoopbackServerTest, CommitCommand) {
  ClientToServerMessage msg = CreateCommitMessage();
  sync_pb::ClientToServerResponse response;

  EXPECT_TRUE(CallPostAndProcessHeaders(lcm_.get(), NULL, msg, &response));
  EXPECT_EQ(sync_pb::SyncEnums::SUCCESS, response.error_code());
  EXPECT_TRUE(response.has_commit());
}

TEST_F(LoopbackServerTest, LoadSavedState) {
  ClientToServerMessage commit_msg = CreateCommitMessage();

  sync_pb::ClientToServerResponse response;

  EXPECT_TRUE(
      CallPostAndProcessHeaders(lcm_.get(), NULL, commit_msg, &response));
  EXPECT_EQ(sync_pb::SyncEnums::SUCCESS, response.error_code());
  EXPECT_TRUE(response.has_commit());

  CancelationSignal signal;
  LoopbackConnectionManager second_user(&signal, persistent_file_);

  ClientToServerMessage get_updates_msg;
  SyncerProtoUtil::SetProtocolVersion(&get_updates_msg);
  get_updates_msg.set_share("required");
  get_updates_msg.set_message_contents(ClientToServerMessage::GET_UPDATES);
  get_updates_msg.mutable_get_updates()
      ->add_from_progress_marker()
      ->set_data_type_id(sync_pb::EntitySpecifics::kBookmarkFieldNumber);

  EXPECT_TRUE(CallPostAndProcessHeaders(&second_user, NULL, get_updates_msg,
                                        &response));
  EXPECT_EQ(sync_pb::SyncEnums::SUCCESS, response.error_code());
  ASSERT_TRUE(response.has_get_updates());
  // Expect to see the three top-level folders and the newly added bookmark!
  EXPECT_EQ(4, response.get_updates().entries_size());
}

TEST_F(LoopbackServerTest, CommitCommandUpdate) {
  ClientToServerMessage commit_msg_1 = CreateCommitMessage();
  sync_pb::ClientToServerResponse response;

  EXPECT_TRUE(
      CallPostAndProcessHeaders(lcm_.get(), NULL, commit_msg_1, &response));
  EXPECT_EQ(sync_pb::SyncEnums::SUCCESS, response.error_code());
  EXPECT_TRUE(response.has_commit());
  const std::string server_id = response.commit().entryresponse(0).id_string();

  ClientToServerMessage get_updates_msg;
  SyncerProtoUtil::SetProtocolVersion(&get_updates_msg);
  get_updates_msg.set_share("required");
  get_updates_msg.set_message_contents(ClientToServerMessage::GET_UPDATES);
  get_updates_msg.mutable_get_updates()
      ->add_from_progress_marker()
      ->set_data_type_id(sync_pb::EntitySpecifics::kBookmarkFieldNumber);

  EXPECT_TRUE(
      CallPostAndProcessHeaders(lcm_.get(), NULL, get_updates_msg, &response));
  EXPECT_EQ(sync_pb::SyncEnums::SUCCESS, response.error_code());
  ASSERT_TRUE(response.has_get_updates());
  // Expect to see the three top-level folders and the newly added bookmark!
  EXPECT_EQ(4, response.get_updates().entries_size());

  ClientToServerMessage commit_msg_2 = CreateCommitMessage();
  auto* entry = commit_msg_2.mutable_commit()->mutable_entries()->Mutable(0);
  entry->set_id_string(server_id);
  entry->set_version(1);
  entry->set_parent_id_string("other_bookmarks");
  entry->mutable_specifics()->mutable_bookmark()->set_url("http://google.bg");

  EXPECT_TRUE(
      CallPostAndProcessHeaders(lcm_.get(), NULL, commit_msg_2, &response));
  EXPECT_EQ(sync_pb::SyncEnums::SUCCESS, response.error_code());
  EXPECT_TRUE(response.has_commit());

  EXPECT_TRUE(
      CallPostAndProcessHeaders(lcm_.get(), NULL, get_updates_msg, &response));
  EXPECT_EQ(sync_pb::SyncEnums::SUCCESS, response.error_code());
  ASSERT_TRUE(response.has_get_updates());
  // Expect to see no fifth bookmark!
  EXPECT_EQ(4, response.get_updates().entries_size());
}

}  // namespace syncer
