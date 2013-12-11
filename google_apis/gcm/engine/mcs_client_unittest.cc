// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/engine/mcs_client.h"

#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "components/webdata/encryptor/encryptor.h"
#include "google_apis/gcm/base/mcs_util.h"
#include "google_apis/gcm/engine/fake_connection_factory.h"
#include "google_apis/gcm/engine/fake_connection_handler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {

namespace {

const uint64 kAndroidId = 54321;
const uint64 kSecurityToken = 12345;

// Number of messages to send when testing batching.
// Note: must be even for tests that split batches in half.
const int kMessageBatchSize = 6;

// The number of unacked messages the client will receive before sending a
// stream ack.
// TODO(zea): get this (and other constants) directly from the mcs client.
const int kAckLimitSize = 10;

// Helper for building arbitrary data messages.
MCSMessage BuildDataMessage(const std::string& from,
                            const std::string& category,
                            int last_stream_id_received,
                            const std::string persistent_id) {
  mcs_proto::DataMessageStanza data_message;
  data_message.set_from(from);
  data_message.set_category(category);
  data_message.set_last_stream_id_received(last_stream_id_received);
  if (!persistent_id.empty())
    data_message.set_persistent_id(persistent_id);
  return MCSMessage(kDataMessageStanzaTag, data_message);
}

// MCSClient with overriden exposed persistent id logic.
class TestMCSClient : public MCSClient {
 public:
  TestMCSClient(const base::FilePath& rmq_path,
                ConnectionFactory* connection_factory,
                scoped_refptr<base::SequencedTaskRunner> blocking_task_runner)
    : MCSClient(rmq_path, connection_factory, blocking_task_runner),
      next_id_(0) {
  }

  virtual std::string GetNextPersistentId() OVERRIDE {
    return base::UintToString(++next_id_);
  }

 private:
  uint32 next_id_;
};

class MCSClientTest : public testing::Test {
 public:
  MCSClientTest();
  virtual ~MCSClientTest();

  void BuildMCSClient();
  void InitializeClient();
  void LoginClient(const std::vector<std::string>& acknowledged_ids);

  TestMCSClient* mcs_client() const { return mcs_client_.get(); }
  FakeConnectionFactory* connection_factory() {
    return &connection_factory_;
  }
  bool init_success() const { return init_success_; }
  uint64 restored_android_id() const { return restored_android_id_; }
  uint64 restored_security_token() const { return restored_security_token_; }
  MCSMessage* received_message() const { return received_message_.get(); }
  std::string sent_message_id() const { return sent_message_id_;}

  FakeConnectionHandler* GetFakeHandler() const;

  void WaitForMCSEvent();
  void PumpLoop();

 private:
  void InitializationCallback(bool success,
                              uint64 restored_android_id,
                              uint64 restored_security_token);
  void MessageReceivedCallback(const MCSMessage& message);
  void MessageSentCallback(const std::string& message_id);

  base::ScopedTempDir temp_directory_;
  base::MessageLoop message_loop_;
  scoped_ptr<base::RunLoop> run_loop_;

  FakeConnectionFactory connection_factory_;
  scoped_ptr<TestMCSClient> mcs_client_;
  bool init_success_;
  uint64 restored_android_id_;
  uint64 restored_security_token_;
  scoped_ptr<MCSMessage> received_message_;
  std::string sent_message_id_;
};

MCSClientTest::MCSClientTest()
    : run_loop_(new base::RunLoop()),
      init_success_(false),
      restored_android_id_(0),
      restored_security_token_(0) {
  EXPECT_TRUE(temp_directory_.CreateUniqueTempDir());
  run_loop_.reset(new base::RunLoop());

  // On OSX, prevent the Keychain permissions popup during unit tests.
#if defined(OS_MACOSX)
    Encryptor::UseMockKeychain(true);
#endif
}

MCSClientTest::~MCSClientTest() {}

void MCSClientTest::BuildMCSClient() {
  mcs_client_.reset(
      new TestMCSClient(temp_directory_.path(),
                        &connection_factory_,
                        message_loop_.message_loop_proxy()));
}

void MCSClientTest::InitializeClient() {
  mcs_client_->Initialize(base::Bind(&MCSClientTest::InitializationCallback,
                                     base::Unretained(this)),
                          base::Bind(&MCSClientTest::MessageReceivedCallback,
                                     base::Unretained(this)),
                          base::Bind(&MCSClientTest::MessageSentCallback,
                                     base::Unretained(this)));
  run_loop_->Run();
  run_loop_.reset(new base::RunLoop());
}

void MCSClientTest::LoginClient(
    const std::vector<std::string>& acknowledged_ids) {
  scoped_ptr<mcs_proto::LoginRequest> login_request =
      BuildLoginRequest(kAndroidId, kSecurityToken);
  for (size_t i = 0; i < acknowledged_ids.size(); ++i)
    login_request->add_received_persistent_id(acknowledged_ids[i]);
  GetFakeHandler()->ExpectOutgoingMessage(
      MCSMessage(kLoginRequestTag,
                 login_request.PassAs<const google::protobuf::MessageLite>()));
  mcs_client_->Login(kAndroidId, kSecurityToken);
  run_loop_->Run();
  run_loop_.reset(new base::RunLoop());
}

FakeConnectionHandler* MCSClientTest::GetFakeHandler() const {
  return reinterpret_cast<FakeConnectionHandler*>(
      connection_factory_.GetConnectionHandler());
}

void MCSClientTest::WaitForMCSEvent() {
  run_loop_->Run();
  run_loop_.reset(new base::RunLoop());
}

void MCSClientTest::PumpLoop() {
  run_loop_->RunUntilIdle();
  run_loop_.reset(new base::RunLoop());
}

void MCSClientTest::InitializationCallback(bool success,
                                           uint64 restored_android_id,
                                           uint64 restored_security_token) {
  init_success_ = success;
  restored_android_id_ = restored_android_id;
  restored_security_token_ = restored_security_token;
  DVLOG(1) << "Initialization callback invoked, killing loop.";
  run_loop_->Quit();
}

void MCSClientTest::MessageReceivedCallback(const MCSMessage& message) {
  received_message_.reset(new MCSMessage(message));
  DVLOG(1) << "Message received callback invoked, killing loop.";
  run_loop_->Quit();
}

void MCSClientTest::MessageSentCallback(const std::string& message_id) {
  DVLOG(1) << "Message sent callback invoked, killing loop.";
  run_loop_->Quit();
}

// Initialize a new client.
TEST_F(MCSClientTest, InitializeNew) {
  BuildMCSClient();
  InitializeClient();
  EXPECT_EQ(0U, restored_android_id());
  EXPECT_EQ(0U, restored_security_token());
  EXPECT_TRUE(init_success());
}

// Initialize a new client, shut it down, then restart the client. Should
// reload the existing device credentials.
TEST_F(MCSClientTest, InitializeExisting) {
  BuildMCSClient();
  InitializeClient();
  LoginClient(std::vector<std::string>());

  // Rebuild the client, to reload from the RMQ.
  BuildMCSClient();
  InitializeClient();
  EXPECT_EQ(kAndroidId, restored_android_id());
  EXPECT_EQ(kSecurityToken, restored_security_token());
  EXPECT_TRUE(init_success());
}

// Log in successfully to the MCS endpoint.
TEST_F(MCSClientTest, LoginSuccess) {
  BuildMCSClient();
  InitializeClient();
  LoginClient(std::vector<std::string>());
  EXPECT_TRUE(connection_factory()->IsEndpointReachable());
  EXPECT_TRUE(init_success());
  ASSERT_TRUE(received_message());
  EXPECT_EQ(kLoginResponseTag, received_message()->tag());
}

// Encounter a server error during the login attempt.
TEST_F(MCSClientTest, FailLogin) {
  BuildMCSClient();
  InitializeClient();
  GetFakeHandler()->set_fail_login(true);
  LoginClient(std::vector<std::string>());
  EXPECT_FALSE(connection_factory()->IsEndpointReachable());
  EXPECT_FALSE(init_success());
  EXPECT_FALSE(received_message());
}

// Send a message without RMQ support.
TEST_F(MCSClientTest, SendMessageNoRMQ) {
  BuildMCSClient();
  InitializeClient();
  LoginClient(std::vector<std::string>());
  MCSMessage message(BuildDataMessage("from", "category", 1, ""));
  GetFakeHandler()->ExpectOutgoingMessage(message);
  mcs_client()->SendMessage(message, false);
  EXPECT_TRUE(GetFakeHandler()->
                  AllOutgoingMessagesReceived());
}

// Send a message with RMQ support.
TEST_F(MCSClientTest, SendMessageRMQ) {
  BuildMCSClient();
  InitializeClient();
  LoginClient(std::vector<std::string>());
  MCSMessage message(BuildDataMessage("from", "category", 1, "1"));
  GetFakeHandler()->ExpectOutgoingMessage(message);
  mcs_client()->SendMessage(message, true);
  EXPECT_TRUE(GetFakeHandler()->
                  AllOutgoingMessagesReceived());
}

// Send a message with RMQ support while disconnected. On reconnect, the message
// should be resent.
TEST_F(MCSClientTest, SendMessageRMQWhileDisconnected) {
  BuildMCSClient();
  InitializeClient();
  LoginClient(std::vector<std::string>());
  GetFakeHandler()->set_fail_send(true);
  MCSMessage message(BuildDataMessage("from", "category", 1, "1"));

  // The initial (failed) send.
  GetFakeHandler()->ExpectOutgoingMessage(message);
  // The login request.
  GetFakeHandler()->ExpectOutgoingMessage(
      MCSMessage(kLoginRequestTag,
                 BuildLoginRequest(kAndroidId, kSecurityToken).
                     PassAs<const google::protobuf::MessageLite>()));
  // The second (re)send.
  GetFakeHandler()->ExpectOutgoingMessage(message);
  mcs_client()->SendMessage(message, true);
  EXPECT_FALSE(GetFakeHandler()->
                  AllOutgoingMessagesReceived());
  GetFakeHandler()->set_fail_send(false);
  connection_factory()->Connect();
  WaitForMCSEvent();  // Wait for the login to finish.
  PumpLoop();         // Wait for the send to happen.
  EXPECT_TRUE(GetFakeHandler()->
                  AllOutgoingMessagesReceived());
}

// Send a message with RMQ support without receiving an acknowledgement. On
// restart the message should be resent.
TEST_F(MCSClientTest, SendMessageRMQOnRestart) {
  BuildMCSClient();
  InitializeClient();
  LoginClient(std::vector<std::string>());
  GetFakeHandler()->set_fail_send(true);
  MCSMessage message(BuildDataMessage("from", "category", 1, "1"));

  // The initial (failed) send.
  GetFakeHandler()->ExpectOutgoingMessage(message);
  GetFakeHandler()->set_fail_send(false);
  mcs_client()->SendMessage(message, true);
  EXPECT_TRUE(GetFakeHandler()->
                  AllOutgoingMessagesReceived());

  // Rebuild the client, which should resend the old message.
  BuildMCSClient();
  InitializeClient();
  LoginClient(std::vector<std::string>());
  GetFakeHandler()->ExpectOutgoingMessage(message);
  PumpLoop();
  EXPECT_TRUE(GetFakeHandler()->
                  AllOutgoingMessagesReceived());
}

// Send messages with RMQ support, followed by receiving a stream ack. On
// restart nothing should be recent.
TEST_F(MCSClientTest, SendMessageRMQWithStreamAck) {
  BuildMCSClient();
  InitializeClient();
  LoginClient(std::vector<std::string>());

  // Send some messages.
  for (int i = 1; i <= kMessageBatchSize; ++i) {
    MCSMessage message(
        BuildDataMessage("from", "category", 1, base::IntToString(i)));
    GetFakeHandler()->ExpectOutgoingMessage(message);
    mcs_client()->SendMessage(message, true);
  }
  EXPECT_TRUE(GetFakeHandler()->
                  AllOutgoingMessagesReceived());

  // Receive the ack.
  scoped_ptr<mcs_proto::IqStanza> ack = BuildStreamAck();
  ack->set_last_stream_id_received(kMessageBatchSize + 1);
  GetFakeHandler()->ReceiveMessage(
      MCSMessage(kIqStanzaTag,
                 ack.PassAs<const google::protobuf::MessageLite>()));
  WaitForMCSEvent();

  // Reconnect and ensure no messages are resent.
  BuildMCSClient();
  InitializeClient();
  LoginClient(std::vector<std::string>());
  PumpLoop();
}

// Send messages with RMQ support. On restart, receive a SelectiveAck with
// the login response. No messages should be resent.
TEST_F(MCSClientTest, SendMessageRMQAckOnReconnect) {
  BuildMCSClient();
  InitializeClient();
  LoginClient(std::vector<std::string>());

  // Send some messages.
  std::vector<std::string> id_list;
  for (int i = 1; i <= kMessageBatchSize; ++i) {
    id_list.push_back(base::IntToString(i));
    MCSMessage message(
        BuildDataMessage("from", "category", 1, id_list.back()));
    GetFakeHandler()->ExpectOutgoingMessage(message);
    mcs_client()->SendMessage(message, true);
  }
  EXPECT_TRUE(GetFakeHandler()->
                  AllOutgoingMessagesReceived());

  // Rebuild the client, and receive an acknowledgment for the messages as
  // part of the login response.
  BuildMCSClient();
  InitializeClient();
  LoginClient(std::vector<std::string>());
  scoped_ptr<mcs_proto::IqStanza> ack(BuildSelectiveAck(id_list));
  GetFakeHandler()->ReceiveMessage(
      MCSMessage(kIqStanzaTag,
                 ack.PassAs<const google::protobuf::MessageLite>()));
  WaitForMCSEvent();
  EXPECT_TRUE(GetFakeHandler()->
                  AllOutgoingMessagesReceived());
}

// Send messages with RMQ support. On restart, receive a SelectiveAck with
// the login response that only acks some messages. The unacked messages should
// be resent.
TEST_F(MCSClientTest, SendMessageRMQPartialAckOnReconnect) {
  BuildMCSClient();
  InitializeClient();
  LoginClient(std::vector<std::string>());

  // Send some messages.
  std::vector<std::string> id_list;
  for (int i = 1; i <= kMessageBatchSize; ++i) {
    id_list.push_back(base::IntToString(i));
    MCSMessage message(
        BuildDataMessage("from", "category", 1, id_list.back()));
    GetFakeHandler()->ExpectOutgoingMessage(message);
    mcs_client()->SendMessage(message, true);
  }
  EXPECT_TRUE(GetFakeHandler()->
                  AllOutgoingMessagesReceived());

  // Rebuild the client, and receive an acknowledgment for the messages as
  // part of the login response.
  BuildMCSClient();
  InitializeClient();
  LoginClient(std::vector<std::string>());

  std::vector<std::string> acked_ids, remaining_ids;
  acked_ids.insert(acked_ids.end(),
                   id_list.begin(),
                   id_list.begin() + kMessageBatchSize / 2);
  remaining_ids.insert(remaining_ids.end(),
                       id_list.begin() + kMessageBatchSize / 2,
                       id_list.end());
  for (int i = 1; i <= kMessageBatchSize / 2; ++i) {
    MCSMessage message(
        BuildDataMessage("from",
                         "category",
                         2,
                         remaining_ids[i - 1]));
    GetFakeHandler()->ExpectOutgoingMessage(message);
  }
  scoped_ptr<mcs_proto::IqStanza> ack(BuildSelectiveAck(acked_ids));
  GetFakeHandler()->ReceiveMessage(
      MCSMessage(kIqStanzaTag,
                 ack.PassAs<const google::protobuf::MessageLite>()));
  WaitForMCSEvent();
  EXPECT_TRUE(GetFakeHandler()->
                  AllOutgoingMessagesReceived());
}

// Receive some messages. On restart, the login request should contain the
// appropriate acknowledged ids.
TEST_F(MCSClientTest, AckOnLogin) {
  BuildMCSClient();
  InitializeClient();
  LoginClient(std::vector<std::string>());

  // Receive some messages.
  std::vector<std::string> id_list;
  for (int i = 1; i <= kMessageBatchSize; ++i) {
    id_list.push_back(base::IntToString(i));
    MCSMessage message(
        BuildDataMessage("from", "category", i, id_list.back()));
    GetFakeHandler()->ReceiveMessage(message);
    WaitForMCSEvent();
    PumpLoop();
  }

  // Restart the client.
  BuildMCSClient();
  InitializeClient();
  LoginClient(id_list);
}

// Receive some messages. On the next send, the outgoing message should contain
// the appropriate last stream id received field to ack the received messages.
TEST_F(MCSClientTest, AckOnSend) {
  BuildMCSClient();
  InitializeClient();
  LoginClient(std::vector<std::string>());

  // Receive some messages.
  std::vector<std::string> id_list;
  for (int i = 1; i <= kMessageBatchSize; ++i) {
    id_list.push_back(base::IntToString(i));
    MCSMessage message(
        BuildDataMessage("from", "category", i, id_list.back()));
    GetFakeHandler()->ReceiveMessage(message);
    WaitForMCSEvent();
    PumpLoop();
  }

  // Trigger a message send, which should acknowledge via stream ack.
  MCSMessage message(
      BuildDataMessage("from", "category", kMessageBatchSize + 1, "1"));
  GetFakeHandler()->ExpectOutgoingMessage(message);
  mcs_client()->SendMessage(message, true);
  EXPECT_TRUE(GetFakeHandler()->
                  AllOutgoingMessagesReceived());
}

// Receive the ack limit in messages, which should trigger an automatic
// stream ack. Receive a heartbeat to confirm the ack.
TEST_F(MCSClientTest, AckWhenLimitReachedWithHeartbeat) {
  BuildMCSClient();
  InitializeClient();
  LoginClient(std::vector<std::string>());

  // The stream ack.
  scoped_ptr<mcs_proto::IqStanza> ack = BuildStreamAck();
  ack->set_last_stream_id_received(kAckLimitSize + 1);
  GetFakeHandler()->ExpectOutgoingMessage(
      MCSMessage(kIqStanzaTag,
                 ack.PassAs<const google::protobuf::MessageLite>()));

  // Receive some messages.
  std::vector<std::string> id_list;
  for (int i = 1; i <= kAckLimitSize; ++i) {
    id_list.push_back(base::IntToString(i));
    MCSMessage message(
        BuildDataMessage("from", "category", i, id_list.back()));
    GetFakeHandler()->ReceiveMessage(message);
    WaitForMCSEvent();
    PumpLoop();
  }
  EXPECT_TRUE(GetFakeHandler()->
                  AllOutgoingMessagesReceived());

  // Receive a heartbeat confirming the ack (and receive the heartbeat ack).
  scoped_ptr<mcs_proto::HeartbeatPing> heartbeat(
      new mcs_proto::HeartbeatPing());
  heartbeat->set_last_stream_id_received(2);

  scoped_ptr<mcs_proto::HeartbeatAck> heartbeat_ack(
      new mcs_proto::HeartbeatAck());
  heartbeat_ack->set_last_stream_id_received(kAckLimitSize + 2);
  GetFakeHandler()->ExpectOutgoingMessage(
      MCSMessage(kHeartbeatAckTag,
                 heartbeat_ack.PassAs<const google::protobuf::MessageLite>()));

  GetFakeHandler()->ReceiveMessage(
      MCSMessage(kHeartbeatPingTag,
                 heartbeat.PassAs<const google::protobuf::MessageLite>()));
  WaitForMCSEvent();
  EXPECT_TRUE(GetFakeHandler()->
                  AllOutgoingMessagesReceived());

  // Rebuild the client. Nothing should be sent on login.
  BuildMCSClient();
  InitializeClient();
  LoginClient(std::vector<std::string>());
  EXPECT_TRUE(GetFakeHandler()->
                  AllOutgoingMessagesReceived());
}

} // namespace

}  // namespace gcm
