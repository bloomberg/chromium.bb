// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/pending_ble_initiator_connection_request.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "base/unguessable_token.h"
#include "chromeos/services/secure_channel/client_connection_parameters.h"
#include "chromeos/services/secure_channel/fake_pending_connection_request_delegate.h"
#include "chromeos/services/secure_channel/test_client_connection_parameters_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace secure_channel {

namespace {
const char kTestFeature[] = "testFeature";
}  // namespace

class SecureChannelPendingBleInitiatorConnectionRequestTest
    : public testing::Test {
 protected:
  SecureChannelPendingBleInitiatorConnectionRequestTest() = default;
  ~SecureChannelPendingBleInitiatorConnectionRequestTest() override = default;

  // testing::Test:
  void SetUp() override {
    fake_pending_connection_request_delegate_ =
        std::make_unique<FakePendingConnectionRequestDelegate>();
    auto client_connection_parameters =
        TestClientConnectionParametersFactory::Get()->Create(kTestFeature);
    client_connection_parameters_id_ = client_connection_parameters.id();

    pending_ble_initiator_request_ =
        PendingBleInitiatorConnectionRequest::Factory::Get()->BuildInstance(
            std::move(client_connection_parameters),
            fake_pending_connection_request_delegate_.get());
  }

  const base::Optional<
      PendingConnectionRequestDelegate::FailedConnectionReason>&
  GetFailedConnectionReason() {
    return fake_pending_connection_request_delegate_
        ->GetFailedConnectionReasonForId(
            pending_ble_initiator_request_->GetRequestId());
  }

  const base::Optional<mojom::ConnectionAttemptFailureReason>&
  GetConnectionAttemptFailureReason() {
    return fake_connection_delegate()->connection_attempt_failure_reason();
  }

  void HandleConnectionFailure(BleInitiatorFailureType failure_type,
                               bool expected_to_become_inactive) {
    base::RunLoop run_loop;
    if (expected_to_become_inactive) {
      fake_connection_delegate()->set_closure_for_next_delegate_callback(
          run_loop.QuitClosure());
    }

    pending_ble_initiator_request_->HandleConnectionFailure(failure_type);

    if (expected_to_become_inactive)
      run_loop.Run();
  }

  FakeConnectionDelegate* fake_connection_delegate() {
    return TestClientConnectionParametersFactory::Get()
        ->GetDelegateForParameters(client_connection_parameters_id_);
  }

 private:
  const base::test::ScopedTaskEnvironment scoped_task_environment_;

  std::unique_ptr<FakePendingConnectionRequestDelegate>
      fake_pending_connection_request_delegate_;
  base::UnguessableToken client_connection_parameters_id_;

  std::unique_ptr<PendingConnectionRequest<BleInitiatorFailureType>>
      pending_ble_initiator_request_;

  DISALLOW_COPY_AND_ASSIGN(
      SecureChannelPendingBleInitiatorConnectionRequestTest);
};

TEST_F(SecureChannelPendingBleInitiatorConnectionRequestTest,
       HandleAuthenticationError) {
  HandleConnectionFailure(BleInitiatorFailureType::kAuthenticationError,
                          true /* expected_to_become_inactive */);
  EXPECT_EQ(
      PendingConnectionRequestDelegate::FailedConnectionReason::kRequestFailed,
      *GetFailedConnectionReason());
  EXPECT_EQ(mojom::ConnectionAttemptFailureReason::AUTHENTICATION_ERROR,
            *GetConnectionAttemptFailureReason());
}

TEST_F(SecureChannelPendingBleInitiatorConnectionRequestTest,
       HandleInvalidBeaconSeeds) {
  HandleConnectionFailure(BleInitiatorFailureType::kInvalidBeaconSeeds,
                          true /* expected_to_become_inactive */);
  EXPECT_EQ(
      PendingConnectionRequestDelegate::FailedConnectionReason::kRequestFailed,
      *GetFailedConnectionReason());
  EXPECT_EQ(
      mojom::ConnectionAttemptFailureReason::REMOTE_DEVICE_INVALID_BEACON_SEEDS,
      *GetConnectionAttemptFailureReason());
}

TEST_F(SecureChannelPendingBleInitiatorConnectionRequestTest,
       HandleGattErrors) {
  // Fail 5 times; no fatal error should occur.
  for (size_t i = 0; i < 5; ++i) {
    HandleConnectionFailure(BleInitiatorFailureType::kGattConnectionError,
                            false /* expected_to_become_inactive */);
    EXPECT_FALSE(GetFailedConnectionReason());
    EXPECT_FALSE(GetConnectionAttemptFailureReason());
  }

  // Fail a 6th time; this should be a fatal error.
  HandleConnectionFailure(BleInitiatorFailureType::kGattConnectionError,
                          true /* expected_to_become_inactive */);
  EXPECT_EQ(
      PendingConnectionRequestDelegate::FailedConnectionReason::kRequestFailed,
      *GetFailedConnectionReason());
  EXPECT_EQ(mojom::ConnectionAttemptFailureReason::GATT_CONNECTION_ERROR,
            *GetConnectionAttemptFailureReason());
}

TEST_F(SecureChannelPendingBleInitiatorConnectionRequestTest, HandleTimeouts) {
  // Fail 2 times; no fatal error should occur.
  for (size_t i = 0; i < 2; ++i) {
    HandleConnectionFailure(
        BleInitiatorFailureType::kTimeoutContactingRemoteDevice,
        false /* expected_to_become_inactive */);
    EXPECT_FALSE(GetFailedConnectionReason());
    EXPECT_FALSE(GetConnectionAttemptFailureReason());
  }

  // Fail a 3rd time; this should be a fatal error.
  HandleConnectionFailure(
      BleInitiatorFailureType::kTimeoutContactingRemoteDevice,
      true /* expected_to_become_inactive */);
  EXPECT_EQ(
      PendingConnectionRequestDelegate::FailedConnectionReason::kRequestFailed,
      *GetFailedConnectionReason());
  EXPECT_EQ(mojom::ConnectionAttemptFailureReason::TIMEOUT_FINDING_DEVICE,
            *GetConnectionAttemptFailureReason());
}

TEST_F(SecureChannelPendingBleInitiatorConnectionRequestTest,
       NonFailingErrors) {
  // Fail 5 times due to GATT errors; no fatal error should occur.
  for (size_t i = 0; i < 5; ++i) {
    HandleConnectionFailure(BleInitiatorFailureType::kGattConnectionError,
                            false /* expected_to_become_inactive */);
    EXPECT_FALSE(GetFailedConnectionReason());
    EXPECT_FALSE(GetConnectionAttemptFailureReason());
  }

  // Fail 2 times due to timeouts; no fatal error should occur.
  for (size_t i = 0; i < 2; ++i) {
    HandleConnectionFailure(
        BleInitiatorFailureType::kTimeoutContactingRemoteDevice,
        false /* expected_to_become_inactive */);
    EXPECT_FALSE(GetFailedConnectionReason());
    EXPECT_FALSE(GetConnectionAttemptFailureReason());
  }

  // Fail due to being interrupted by a higher-priority attempt; no fatal error
  // should occur.
  HandleConnectionFailure(
      BleInitiatorFailureType::kInterruptedByHigherPriorityConnectionAttempt,
      false /* expected_to_become_inactive */);
  EXPECT_FALSE(GetFailedConnectionReason());
  EXPECT_FALSE(GetConnectionAttemptFailureReason());
}

}  // namespace secure_channel

}  // namespace chromeos
