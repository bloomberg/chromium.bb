// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/pending_ble_listener_connection_request.h"

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

class SecureChannelPendingBleListenerConnectionRequestTest
    : public testing::Test {
 protected:
  SecureChannelPendingBleListenerConnectionRequestTest() = default;
  ~SecureChannelPendingBleListenerConnectionRequestTest() override = default;

  // testing::Test:
  void SetUp() override {
    fake_pending_connection_request_delegate_ =
        std::make_unique<FakePendingConnectionRequestDelegate>();
    auto client_connection_parameters =
        TestClientConnectionParametersFactory::Get()->Create(kTestFeature);
    client_connection_parameters_id_ = client_connection_parameters.id();

    pending_ble_listener_request_ =
        PendingBleListenerConnectionRequest::Factory::Get()->BuildInstance(
            std::move(client_connection_parameters),
            fake_pending_connection_request_delegate_.get());
  }

  const base::Optional<
      PendingConnectionRequestDelegate::FailedConnectionReason>&
  GetFailedConnectionReason() {
    return fake_pending_connection_request_delegate_
        ->GetFailedConnectionReasonForId(
            pending_ble_listener_request_->GetRequestId());
  }

  const base::Optional<mojom::ConnectionAttemptFailureReason>&
  GetConnectionAttemptFailureReason() {
    return fake_connection_delegate()->connection_attempt_failure_reason();
  }

  void HandleConnectionFailure(BleListenerFailureType failure_type) {
    base::RunLoop run_loop;
    fake_connection_delegate()->set_closure_for_next_delegate_callback(
        run_loop.QuitClosure());

    pending_ble_listener_request_->HandleConnectionFailure(failure_type);

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

  std::unique_ptr<PendingConnectionRequest<BleListenerFailureType>>
      pending_ble_listener_request_;

  DISALLOW_COPY_AND_ASSIGN(
      SecureChannelPendingBleListenerConnectionRequestTest);
};

TEST_F(SecureChannelPendingBleListenerConnectionRequestTest,
       HandleAuthenticationError) {
  HandleConnectionFailure(BleListenerFailureType::kAuthenticationError);
  EXPECT_EQ(
      PendingConnectionRequestDelegate::FailedConnectionReason::kRequestFailed,
      *GetFailedConnectionReason());
  EXPECT_EQ(mojom::ConnectionAttemptFailureReason::AUTHENTICATION_ERROR,
            *GetConnectionAttemptFailureReason());
}

TEST_F(SecureChannelPendingBleListenerConnectionRequestTest,
       HandleInvalidBeaconSeeds) {
  HandleConnectionFailure(BleListenerFailureType::kInvalidBeaconSeeds);
  EXPECT_EQ(
      PendingConnectionRequestDelegate::FailedConnectionReason::kRequestFailed,
      *GetFailedConnectionReason());
  EXPECT_EQ(
      mojom::ConnectionAttemptFailureReason::REMOTE_DEVICE_INVALID_BEACON_SEEDS,
      *GetConnectionAttemptFailureReason());
}

}  // namespace secure_channel

}  // namespace chromeos
