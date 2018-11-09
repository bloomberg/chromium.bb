// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/proximity_auth/remote_device_life_cycle_impl.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/chromeos_features.h"
#include "chromeos/components/proximity_auth/messenger.h"
#include "chromeos/services/secure_channel/public/cpp/client/fake_client_channel.h"
#include "chromeos/services/secure_channel/public/cpp/client/fake_connection_attempt.h"
#include "chromeos/services/secure_channel/public/cpp/client/fake_secure_channel_client.h"
#include "components/cryptauth/authenticator.h"
#include "components/cryptauth/connection_finder.h"
#include "components/cryptauth/fake_connection.h"
#include "components/cryptauth/remote_device_test_util.h"
#include "components/cryptauth/secure_context.h"
#include "components/cryptauth/wire_message.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Mock;
using testing::NiceMock;
using testing::Return;
using testing::StrictMock;

namespace proximity_auth {

namespace {

class StubSecureContext : public cryptauth::SecureContext {
 public:
  StubSecureContext() {}
  ~StubSecureContext() override {}

  void Decode(const std::string& encoded_message,
              const MessageCallback& callback) override {
    NOTREACHED();
  }

  void Encode(const std::string& message,
              const MessageCallback& callback) override {
    NOTREACHED();
  }

  ProtocolVersion GetProtocolVersion() const override {
    NOTREACHED();
    return SecureContext::PROTOCOL_VERSION_THREE_ONE;
  }

  std::string GetChannelBindingData() const override { return std::string(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(StubSecureContext);
};

class FakeConnectionFinder : public cryptauth::ConnectionFinder {
 public:
  explicit FakeConnectionFinder(cryptauth::RemoteDeviceRef remote_device)
      : remote_device_(remote_device), connection_(nullptr) {}
  ~FakeConnectionFinder() override {}

  void OnConnectionFound() {
    ASSERT_FALSE(connection_callback_.is_null());
    std::unique_ptr<cryptauth::FakeConnection> scoped_connection_(
        new cryptauth::FakeConnection(remote_device_));
    connection_ = scoped_connection_.get();
    connection_callback_.Run(std::move(scoped_connection_));
  }

  cryptauth::FakeConnection* connection() { return connection_; }

 private:
  // cryptauth::ConnectionFinder:
  void Find(const cryptauth::ConnectionFinder::ConnectionCallback&
                connection_callback) override {
    ASSERT_TRUE(connection_callback_.is_null());
    connection_callback_ = connection_callback;
  }

  const cryptauth::RemoteDeviceRef remote_device_;

  cryptauth::FakeConnection* connection_;

  cryptauth::ConnectionFinder::ConnectionCallback connection_callback_;

  DISALLOW_COPY_AND_ASSIGN(FakeConnectionFinder);
};

class FakeAuthenticator : public cryptauth::Authenticator {
 public:
  explicit FakeAuthenticator(cryptauth::Connection* connection)
      : connection_(connection) {}
  ~FakeAuthenticator() override {
    // This object should be destroyed immediately after authentication is
    // complete in order not to outlive the underlying connection.
    EXPECT_FALSE(callback_.is_null());
    EXPECT_EQ(cryptauth::kTestRemoteDevicePublicKey,
              connection_->remote_device().public_key());
  }

  void OnAuthenticationResult(cryptauth::Authenticator::Result result) {
    ASSERT_FALSE(callback_.is_null());
    std::unique_ptr<cryptauth::SecureContext> secure_context;
    if (result == Authenticator::Result::SUCCESS)
      secure_context.reset(new StubSecureContext());
    callback_.Run(result, std::move(secure_context));
  }

 private:
  // cryptauth::Authenticator:
  void Authenticate(const AuthenticationCallback& callback) override {
    ASSERT_TRUE(callback_.is_null());
    callback_ = callback;
  }

  cryptauth::Connection* connection_;

  AuthenticationCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(FakeAuthenticator);
};

// Subclass of RemoteDeviceLifeCycleImpl to make it testable.
class TestableRemoteDeviceLifeCycleImpl : public RemoteDeviceLifeCycleImpl {
 public:
  TestableRemoteDeviceLifeCycleImpl(
      cryptauth::RemoteDeviceRef remote_device,
      base::Optional<cryptauth::RemoteDeviceRef> local_device,
      chromeos::secure_channel::SecureChannelClient* secure_channel_client)
      : RemoteDeviceLifeCycleImpl(remote_device,
                                  local_device,
                                  secure_channel_client),
        remote_device_(remote_device) {}

  ~TestableRemoteDeviceLifeCycleImpl() override {}

 private:
  const cryptauth::RemoteDeviceRef remote_device_;

  DISALLOW_COPY_AND_ASSIGN(TestableRemoteDeviceLifeCycleImpl);
};

}  // namespace

class ProximityAuthRemoteDeviceLifeCycleImplTest
    : public testing::Test,
      public RemoteDeviceLifeCycle::Observer {
 protected:
  ProximityAuthRemoteDeviceLifeCycleImplTest()
      : test_remote_device_(cryptauth::CreateRemoteDeviceRefForTest()),
        test_local_device_(cryptauth::CreateRemoteDeviceRefForTest()),
        fake_secure_channel_client_(
            std::make_unique<
                chromeos::secure_channel::FakeSecureChannelClient>()),
        life_cycle_(test_remote_device_,
                    test_local_device_,
                    fake_secure_channel_client_.get()),
        task_runner_(new base::TestSimpleTaskRunner()),
        thread_task_runner_handle_(task_runner_) {}

  ~ProximityAuthRemoteDeviceLifeCycleImplTest() override {
    life_cycle_.RemoveObserver(this);
  }

  void CreateFakeConnectionAttempt() {
    auto fake_connection_attempt =
        std::make_unique<chromeos::secure_channel::FakeConnectionAttempt>();
    fake_connection_attempt_ = fake_connection_attempt.get();
    fake_secure_channel_client_->set_next_listen_connection_attempt(
        test_remote_device_, test_local_device_,
        std::move(fake_connection_attempt));
  }

  void StartLifeCycle() {
    EXPECT_EQ(RemoteDeviceLifeCycle::State::STOPPED, life_cycle_.GetState());
    life_cycle_.AddObserver(this);

    EXPECT_CALL(*this, OnLifeCycleStateChanged(
                           RemoteDeviceLifeCycle::State::STOPPED,
                           RemoteDeviceLifeCycle::State::FINDING_CONNECTION));
    life_cycle_.Start();
    task_runner_->RunUntilIdle();
    Mock::VerifyAndClearExpectations(this);

    EXPECT_EQ(RemoteDeviceLifeCycle::State::FINDING_CONNECTION,
              life_cycle_.GetState());
  }

  void SimulateSuccessfulAuthenticatedConnection() {
    EXPECT_EQ(RemoteDeviceLifeCycle::State::FINDING_CONNECTION,
              life_cycle_.GetState());

    EXPECT_CALL(*this, OnLifeCycleStateChanged(
                           RemoteDeviceLifeCycle::State::FINDING_CONNECTION,
                           RemoteDeviceLifeCycle::State::AUTHENTICATING));

    auto fake_client_channel =
        std::make_unique<chromeos::secure_channel::FakeClientChannel>();
    auto* fake_client_channel_raw = fake_client_channel.get();
    fake_connection_attempt_->NotifyConnection(std::move(fake_client_channel));

    Mock::VerifyAndClearExpectations(this);

    EXPECT_CALL(*this,
                OnLifeCycleStateChanged(
                    RemoteDeviceLifeCycle::State::AUTHENTICATING,
                    RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED));
    task_runner_->RunUntilIdle();

    EXPECT_EQ(fake_client_channel_raw, life_cycle_.GetChannel());
    EXPECT_EQ(fake_client_channel_raw,
              life_cycle_.GetMessenger()->GetChannel());
  }

  void SimulateFailureToAuthenticateConnection(
      chromeos::secure_channel::mojom::ConnectionAttemptFailureReason
          failure_reason,
      RemoteDeviceLifeCycle::State expected_life_cycle_state) {
    EXPECT_EQ(RemoteDeviceLifeCycle::State::FINDING_CONNECTION,
              life_cycle_.GetState());

    EXPECT_CALL(*this, OnLifeCycleStateChanged(
                           RemoteDeviceLifeCycle::State::FINDING_CONNECTION,
                           expected_life_cycle_state));

    fake_connection_attempt_->NotifyConnectionAttemptFailure(failure_reason);

    EXPECT_EQ(nullptr, life_cycle_.GetChannel());
    EXPECT_EQ(nullptr, life_cycle_.GetMessenger());

    EXPECT_EQ(expected_life_cycle_state, life_cycle_.GetState());
  }

  MOCK_METHOD2(OnLifeCycleStateChanged,
               void(RemoteDeviceLifeCycle::State old_state,
                    RemoteDeviceLifeCycle::State new_state));

  cryptauth::RemoteDeviceRef test_remote_device_;
  cryptauth::RemoteDeviceRef test_local_device_;
  std::unique_ptr<chromeos::secure_channel::FakeSecureChannelClient>
      fake_secure_channel_client_;
  TestableRemoteDeviceLifeCycleImpl life_cycle_;

  chromeos::secure_channel::FakeConnectionAttempt* fake_connection_attempt_;

  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle thread_task_runner_handle_;
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProximityAuthRemoteDeviceLifeCycleImplTest);
};

TEST_F(ProximityAuthRemoteDeviceLifeCycleImplTest,
       MultiDeviceApiEnabled_Success) {
  CreateFakeConnectionAttempt();

  StartLifeCycle();
  SimulateSuccessfulAuthenticatedConnection();
}

TEST_F(ProximityAuthRemoteDeviceLifeCycleImplTest,
       MultiDeviceApiEnabled_Failure) {
  CreateFakeConnectionAttempt();

  StartLifeCycle();
  SimulateFailureToAuthenticateConnection(
      chromeos::secure_channel::mojom::ConnectionAttemptFailureReason::
          AUTHENTICATION_ERROR /* failure_reason */,
      RemoteDeviceLifeCycle::State::
          AUTHENTICATION_FAILED /* expected_life_cycle_state */);
}

TEST_F(ProximityAuthRemoteDeviceLifeCycleImplTest,
       MultiDeviceApiEnabled_Failure_BluetoothNotPresent) {
  CreateFakeConnectionAttempt();

  StartLifeCycle();
  SimulateFailureToAuthenticateConnection(
      chromeos::secure_channel::mojom::ConnectionAttemptFailureReason::
          ADAPTER_NOT_PRESENT /* failure_reason */,
      RemoteDeviceLifeCycle::State::STOPPED /* expected_life_cycle_state */);
}

TEST_F(ProximityAuthRemoteDeviceLifeCycleImplTest,
       MultiDeviceApiEnabled_Failure_BluetoothNotPowered) {
  CreateFakeConnectionAttempt();

  StartLifeCycle();
  SimulateFailureToAuthenticateConnection(
      chromeos::secure_channel::mojom::ConnectionAttemptFailureReason::
          ADAPTER_DISABLED /* failure_reason */,
      RemoteDeviceLifeCycle::State::STOPPED /* expected_life_cycle_state */);
}

}  // namespace proximity_auth
