// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/device_to_device_authenticator.h"

#include <utility>

#include "base/base64url.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_vector.h"
#include "base/rand_util.h"
#include "base/timer/mock_timer.h"
#include "components/proximity_auth/connection.h"
#include "components/proximity_auth/cryptauth/fake_secure_message_delegate.h"
#include "components/proximity_auth/device_to_device_responder_operations.h"
#include "components/proximity_auth/proximity_auth_test_util.h"
#include "components/proximity_auth/secure_context.h"
#include "components/proximity_auth/wire_message.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace proximity_auth {

namespace {

// The account id of the user.
const char kAccountId[] = "example@gmail.com";

// The initiator's session public key in base64url form. Note that this is
// actually a serialized proto.
const char kInitiatorSessionPublicKeyBase64[] =
    "CAESRQogOlH8DgPMQu7eAt-b6yoTXcazG8mAl6SPC5Ds-LTULIcSIQDZDMqsoYRO4tNMej1FB"
    "El1sTiTiVDqrcGq-CkYCzDThw==";

// The initiator's session public key in base64url form. Note that this is
// actually a serialized proto.
const char kResponderSessionPublicKeyBase64[] =
    "CAESRgohAN9QYU5HySO14Gi9PDIClacBnC0C8wqPwXsNHUNG_vXlEiEAggzU80ZOd9DWuCBdp"
    "6bzpGcC-oj1yrwdVCHGg_yeaAQ=";

// Callback saving a string from |result| to |result_out|.
void SaveStringResult(std::string* result_out, const std::string& result) {
  *result_out = result;
}

// Callback saving a boolean from |result| to |result_out|.
void SaveBooleanResult(bool* result_out, bool result) {
  *result_out = result;
}

// Callback saving the result of ValidateHelloMessage().
void SaveValidateHelloMessageResult(bool* validated_out,
                                    std::string* public_key_out,
                                    bool validated,
                                    const std::string& public_key) {
  *validated_out = validated;
  *public_key_out = public_key;
}

// Connection implementation for testing.
class FakeConnection : public Connection {
 public:
  FakeConnection(const RemoteDevice& remote_device)
      : Connection(remote_device), connection_blocked_(false) {}
  ~FakeConnection() override {}

  // Connection:
  void Connect() override { SetStatus(Connection::Status::CONNECTED); }
  void Disconnect() override { SetStatus(Connection::Status::DISCONNECTED); }

  using Connection::OnBytesReceived;

  void ClearMessageBuffer() { message_buffer_.clear(); }

  const ScopedVector<WireMessage>& message_buffer() { return message_buffer_; }

  void set_connection_blocked(bool connection_blocked) {
    connection_blocked_ = connection_blocked;
  }

  bool connection_blocked() { return connection_blocked_; }

 protected:
  // Connection:
  void SendMessageImpl(std::unique_ptr<WireMessage> message) override {
    const WireMessage& message_alias = *message;
    message_buffer_.push_back(std::move(message));
    OnDidSendMessage(message_alias, !connection_blocked_);
  }

 private:
  ScopedVector<WireMessage> message_buffer_;

  bool connection_blocked_;

  DISALLOW_COPY_AND_ASSIGN(FakeConnection);
};

// Harness for testing DeviceToDeviceAuthenticator.
class DeviceToDeviceAuthenticatorForTest : public DeviceToDeviceAuthenticator {
 public:
  DeviceToDeviceAuthenticatorForTest(
      Connection* connection,
      std::unique_ptr<SecureMessageDelegate> secure_message_delegate)
      : DeviceToDeviceAuthenticator(connection,
                                    kAccountId,
                                    std::move(secure_message_delegate)),
        timer_(nullptr) {}
  ~DeviceToDeviceAuthenticatorForTest() override {}

  base::MockTimer* timer() { return timer_; }

 private:
  // DeviceToDeviceAuthenticator:
  std::unique_ptr<base::Timer> CreateTimer() override {
    bool retain_user_task = false;
    bool is_repeating = false;

    std::unique_ptr<base::MockTimer> timer(
        new base::MockTimer(retain_user_task, is_repeating));

    timer_ = timer.get();
    return std::move(timer);
  }

  // This instance is owned by the super class.
  base::MockTimer* timer_;

  DISALLOW_COPY_AND_ASSIGN(DeviceToDeviceAuthenticatorForTest);
};

}  // namespace

class ProximityAuthDeviceToDeviceAuthenticatorTest : public testing::Test {
 public:
  ProximityAuthDeviceToDeviceAuthenticatorTest()
      : remote_device_(CreateClassicRemoteDeviceForTest()),
        connection_(remote_device_),
        secure_message_delegate_(new FakeSecureMessageDelegate),
        authenticator_(&connection_,
                       base::WrapUnique(secure_message_delegate_)) {}
  ~ProximityAuthDeviceToDeviceAuthenticatorTest() override {}

  void SetUp() override {
    // Set up the session asymmetric keys for both the local and remote devices.
    ASSERT_TRUE(
        base::Base64UrlDecode(kInitiatorSessionPublicKeyBase64,
                              base::Base64UrlDecodePolicy::REQUIRE_PADDING,
                              &local_session_public_key_));
    ASSERT_TRUE(
        base::Base64UrlDecode(kResponderSessionPublicKeyBase64,
                              base::Base64UrlDecodePolicy::REQUIRE_PADDING,
                              &remote_session_public_key_));
    remote_session_private_key_ =
        secure_message_delegate_->GetPrivateKeyForPublicKey(
            remote_session_public_key_),

    secure_message_delegate_->set_next_public_key(local_session_public_key_);
    connection_.Connect();

    secure_message_delegate_->DeriveKey(
        remote_session_private_key_, local_session_public_key_,
        base::Bind(&SaveStringResult, &session_symmetric_key_));
  }

  // Begins authentication, and returns the [Hello] message sent from the local
  // device to the remote device.
  std::string BeginAuthentication() {
    authenticator_.Authenticate(base::Bind(
        &ProximityAuthDeviceToDeviceAuthenticatorTest::OnAuthenticationResult,
        base::Unretained(this)));

    EXPECT_EQ(1u, connection_.message_buffer().size());
    EXPECT_EQ(std::string("permit://google.com/easyunlock/v1/") + kAccountId,
              connection_.message_buffer()[0]->permit_id());
    std::string hello_message = connection_.message_buffer()[0]->payload();
    connection_.ClearMessageBuffer();

    bool validated = false;
    std::string local_session_public_key;
    DeviceToDeviceResponderOperations::ValidateHelloMessage(
        hello_message, remote_device_.persistent_symmetric_key,
        secure_message_delegate_,
        base::Bind(&SaveValidateHelloMessageResult, &validated,
                   &local_session_public_key));

    EXPECT_TRUE(validated);
    EXPECT_EQ(local_session_public_key_, local_session_public_key);

    return hello_message;
  }

  // Simulate receiving a valid [Responder Auth] message from the remote device.
  std::string SimulateResponderAuth(const std::string& hello_message) {
    std::string remote_device_private_key =
        secure_message_delegate_->GetPrivateKeyForPublicKey(
            kTestRemoteDevicePublicKey);

    std::string responder_auth_message;
    DeviceToDeviceResponderOperations::CreateResponderAuthMessage(
        hello_message, remote_session_public_key_, remote_session_private_key_,
        remote_device_private_key, remote_device_.persistent_symmetric_key,
        secure_message_delegate_,
        base::Bind(&SaveStringResult, &responder_auth_message));
    EXPECT_FALSE(responder_auth_message.empty());

    WireMessage wire_message(responder_auth_message);
    connection_.OnBytesReceived(wire_message.Serialize());

    return responder_auth_message;
  }

  void OnAuthenticationResult(Authenticator::Result result,
                              std::unique_ptr<SecureContext> secure_context) {
    secure_context_ = std::move(secure_context);
    OnAuthenticationResultProxy(result);
  }

  MOCK_METHOD1(OnAuthenticationResultProxy, void(Authenticator::Result result));

  // Contains information about the remote device.
  const RemoteDevice remote_device_;

  // Simulates the connection to the remote device.
  FakeConnection connection_;

  // The SecureMessageDelegate used by the authenticator.
  // Owned by |authenticator_|.
  FakeSecureMessageDelegate* secure_message_delegate_;

  // The DeviceToDeviceAuthenticator under test.
  DeviceToDeviceAuthenticatorForTest authenticator_;

  // The session keys in play during authentication.
  std::string local_session_public_key_;
  std::string remote_session_public_key_;
  std::string remote_session_private_key_;
  std::string session_symmetric_key_;

  // Stores the SecureContext returned after authentication succeeds.
  std::unique_ptr<SecureContext> secure_context_;

  DISALLOW_COPY_AND_ASSIGN(ProximityAuthDeviceToDeviceAuthenticatorTest);
};

TEST_F(ProximityAuthDeviceToDeviceAuthenticatorTest, AuthenticateSucceeds) {
  // Starts the authentication protocol and grab [Hello] message.
  std::string hello_message = BeginAuthentication();

  // Simulate receiving a valid [Responder Auth] from the remote device.
  EXPECT_CALL(*this,
              OnAuthenticationResultProxy(Authenticator::Result::SUCCESS));
  std::string responder_auth_message = SimulateResponderAuth(hello_message);
  EXPECT_TRUE(secure_context_);

  // Validate the local device sends a valid [Initiator Auth] message.
  ASSERT_EQ(1u, connection_.message_buffer().size());
  std::string initiator_auth = connection_.message_buffer()[0]->payload();

  bool initiator_auth_validated = false;
  DeviceToDeviceResponderOperations::ValidateInitiatorAuthMessage(
      initiator_auth, session_symmetric_key_,
      remote_device_.persistent_symmetric_key, responder_auth_message,
      secure_message_delegate_,
      base::Bind(&SaveBooleanResult, &initiator_auth_validated));
  ASSERT_TRUE(initiator_auth_validated);
}

TEST_F(ProximityAuthDeviceToDeviceAuthenticatorTest, ResponderRejectsHello) {
  std::string hello_message = BeginAuthentication();

  // If the responder could not validate the [Hello message], it essentially
  // sends random bytes back for privacy reasons.
  WireMessage wire_message(base::RandBytesAsString(300u));
  EXPECT_CALL(*this,
              OnAuthenticationResultProxy(Authenticator::Result::FAILURE));
  connection_.OnBytesReceived(wire_message.Serialize());
  EXPECT_FALSE(secure_context_);
}

TEST_F(ProximityAuthDeviceToDeviceAuthenticatorTest, ResponderAuthTimesOut) {
  // Starts the authentication protocol and grab [Hello] message.
  std::string hello_message = BeginAuthentication();
  ASSERT_TRUE(authenticator_.timer());
  EXPECT_CALL(*this,
              OnAuthenticationResultProxy(Authenticator::Result::FAILURE));
  authenticator_.timer()->Fire();
  EXPECT_FALSE(secure_context_);
}

TEST_F(ProximityAuthDeviceToDeviceAuthenticatorTest,
       DisconnectsWaitingForResponderAuth) {
  std::string hello_message = BeginAuthentication();
  EXPECT_CALL(*this,
              OnAuthenticationResultProxy(Authenticator::Result::DISCONNECTED));
  connection_.Disconnect();
  EXPECT_FALSE(secure_context_);
}

TEST_F(ProximityAuthDeviceToDeviceAuthenticatorTest, NotConnectedInitially) {
  connection_.Disconnect();
  EXPECT_CALL(*this,
              OnAuthenticationResultProxy(Authenticator::Result::DISCONNECTED));
  authenticator_.Authenticate(base::Bind(
      &ProximityAuthDeviceToDeviceAuthenticatorTest::OnAuthenticationResult,
      base::Unretained(this)));
  EXPECT_FALSE(secure_context_);
}

TEST_F(ProximityAuthDeviceToDeviceAuthenticatorTest, FailToSendHello) {
  connection_.set_connection_blocked(true);
  EXPECT_CALL(*this,
              OnAuthenticationResultProxy(Authenticator::Result::FAILURE));
  authenticator_.Authenticate(base::Bind(
      &ProximityAuthDeviceToDeviceAuthenticatorTest::OnAuthenticationResult,
      base::Unretained(this)));
  EXPECT_FALSE(secure_context_);
}

TEST_F(ProximityAuthDeviceToDeviceAuthenticatorTest, FailToSendInitiatorAuth) {
  std::string hello_message = BeginAuthentication();

  connection_.set_connection_blocked(true);
  EXPECT_CALL(*this,
              OnAuthenticationResultProxy(Authenticator::Result::FAILURE));
  SimulateResponderAuth(hello_message);
  EXPECT_FALSE(secure_context_);
}

TEST_F(ProximityAuthDeviceToDeviceAuthenticatorTest,
       SendMessagesAfterAuthenticationSuccess) {
  std::string hello_message = BeginAuthentication();
  EXPECT_CALL(*this,
              OnAuthenticationResultProxy(Authenticator::Result::SUCCESS));
  SimulateResponderAuth(hello_message);

  // Test that the authenticator is properly cleaned up after authentication
  // completes.
  WireMessage wire_message(base::RandBytesAsString(300u));
  connection_.SendMessage(
      base::WrapUnique(new WireMessage(base::RandBytesAsString(300u))));
  connection_.OnBytesReceived(wire_message.Serialize());
  connection_.SendMessage(
      base::WrapUnique(new WireMessage(base::RandBytesAsString(300u))));
  connection_.OnBytesReceived(wire_message.Serialize());
}

}  // namespace proximity_auth
