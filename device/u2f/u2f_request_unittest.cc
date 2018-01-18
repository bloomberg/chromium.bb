// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>
#include <string>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/test_mock_time_task_runner.h"
#include "device/u2f/mock_u2f_device.h"
#include "device/u2f/mock_u2f_discovery.h"
#include "device/u2f/u2f_request.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace device {
namespace {

constexpr char kTestRelyingPartyId[] = "google.com";

class FakeU2fRequest : public U2fRequest {
 public:
  FakeU2fRequest(std::string relying_party_id,
                 std::vector<U2fDiscovery*> discoveries)
      : U2fRequest(std::move(relying_party_id), std::move(discoveries)) {}
  ~FakeU2fRequest() override = default;

  void TryDevice() override {
    // Do nothing.
  }
};

}  // namespace

class U2fRequestTest : public testing::Test {
 public:
  U2fRequestTest()
      : task_runner_(base::MakeRefCounted<base::TestMockTimeTaskRunner>(
            base::TestMockTimeTaskRunner::Type::kBoundToThread)) {}

 protected:
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
};

TEST_F(U2fRequestTest, TestIterateDevice) {
  MockU2fDiscovery discovery;
  FakeU2fRequest request(kTestRelyingPartyId, {&discovery});

  auto device0 = std::make_unique<MockU2fDevice>();
  auto device1 = std::make_unique<MockU2fDevice>();
  EXPECT_CALL(*device1.get(), GetId())
      .WillRepeatedly(testing::Return("device0"));
  EXPECT_CALL(*device1.get(), GetId())
      .WillRepeatedly(testing::Return("device1"));
  // Add two U2F devices
  discovery.AddDevice(std::move(device0));
  discovery.AddDevice(std::move(device1));

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
  task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_EQ(mock_device, request.current_device_);
  EXPECT_EQ(static_cast<size_t>(1), request.devices_.size());
  EXPECT_EQ(static_cast<size_t>(0), request.attempted_devices_.size());
}

TEST_F(U2fRequestTest, TestBasicMachine) {
  MockU2fDiscovery discovery;
  EXPECT_CALL(discovery, Start())
      .WillOnce(testing::Invoke(&discovery, &MockU2fDiscovery::StartSuccess));

  FakeU2fRequest request(kTestRelyingPartyId, {&discovery});
  request.Start();

  // Add one U2F device
  auto device = std::make_unique<MockU2fDevice>();
  EXPECT_CALL(*device, GetId());
  EXPECT_CALL(*device, TryWinkRef(_))
      .WillOnce(testing::Invoke(MockU2fDevice::WinkDoNothing));
  discovery.AddDevice(std::move(device));

  EXPECT_EQ(U2fRequest::State::BUSY, request.state_);
}

TEST_F(U2fRequestTest, TestAlreadyPresentDevice) {
  MockU2fDiscovery discovery;
  auto device = std::make_unique<MockU2fDevice>();
  EXPECT_CALL(*device, GetId());
  discovery.AddDevice(std::move(device));

  EXPECT_CALL(discovery, Start())
      .WillOnce(testing::Invoke(&discovery, &MockU2fDiscovery::StartSuccess));

  FakeU2fRequest request(kTestRelyingPartyId, {&discovery});
  request.Start();

  EXPECT_NE(nullptr, request.current_device_);
}

}  // namespace device
