// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/test_mock_time_task_runner.h"
#include "device/u2f/mock_u2f_device.h"
#include "device/u2f/mock_u2f_discovery.h"
#include "device/u2f/u2f_request.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {
namespace {

class FakeU2fRequest : public U2fRequest {
 public:
  FakeU2fRequest(std::vector<std::unique_ptr<U2fDiscovery>> discoveries,
                 const ResponseCallback& cb)
      : U2fRequest(std::move(discoveries), cb) {}
  ~FakeU2fRequest() override {}

  void TryDevice() override {
    cb_.Run(U2fReturnCode::SUCCESS, std::vector<uint8_t>());
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
  // No discoveries are needed, since |request.devices_| is accessed directly.
  auto do_nothing = [](U2fReturnCode, const std::vector<uint8_t>&) {};
  FakeU2fRequest request({}, base::Bind(do_nothing));

  // Add two U2F devices
  request.devices_.push_back(std::make_unique<MockU2fDevice>());
  request.devices_.push_back(std::make_unique<MockU2fDevice>());

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
  auto* mock_device =
      static_cast<MockU2fDevice*>(request.devices_.front().get());
  EXPECT_CALL(*mock_device, TryWink(testing::_));
  task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_EQ(mock_device, request.current_device_.get());
  EXPECT_EQ(static_cast<size_t>(1), request.devices_.size());
  EXPECT_EQ(static_cast<size_t>(0), request.attempted_devices_.size());
}

TEST_F(U2fRequestTest, TestBasicMachine) {
  auto discovery = std::make_unique<MockU2fDiscovery>();
  EXPECT_CALL(*discovery, Start())
      .WillOnce(
          testing::Invoke(discovery.get(), &MockU2fDiscovery::StartSuccess));

  std::vector<std::unique_ptr<U2fDiscovery>> discoveries;
  MockU2fDiscovery* discovery_weak = discovery.get();
  discoveries.push_back(std::move(discovery));

  auto do_nothing = [](U2fReturnCode, const std::vector<uint8_t>&) {};
  FakeU2fRequest request(std::move(discoveries), base::Bind(do_nothing));
  request.Start();

  // Add one U2F device
  auto device = std::make_unique<MockU2fDevice>();
  EXPECT_CALL(*device, TryWink(testing::_))
      .WillOnce(testing::Invoke(MockU2fDevice::WinkDoNothing));
  discovery_weak->AddDevice(std::move(device));

  EXPECT_EQ(U2fRequest::State::BUSY, request.state_);
}

}  // namespace device
