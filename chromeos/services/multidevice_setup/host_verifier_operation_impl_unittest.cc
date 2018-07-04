// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/host_verifier_operation_impl.h"

#include <memory>

#include "base/macros.h"
#include "base/timer/mock_timer.h"
#include "chromeos/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/services/multidevice_setup/fake_host_verifier_operation.h"
#include "chromeos/services/secure_channel/public/cpp/client/fake_client_channel.h"
#include "chromeos/services/secure_channel/public/cpp/client/fake_connection_attempt.h"
#include "chromeos/services/secure_channel/public/cpp/client/fake_secure_channel_client.h"
#include "components/cryptauth/remote_device_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace multidevice_setup {

namespace {

const size_t kNumTestDevices = 5;

enum class ResponseType {
  kUnrelated,
  kNoResultCode,
  kErrorResultCode,
  kSuccess
};

std::string CreatePayloadForResponseType(ResponseType response_type) {
  BetterTogetherSetupMessageWrapper wrapper;
  wrapper.set_type(MessageType::ENABLE_BETTER_TOGETHER_RESPONSE);

  switch (response_type) {
    case ResponseType::kUnrelated: {
      return "unrelated";
    }

    case ResponseType::kNoResultCode: {
      EnableBetterTogetherResponse response;
      wrapper.set_payload(response.SerializeAsString());
      return wrapper.SerializeAsString();
    }

    case ResponseType::kErrorResultCode: {
      EnableBetterTogetherResponse response;
      response.set_result_code(EnableBetterTogetherResponse::ERROR);
      wrapper.set_payload(response.SerializeAsString());
      return wrapper.SerializeAsString();
    }

    case ResponseType::kSuccess: {
      EnableBetterTogetherResponse response;
      response.set_result_code(EnableBetterTogetherResponse::NORMAL);
      wrapper.set_payload(response.SerializeAsString());
      return wrapper.SerializeAsString();
    }
  }
}

}  // namespace

class MultiDeviceSetupHostVerifierOperationImplTest : public testing::Test {
 protected:
  MultiDeviceSetupHostVerifierOperationImplTest()
      : test_devices_(
            cryptauth::CreateRemoteDeviceRefListForTest(kNumTestDevices)) {}
  ~MultiDeviceSetupHostVerifierOperationImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    fake_delegate_ = std::make_unique<FakeHostVerifierOperationDelegate>();
    fake_device_sync_client_ =
        std::make_unique<device_sync::FakeDeviceSyncClient>();
    fake_secure_channel_client_ =
        std::make_unique<secure_channel::FakeSecureChannelClient>();
    auto mock_timer = std::make_unique<base::MockOneShotTimer>();
    mock_timer_ = mock_timer.get();

    operation_ = HostVerifierOperationImpl::Factory::Get()->BuildInstance(
        fake_delegate_.get(), remote_device(), local_device(),
        fake_device_sync_client_.get(), fake_secure_channel_client_.get(),
        std::move(mock_timer));

    // The operation should have started its timer immediately.
    EXPECT_TRUE(mock_timer_->IsRunning());
  }

  void Timeout() { mock_timer_->Fire(); }

  void CancelAndVerifyResult() {
    operation_->CancelOperation();
    EXPECT_EQ(HostVerifierOperation::Result::kCanceled, GetOperationResult());
  }

  // Note: If |error_code| is set, then |should_remote_device_be_eligible| is
  // ignored. The eligible/ineligible device lists are only provided if there
  // was no error.
  void CompletePendingFindEligibleDevicesResponse(
      const base::Optional<std::string>& error_code = base::nullopt,
      bool should_remote_device_be_eligible = true) {
    cryptauth::RemoteDeviceRefList eligible_devices;
    cryptauth::RemoteDeviceRefList ineligible_devices;

    if (!error_code) {
      // Always make device 2 eligible.
      eligible_devices.push_back(test_devices_[2]);

      if (should_remote_device_be_eligible)
        eligible_devices.push_back(remote_device());
      else
        ineligible_devices.push_back(remote_device());

      // Always make the local device as well as devices 3 and 4 ineligible.
      ineligible_devices.push_back(local_device());
      ineligible_devices.push_back(test_devices_[3]);
      ineligible_devices.push_back(test_devices_[4]);

      if (should_remote_device_be_eligible) {
        auto fake_connection_attempt =
            std::make_unique<secure_channel::FakeConnectionAttempt>();
        fake_connection_attempt_ = fake_connection_attempt.get();
        fake_secure_channel_client_->set_next_listen_connection_attempt(
            remote_device(), local_device(),
            std::move(fake_connection_attempt));
      }
    }

    EXPECT_FALSE(GetOperationResult());
    fake_device_sync_client_->InvokePendingFindEligibleDevicesCallback(
        error_code, eligible_devices, ineligible_devices);

    if (error_code) {
      EXPECT_EQ(HostVerifierOperation::Result::kErrorCallingFindEligibleDevices,
                GetOperationResult());
    } else if (!should_remote_device_be_eligible) {
      EXPECT_EQ(HostVerifierOperation::Result::kDeviceToVerifyIsNotEligible,
                GetOperationResult());
    } else {
      EXPECT_FALSE(GetOperationResult());
    }
  }

  void FailToCreateConnectionAndVerifyState(
      secure_channel::mojom::ConnectionAttemptFailureReason failure_reason) {
    EXPECT_FALSE(GetOperationResult());
    fake_connection_attempt_->NotifyConnectionAttemptFailure(failure_reason);
    EXPECT_EQ(HostVerifierOperation::Result::kConnectionAttemptFailed,
              GetOperationResult());
  }

  void CreateConnectionSuccessfully() {
    EXPECT_FALSE(GetOperationResult());
    auto fake_client_channel =
        std::make_unique<secure_channel::FakeClientChannel>();
    fake_client_channel_ = fake_client_channel.get();
    fake_connection_attempt_->NotifyConnection(std::move(fake_client_channel));

    EXPECT_EQ(1u, fake_client_channel_->sent_messages().size());
    EXPECT_EQ(
        HostVerifierOperationImpl::CreateWrappedEnableBetterTogetherRequest()
            .SerializeAsString(),
        fake_client_channel_->sent_messages()[0].first);
  }

  void ReceiveResponseAndVerifyState(ResponseType response_type) {
    fake_client_channel_->NotifyMessageReceived(
        CreatePayloadForResponseType(response_type));

    switch (response_type) {
      case ResponseType::kUnrelated:
        EXPECT_FALSE(GetOperationResult());
        break;
      case ResponseType::kNoResultCode:
        EXPECT_EQ(HostVerifierOperation::Result::kReceivedInvalidResponse,
                  GetOperationResult());
        break;
      case ResponseType::kErrorResultCode:
        EXPECT_EQ(HostVerifierOperation::Result::kReceivedErrorResponse,
                  GetOperationResult());
        break;
      case ResponseType::kSuccess:
        EXPECT_EQ(HostVerifierOperation::Result::kSuccess,
                  GetOperationResult());
        break;
    }
  }

  base::Optional<HostVerifierOperation::Result> GetOperationResult() {
    // Both |operation_| and |fake_delegate_| should have identical result
    // values.
    EXPECT_EQ(operation_->result(), fake_delegate_->result());
    return operation_->result();
  }

  secure_channel::FakeClientChannel* fake_client_channel() {
    return fake_client_channel_;
  }

  const cryptauth::RemoteDeviceRef& local_device() { return test_devices_[0]; }
  const cryptauth::RemoteDeviceRef& remote_device() { return test_devices_[1]; }

 private:
  const cryptauth::RemoteDeviceRefList test_devices_;

  std::unique_ptr<FakeHostVerifierOperationDelegate> fake_delegate_;
  std::unique_ptr<device_sync::FakeDeviceSyncClient> fake_device_sync_client_;
  std::unique_ptr<secure_channel::FakeSecureChannelClient>
      fake_secure_channel_client_;
  base::MockOneShotTimer* mock_timer_ = nullptr;

  secure_channel::FakeConnectionAttempt* fake_connection_attempt_ = nullptr;
  secure_channel::FakeClientChannel* fake_client_channel_ = nullptr;

  std::unique_ptr<HostVerifierOperation> operation_;

  DISALLOW_COPY_AND_ASSIGN(MultiDeviceSetupHostVerifierOperationImplTest);
};

TEST_F(MultiDeviceSetupHostVerifierOperationImplTest,
       TimeoutFindingEligibleDevices) {
  Timeout();
  EXPECT_EQ(HostVerifierOperation::Result::kTimeoutFindingEligibleDevices,
            GetOperationResult());
}

TEST_F(MultiDeviceSetupHostVerifierOperationImplTest,
       CancelWhileFindingEligibleDevices) {
  CancelAndVerifyResult();
}

TEST_F(MultiDeviceSetupHostVerifierOperationImplTest,
       ErrorCallingFindEligibleDevices) {
  CompletePendingFindEligibleDevicesResponse("errorCode");
}

TEST_F(MultiDeviceSetupHostVerifierOperationImplTest,
       DeviceToVerifyIsNotEligible) {
  CompletePendingFindEligibleDevicesResponse(
      base::nullopt /* error_code */,
      false /* should_remote_device_be_eligible */);
}

TEST_F(MultiDeviceSetupHostVerifierOperationImplTest,
       TimeoutFindingConnection) {
  CompletePendingFindEligibleDevicesResponse();
  Timeout();
  EXPECT_EQ(HostVerifierOperation::Result::kTimeoutFindingConnection,
            GetOperationResult());
}

TEST_F(MultiDeviceSetupHostVerifierOperationImplTest,
       CancelWhileFindingConnection) {
  CompletePendingFindEligibleDevicesResponse();
  CancelAndVerifyResult();
}

TEST_F(MultiDeviceSetupHostVerifierOperationImplTest, ConnectionAttemptFailed) {
  CompletePendingFindEligibleDevicesResponse();
  FailToCreateConnectionAndVerifyState(
      secure_channel::mojom::ConnectionAttemptFailureReason::
          AUTHENTICATION_ERROR);
}

TEST_F(MultiDeviceSetupHostVerifierOperationImplTest,
       ConnectionDisconnectedUnexpectedly) {
  CompletePendingFindEligibleDevicesResponse();
  CreateConnectionSuccessfully();
  fake_client_channel()->NotifyDisconnected();
  EXPECT_EQ(HostVerifierOperation::Result::kConnectionDisconnectedUnexpectedly,
            GetOperationResult());
}

TEST_F(MultiDeviceSetupHostVerifierOperationImplTest,
       TimeoutReceivingResponse) {
  CompletePendingFindEligibleDevicesResponse();
  CreateConnectionSuccessfully();
  Timeout();
  EXPECT_EQ(HostVerifierOperation::Result::kTimeoutReceivingResponse,
            GetOperationResult());
}

TEST_F(MultiDeviceSetupHostVerifierOperationImplTest,
       CancelWhileWaitingForResponse) {
  CompletePendingFindEligibleDevicesResponse();
  CreateConnectionSuccessfully();
  CancelAndVerifyResult();
}

TEST_F(MultiDeviceSetupHostVerifierOperationImplTest,
       ReceivedInvalidResponse_NoResultCode) {
  CompletePendingFindEligibleDevicesResponse();
  CreateConnectionSuccessfully();
  ReceiveResponseAndVerifyState(ResponseType::kNoResultCode);
}

TEST_F(MultiDeviceSetupHostVerifierOperationImplTest,
       ReceivedInvalidResponse_ErrorResultCode) {
  CompletePendingFindEligibleDevicesResponse();
  CreateConnectionSuccessfully();
  ReceiveResponseAndVerifyState(ResponseType::kErrorResultCode);
}

TEST_F(MultiDeviceSetupHostVerifierOperationImplTest, Success) {
  CompletePendingFindEligibleDevicesResponse();
  CreateConnectionSuccessfully();
  ReceiveResponseAndVerifyState(ResponseType::kSuccess);
}

TEST_F(MultiDeviceSetupHostVerifierOperationImplTest,
       ReceiveUnrelatedMessageThenSuccess) {
  CompletePendingFindEligibleDevicesResponse();
  CreateConnectionSuccessfully();
  ReceiveResponseAndVerifyState(ResponseType::kUnrelated);
  ReceiveResponseAndVerifyState(ResponseType::kSuccess);
}

}  // namespace multidevice_setup

}  // namespace chromeos
