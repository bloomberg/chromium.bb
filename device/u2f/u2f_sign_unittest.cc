// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>
#include <utility>

#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/test_io_thread.h"
#include "device/u2f/mock_u2f_device.h"
#include "device/u2f/mock_u2f_discovery.h"
#include "device/u2f/u2f_sign.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {
class U2fSignTest : public testing::Test {
 public:
  U2fSignTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::UI) {}

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
};

class TestSignCallback {
 public:
  TestSignCallback()
      : callback_(base::Bind(&TestSignCallback::ReceivedCallback,
                             base::Unretained(this))) {}
  ~TestSignCallback() {}

  void ReceivedCallback(U2fReturnCode status_code,
                        const std::vector<uint8_t>& response) {
    response_ = std::make_pair(status_code, response);
    closure_.Run();
  }

  std::pair<U2fReturnCode, std::vector<uint8_t>>& WaitForCallback() {
    closure_ = run_loop_.QuitClosure();
    run_loop_.Run();
    return response_;
  }

  const U2fRequest::ResponseCallback& callback() { return callback_; }

 private:
  std::pair<U2fReturnCode, std::vector<uint8_t>> response_;
  base::Closure closure_;
  U2fRequest::ResponseCallback callback_;
  base::RunLoop run_loop_;
};

TEST_F(U2fSignTest, TestSignSuccess) {
  std::vector<uint8_t> key(32, 0xA);
  std::vector<std::vector<uint8_t>> handles = {key};
  std::unique_ptr<MockU2fDevice> device(new MockU2fDevice());
  auto discovery = std::make_unique<MockU2fDiscovery>();

  EXPECT_CALL(*device.get(), DeviceTransactPtr(testing::_, testing::_))
      .WillOnce(testing::Invoke(MockU2fDevice::NoErrorSign));
  EXPECT_CALL(*device.get(), TryWink(testing::_))
      .WillOnce(testing::Invoke(MockU2fDevice::WinkDoNothing));
  EXPECT_CALL(*discovery, Start())
      .WillOnce(testing::Invoke(discovery.get(),
                                &MockU2fDiscovery::StartSuccessAsync));
  TestSignCallback cb;

  std::vector<std::unique_ptr<U2fDiscovery>> discoveries;
  MockU2fDiscovery* discovery_weak = discovery.get();
  discoveries.push_back(std::move(discovery));
  std::unique_ptr<U2fRequest> request = U2fSign::TrySign(
      handles, std::vector<uint8_t>(32), std::vector<uint8_t>(32),
      std::move(discoveries), cb.callback());

  discovery_weak->AddDevice(std::move(device));
  std::pair<U2fReturnCode, std::vector<uint8_t>>& response =
      cb.WaitForCallback();
  EXPECT_EQ(U2fReturnCode::SUCCESS, response.first);
  // Correct key was sent so a sign response is expected
  ASSERT_LT(static_cast<size_t>(0), response.second.size());
  EXPECT_EQ(static_cast<uint8_t>(MockU2fDevice::kSign), response.second[0]);
}

TEST_F(U2fSignTest, TestDelayedSuccess) {
  std::vector<uint8_t> key(32, 0xA);
  std::vector<std::vector<uint8_t>> handles = {key};
  std::unique_ptr<MockU2fDevice> device(new MockU2fDevice());
  auto discovery = std::make_unique<MockU2fDiscovery>();

  // Go through the state machine twice before success
  EXPECT_CALL(*device.get(), DeviceTransactPtr(testing::_, testing::_))
      .WillOnce(testing::Invoke(MockU2fDevice::NotSatisfied))
      .WillOnce(testing::Invoke(MockU2fDevice::NoErrorSign));
  EXPECT_CALL(*device.get(), TryWink(testing::_))
      .Times(2)
      .WillRepeatedly(testing::Invoke(MockU2fDevice::WinkDoNothing));
  EXPECT_CALL(*discovery, Start())
      .WillOnce(testing::Invoke(discovery.get(),
                                &MockU2fDiscovery::StartSuccessAsync));
  TestSignCallback cb;

  std::vector<std::unique_ptr<U2fDiscovery>> discoveries;
  MockU2fDiscovery* discovery_weak = discovery.get();
  discoveries.push_back(std::move(discovery));
  std::unique_ptr<U2fRequest> request = U2fSign::TrySign(
      handles, std::vector<uint8_t>(32), std::vector<uint8_t>(32),
      std::move(discoveries), cb.callback());
  request->Start();
  discovery_weak->AddDevice(std::move(device));
  std::pair<U2fReturnCode, std::vector<uint8_t>>& response =
      cb.WaitForCallback();
  EXPECT_EQ(U2fReturnCode::SUCCESS, response.first);
  // Correct key was sent so a sign response is expected
  ASSERT_LT(static_cast<size_t>(0), response.second.size());
  EXPECT_EQ(static_cast<uint8_t>(MockU2fDevice::kSign), response.second[0]);
}

TEST_F(U2fSignTest, TestMultipleHandles) {
  std::vector<uint8_t> key(32, 0xA);
  std::vector<uint8_t> wrong_key0(32, 0xB);
  std::vector<uint8_t> wrong_key1(32, 0xC);
  std::vector<uint8_t> wrong_key2(32, 0xD);
  // Put wrong key first to ensure that it will be tested before the correct key
  std::vector<std::vector<uint8_t>> handles = {wrong_key0, wrong_key1,
                                               wrong_key2, key};
  std::unique_ptr<MockU2fDevice> device(new MockU2fDevice());
  auto discovery = std::make_unique<MockU2fDiscovery>();

  // Wrong key would respond with SW_WRONG_DATA
  EXPECT_CALL(*device.get(), DeviceTransactPtr(testing::_, testing::_))
      .WillOnce(testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(testing::Invoke(MockU2fDevice::NoErrorSign));
  // Only one wink expected per device
  EXPECT_CALL(*device.get(), TryWink(testing::_))
      .WillOnce(testing::Invoke(MockU2fDevice::WinkDoNothing));
  EXPECT_CALL(*discovery, Start())
      .WillOnce(testing::Invoke(discovery.get(),
                                &MockU2fDiscovery::StartSuccessAsync));

  TestSignCallback cb;
  std::vector<std::unique_ptr<U2fDiscovery>> discoveries;
  MockU2fDiscovery* discovery_weak = discovery.get();
  discoveries.push_back(std::move(discovery));
  std::unique_ptr<U2fRequest> request = U2fSign::TrySign(
      handles, std::vector<uint8_t>(32), std::vector<uint8_t>(32),
      std::move(discoveries), cb.callback());
  discovery_weak->AddDevice(std::move(device));
  std::pair<U2fReturnCode, std::vector<uint8_t>>& response =
      cb.WaitForCallback();
  EXPECT_EQ(U2fReturnCode::SUCCESS, response.first);
  // Correct key was sent so a sign response is expected
  ASSERT_LT(static_cast<size_t>(0), response.second.size());
  EXPECT_EQ(static_cast<uint8_t>(MockU2fDevice::kSign), response.second[0]);
}

TEST_F(U2fSignTest, TestMultipleDevices) {
  std::vector<uint8_t> key0(32, 0xA);
  std::vector<uint8_t> key1(32, 0xB);
  // Second device will have a successful touch
  std::vector<std::vector<uint8_t>> handles = {key0, key1};
  std::unique_ptr<MockU2fDevice> device0(new MockU2fDevice());
  std::unique_ptr<MockU2fDevice> device1(new MockU2fDevice());
  auto discovery = std::make_unique<MockU2fDiscovery>();

  EXPECT_CALL(*device0.get(), DeviceTransactPtr(testing::_, testing::_))
      .WillOnce(testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(testing::Invoke(MockU2fDevice::NotSatisfied));
  // One wink per device
  EXPECT_CALL(*device0.get(), TryWink(testing::_))
      .WillOnce(testing::Invoke(MockU2fDevice::WinkDoNothing));
  EXPECT_CALL(*device1.get(), DeviceTransactPtr(testing::_, testing::_))
      .WillOnce(testing::Invoke(MockU2fDevice::NoErrorSign));
  EXPECT_CALL(*device1.get(), TryWink(testing::_))
      .WillOnce(testing::Invoke(MockU2fDevice::WinkDoNothing));
  EXPECT_CALL(*discovery, Start())
      .WillOnce(testing::Invoke(discovery.get(),
                                &MockU2fDiscovery::StartSuccessAsync));

  TestSignCallback cb;
  std::vector<std::unique_ptr<U2fDiscovery>> discoveries;
  MockU2fDiscovery* discovery_weak = discovery.get();
  discoveries.push_back(std::move(discovery));
  std::unique_ptr<U2fRequest> request = U2fSign::TrySign(
      handles, std::vector<uint8_t>(32), std::vector<uint8_t>(32),
      std::move(discoveries), cb.callback());
  discovery_weak->AddDevice(std::move(device0));
  discovery_weak->AddDevice(std::move(device1));
  std::pair<U2fReturnCode, std::vector<uint8_t>>& response =
      cb.WaitForCallback();
  EXPECT_EQ(U2fReturnCode::SUCCESS, response.first);
  // Correct key was sent so a sign response is expected
  ASSERT_LT(static_cast<size_t>(0), response.second.size());
  EXPECT_EQ(static_cast<uint8_t>(MockU2fDevice::kSign), response.second[0]);
}

TEST_F(U2fSignTest, TestFakeEnroll) {
  std::vector<uint8_t> key0(32, 0xA);
  std::vector<uint8_t> key1(32, 0xB);
  // Second device will be have a successful touch
  std::vector<std::vector<uint8_t>> handles = {key0, key1};
  std::unique_ptr<MockU2fDevice> device0(new MockU2fDevice());
  std::unique_ptr<MockU2fDevice> device1(new MockU2fDevice());
  auto discovery = std::make_unique<MockU2fDiscovery>();

  EXPECT_CALL(*device0.get(), DeviceTransactPtr(testing::_, testing::_))
      .WillOnce(testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(testing::Invoke(MockU2fDevice::NotSatisfied));
  // One wink per device
  EXPECT_CALL(*device0.get(), TryWink(testing::_))
      .WillOnce(testing::Invoke(MockU2fDevice::WinkDoNothing));
  // Both keys will be tried, when both fail, register is tried on that device
  EXPECT_CALL(*device1.get(), DeviceTransactPtr(testing::_, testing::_))
      .WillOnce(testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(testing::Invoke(MockU2fDevice::NoErrorRegister));
  EXPECT_CALL(*device1.get(), TryWink(testing::_))
      .WillOnce(testing::Invoke(MockU2fDevice::WinkDoNothing));
  EXPECT_CALL(*discovery, Start())
      .WillOnce(testing::Invoke(discovery.get(),
                                &MockU2fDiscovery::StartSuccessAsync));

  TestSignCallback cb;
  std::vector<std::unique_ptr<U2fDiscovery>> discoveries;
  MockU2fDiscovery* discovery_weak = discovery.get();
  discoveries.push_back(std::move(discovery));
  std::unique_ptr<U2fRequest> request = U2fSign::TrySign(
      handles, std::vector<uint8_t>(32), std::vector<uint8_t>(32),
      std::move(discoveries), cb.callback());
  request->Start();
  discovery_weak->AddDevice(std::move(device0));
  discovery_weak->AddDevice(std::move(device1));
  std::pair<U2fReturnCode, std::vector<uint8_t>>& response =
      cb.WaitForCallback();
  EXPECT_EQ(U2fReturnCode::SUCCESS, response.first);
  // Device that responded had no correct keys, so a registration response is
  // expected
  ASSERT_LT(static_cast<size_t>(0), response.second.size());
  EXPECT_EQ(static_cast<uint8_t>(MockU2fDevice::kRegister), response.second[0]);
}

}  // namespace device
