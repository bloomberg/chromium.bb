// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/numerics/safe_conversions.h"
#include "base/test/scoped_task_environment.h"
#include "device/fido/fake_fido_discovery.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_device.h"
#include "device/fido/fido_request_handler.h"
#include "device/fido/fido_task.h"
#include "device/fido/fido_test_data.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/mock_fido_device.h"
#include "device/fido/test_callback_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace device {

namespace {

using FakeTaskCallback =
    base::OnceCallback<void(CtapDeviceResponseCode status_code,
                            base::Optional<std::vector<uint8_t>>)>;
using FakeHandlerCallback =
    base::OnceCallback<void(FidoReturnCode status_code,
                            base::Optional<std::vector<uint8_t>> response_data,
                            FidoTransportProtocol)>;
using FakeHandlerCallbackReceiver =
    test::StatusAndValuesCallbackReceiver<FidoReturnCode,
                                          base::Optional<std::vector<uint8_t>>,
                                          FidoTransportProtocol>;

enum class FakeTaskResponse : uint8_t {
  kSuccess = 0x00,
  kErrorReceivedAfterObtainingUserPresence = 0x01,
  kProcessingError = 0x02,
};

class TestTransportAvailabilityObserver
    : public FidoRequestHandlerBase::TransportAvailabilityObserver {
 public:
  using TransportAvailabilityNotificationReceiver = test::TestCallbackReceiver<
      FidoRequestHandlerBase::TransportAvailabilityInfo>;

  TestTransportAvailabilityObserver() {}
  ~TestTransportAvailabilityObserver() override {}

  void WaitForAndExpectAvailableTransportsAre(
      base::flat_set<FidoTransportProtocol> expected_transports) {
    transport_availability_notification_receiver_.WaitForCallback();
    auto result =
        std::get<0>(*transport_availability_notification_receiver_.result());
    EXPECT_THAT(result.available_transports,
                ::testing::UnorderedElementsAreArray(expected_transports));
  }

 protected:
  // FidoRequestHandlerBase::TransportAvailabilityObserver:
  void OnTransportAvailabilityEnumerated(
      FidoRequestHandlerBase::TransportAvailabilityInfo data) override {
    transport_availability_notification_receiver_.callback().Run(
        std::move(data));
  }

  void BluetoothAdapterPowerChanged(bool is_powered_on) override {}
  void FidoAuthenticatorAdded(const FidoAuthenticator& authenticator,
                              bool* hold_off_request) override {}
  void FidoAuthenticatorRemoved(base::StringPiece device_id) override {}

 private:
  TransportAvailabilityNotificationReceiver
      transport_availability_notification_receiver_;

  DISALLOW_COPY_AND_ASSIGN(TestTransportAvailabilityObserver);
};

// Fake FidoTask implementation that sends an empty byte array to the device
// when StartTask() is invoked.
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

  void CompletionCallback(
      base::Optional<std::vector<uint8_t>> device_response) {
    DCHECK(device_response && device_response->size() == 1);
    switch (static_cast<FakeTaskResponse>(device_response->front())) {
      case FakeTaskResponse::kSuccess:
        std::move(callback_).Run(CtapDeviceResponseCode::kSuccess,
                                 std::vector<uint8_t>());
        return;

      case FakeTaskResponse::kErrorReceivedAfterObtainingUserPresence:
        std::move(callback_).Run(CtapDeviceResponseCode::kCtap2ErrNoCredentials,
                                 std::vector<uint8_t>());
        return;

      case FakeTaskResponse::kProcessingError:
      default:
        std::move(callback_).Run(CtapDeviceResponseCode::kCtap2ErrOther,
                                 base::nullopt);
        return;
    }
  }

 private:
  FakeTaskCallback callback_;
  base::WeakPtrFactory<FakeFidoTask> weak_factory_;
};

class FakeFidoAuthenticator : public FidoDeviceAuthenticator {
 public:
  explicit FakeFidoAuthenticator(FidoDevice* device)
      : FidoDeviceAuthenticator(device) {}

  void RunFakeTask(FakeTaskCallback callback) {
    SetTaskForTesting(
        std::make_unique<FakeFidoTask>(device(), std::move(callback)));
  }
};

class FakeFidoRequestHandler : public FidoRequestHandler<std::vector<uint8_t>> {
 public:
  FakeFidoRequestHandler(
      const base::flat_set<FidoTransportProtocol>& protocols,
      FakeHandlerCallback callback,
      AddPlatformAuthenticatorCallback add_platform_authenticator =
          AddPlatformAuthenticatorCallback())
      : FidoRequestHandler(nullptr /* connector */,
                           protocols,
                           std::move(callback),
                           std::move(add_platform_authenticator)),
        weak_factory_(this) {
    Start();
  }
  ~FakeFidoRequestHandler() override = default;

  void DispatchRequest(FidoAuthenticator* authenticator) override {
    static_cast<FakeFidoAuthenticator*>(authenticator)
        ->RunFakeTask(
            base::BindOnce(&FakeFidoRequestHandler::OnAuthenticatorResponse,
                           weak_factory_.GetWeakPtr(), authenticator));
  }

  std::unique_ptr<FidoDeviceAuthenticator> CreateAuthenticatorFromDevice(
      FidoDevice* device) override {
    return std::make_unique<FakeFidoAuthenticator>(device);
  }

 private:
  base::WeakPtrFactory<FakeFidoRequestHandler> weak_factory_;
};

std::vector<uint8_t> CreateFakeSuccessDeviceResponse() {
  return {base::strict_cast<uint8_t>(FakeTaskResponse::kSuccess)};
}

std::vector<uint8_t> CreateFakeUserPresenceVerifiedError() {
  return {base::strict_cast<uint8_t>(
      FakeTaskResponse::kErrorReceivedAfterObtainingUserPresence)};
}

std::vector<uint8_t> CreateFakeDeviceProcesssingError() {
  return {base::strict_cast<uint8_t>(FakeTaskResponse::kProcessingError)};
}

}  // namespace

class FidoRequestHandlerTest : public ::testing::Test {
 public:
  void ForgeNextHidDiscovery() {
    discovery_ = scoped_fake_discovery_factory_.ForgeNextHidDiscovery();
    ble_discovery_ = scoped_fake_discovery_factory_.ForgeNextBleDiscovery();
  }

  std::unique_ptr<FakeFidoRequestHandler> CreateFakeHandler() {
    ForgeNextHidDiscovery();
    return std::make_unique<FakeFidoRequestHandler>(
        base::flat_set<FidoTransportProtocol>(
            {FidoTransportProtocol::kUsbHumanInterfaceDevice,
             FidoTransportProtocol::kBluetoothLowEnergy}),
        cb_.callback());
  }

  std::unique_ptr<FakeFidoRequestHandler>
  CreateFakeHandlerWithPlatformAuthenticatorCallback(
      FidoRequestHandlerBase::AddPlatformAuthenticatorCallback
          add_platform_authenticator) {
    return std::make_unique<FakeFidoRequestHandler>(
        base::flat_set<FidoTransportProtocol>(
            {FidoTransportProtocol::kInternal}),
        cb_.callback(), std::move(add_platform_authenticator));
  }

  test::FakeFidoDiscovery* discovery() const { return discovery_; }
  test::FakeFidoDiscovery* ble_discovery() const { return ble_discovery_; }
  FakeHandlerCallbackReceiver& callback() { return cb_; }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_{
      base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME};
  test::ScopedFakeFidoDiscoveryFactory scoped_fake_discovery_factory_;
  test::FakeFidoDiscovery* discovery_;
  test::FakeFidoDiscovery* ble_discovery_;
  FakeHandlerCallbackReceiver cb_;
};

TEST_F(FidoRequestHandlerTest, TestSingleDeviceSuccess) {
  auto request_handler = CreateFakeHandler();
  discovery()->WaitForCallToStartAndSimulateSuccess();

  auto device = std::make_unique<MockFidoDevice>();
  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetInfo, base::nullopt);
  EXPECT_CALL(*device, GetId()).WillRepeatedly(testing::Return("device0"));
  // Device returns success response.
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
  device0->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetInfo,
      test_data::kTestAuthenticatorGetInfoResponse);
  EXPECT_CALL(*device0, GetId()).WillRepeatedly(testing::Return("device0"));
  device0->ExpectRequestAndDoNotRespond(std::vector<uint8_t>());
  EXPECT_CALL(*device0, Cancel());
  auto device1 = std::make_unique<MockFidoDevice>();
  device1->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetInfo,
      test_data::kTestAuthenticatorGetInfoResponse);
  EXPECT_CALL(*device1, GetId()).WillRepeatedly(testing::Return("device1"));
  device1->ExpectRequestAndDoNotRespond(std::vector<uint8_t>());
  EXPECT_CALL(*device1, Cancel());

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
  device0->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetInfo,
      test_data::kTestAuthenticatorGetInfoResponse);
  EXPECT_CALL(*device0, GetId()).WillRepeatedly(testing::Return("device0"));
  // Device is unresponsive and cancel command is invoked afterwards.
  device0->ExpectRequestAndDoNotRespond(std::vector<uint8_t>());
  EXPECT_CALL(*device0, Cancel());

  // Represents a connected device that response successfully.
  auto device1 = std::make_unique<MockFidoDevice>();
  device1->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetInfo,
      test_data::kTestAuthenticatorGetInfoResponse);
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
  device0->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetInfo,
      test_data::kTestAuthenticatorGetInfoResponse);
  EXPECT_CALL(*device0, GetId()).WillRepeatedly(testing::Return("device0"));
  device0->ExpectRequestAndRespondWith(std::vector<uint8_t>(),
                                       CreateFakeSuccessDeviceResponse(),
                                       base::TimeDelta::FromMicroseconds(1));

  // Represents a device that returns a success response after a longer time
  // delay.
  auto device1 = std::make_unique<MockFidoDevice>();
  device1->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetInfo,
      test_data::kTestAuthenticatorGetInfoResponse);
  EXPECT_CALL(*device1, GetId()).WillRepeatedly(testing::Return("device1"));
  device1->ExpectRequestAndRespondWith(std::vector<uint8_t>(),
                                       CreateFakeSuccessDeviceResponse(),
                                       base::TimeDelta::FromMicroseconds(10));
  // Cancel command is invoked after receiving response from |device0|.
  EXPECT_CALL(*device1, Cancel());

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

  // Represents a connected device that immediately responds with a processing
  // error.
  auto device0 = std::make_unique<MockFidoDevice>();
  device0->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetInfo,
      test_data::kTestAuthenticatorGetInfoResponse);
  EXPECT_CALL(*device0, GetId()).WillRepeatedly(testing::Return("device0"));
  device0->ExpectRequestAndRespondWith(std::vector<uint8_t>(),
                                       CreateFakeDeviceProcesssingError());

  // Represents a device that returns an UP verified failure response after a
  // small time delay.
  auto device1 = std::make_unique<MockFidoDevice>();
  device1->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetInfo,
      test_data::kTestAuthenticatorGetInfoResponse);
  EXPECT_CALL(*device1, GetId()).WillRepeatedly(testing::Return("device1"));
  device1->ExpectRequestAndRespondWith(std::vector<uint8_t>(),
                                       CreateFakeUserPresenceVerifiedError(),
                                       base::TimeDelta::FromMicroseconds(1));

  // Represents a device that returns an UP verified failure response after a
  // big time delay.
  auto device2 = std::make_unique<MockFidoDevice>();
  device2->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetInfo,
      test_data::kTestAuthenticatorGetInfoResponse);
  EXPECT_CALL(*device2, GetId()).WillRepeatedly(testing::Return("device2"));
  device2->ExpectRequestAndRespondWith(std::vector<uint8_t>(),
                                       CreateFakeDeviceProcesssingError(),
                                       base::TimeDelta::FromMicroseconds(10));
  EXPECT_CALL(*device2, Cancel());

  discovery()->AddDevice(std::move(device0));
  discovery()->AddDevice(std::move(device1));
  discovery()->AddDevice(std::move(device2));

  scoped_task_environment_.FastForwardUntilNoTasksRemain();
  callback().WaitForCallback();
  EXPECT_TRUE(request_handler->is_complete());
  EXPECT_EQ(FidoReturnCode::kUserConsentButCredentialNotRecognized,
            callback().status());
}

// Requests should be dispatched to the authenticator returned from the
// AddPlatformAuthenticatorCallback if one is passed.
TEST_F(FidoRequestHandlerTest, TestPlatformAuthenticatorCallback) {
  // A platform authenticator usually wouldn't usually use a FidoDevice, but
  // that's not the point of the test here. The test is only trying to ensure
  // the authenticator gets injected and used.
  auto device = MockFidoDevice::MakeCtap();
  EXPECT_CALL(*device, GetId()).WillRepeatedly(testing::Return("device0"));
  // Device returns success response.
  device->ExpectRequestAndRespondWith(std::vector<uint8_t>(),
                                      CreateFakeSuccessDeviceResponse());

  FidoRequestHandlerBase::AddPlatformAuthenticatorCallback
      make_platform_authenticator = base::BindOnce(
          [](FidoDevice* device) -> std::unique_ptr<FidoAuthenticator> {
            return std::make_unique<FakeFidoAuthenticator>(device);
          },
          device.get());

  TestTransportAvailabilityObserver observer;
  auto request_handler = CreateFakeHandlerWithPlatformAuthenticatorCallback(
      std::move(make_platform_authenticator));
  request_handler->set_observer(&observer);

  observer.WaitForAndExpectAvailableTransportsAre(
      {FidoTransportProtocol::kInternal});

  callback().WaitForCallback();
  EXPECT_TRUE(request_handler->is_complete());
  EXPECT_EQ(FidoReturnCode::kSuccess, callback().status());
}

TEST_F(FidoRequestHandlerTest,
       InternalTransportDisallowedIfFactoryYieldsNoAuthenticator) {
  TestTransportAvailabilityObserver observer;
  auto request_handler =
      CreateFakeHandlerWithPlatformAuthenticatorCallback(base::BindOnce(
          []() -> std::unique_ptr<FidoAuthenticator> { return nullptr; }));
  request_handler->set_observer(&observer);
  observer.WaitForAndExpectAvailableTransportsAre({});
}

TEST_F(FidoRequestHandlerTest,
       BleTransportAllowedIfDiscoveryStartsSuccessfully) {
  TestTransportAvailabilityObserver observer;
  auto request_handler = CreateFakeHandler();
  request_handler->set_observer(&observer);

  discovery()->WaitForCallToStartAndSimulateSuccess();
  ble_discovery()->WaitForCallToStartAndSimulateSuccess();

  observer.WaitForAndExpectAvailableTransportsAre(
      {FidoTransportProtocol::kUsbHumanInterfaceDevice,
       FidoTransportProtocol::kBluetoothLowEnergy});
}

TEST_F(FidoRequestHandlerTest, BleTransportDisallowedIfDiscoveryFails) {
  TestTransportAvailabilityObserver observer;
  auto request_handler = CreateFakeHandler();
  request_handler->set_observer(&observer);

  discovery()->WaitForCallToStartAndSimulateSuccess();
  ble_discovery()->WaitForCallToStart();
  ble_discovery()->SimulateStarted(false /* success */);

  observer.WaitForAndExpectAvailableTransportsAre(
      {FidoTransportProtocol::kUsbHumanInterfaceDevice});
}

TEST_F(FidoRequestHandlerTest,
       TransportAvailabilityNotificationOnObserverSetLate) {
  TestTransportAvailabilityObserver observer;
  auto request_handler = CreateFakeHandler();

  discovery()->WaitForCallToStartAndSimulateSuccess();
  ble_discovery()->WaitForCallToStartAndSimulateSuccess();
  scoped_task_environment_.FastForwardUntilNoTasksRemain();

  request_handler->set_observer(&observer);
  observer.WaitForAndExpectAvailableTransportsAre(
      {FidoTransportProtocol::kUsbHumanInterfaceDevice,
       FidoTransportProtocol::kBluetoothLowEnergy});
}

}  // namespace device
