// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include "base/test/scoped_task_environment.h"
#include "device/fido/fake_fido_discovery.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_device.h"
#include "device/fido/fido_request_handler.h"
#include "device/fido/fido_response_test_data.h"
#include "device/fido/fido_task.h"
#include "device/fido/mock_fido_device.h"
#include "device/fido/test_callback_receiver.h"
#include "device/fido/u2f_transport_protocol.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace device {

namespace {

using FakeTaskCallback =
    base::OnceCallback<void(CtapDeviceResponseCode status_code,
                            base::Optional<std::vector<uint8_t>>)>;
using FakeHandlerCallback = base::OnceCallback<void(
    FidoReturnCode status_code,
    base::Optional<std::vector<uint8_t>> response_data)>;
using FakeHandlerCallbackReceiver =
    test::StatusAndValueCallbackReceiver<FidoReturnCode,
                                         base::Optional<std::vector<uint8_t>>>;

// Fake FidoTask implementation that treats all response from FidoDevice as
// success if the response is a non-empty vector and sends an empty byte array
// to the device when StartTask() is invoked.
class FakeFidoTask : public FidoTask {
 public:
  FakeFidoTask(FidoDevice* device, FakeTaskCallback callback)
      : FidoTask(device), callback_(std::move(callback)), weak_factory_(this) {}
  ~FakeFidoTask() override = default;

  void StartTask() override {
    device()->DeviceTransact(std::vector<uint8_t>(),
                             base::BindOnce(&FakeFidoTask::CompletionCallback,
                                            weak_factory_.GetWeakPtr()));
  }

  // Fake callback that treats all response with non-empty |device_response| as
  // a successful response, empty (not base::nullopt) response as an UP-verified
  // error, and base::nullopt as a device processing error.
  // TODO(hongjunchoi): Change criteria for deciding when to return success,
  // UP-verified error, or processing error for readability.
  void CompletionCallback(
      base::Optional<std::vector<uint8_t>> device_response) {
    if (!device_response) {
      std::move(callback_).Run(CtapDeviceResponseCode::kCtap2ErrOther,
                               base::nullopt);
      return;
    }

    device_response->empty()
        ? std::move(callback_).Run(CtapDeviceResponseCode::kCtap2ErrNotAllowed,
                                   base::nullopt)
        : std::move(callback_).Run(CtapDeviceResponseCode::kSuccess,
                                   std::vector<uint8_t>());
  }

 private:
  FakeTaskCallback callback_;
  base::WeakPtrFactory<FakeFidoTask> weak_factory_;
};

class FakeFidoRequestHandler : public FidoRequestHandler<std::vector<uint8_t>> {
 public:
  FakeFidoRequestHandler(const base::flat_set<U2fTransportProtocol>& protocols,
                         FakeHandlerCallback callback)
      : FidoRequestHandler(nullptr /* connector */,
                           protocols,
                           std::move(callback)),
        weak_factory_(this) {}
  ~FakeFidoRequestHandler() override = default;

  std::unique_ptr<FidoTask> CreateTaskForNewDevice(
      FidoDevice* device) override {
    return std::make_unique<FakeFidoTask>(
        device, base::BindOnce(&FakeFidoRequestHandler::OnDeviceResponse,
                               weak_factory_.GetWeakPtr(), device));
  }

 private:
  FakeHandlerCallback callback_;
  base::WeakPtrFactory<FakeFidoRequestHandler> weak_factory_;
};

std::vector<uint8_t> CreateFakeSuccessDeviceResponse() {
  return std::vector<uint8_t>{
      base::strict_cast<uint8_t>(CtapDeviceResponseCode::kSuccess)};
}

}  // namespace

class FidoRequestHandlerTest : public ::testing::Test {
 public:
  void ForgeNextHidDiscovery() {
    discovery_ = scoped_fake_discovery_factory_.ForgeNextHidDiscovery();
  }

  std::unique_ptr<FakeFidoRequestHandler> CreateFakeHandler() {
    ForgeNextHidDiscovery();
    return std::make_unique<FakeFidoRequestHandler>(
        base::flat_set<U2fTransportProtocol>(
            {U2fTransportProtocol::kUsbHumanInterfaceDevice}),
        cb_.callback());
  }

  test::FakeFidoDiscovery* discovery() const { return discovery_; }
  FakeHandlerCallbackReceiver& callback() { return cb_; }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_{
      base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME};
  test::ScopedFakeFidoDiscoveryFactory scoped_fake_discovery_factory_;
  test::FakeFidoDiscovery* discovery_;
  FakeHandlerCallbackReceiver cb_;
};

TEST_F(FidoRequestHandlerTest, TestSingleDeviceSuccess) {
  auto request_handler = CreateFakeHandler();
  discovery()->WaitForCallToStartAndSimulateSuccess();

  auto device = std::make_unique<MockFidoDevice>();
  EXPECT_CALL(*device, GetId()).WillRepeatedly(testing::Return("device0"));
  // Device returns success (non-empty vector) response.
  device->ExpectRequestAndRespondWith(std::vector<uint8_t>(),
                                      CreateFakeSuccessDeviceResponse());

  discovery()->AddDevice(std::move(device));
  callback().WaitForCallback();
  EXPECT_EQ(FidoReturnCode::kSuccess, callback().status());
  EXPECT_TRUE(request_handler->is_complete());
}

// Tests a scenario where two unresponsive authenticators are connected and
// cancel request has been sent either from the user or from the relying party
// (i.e. FidoRequestHandler object is destroyed.) Upon destruction, cancel
// command must be invoked to all connected authenticators.
TEST_F(FidoRequestHandlerTest, TestAuthenticatorHandlerReset) {
  auto request_handler = CreateFakeHandler();
  discovery()->WaitForCallToStartAndSimulateSuccess();

  auto device0 = std::make_unique<MockFidoDevice>();
  device0->set_supported_protocol(ProtocolVersion::kCtap);
  EXPECT_CALL(*device0, GetId()).WillRepeatedly(testing::Return("device0"));
  device0->ExpectRequestAndDoNotRespond(std::vector<uint8_t>());
  device0->ExpectCtap2CommandAndDoNotRespond(
      CtapRequestCommand::kAuthenticatorCancel);

  auto device1 = std::make_unique<MockFidoDevice>();
  device1->set_supported_protocol(ProtocolVersion::kCtap);
  EXPECT_CALL(*device1, GetId()).WillRepeatedly(testing::Return("device1"));
  device1->ExpectRequestAndDoNotRespond(std::vector<uint8_t>());
  device1->ExpectCtap2CommandAndDoNotRespond(
      CtapRequestCommand::kAuthenticatorCancel);

  discovery()->AddDevice(std::move(device0));
  discovery()->AddDevice(std::move(device1));
  scoped_task_environment_.FastForwardUntilNoTasksRemain();
  request_handler.reset();
}

// Test a scenario where 2 devices are connected and a response is received from
// only a single device(device1) and the remaining device hangs.
TEST_F(FidoRequestHandlerTest, TestRequestWithMultipleDevices) {
  auto request_handler = CreateFakeHandler();
  discovery()->WaitForCallToStartAndSimulateSuccess();

  // Represents a connected device that hangs without a response.
  auto device0 = std::make_unique<MockFidoDevice>();
  device0->set_supported_protocol(ProtocolVersion::kCtap);
  EXPECT_CALL(*device0, GetId()).WillRepeatedly(testing::Return("device0"));
  // Device is unresponsive and cancel command is invoked afterwards.
  device0->ExpectRequestAndDoNotRespond(std::vector<uint8_t>());
  device0->ExpectCtap2CommandAndDoNotRespond(
      CtapRequestCommand::kAuthenticatorCancel);

  // Represents a connected device that response successfully.
  auto device1 = std::make_unique<MockFidoDevice>();
  device1->set_supported_protocol(ProtocolVersion::kCtap);
  EXPECT_CALL(*device1, GetId()).WillRepeatedly(testing::Return("device1"));
  device1->ExpectRequestAndRespondWith(std::vector<uint8_t>(),
                                       CreateFakeSuccessDeviceResponse());

  discovery()->AddDevice(std::move(device0));
  discovery()->AddDevice(std::move(device1));

  callback().WaitForCallback();
  EXPECT_TRUE(request_handler->is_complete());
  EXPECT_EQ(FidoReturnCode::kSuccess, callback().status());
}

// Test a scenario where 2 devices respond successfully with small time
// delay. Only the first received response should be passed on to the relying
// party, and cancel request should be sent to the other authenticator.
TEST_F(FidoRequestHandlerTest, TestRequestWithMultipleSuccessResponses) {
  auto request_handler = CreateFakeHandler();
  discovery()->WaitForCallToStartAndSimulateSuccess();

  // Represents a connected device that responds successfully after small time
  // delay.
  auto device0 = std::make_unique<MockFidoDevice>();
  device0->set_supported_protocol(ProtocolVersion::kCtap);
  EXPECT_CALL(*device0, GetId()).WillRepeatedly(testing::Return("device0"));
  // Device responds successfully after short delay.
  device0->ExpectRequestAndRespondWith(std::vector<uint8_t>(),
                                       CreateFakeSuccessDeviceResponse(),
                                       base::TimeDelta::FromMicroseconds(1));

  // Represents a device that returns a success response after a longer time
  // delay.
  auto device1 = std::make_unique<MockFidoDevice>();
  device1->set_supported_protocol(ProtocolVersion::kCtap);

  EXPECT_CALL(*device1, GetId()).WillRepeatedly(testing::Return("device1"));
  // Returns success response after long delay.
  device1->ExpectRequestAndRespondWith(std::vector<uint8_t>(),
                                       CreateFakeSuccessDeviceResponse(),
                                       base::TimeDelta::FromMicroseconds(10));
  // Cancel command is invoked after receiving response from |device0|.
  device1->ExpectCtap2CommandAndDoNotRespond(
      CtapRequestCommand::kAuthenticatorCancel);

  discovery()->AddDevice(std::move(device0));
  discovery()->AddDevice(std::move(device1));

  scoped_task_environment_.FastForwardUntilNoTasksRemain();
  callback().WaitForCallback();
  EXPECT_TRUE(request_handler->is_complete());
  EXPECT_EQ(FidoReturnCode::kSuccess, callback().status());
}

// Test a scenario where 3 devices respond with a processing error, an UP(user
// presence) verified failure response with small time delay, and an UP verified
// failure response with big time delay, respectively. Request for device with
// processing error should be immediately dropped. Also, for UP verified
// failures, the first received response should be passed on to the relying
// party and cancel command should be sent to the remaining device.
TEST_F(FidoRequestHandlerTest, TestRequestWithMultipleFailureResponses) {
  auto request_handler = CreateFakeHandler();
  discovery()->WaitForCallToStartAndSimulateSuccess();

  // Represents a connected device that immediately responds with processing
  // error.
  auto device0 = std::make_unique<MockFidoDevice>();
  EXPECT_CALL(*device0, GetId()).WillRepeatedly(testing::Return("device0"));
  // Responds with base::nullopt which represents a processing error.
  device0->ExpectRequestAndRespondWith(std::vector<uint8_t>(), base::nullopt);

  // Represents a device that returns UP verified failure response after a small
  // time delay.
  auto device1 = std::make_unique<MockFidoDevice>();
  EXPECT_CALL(*device1, GetId()).WillRepeatedly(testing::Return("device1"));
  device1->ExpectRequestAndRespondWith(
      std::vector<uint8_t>(),
      // Responds with an empty vector that represents a UP-verified error.
      std::vector<uint8_t>(), base::TimeDelta::FromMicroseconds(1));

  // Represents a device that returns UP verified failure response after a big
  // time delay.
  auto device2 = std::make_unique<MockFidoDevice>();
  device2->set_supported_protocol(ProtocolVersion::kCtap);
  EXPECT_CALL(*device2, GetId()).WillRepeatedly(testing::Return("device2"));
  device2->ExpectRequestAndRespondWith(
      std::vector<uint8_t>(),
      // Responds with an empty vector that represents a UP-verified error.
      std::vector<uint8_t>(), base::TimeDelta::FromMicroseconds(10));
  device2->ExpectCtap2CommandAndDoNotRespond(
      CtapRequestCommand::kAuthenticatorCancel);

  discovery()->AddDevice(std::move(device0));
  discovery()->AddDevice(std::move(device1));
  discovery()->AddDevice(std::move(device2));

  scoped_task_environment_.FastForwardUntilNoTasksRemain();
  callback().WaitForCallback();
  EXPECT_TRUE(request_handler->is_complete());
  EXPECT_EQ(FidoReturnCode::kConditionsNotSatisfied, callback().status());
}

}  // namespace device
