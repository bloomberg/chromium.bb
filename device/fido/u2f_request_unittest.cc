// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>
#include <string>
#include <utility>

#include "base/test/scoped_task_environment.h"
#include "device/fido/mock_u2f_device.h"
#include "device/fido/mock_u2f_discovery.h"
#include "device/fido/u2f_request.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace device {

namespace {

class FakeU2fRequest : public U2fRequest {
 public:
  explicit FakeU2fRequest()
      : U2fRequest(nullptr /* connector */,
                   base::flat_set<U2fTransportProtocol>(),
                   std::vector<uint8_t>(),
                   std::vector<uint8_t>(),
                   std::vector<std::vector<uint8_t>>()) {}
  ~FakeU2fRequest() override = default;

  void TryDevice() override {
    // Do nothing.
  }
};

// Convenience functions for setting one and two mock discoveries, respectively.
MockU2fDiscovery* SetMockDiscovery(
    U2fRequest* request,
    std::unique_ptr<MockU2fDiscovery> discovery) {
  auto* raw_discovery = discovery.get();
  std::vector<std::unique_ptr<U2fDiscovery>> discoveries;
  discoveries.push_back(std::move(discovery));
  request->SetDiscoveriesForTesting(std::move(discoveries));
  return raw_discovery;
}

std::pair<MockU2fDiscovery*, MockU2fDiscovery*> SetMockDiscoveries(
    U2fRequest* request,
    std::unique_ptr<MockU2fDiscovery> discovery_1,
    std::unique_ptr<MockU2fDiscovery> discovery_2) {
  auto* raw_discovery_1 = discovery_1.get();
  auto* raw_discovery_2 = discovery_2.get();
  std::vector<std::unique_ptr<U2fDiscovery>> discoveries;
  discoveries.push_back(std::move(discovery_1));
  discoveries.push_back(std::move(discovery_2));
  request->SetDiscoveriesForTesting(std::move(discoveries));
  return {raw_discovery_1, raw_discovery_2};
}

}  // namespace

class U2fRequestTest : public testing::Test {
 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_{
      base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME};
};

TEST_F(U2fRequestTest, TestIterateDevice) {
  FakeU2fRequest request;
  auto* discovery =
      SetMockDiscovery(&request, std::make_unique<MockU2fDiscovery>());
  auto device0 = std::make_unique<MockU2fDevice>();
  auto device1 = std::make_unique<MockU2fDevice>();
  EXPECT_CALL(*device0, GetId()).WillRepeatedly(testing::Return("device0"));
  EXPECT_CALL(*device1, GetId()).WillRepeatedly(testing::Return("device1"));
  // Add two U2F devices
  discovery->AddDevice(std::move(device0));
  discovery->AddDevice(std::move(device1));

  // Move first device to current
  request.IterateDevice();
  ASSERT_NE(nullptr, request.current_device_);
  EXPECT_EQ(static_cast<size_t>(1), request.devices_.size());

  // Move second device to current, first to attempted
  request.IterateDevice();
  ASSERT_NE(nullptr, request.current_device_);
  EXPECT_EQ(static_cast<size_t>(1), request.attempted_devices_.size());

  // Move second device from current to attempted, move attempted to devices as
  // all devices have been attempted
  request.IterateDevice();

  ASSERT_EQ(nullptr, request.current_device_);
  EXPECT_EQ(static_cast<size_t>(2), request.devices_.size());
  EXPECT_EQ(static_cast<size_t>(0), request.attempted_devices_.size());

  // Moving attempted devices results in a delayed retry, after which the first
  // device will be tried again. Check for the expected behavior here.
  auto* mock_device = static_cast<MockU2fDevice*>(request.devices_.front());
  EXPECT_CALL(*mock_device, TryWinkRef(_));
  scoped_task_environment_.FastForwardUntilNoTasksRemain();

  EXPECT_EQ(mock_device, request.current_device_);
  EXPECT_EQ(static_cast<size_t>(1), request.devices_.size());
  EXPECT_EQ(static_cast<size_t>(0), request.attempted_devices_.size());
}

TEST_F(U2fRequestTest, TestBasicMachine) {
  FakeU2fRequest request;
  auto* discovery =
      SetMockDiscovery(&request, std::make_unique<MockU2fDiscovery>());
  EXPECT_CALL(*discovery, Start())
      .WillOnce(testing::Invoke(discovery, &MockU2fDiscovery::StartSuccess));
  request.Start();

  // Add one U2F device
  auto device = std::make_unique<MockU2fDevice>();
  EXPECT_CALL(*device, GetId());
  EXPECT_CALL(*device, TryWinkRef(_))
      .WillOnce(testing::Invoke(MockU2fDevice::WinkDoNothing));
  discovery->AddDevice(std::move(device));

  EXPECT_EQ(U2fRequest::State::BUSY, request.state_);
}

TEST_F(U2fRequestTest, TestAlreadyPresentDevice) {
  auto discovery = std::make_unique<MockU2fDiscovery>();
  auto device = std::make_unique<MockU2fDevice>();
  EXPECT_CALL(*device, GetId()).WillRepeatedly(::testing::Return("device"));
  discovery->AddDevice(std::move(device));

  FakeU2fRequest request;
  EXPECT_CALL(*discovery, Start())
      .WillOnce(
          testing::Invoke(discovery.get(), &MockU2fDiscovery::StartSuccess));
  SetMockDiscovery(&request, std::move(discovery));
  request.Start();

  EXPECT_NE(nullptr, request.current_device_);
}

TEST_F(U2fRequestTest, TestMultipleDiscoveries) {
  // Create a fake request with two different discoveries that both start up
  // successfully.
  FakeU2fRequest request;
  MockU2fDiscovery* discoveries[2];
  std::tie(discoveries[0], discoveries[1]) =
      SetMockDiscoveries(&request, std::make_unique<MockU2fDiscovery>(),
                         std::make_unique<MockU2fDiscovery>());

  EXPECT_CALL(*discoveries[0], Start())
      .WillOnce(
          testing::Invoke(discoveries[0], &MockU2fDiscovery::StartSuccess));
  EXPECT_CALL(*discoveries[1], Start())
      .WillOnce(
          testing::Invoke(discoveries[1], &MockU2fDiscovery::StartSuccess));
  request.Start();

  // Let each discovery find a device.
  auto device_1 = std::make_unique<MockU2fDevice>();
  auto device_2 = std::make_unique<MockU2fDevice>();
  EXPECT_CALL(*device_1, GetId()).WillRepeatedly(testing::Return("device_1"));
  EXPECT_CALL(*device_2, GetId()).WillRepeatedly(testing::Return("device_2"));
  auto* device_1_ptr = device_1.get();
  auto* device_2_ptr = device_2.get();
  discoveries[0]->AddDevice(std::move(device_1));
  discoveries[1]->AddDevice(std::move(device_2));

  // Iterate through the devices and make sure they are considered in the same
  // order as they were added.
  EXPECT_EQ(device_1_ptr, request.current_device_);
  request.IterateDevice();

  EXPECT_EQ(device_2_ptr, request.current_device_);
  request.IterateDevice();

  EXPECT_EQ(nullptr, request.current_device_);
  EXPECT_EQ(2u, request.devices_.size());

  // Add a third device.
  auto device_3 = std::make_unique<MockU2fDevice>();
  EXPECT_CALL(*device_3, GetId()).WillRepeatedly(testing::Return("device_3"));
  auto* device_3_ptr = device_3.get();
  discoveries[0]->AddDevice(std::move(device_3));

  // Exhaust the timeout and remove the first two devices, making sure the just
  // added one is the only device considered.
  scoped_task_environment_.FastForwardUntilNoTasksRemain();
  discoveries[1]->RemoveDevice("device_2");
  discoveries[0]->RemoveDevice("device_1");
  EXPECT_EQ(device_3_ptr, request.current_device_);

  // Finally remove the last remaining device.
  discoveries[0]->RemoveDevice("device_3");
  EXPECT_EQ(nullptr, request.current_device_);
}

TEST_F(U2fRequestTest, TestSlowDiscovery) {
  // Create a fake request with two different discoveries that start at
  // different times.
  FakeU2fRequest request;
  MockU2fDiscovery* fast_discovery;
  MockU2fDiscovery* slow_discovery;
  std::tie(fast_discovery, slow_discovery) =
      SetMockDiscoveries(&request, std::make_unique<MockU2fDiscovery>(),
                         std::make_unique<MockU2fDiscovery>());

  EXPECT_CALL(*fast_discovery, Start())
      .WillOnce(
          testing::Invoke(fast_discovery, &MockU2fDiscovery::StartSuccess));
  // slow_discovery does not succeed immediately.
  EXPECT_CALL(*slow_discovery, Start());

  // Let each discovery find a device.
  auto fast_device = std::make_unique<MockU2fDevice>();
  auto slow_device = std::make_unique<MockU2fDevice>();
  EXPECT_CALL(*fast_device, GetId())
      .WillRepeatedly(testing::Return("fast_device"));
  EXPECT_CALL(*slow_device, GetId())
      .WillRepeatedly(testing::Return("slow_device"));

  bool fast_winked = false;
  EXPECT_CALL(*fast_device, TryWinkRef(_))
      .WillOnce(testing::DoAll(testing::Assign(&fast_winked, true),
                               testing::Invoke(MockU2fDevice::WinkDoNothing)))
      .WillRepeatedly(testing::Invoke(MockU2fDevice::WinkDoNothing));
  bool slow_winked = false;
  EXPECT_CALL(*slow_device, TryWinkRef(_))
      .WillOnce(testing::DoAll(testing::Assign(&slow_winked, true),
                               testing::Invoke(MockU2fDevice::WinkDoNothing)));
  auto* fast_device_ptr = fast_device.get();
  auto* slow_device_ptr = slow_device.get();
  fast_discovery->AddDeviceWithoutNotification(std::move(fast_device));

  EXPECT_EQ(nullptr, request.current_device_);
  request.state_ = U2fRequest::State::INIT;

  // The discoveries will be started and |fast_discovery| will succeed
  // immediately with a device already found.
  EXPECT_FALSE(fast_winked);
  request.Start();

  EXPECT_TRUE(fast_winked);
  EXPECT_EQ(fast_device_ptr, request.current_device_);
  EXPECT_EQ(U2fRequest::State::BUSY, request.state_);

  // There are no more devices at this time.
  request.state_ = U2fRequest::State::IDLE;
  request.Transition();
  EXPECT_EQ(nullptr, request.current_device_);
  EXPECT_EQ(U2fRequest::State::OFF, request.state_);

  // All devices have been tried and have been re-enqueued to try again in the
  // future. Now |slow_discovery| starts:

  slow_discovery->AddDeviceWithoutNotification(std::move(slow_device));
  slow_discovery->StartSuccess();

  // |fast_device| is already enqueued and will be retried immediately.
  EXPECT_EQ(fast_device_ptr, request.current_device_);
  EXPECT_EQ(U2fRequest::State::BUSY, request.state_);

  // Next the newly found |slow_device| will be tried.
  request.state_ = U2fRequest::State::IDLE;
  EXPECT_FALSE(slow_winked);
  request.Transition();
  EXPECT_TRUE(slow_winked);
  EXPECT_EQ(slow_device_ptr, request.current_device_);
  EXPECT_EQ(U2fRequest::State::BUSY, request.state_);

  // All discoveries are complete so the request transitions to |OFF|.
  request.state_ = U2fRequest::State::IDLE;
  request.Transition();
  EXPECT_EQ(nullptr, request.current_device_);
  EXPECT_EQ(U2fRequest::State::OFF, request.state_);
}

TEST_F(U2fRequestTest, TestMultipleDiscoveriesWithFailures) {
  {
    // Create a fake request with two different discoveries that both start up
    // unsuccessfully.
    FakeU2fRequest request;
    MockU2fDiscovery* discoveries[2];
    std::tie(discoveries[0], discoveries[1]) =
        SetMockDiscoveries(&request, std::make_unique<MockU2fDiscovery>(),
                           std::make_unique<MockU2fDiscovery>());

    EXPECT_CALL(*discoveries[0], Start())
        .WillOnce(
            testing::Invoke(discoveries[0], &MockU2fDiscovery::StartFailure));
    EXPECT_CALL(*discoveries[1], Start())
        .WillOnce(
            testing::Invoke(discoveries[1], &MockU2fDiscovery::StartFailure));
    request.Start();
    EXPECT_EQ(U2fRequest::State::OFF, request.state_);
  }

  {
    // Create a fake request with two different discoveries, where only one
    // starts up successfully.
    FakeU2fRequest request;
    MockU2fDiscovery* discoveries[2];
    std::tie(discoveries[0], discoveries[1]) =
        SetMockDiscoveries(&request, std::make_unique<MockU2fDiscovery>(),
                           std::make_unique<MockU2fDiscovery>());

    EXPECT_CALL(*discoveries[0], Start())
        .WillOnce(
            testing::Invoke(discoveries[0], &MockU2fDiscovery::StartSuccess));
    EXPECT_CALL(*discoveries[1], Start())
        .WillOnce(
            testing::Invoke(discoveries[1], &MockU2fDiscovery::StartFailure));

    auto device0 = std::make_unique<MockU2fDevice>();
    EXPECT_CALL(*device0, GetId()).WillRepeatedly(testing::Return("device_0"));
    EXPECT_CALL(*device0, TryWinkRef(_))
        .WillOnce(testing::Invoke(MockU2fDevice::WinkDoNothing));
    discoveries[0]->AddDevice(std::move(device0));

    request.Start();
    EXPECT_EQ(U2fRequest::State::BUSY, request.state_);

    // Simulate an action that sets the request state to idle.
    // This and the call to Transition() below is necessary to trigger iterating
    // and trying the new device.
    request.state_ = U2fRequest::State::IDLE;

    // Adding another device should trigger examination and a busy state.
    auto device1 = std::make_unique<MockU2fDevice>();
    EXPECT_CALL(*device1, GetId()).WillRepeatedly(testing::Return("device_1"));
    EXPECT_CALL(*device1, TryWinkRef(_))
        .WillOnce(testing::Invoke(MockU2fDevice::WinkDoNothing));
    discoveries[0]->AddDevice(std::move(device1));

    request.Transition();
    EXPECT_EQ(U2fRequest::State::BUSY, request.state_);
  }
}

TEST_F(U2fRequestTest, TestEncodeVersionRequest) {
  constexpr uint8_t kEncodedU2fVersionRequest[] = {0x00, 0x03, 0x00, 0x00,
                                                   0x00, 0x00, 0x00};
  EXPECT_THAT(U2fRequest::GetU2fVersionApduCommand(false)->GetEncodedCommand(),
              testing::ElementsAreArray(kEncodedU2fVersionRequest));

  // Legacy version command contains 2 extra null bytes compared to ISO 7816-4
  // format.
  constexpr uint8_t kEncodedU2fLegacyVersionRequest[] = {
      0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  EXPECT_THAT(U2fRequest::GetU2fVersionApduCommand(true)->GetEncodedCommand(),
              testing::ElementsAreArray(kEncodedU2fLegacyVersionRequest));
}

}  // namespace device
