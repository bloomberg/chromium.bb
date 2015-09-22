// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/messenger_impl.h"

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/proximity_auth/connection.h"
#include "components/proximity_auth/messenger_observer.h"
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
  ~MockSecureContext() override {}

  MOCK_CONST_METHOD0(GetReceivedAuthMessage, std::string());
  MOCK_CONST_METHOD0(GetProtocolVersion, ProtocolVersion());

  void Encode(const std::string& message,
              const MessageCallback& callback) override {
    callback.Run(message + kFakeEncodingSuffix);
  }

  void Decode(const std::string& encoded_message,
              const MessageCallback& callback) override {
    EXPECT_THAT(encoded_message, EndsWith(kFakeEncodingSuffix));
    std::string decoded_message = encoded_message;
    decoded_message.erase(decoded_message.rfind(kFakeEncodingSuffix));
    callback.Run(decoded_message);
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
    return make_scoped_ptr(new WireMessage(pending_payload_));
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

class MockMessengerObserver : public MessengerObserver {
 public:
  explicit MockMessengerObserver(Messenger* messenger) : messenger_(messenger) {
    messenger_->AddObserver(this);
  }
  virtual ~MockMessengerObserver() { messenger_->RemoveObserver(this); }

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
  // The messenger that |this| instance observes.
  Messenger* const messenger_;

  DISALLOW_COPY_AND_ASSIGN(MockMessengerObserver);
};

class TestMessenger : public MessengerImpl {
 public:
  TestMessenger()
      : MessengerImpl(make_scoped_ptr(new NiceMock<FakeConnection>()),
                      make_scoped_ptr(new NiceMock<MockSecureContext>())) {}
  ~TestMessenger() override {}

  // Simple getters for the mock objects owned by |this| messenger.
  FakeConnection* GetFakeConnection() {
    return static_cast<FakeConnection*>(connection());
  }
  MockSecureContext* GetMockSecureContext() {
    return static_cast<MockSecureContext*>(secure_context());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestMessenger);
};

}  // namespace

TEST(ProximityAuthMessengerImplTest, SupportsSignIn_ProtocolVersionThreeZero) {
  TestMessenger messenger;
  ON_CALL(*messenger.GetMockSecureContext(), GetProtocolVersion())
      .WillByDefault(Return(SecureContext::PROTOCOL_VERSION_THREE_ZERO));
  EXPECT_FALSE(messenger.SupportsSignIn());
}

TEST(ProximityAuthMessengerImplTest, SupportsSignIn_ProtocolVersionThreeOne) {
  TestMessenger messenger;
  ON_CALL(*messenger.GetMockSecureContext(), GetProtocolVersion())
      .WillByDefault(Return(SecureContext::PROTOCOL_VERSION_THREE_ONE));
  EXPECT_TRUE(messenger.SupportsSignIn());
}

TEST(ProximityAuthMessengerImplTest,
     OnConnectionStatusChanged_ConnectionDisconnects) {
  TestMessenger messenger;
  MockMessengerObserver observer(&messenger);

  EXPECT_CALL(observer, OnDisconnected());
  messenger.GetFakeConnection()->Disconnect();
}

TEST(ProximityAuthMessengerImplTest, DispatchUnlockEvent_SendsExpectedMessage) {
  TestMessenger messenger;
  messenger.DispatchUnlockEvent();

  WireMessage* message = messenger.GetFakeConnection()->current_message();
  ASSERT_TRUE(message);
  EXPECT_EQ(std::string(), message->permit_id());
  EXPECT_EQ(
      "{"
      "\"name\":\"easy_unlock\","
      "\"type\":\"event\""
      "}, but encoded",
      message->payload());
}

TEST(ProximityAuthMessengerImplTest, DispatchUnlockEvent_SendMessageFails) {
  TestMessenger messenger;
  MockMessengerObserver observer(&messenger);
  messenger.DispatchUnlockEvent();

  EXPECT_CALL(observer, OnUnlockEventSent(false));
  messenger.GetFakeConnection()->FinishSendingMessageWithSuccess(false);
}

TEST(ProximityAuthMessengerImplTest, DispatchUnlockEvent_SendMessageSucceeds) {
  TestMessenger messenger;
  MockMessengerObserver observer(&messenger);
  messenger.DispatchUnlockEvent();

  EXPECT_CALL(observer, OnUnlockEventSent(true));
  messenger.GetFakeConnection()->FinishSendingMessageWithSuccess(true);
}

TEST(ProximityAuthMessengerImplTest,
     RequestDecryption_SignInUnsupported_DoesntSendMessage) {
  TestMessenger messenger;
  ON_CALL(*messenger.GetMockSecureContext(), GetProtocolVersion())
      .WillByDefault(Return(SecureContext::PROTOCOL_VERSION_THREE_ZERO));
  messenger.RequestDecryption(kChallenge);
  EXPECT_FALSE(messenger.GetFakeConnection()->current_message());
}

TEST(ProximityAuthMessengerImplTest, RequestDecryption_SendsExpectedMessage) {
  TestMessenger messenger;
  messenger.RequestDecryption(kChallenge);

  WireMessage* message = messenger.GetFakeConnection()->current_message();
  ASSERT_TRUE(message);
  EXPECT_EQ(std::string(), message->permit_id());
  EXPECT_EQ(
      "{"
      "\"encrypted_data\":\"YSBtb3N0IGRpZmZpY3VsdCBjaGFsbGVuZ2U=\","
      "\"type\":\"decrypt_request\""
      "}, but encoded",
      message->payload());
}

TEST(ProximityAuthMessengerImplTest,
     RequestDecryption_SendsExpectedMessage_UsingBase64UrlEncoding) {
  TestMessenger messenger;
  messenger.RequestDecryption("\xFF\xE6");

  WireMessage* message = messenger.GetFakeConnection()->current_message();
  ASSERT_TRUE(message);
  EXPECT_EQ(std::string(), message->permit_id());
  EXPECT_EQ(
      "{"
      "\"encrypted_data\":\"_-Y=\","
      "\"type\":\"decrypt_request\""
      "}, but encoded",
      message->payload());
}

TEST(ProximityAuthMessengerImplTest, RequestDecryption_SendMessageFails) {
  TestMessenger messenger;
  MockMessengerObserver observer(&messenger);
  messenger.RequestDecryption(kChallenge);

  EXPECT_CALL(observer, OnDecryptResponseProxy(nullptr));
  messenger.GetFakeConnection()->FinishSendingMessageWithSuccess(false);
}

TEST(ProximityAuthMessengerImplTest,
     RequestDecryption_SendSucceeds_WaitsForReply) {
  TestMessenger messenger;
  MockMessengerObserver observer(&messenger);
  messenger.RequestDecryption(kChallenge);

  EXPECT_CALL(observer, OnDecryptResponseProxy(_)).Times(0);
  messenger.GetFakeConnection()->FinishSendingMessageWithSuccess(true);
}

TEST(ProximityAuthMessengerImplTest,
     RequestDecryption_SendSucceeds_NotifiesObserversOnReply_NoData) {
  TestMessenger messenger;
  MockMessengerObserver observer(&messenger);
  messenger.RequestDecryption(kChallenge);
  messenger.GetFakeConnection()->FinishSendingMessageWithSuccess(true);

  EXPECT_CALL(observer, OnDecryptResponseProxy(nullptr));
  messenger.GetFakeConnection()->ReceiveMessageWithPayload(
      "{\"type\":\"decrypt_response\"}, but encoded");
}

TEST(ProximityAuthMessengerImplTest,
     RequestDecryption_SendSucceeds_NotifiesObserversOnReply_InvalidData) {
  TestMessenger messenger;
  MockMessengerObserver observer(&messenger);
  messenger.RequestDecryption(kChallenge);
  messenger.GetFakeConnection()->FinishSendingMessageWithSuccess(true);

  EXPECT_CALL(observer, OnDecryptResponseProxy(nullptr));
  messenger.GetFakeConnection()->ReceiveMessageWithPayload(
      "{"
      "\"type\":\"decrypt_response\","
      "\"data\":\"not a base64-encoded string\""
      "}, but encoded");
}

TEST(ProximityAuthMessengerImplTest,
     RequestDecryption_SendSucceeds_NotifiesObserversOnReply_ValidData) {
  TestMessenger messenger;
  MockMessengerObserver observer(&messenger);
  messenger.RequestDecryption(kChallenge);
  messenger.GetFakeConnection()->FinishSendingMessageWithSuccess(true);

  EXPECT_CALL(observer, OnDecryptResponseProxy(Pointee(Eq("a winner is you"))));
  messenger.GetFakeConnection()->ReceiveMessageWithPayload(
      "{"
      "\"type\":\"decrypt_response\","
      "\"data\":\"YSB3aW5uZXIgaXMgeW91\""  // "a winner is you", base64-encoded
      "}, but encoded");
}

// Verify that the messenger correctly parses base64url encoded data.
TEST(ProximityAuthMessengerImplTest,
     RequestDecryption_SendSucceeds_ParsesBase64UrlEncodingInReply) {
  TestMessenger messenger;
  MockMessengerObserver observer(&messenger);
  messenger.RequestDecryption(kChallenge);
  messenger.GetFakeConnection()->FinishSendingMessageWithSuccess(true);

  EXPECT_CALL(observer, OnDecryptResponseProxy(Pointee(Eq("\xFF\xE6"))));
  messenger.GetFakeConnection()->ReceiveMessageWithPayload(
      "{"
      "\"type\":\"decrypt_response\","
      "\"data\":\"_-Y=\""  // "\0xFF\0xE6", base64url-encoded.
      "}, but encoded");
}

TEST(ProximityAuthMessengerImplTest,
     RequestUnlock_SignInUnsupported_DoesntSendMessage) {
  TestMessenger messenger;
  ON_CALL(*messenger.GetMockSecureContext(), GetProtocolVersion())
      .WillByDefault(Return(SecureContext::PROTOCOL_VERSION_THREE_ZERO));
  messenger.RequestUnlock();
  EXPECT_FALSE(messenger.GetFakeConnection()->current_message());
}

TEST(ProximityAuthMessengerImplTest, RequestUnlock_SendsExpectedMessage) {
  TestMessenger messenger;
  messenger.RequestUnlock();

  WireMessage* message = messenger.GetFakeConnection()->current_message();
  ASSERT_TRUE(message);
  EXPECT_EQ(std::string(), message->permit_id());
  EXPECT_EQ("{\"type\":\"unlock_request\"}, but encoded", message->payload());
}

TEST(ProximityAuthMessengerImplTest, RequestUnlock_SendMessageFails) {
  TestMessenger messenger;
  MockMessengerObserver observer(&messenger);
  messenger.RequestUnlock();

  EXPECT_CALL(observer, OnUnlockResponse(false));
  messenger.GetFakeConnection()->FinishSendingMessageWithSuccess(false);
}

TEST(ProximityAuthMessengerImplTest, RequestUnlock_SendSucceeds_WaitsForReply) {
  TestMessenger messenger;
  MockMessengerObserver observer(&messenger);
  messenger.RequestUnlock();

  EXPECT_CALL(observer, OnUnlockResponse(_)).Times(0);
  messenger.GetFakeConnection()->FinishSendingMessageWithSuccess(true);
}

TEST(ProximityAuthMessengerImplTest,
     RequestUnlock_SendSucceeds_NotifiesObserversOnReply) {
  TestMessenger messenger;
  MockMessengerObserver observer(&messenger);
  messenger.RequestUnlock();
  messenger.GetFakeConnection()->FinishSendingMessageWithSuccess(true);

  EXPECT_CALL(observer, OnUnlockResponse(true));
  messenger.GetFakeConnection()->ReceiveMessageWithPayload(
      "{\"type\":\"unlock_response\"}, but encoded");
}

TEST(ProximityAuthMessengerImplTest,
     OnMessageReceived_RemoteStatusUpdate_Invalid) {
  TestMessenger messenger;
  MockMessengerObserver observer(&messenger);

  // Receive a status update message that's missing all the data.
  EXPECT_CALL(observer, OnRemoteStatusUpdate(_)).Times(0);
  messenger.GetFakeConnection()->ReceiveMessageWithPayload(
      "{\"type\":\"status_update\"}, but encoded");
}

TEST(ProximityAuthMessengerImplTest,
     OnMessageReceived_RemoteStatusUpdate_Valid) {
  TestMessenger messenger;
  MockMessengerObserver observer(&messenger);

  EXPECT_CALL(observer,
              OnRemoteStatusUpdate(
                  AllOf(Field(&RemoteStatusUpdate::user_presence, USER_PRESENT),
                        Field(&RemoteStatusUpdate::secure_screen_lock_state,
                              SECURE_SCREEN_LOCK_ENABLED),
                        Field(&RemoteStatusUpdate::trust_agent_state,
                              TRUST_AGENT_UNSUPPORTED))));
  messenger.GetFakeConnection()->ReceiveMessageWithPayload(
      "{"
      "\"type\":\"status_update\","
      "\"user_presence\":\"present\","
      "\"secure_screen_lock\":\"enabled\","
      "\"trust_agent\":\"unsupported\""
      "}, but encoded");
}

TEST(ProximityAuthMessengerImplTest, OnMessageReceived_InvalidJSON) {
  TestMessenger messenger;
  StrictMock<MockMessengerObserver> observer(&messenger);
  messenger.RequestUnlock();
  messenger.GetFakeConnection()->FinishSendingMessageWithSuccess(true);

  // The StrictMock will verify that no observer methods are called.
  messenger.GetFakeConnection()->ReceiveMessageWithPayload(
      "Not JSON, but encoded");
}

TEST(ProximityAuthMessengerImplTest, OnMessageReceived_MissingTypeField) {
  TestMessenger messenger;
  StrictMock<MockMessengerObserver> observer(&messenger);
  messenger.RequestUnlock();
  messenger.GetFakeConnection()->FinishSendingMessageWithSuccess(true);

  // The StrictMock will verify that no observer methods are called.
  messenger.GetFakeConnection()->ReceiveMessageWithPayload(
      "{\"some key that's not 'type'\":\"some value\"}, but encoded");
}

TEST(ProximityAuthMessengerImplTest, OnMessageReceived_UnexpectedReply) {
  TestMessenger messenger;
  StrictMock<MockMessengerObserver> observer(&messenger);

  // The StrictMock will verify that no observer methods are called.
  messenger.GetFakeConnection()->ReceiveMessageWithPayload(
      "{\"type\":\"unlock_response\"}, but encoded");
}

TEST(ProximityAuthMessengerImplTest,
     OnMessageReceived_MismatchedReply_UnlockInReplyToDecrypt) {
  TestMessenger messenger;
  StrictMock<MockMessengerObserver> observer(&messenger);

  messenger.RequestDecryption(kChallenge);
  messenger.GetFakeConnection()->FinishSendingMessageWithSuccess(true);

  // The StrictMock will verify that no observer methods are called.
  messenger.GetFakeConnection()->ReceiveMessageWithPayload(
      "{\"type\":\"unlock_response\"}, but encoded");
}

TEST(ProximityAuthMessengerImplTest,
     OnMessageReceived_MismatchedReply_DecryptInReplyToUnlock) {
  TestMessenger messenger;
  StrictMock<MockMessengerObserver> observer(&messenger);

  messenger.RequestUnlock();
  messenger.GetFakeConnection()->FinishSendingMessageWithSuccess(true);

  // The StrictMock will verify that no observer methods are called.
  messenger.GetFakeConnection()->ReceiveMessageWithPayload(
      "{"
      "\"type\":\"decrypt_response\","
      "\"data\":\"YSB3aW5uZXIgaXMgeW91\""
      "}, but encoded");
}

TEST(ProximityAuthMessengerImplTest, BuffersMessages_WhileSending) {
  TestMessenger messenger;
  MockMessengerObserver observer(&messenger);

  // Initiate a decryption request, and then initiate an unlock request before
  // the decryption request is even finished sending.
  messenger.RequestDecryption(kChallenge);
  messenger.RequestUnlock();

  EXPECT_CALL(observer, OnDecryptResponseProxy(nullptr));
  messenger.GetFakeConnection()->FinishSendingMessageWithSuccess(false);

  EXPECT_CALL(observer, OnUnlockResponse(false));
  messenger.GetFakeConnection()->FinishSendingMessageWithSuccess(false);
}

TEST(ProximityAuthMessengerImplTest, BuffersMessages_WhileAwaitingReply) {
  TestMessenger messenger;
  MockMessengerObserver observer(&messenger);

  // Initiate a decryption request, and allow the message to be sent.
  messenger.RequestDecryption(kChallenge);
  messenger.GetFakeConnection()->FinishSendingMessageWithSuccess(true);

  // At this point, the messenger is awaiting a reply to the decryption message.
  // While it's waiting, initiate an unlock request.
  messenger.RequestUnlock();

  // Now simulate a response arriving for the original decryption request.
  EXPECT_CALL(observer, OnDecryptResponseProxy(Pointee(Eq("a winner is you"))));
  messenger.GetFakeConnection()->ReceiveMessageWithPayload(
      "{"
      "\"type\":\"decrypt_response\","
      "\"data\":\"YSB3aW5uZXIgaXMgeW91\""
      "}, but encoded");

  // The unlock request should have remained buffered, and should only now be
  // sent.
  EXPECT_CALL(observer, OnUnlockResponse(false));
  messenger.GetFakeConnection()->FinishSendingMessageWithSuccess(false);
}

}  // namespace proximity_auth
