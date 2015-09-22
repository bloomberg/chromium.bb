// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/remote_device_life_cycle_impl.h"

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/test/test_simple_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "components/proximity_auth/authenticator.h"
#include "components/proximity_auth/connection.h"
#include "components/proximity_auth/connection_finder.h"
#include "components/proximity_auth/messenger.h"
#include "components/proximity_auth/secure_context.h"
#include "components/proximity_auth/wire_message.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Mock;
using testing::NiceMock;
using testing::Return;
using testing::StrictMock;

namespace proximity_auth {

namespace {

// Attributes of the remote device under test.
const char kRemoteDeviceName[] = "remote device";
const char kRemoteDevicePublicKey[] = "public key";
const char kRemoteDeviceBluetoothAddress[] = "AA:BB:CC:DD:EE:FF";
const char kRemoteDevicePSK[] = "remote device psk";

class StubConnection : public Connection {
 public:
  StubConnection() : Connection(RemoteDevice()) {
    SetStatus(Connection::Status::CONNECTED);
  }

  ~StubConnection() override {}

  // Connection:
  void Connect() override { NOTREACHED(); }

  void Disconnect() override { SetStatus(Connection::Status::DISCONNECTED); }

  void SendMessageImpl(scoped_ptr<WireMessage> message) override {
    NOTREACHED();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(StubConnection);
};

class StubSecureContext : public SecureContext {
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

 private:
  DISALLOW_COPY_AND_ASSIGN(StubSecureContext);
};

class FakeConnectionFinder : public ConnectionFinder {
 public:
  FakeConnectionFinder() : connection_(nullptr) {}
  ~FakeConnectionFinder() override {}

  void OnConnectionFound() {
    ASSERT_FALSE(connection_callback_.is_null());
    scoped_ptr<StubConnection> scoped_connection_(new StubConnection());
    connection_ = scoped_connection_.get();
    connection_callback_.Run(scoped_connection_.Pass());
  }

  StubConnection* connection() { return connection_; }

 private:
  // ConnectionFinder:
  void Find(const ConnectionCallback& connection_callback) override {
    ASSERT_TRUE(connection_callback_.is_null());
    connection_callback_ = connection_callback;
  }

  StubConnection* connection_;

  ConnectionCallback connection_callback_;

  DISALLOW_COPY_AND_ASSIGN(FakeConnectionFinder);
};

class FakeAuthenticator : public Authenticator {
 public:
  FakeAuthenticator() {}
  ~FakeAuthenticator() override {}

  void OnAuthenticationResult(Authenticator::Result result) {
    ASSERT_FALSE(callback_.is_null());
    scoped_ptr<SecureContext> secure_context;
    if (result == Authenticator::Result::SUCCESS)
      secure_context.reset(new StubSecureContext());
    callback_.Run(result, secure_context.Pass());
  }

 private:
  // Authenticator:
  void Authenticate(const AuthenticationCallback& callback) override {
    ASSERT_TRUE(callback_.is_null());
    callback_ = callback;
  }

  AuthenticationCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(FakeAuthenticator);
};

// Subclass of RemoteDeviceLifeCycleImpl to make it testable.
class TestableRemoteDeviceLifeCycleImpl : public RemoteDeviceLifeCycleImpl {
 public:
  TestableRemoteDeviceLifeCycleImpl()
      : RemoteDeviceLifeCycleImpl(RemoteDevice(kRemoteDeviceName,
                                               kRemoteDevicePublicKey,
                                               kRemoteDeviceBluetoothAddress,
                                               kRemoteDevicePSK),
                                  nullptr) {}

  ~TestableRemoteDeviceLifeCycleImpl() override {}

  FakeConnectionFinder* connection_finder() { return connection_finder_; }
  FakeAuthenticator* authenticator() { return authenticator_; }

 private:
  scoped_ptr<ConnectionFinder> CreateConnectionFinder() override {
    scoped_ptr<FakeConnectionFinder> scoped_connection_finder(
        new FakeConnectionFinder());
    connection_finder_ = scoped_connection_finder.get();
    return scoped_connection_finder.Pass();
  }

  scoped_ptr<Authenticator> CreateAuthenticator() override {
    scoped_ptr<FakeAuthenticator> scoped_authenticator(new FakeAuthenticator());
    authenticator_ = scoped_authenticator.get();
    return scoped_authenticator.Pass();
  }

  FakeConnectionFinder* connection_finder_;
  FakeAuthenticator* authenticator_;

  DISALLOW_COPY_AND_ASSIGN(TestableRemoteDeviceLifeCycleImpl);
};

}  // namespace

class ProximityAuthRemoteDeviceLifeCycleImplTest
    : public testing::Test,
      public RemoteDeviceLifeCycle::Observer {
 protected:
  ProximityAuthRemoteDeviceLifeCycleImplTest()
      : task_runner_(new base::TestSimpleTaskRunner()),
        thread_task_runner_handle_(task_runner_) {}

  ~ProximityAuthRemoteDeviceLifeCycleImplTest() override {
    life_cycle_.RemoveObserver(this);
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

  StubConnection* OnConnectionFound() {
    EXPECT_EQ(RemoteDeviceLifeCycle::State::FINDING_CONNECTION,
              life_cycle_.GetState());

    EXPECT_CALL(*this, OnLifeCycleStateChanged(
                           RemoteDeviceLifeCycle::State::FINDING_CONNECTION,
                           RemoteDeviceLifeCycle::State::AUTHENTICATING));
    life_cycle_.connection_finder()->OnConnectionFound();
    task_runner_->RunUntilIdle();
    Mock::VerifyAndClearExpectations(this);

    EXPECT_EQ(RemoteDeviceLifeCycle::State::AUTHENTICATING,
              life_cycle_.GetState());
    return life_cycle_.connection_finder()->connection();
  }

  void Authenticate(Authenticator::Result result) {
    EXPECT_EQ(RemoteDeviceLifeCycle::State::AUTHENTICATING,
              life_cycle_.GetState());

    RemoteDeviceLifeCycle::State expected_state =
        (result == Authenticator::Result::SUCCESS)
            ? RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED
            : RemoteDeviceLifeCycle::State::AUTHENTICATION_FAILED;

    EXPECT_CALL(*this, OnLifeCycleStateChanged(
                           RemoteDeviceLifeCycle::State::AUTHENTICATING,
                           expected_state));
    life_cycle_.authenticator()->OnAuthenticationResult(result);

    if (result == Authenticator::Result::SUCCESS)
      task_runner_->RunUntilIdle();

    EXPECT_EQ(expected_state, life_cycle_.GetState());
    Mock::VerifyAndClearExpectations(this);
  }

  MOCK_METHOD2(OnLifeCycleStateChanged,
               void(RemoteDeviceLifeCycle::State old_state,
                    RemoteDeviceLifeCycle::State new_state));

  TestableRemoteDeviceLifeCycleImpl life_cycle_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle thread_task_runner_handle_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProximityAuthRemoteDeviceLifeCycleImplTest);
};

TEST_F(ProximityAuthRemoteDeviceLifeCycleImplTest, AuthenticateAndDisconnect) {
  StartLifeCycle();
  for (size_t i = 0; i < 3; ++i) {
    Connection* connection = OnConnectionFound();
    Authenticate(Authenticator::Result::SUCCESS);
    EXPECT_TRUE(life_cycle_.GetMessenger());

    EXPECT_CALL(*this,
                OnLifeCycleStateChanged(
                    RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED,
                    RemoteDeviceLifeCycle::State::FINDING_CONNECTION));
    connection->Disconnect();
    Mock::VerifyAndClearExpectations(this);
  }
}

TEST_F(ProximityAuthRemoteDeviceLifeCycleImplTest, AuthenticationFails) {
  // Simulate an authentication failure after connecting to the device.
  StartLifeCycle();
  OnConnectionFound();
  Authenticate(Authenticator::Result::FAILURE);
  EXPECT_FALSE(life_cycle_.GetMessenger());

  // After a delay, the life cycle should return to FINDING_CONNECTION.
  EXPECT_CALL(*this, OnLifeCycleStateChanged(
                         RemoteDeviceLifeCycle::State::AUTHENTICATION_FAILED,
                         RemoteDeviceLifeCycle::State::FINDING_CONNECTION));
  task_runner_->RunUntilIdle();
  EXPECT_EQ(RemoteDeviceLifeCycle::State::FINDING_CONNECTION,
            life_cycle_.GetState());

  // Try failing with the DISCONNECTED state instead.
  OnConnectionFound();
  Authenticate(Authenticator::Result::DISCONNECTED);
  EXPECT_FALSE(life_cycle_.GetMessenger());

  // Check we're back in FINDING_CONNECTION state again.
  EXPECT_CALL(*this, OnLifeCycleStateChanged(
                         RemoteDeviceLifeCycle::State::AUTHENTICATION_FAILED,
                         RemoteDeviceLifeCycle::State::FINDING_CONNECTION));
  task_runner_->RunUntilIdle();
  EXPECT_EQ(RemoteDeviceLifeCycle::State::FINDING_CONNECTION,
            life_cycle_.GetState());
}

TEST_F(ProximityAuthRemoteDeviceLifeCycleImplTest,
       AuthenticationFailsThenSucceeds) {
  // Authentication fails on first pass.
  StartLifeCycle();
  OnConnectionFound();
  Authenticate(Authenticator::Result::FAILURE);
  EXPECT_FALSE(life_cycle_.GetMessenger());
  EXPECT_CALL(*this, OnLifeCycleStateChanged(_, _));
  task_runner_->RunUntilIdle();

  // Authentication succeeds on second pass.
  Connection* connection = OnConnectionFound();
  Authenticate(Authenticator::Result::SUCCESS);
  EXPECT_TRUE(life_cycle_.GetMessenger());
  EXPECT_CALL(*this, OnLifeCycleStateChanged(_, _));
  connection->Disconnect();
}

}  // namespace proximity_auth
