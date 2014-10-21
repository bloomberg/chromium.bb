// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/client.h"

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/proximity_auth/client_observer.h"
#include "components/proximity_auth/connection.h"
#include "components/proximity_auth/remote_device.h"
#include "components/proximity_auth/remote_status_update.h"
#include "components/proximity_auth/secure_context.h"
#include "components/proximity_auth/wire_message.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AllOf;
using testing::EndsWith;
using testing::Eq;
using testing::Field;
using testing::NiceMock;
using testing::Pointee;
using testing::Return;
using testing::StrictMock;

namespace proximity_auth {
namespace {

const char kChallenge[] = "a most difficult challenge";
const char kFakeEncodingSuffix[] = ", but encoded";

class MockSecureContext : public SecureContext {
 public:
  MockSecureContext() {
    // By default, mock a secure context that uses the 3.1 protocol. Individual
    // tests override this as needed.
    ON_CALL(*this, GetProtocolVersion())
        .WillByDefault(Return(SecureContext::PROTOCOL_VERSION_THREE_ONE));
  }
  virtual ~MockSecureContext() {}

  MOCK_CONST_METHOD0(GetReceivedAuthMessage, std::string());
  MOCK_CONST_METHOD0(GetProtocolVersion, ProtocolVersion());

  virtual std::string Encode(const std::string& message) override {
    return message + kFakeEncodingSuffix;
  }

  virtual std::string Decode(const std::string& encoded_message) override {
    EXPECT_THAT(encoded_message, EndsWith(kFakeEncodingSuffix));
    std::string decoded_message = encoded_message;
    decoded_message.erase(decoded_message.rfind(kFakeEncodingSuffix));
    return decoded_message;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSecureContext);
};

class FakeConnection : public Connection {
 public:
  FakeConnection() : Connection(RemoteDevice()) { Connect(); }
  ~FakeConnection() override { Disconnect(); }

  void Connect() override { SetStatus(CONNECTED); }

  void Disconnect() override { SetStatus(DISCONNECTED); }

  void SendMessageImpl(scoped_ptr<WireMessage> message) override {
    ASSERT_FALSE(current_message_);
    current_message_ = message.Pass();
  }

  // Completes the current send operation with success |success|.
  void FinishSendingMessageWithSuccess(bool success) {
    ASSERT_TRUE(current_message_);
    // Capture a copy of the message, as OnDidSendMessage() might reentrantly
    // call SendMessage().
    scoped_ptr<WireMessage> sent_message = current_message_.Pass();
    OnDidSendMessage(*sent_message, success);
  }

  // Simulates receiving a wire message with the given |payload|.
  void ReceiveMessageWithPayload(const std::string& payload) {
    pending_payload_ = payload;
    OnBytesReceived(std::string());
    pending_payload_.clear();
  }

  // Returns a message containing the payload set via
  // ReceiveMessageWithPayload().
  scoped_ptr<WireMessage> DeserializeWireMessage(
      bool* is_incomplete_message) override {
    *is_incomplete_message = false;
    return make_scoped_ptr(new WireMessage(std::string(), pending_payload_));
  }

  WireMessage* current_message() { return current_message_.get(); }

 private:
  // The message currently being sent. Only set between a call to
  // SendMessageImpl() and FinishSendingMessageWithSuccess().
  scoped_ptr<WireMessage> current_message_;

  // The payload that should be returned when DeserializeWireMessage() is
  // called.
  std::string pending_payload_;

  DISALLOW_COPY_AND_ASSIGN(FakeConnection);
};

class MockClientObserver : public ClientObserver {
 public:
  explicit MockClientObserver(Client* client) : client_(client) {
    client_->AddObserver(this);
  }
  virtual ~MockClientObserver() { client_->RemoveObserver(this); }

  MOCK_METHOD1(OnUnlockEventSent, void(bool success));
  MOCK_METHOD1(OnRemoteStatusUpdate,
               void(const RemoteStatusUpdate& status_update));
  MOCK_METHOD1(OnDecryptResponseProxy,
               void(const std::string* decrypted_bytes));
  MOCK_METHOD1(OnUnlockResponse, void(bool success));
  MOCK_METHOD0(OnDisconnected, void());

  virtual void OnDecryptResponse(scoped_ptr<std::string> decrypted_bytes) {
    OnDecryptResponseProxy(decrypted_bytes.get());
  }

 private:
  // The client that |this| instance observes.
  Client* const client_;

  DISALLOW_COPY_AND_ASSIGN(MockClientObserver);
};

class TestClient : public Client {
 public:
  TestClient()
      : Client(make_scoped_ptr(new NiceMock<FakeConnection>()),
               make_scoped_ptr(new NiceMock<MockSecureContext>())) {}
  ~TestClient() override {}

  // Simple getters for the mock objects owned by |this| client.
  FakeConnection* GetFakeConnection() {
    return static_cast<FakeConnection*>(connection());
  }
  MockSecureContext* GetMockSecureContext() {
    return static_cast<MockSecureContext*>(secure_context());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestClient);
};

}  // namespace

TEST(ProximityAuthClientTest, SupportsSignIn_ProtocolVersionThreeZero) {
  TestClient client;
  ON_CALL(*client.GetMockSecureContext(), GetProtocolVersion())
      .WillByDefault(Return(SecureContext::PROTOCOL_VERSION_THREE_ZERO));
  EXPECT_FALSE(client.SupportsSignIn());
}

TEST(ProximityAuthClientTest, SupportsSignIn_ProtocolVersionThreeOne) {
  TestClient client;
  ON_CALL(*client.GetMockSecureContext(), GetProtocolVersion())
      .WillByDefault(Return(SecureContext::PROTOCOL_VERSION_THREE_ONE));
  EXPECT_TRUE(client.SupportsSignIn());
}

TEST(ProximityAuthClientTest, OnConnectionStatusChanged_ConnectionDisconnects) {
  TestClient client;
  MockClientObserver observer(&client);

  EXPECT_CALL(observer, OnDisconnected());
  client.GetFakeConnection()->Disconnect();
}

TEST(ProximityAuthClientTest, DispatchUnlockEvent_SendsExpectedMessage) {
  TestClient client;
  client.DispatchUnlockEvent();

  WireMessage* message = client.GetFakeConnection()->current_message();
  ASSERT_TRUE(message);
  EXPECT_EQ(std::string(), message->permit_id());
  EXPECT_EQ(
      "{"
      "\"name\":\"easy_unlock\","
      "\"type\":\"event\""
      "}, but encoded",
      message->payload());
}

TEST(ProximityAuthClientTest, DispatchUnlockEvent_SendMessageFails) {
  TestClient client;
  MockClientObserver observer(&client);
  client.DispatchUnlockEvent();

  EXPECT_CALL(observer, OnUnlockEventSent(false));
  client.GetFakeConnection()->FinishSendingMessageWithSuccess(false);
}

TEST(ProximityAuthClientTest, DispatchUnlockEvent_SendMessageSucceeds) {
  TestClient client;
  MockClientObserver observer(&client);
  client.DispatchUnlockEvent();

  EXPECT_CALL(observer, OnUnlockEventSent(true));
  client.GetFakeConnection()->FinishSendingMessageWithSuccess(true);
}

TEST(ProximityAuthClientTest,
     RequestDecryption_SignInUnsupported_DoesntSendMessage) {
  TestClient client;
  ON_CALL(*client.GetMockSecureContext(), GetProtocolVersion())
      .WillByDefault(Return(SecureContext::PROTOCOL_VERSION_THREE_ZERO));
  client.RequestDecryption(kChallenge);
  EXPECT_FALSE(client.GetFakeConnection()->current_message());
}

TEST(ProximityAuthClientTest, RequestDecryption_SendsExpectedMessage) {
  TestClient client;
  client.RequestDecryption(kChallenge);

  WireMessage* message = client.GetFakeConnection()->current_message();
  ASSERT_TRUE(message);
  EXPECT_EQ(std::string(), message->permit_id());
  EXPECT_EQ(
      "{"
      "\"encrypted_data\":\"YSBtb3N0IGRpZmZpY3VsdCBjaGFsbGVuZ2U=\","
      "\"type\":\"decrypt_request\""
      "}, but encoded",
      message->payload());
}

TEST(ProximityAuthClientTest,
     RequestDecryption_SendsExpectedMessage_UsingBase64UrlEncoding) {
  TestClient client;
  client.RequestDecryption("\xFF\xE6");

  WireMessage* message = client.GetFakeConnection()->current_message();
  ASSERT_TRUE(message);
  EXPECT_EQ(std::string(), message->permit_id());
  EXPECT_EQ(
      "{"
      "\"encrypted_data\":\"_-Y=\","
      "\"type\":\"decrypt_request\""
      "}, but encoded",
      message->payload());
}

TEST(ProximityAuthClientTest, RequestDecryption_SendMessageFails) {
  TestClient client;
  MockClientObserver observer(&client);
  client.RequestDecryption(kChallenge);

  EXPECT_CALL(observer, OnDecryptResponseProxy(nullptr));
  client.GetFakeConnection()->FinishSendingMessageWithSuccess(false);
}

TEST(ProximityAuthClientTest, RequestDecryption_SendSucceeds_WaitsForReply) {
  TestClient client;
  MockClientObserver observer(&client);
  client.RequestDecryption(kChallenge);

  EXPECT_CALL(observer, OnDecryptResponseProxy(_)).Times(0);
  client.GetFakeConnection()->FinishSendingMessageWithSuccess(true);
}

TEST(ProximityAuthClientTest,
     RequestDecryption_SendSucceeds_NotifiesObserversOnReply_NoData) {
  TestClient client;
  MockClientObserver observer(&client);
  client.RequestDecryption(kChallenge);
  client.GetFakeConnection()->FinishSendingMessageWithSuccess(true);

  EXPECT_CALL(observer, OnDecryptResponseProxy(nullptr));
  client.GetFakeConnection()->ReceiveMessageWithPayload(
      "{\"type\":\"decrypt_response\"}, but encoded");
}

TEST(ProximityAuthClientTest,
     RequestDecryption_SendSucceeds_NotifiesObserversOnReply_InvalidData) {
  TestClient client;
  MockClientObserver observer(&client);
  client.RequestDecryption(kChallenge);
  client.GetFakeConnection()->FinishSendingMessageWithSuccess(true);

  EXPECT_CALL(observer, OnDecryptResponseProxy(nullptr));
  client.GetFakeConnection()->ReceiveMessageWithPayload(
      "{"
      "\"type\":\"decrypt_response\","
      "\"data\":\"not a base64-encoded string\""
      "}, but encoded");
}

TEST(ProximityAuthClientTest,
     RequestDecryption_SendSucceeds_NotifiesObserversOnReply_ValidData) {
  TestClient client;
  MockClientObserver observer(&client);
  client.RequestDecryption(kChallenge);
  client.GetFakeConnection()->FinishSendingMessageWithSuccess(true);

  EXPECT_CALL(observer, OnDecryptResponseProxy(Pointee(Eq("a winner is you"))));
  client.GetFakeConnection()->ReceiveMessageWithPayload(
      "{"
      "\"type\":\"decrypt_response\","
      "\"data\":\"YSB3aW5uZXIgaXMgeW91\""  // "a winner is you", base64-encoded
      "}, but encoded");
}

// Verify that the client correctly parses base64url encoded data.
TEST(ProximityAuthClientTest,
     RequestDecryption_SendSucceeds_ParsesBase64UrlEncodingInReply) {
  TestClient client;
  MockClientObserver observer(&client);
  client.RequestDecryption(kChallenge);
  client.GetFakeConnection()->FinishSendingMessageWithSuccess(true);

  EXPECT_CALL(observer, OnDecryptResponseProxy(Pointee(Eq("\xFF\xE6"))));
  client.GetFakeConnection()->ReceiveMessageWithPayload(
      "{"
      "\"type\":\"decrypt_response\","
      "\"data\":\"_-Y=\""  // "\0xFF\0xE6", base64url-encoded.
      "}, but encoded");
}

TEST(ProximityAuthClientTest,
     RequestUnlock_SignInUnsupported_DoesntSendMessage) {
  TestClient client;
  ON_CALL(*client.GetMockSecureContext(), GetProtocolVersion())
      .WillByDefault(Return(SecureContext::PROTOCOL_VERSION_THREE_ZERO));
  client.RequestUnlock();
  EXPECT_FALSE(client.GetFakeConnection()->current_message());
}

TEST(ProximityAuthClientTest, RequestUnlock_SendsExpectedMessage) {
  TestClient client;
  client.RequestUnlock();

  WireMessage* message = client.GetFakeConnection()->current_message();
  ASSERT_TRUE(message);
  EXPECT_EQ(std::string(), message->permit_id());
  EXPECT_EQ("{\"type\":\"unlock_request\"}, but encoded", message->payload());
}

TEST(ProximityAuthClientTest, RequestUnlock_SendMessageFails) {
  TestClient client;
  MockClientObserver observer(&client);
  client.RequestUnlock();

  EXPECT_CALL(observer, OnUnlockResponse(false));
  client.GetFakeConnection()->FinishSendingMessageWithSuccess(false);
}

TEST(ProximityAuthClientTest, RequestUnlock_SendSucceeds_WaitsForReply) {
  TestClient client;
  MockClientObserver observer(&client);
  client.RequestUnlock();

  EXPECT_CALL(observer, OnUnlockResponse(_)).Times(0);
  client.GetFakeConnection()->FinishSendingMessageWithSuccess(true);
}

TEST(ProximityAuthClientTest,
     RequestUnlock_SendSucceeds_NotifiesObserversOnReply) {
  TestClient client;
  MockClientObserver observer(&client);
  client.RequestUnlock();
  client.GetFakeConnection()->FinishSendingMessageWithSuccess(true);

  EXPECT_CALL(observer, OnUnlockResponse(true));
  client.GetFakeConnection()->ReceiveMessageWithPayload(
      "{\"type\":\"unlock_response\"}, but encoded");
}

TEST(ProximityAuthClientTest, OnMessageReceived_RemoteStatusUpdate_Invalid) {
  TestClient client;
  MockClientObserver observer(&client);

  // Receive a status update message that's missing all the data.
  EXPECT_CALL(observer, OnRemoteStatusUpdate(_)).Times(0);
  client.GetFakeConnection()->ReceiveMessageWithPayload(
      "{\"type\":\"status_update\"}, but encoded");
}

TEST(ProximityAuthClientTest, OnMessageReceived_RemoteStatusUpdate_Valid) {
  TestClient client;
  MockClientObserver observer(&client);

  EXPECT_CALL(observer,
              OnRemoteStatusUpdate(
                  AllOf(Field(&RemoteStatusUpdate::user_presence, USER_PRESENT),
                        Field(&RemoteStatusUpdate::secure_screen_lock_state,
                              SECURE_SCREEN_LOCK_ENABLED),
                        Field(&RemoteStatusUpdate::trust_agent_state,
                              TRUST_AGENT_UNSUPPORTED))));
  client.GetFakeConnection()->ReceiveMessageWithPayload(
      "{"
      "\"type\":\"status_update\","
      "\"user_presence\":\"present\","
      "\"secure_screen_lock\":\"enabled\","
      "\"trust_agent\":\"unsupported\""
      "}, but encoded");
}

TEST(ProximityAuthClientTest, OnMessageReceived_InvalidJSON) {
  TestClient client;
  StrictMock<MockClientObserver> observer(&client);
  client.RequestUnlock();
  client.GetFakeConnection()->FinishSendingMessageWithSuccess(true);

  // The StrictMock will verify that no observer methods are called.
  client.GetFakeConnection()->ReceiveMessageWithPayload(
      "Not JSON, but encoded");
}

TEST(ProximityAuthClientTest, OnMessageReceived_MissingTypeField) {
  TestClient client;
  StrictMock<MockClientObserver> observer(&client);
  client.RequestUnlock();
  client.GetFakeConnection()->FinishSendingMessageWithSuccess(true);

  // The StrictMock will verify that no observer methods are called.
  client.GetFakeConnection()->ReceiveMessageWithPayload(
      "{\"some key that's not 'type'\":\"some value\"}, but encoded");
}

TEST(ProximityAuthClientTest, OnMessageReceived_UnexpectedReply) {
  TestClient client;
  StrictMock<MockClientObserver> observer(&client);

  // The StrictMock will verify that no observer methods are called.
  client.GetFakeConnection()->ReceiveMessageWithPayload(
      "{\"type\":\"unlock_response\"}, but encoded");
}

TEST(ProximityAuthClientTest,
     OnMessageReceived_MismatchedReply_UnlockInReplyToDecrypt) {
  TestClient client;
  StrictMock<MockClientObserver> observer(&client);

  client.RequestDecryption(kChallenge);
  client.GetFakeConnection()->FinishSendingMessageWithSuccess(true);

  // The StrictMock will verify that no observer methods are called.
  client.GetFakeConnection()->ReceiveMessageWithPayload(
      "{\"type\":\"unlock_response\"}, but encoded");
}

TEST(ProximityAuthClientTest,
     OnMessageReceived_MismatchedReply_DecryptInReplyToUnlock) {
  TestClient client;
  StrictMock<MockClientObserver> observer(&client);

  client.RequestUnlock();
  client.GetFakeConnection()->FinishSendingMessageWithSuccess(true);

  // The StrictMock will verify that no observer methods are called.
  client.GetFakeConnection()->ReceiveMessageWithPayload(
      "{"
      "\"type\":\"decrypt_response\","
      "\"data\":\"YSB3aW5uZXIgaXMgeW91\""
      "}, but encoded");
}

TEST(ProximityAuthClientTest, BuffersMessages_WhileSending) {
  TestClient client;
  MockClientObserver observer(&client);

  // Initiate a decryption request, and then initiate an unlock request before
  // the decryption request is even finished sending.
  client.RequestDecryption(kChallenge);
  client.RequestUnlock();

  EXPECT_CALL(observer, OnDecryptResponseProxy(nullptr));
  client.GetFakeConnection()->FinishSendingMessageWithSuccess(false);

  EXPECT_CALL(observer, OnUnlockResponse(false));
  client.GetFakeConnection()->FinishSendingMessageWithSuccess(false);
}

TEST(ProximityAuthClientTest, BuffersMessages_WhileAwaitingReply) {
  TestClient client;
  MockClientObserver observer(&client);

  // Initiate a decryption request, and allow the message to be sent.
  client.RequestDecryption(kChallenge);
  client.GetFakeConnection()->FinishSendingMessageWithSuccess(true);

  // At this point, the client is awaiting a reply to the decryption message.
  // While it's waiting, initiate an unlock request.
  client.RequestUnlock();

  // Now simulate a response arriving for the original decryption request.
  EXPECT_CALL(observer, OnDecryptResponseProxy(Pointee(Eq("a winner is you"))));
  client.GetFakeConnection()->ReceiveMessageWithPayload(
      "{"
      "\"type\":\"decrypt_response\","
      "\"data\":\"YSB3aW5uZXIgaXMgeW91\""
      "}, but encoded");

  // The unlock request should have remained buffered, and should only now be
  // sent.
  EXPECT_CALL(observer, OnUnlockResponse(false));
  client.GetFakeConnection()->FinishSendingMessageWithSuccess(false);
}

}  // namespace proximity_auth
