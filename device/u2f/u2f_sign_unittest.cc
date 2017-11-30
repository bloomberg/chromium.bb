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

using ::testing::_;

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
  ~TestSignCallback() = default;

  void ReceivedCallback(U2fReturnCode status_code,
                        const std::vector<uint8_t>& response,
                        const std::vector<uint8_t>& key_handle) {
    response_ = std::make_tuple(status_code, response, key_handle);
    closure_.Run();
  }

  std::tuple<U2fReturnCode, std::vector<uint8_t>, std::vector<uint8_t>>&
  WaitForCallback() {
    closure_ = run_loop_.QuitClosure();
    run_loop_.Run();
    return response_;
  }

  const U2fRequest::ResponseCallback& callback() { return callback_; }

 private:
  std::tuple<U2fReturnCode, std::vector<uint8_t>, std::vector<uint8_t>>
      response_;
  base::Closure closure_;
  U2fRequest::ResponseCallback callback_;
  base::RunLoop run_loop_;
};

TEST_F(U2fSignTest, TestSignSuccess) {
  std::vector<uint8_t> key(32, 0xA);
  std::vector<std::vector<uint8_t>> handles = {key};
  std::unique_ptr<MockU2fDevice> device(new MockU2fDevice());
  auto discovery = std::make_unique<MockU2fDiscovery>();

  EXPECT_CALL(*device.get(), DeviceTransactPtr(_, _))
      .WillOnce(testing::Invoke(MockU2fDevice::NoErrorSign));
  EXPECT_CALL(*device.get(), TryWinkRef(_))
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
  std::tuple<U2fReturnCode, std::vector<uint8_t>, std::vector<uint8_t>>&
      response = cb.WaitForCallback();
  EXPECT_EQ(U2fReturnCode::SUCCESS, std::get<0>(response));

  // Correct key was sent so a sign response is expected
  ASSERT_GE(1u, std::get<1>(response).size());
  EXPECT_EQ(static_cast<uint8_t>(MockU2fDevice::kSign),
            std::get<1>(response)[0]);

  // Verify that we get the key handle used for signing.
  EXPECT_EQ(key, std::get<2>(response));
}

TEST_F(U2fSignTest, TestDelayedSuccess) {
  std::vector<uint8_t> key(32, 0xA);
  std::vector<std::vector<uint8_t>> handles = {key};
  std::unique_ptr<MockU2fDevice> device(new MockU2fDevice());
  auto discovery = std::make_unique<MockU2fDiscovery>();

  // Go through the state machine twice before success
  EXPECT_CALL(*device.get(), DeviceTransactPtr(_, _))
      .WillOnce(testing::Invoke(MockU2fDevice::NotSatisfied))
      .WillOnce(testing::Invoke(MockU2fDevice::NoErrorSign));
  EXPECT_CALL(*device.get(), TryWinkRef(_))
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
  std::tuple<U2fReturnCode, std::vector<uint8_t>, std::vector<uint8_t>>&
      response = cb.WaitForCallback();

  EXPECT_EQ(U2fReturnCode::SUCCESS, std::get<0>(response));

  // Correct key was sent so a sign response is expected
  ASSERT_GE(1u, std::get<1>(response).size());
  EXPECT_EQ(static_cast<uint8_t>(MockU2fDevice::kSign),
            std::get<1>(response)[0]);

  // Verify that we get the key handle used for signing.
  EXPECT_EQ(key, std::get<2>(response));
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
  EXPECT_CALL(*device.get(), DeviceTransactPtr(_, _))
      .WillOnce(testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(testing::Invoke(MockU2fDevice::NoErrorSign));
  // Only one wink expected per device
  EXPECT_CALL(*device.get(), TryWinkRef(_))
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
  std::tuple<U2fReturnCode, std::vector<uint8_t>, std::vector<uint8_t>>&
      response = cb.WaitForCallback();
  EXPECT_EQ(U2fReturnCode::SUCCESS, std::get<0>(response));

  // Correct key was sent so a sign response is expected
  ASSERT_GE(1u, std::get<1>(response).size());
  EXPECT_EQ(static_cast<uint8_t>(MockU2fDevice::kSign),
            std::get<1>(response)[0]);

  // Verify that we get the key handle used for signing.
  EXPECT_EQ(key, std::get<2>(response));
}

TEST_F(U2fSignTest, TestMultipleDevices) {
  std::vector<uint8_t> key0(32, 0xA);
  std::vector<uint8_t> key1(32, 0xB);
  // Second device will have a successful touch
  std::vector<std::vector<uint8_t>> handles = {key0, key1};
  std::unique_ptr<MockU2fDevice> device0(new MockU2fDevice());
  std::unique_ptr<MockU2fDevice> device1(new MockU2fDevice());
  auto discovery = std::make_unique<MockU2fDiscovery>();

  EXPECT_CALL(*device0.get(), DeviceTransactPtr(_, _))
      .WillOnce(testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(testing::Invoke(MockU2fDevice::NotSatisfied));
  // One wink per device
  EXPECT_CALL(*device0.get(), TryWinkRef(_))
      .WillOnce(testing::Invoke(MockU2fDevice::WinkDoNothing));
  EXPECT_CALL(*device1.get(), DeviceTransactPtr(_, _))
      .WillOnce(testing::Invoke(MockU2fDevice::NoErrorSign));
  EXPECT_CALL(*device1.get(), TryWinkRef(_))
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
  std::tuple<U2fReturnCode, std::vector<uint8_t>, std::vector<uint8_t>>&
      response = cb.WaitForCallback();
  EXPECT_EQ(U2fReturnCode::SUCCESS, std::get<0>(response));

  // Correct key was sent so a sign response is expected
  ASSERT_GE(1u, std::get<1>(response).size());
  EXPECT_EQ(static_cast<uint8_t>(MockU2fDevice::kSign),
            std::get<1>(response)[0]);

  // Verify that we get the key handle used for signing.
  EXPECT_EQ(key0, std::get<2>(response));
}

TEST_F(U2fSignTest, TestFakeEnroll) {
  std::vector<uint8_t> key0(32, 0xA);
  std::vector<uint8_t> key1(32, 0xB);
  // Second device will be have a successful touch
  std::vector<std::vector<uint8_t>> handles = {key0, key1};
  std::unique_ptr<MockU2fDevice> device0(new MockU2fDevice());
  std::unique_ptr<MockU2fDevice> device1(new MockU2fDevice());
  auto discovery = std::make_unique<MockU2fDiscovery>();

  EXPECT_CALL(*device0.get(), DeviceTransactPtr(_, _))
      .WillOnce(testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(testing::Invoke(MockU2fDevice::NotSatisfied));
  // One wink per device
  EXPECT_CALL(*device0.get(), TryWinkRef(_))
      .WillOnce(testing::Invoke(MockU2fDevice::WinkDoNothing));
  // Both keys will be tried, when both fail, register is tried on that device
  EXPECT_CALL(*device1.get(), DeviceTransactPtr(_, _))
      .WillOnce(testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(testing::Invoke(MockU2fDevice::NoErrorRegister));
  EXPECT_CALL(*device1.get(), TryWinkRef(_))
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
  std::tuple<U2fReturnCode, std::vector<uint8_t>, std::vector<uint8_t>>&
      response = cb.WaitForCallback();
  EXPECT_EQ(U2fReturnCode::SUCCESS, std::get<0>(response));

  // Device that responded had no correct keys, so a registration response is
  // expected.
  ASSERT_GE(1u, std::get<1>(response).size());
  EXPECT_EQ(static_cast<uint8_t>(MockU2fDevice::kRegister),
            std::get<1>(response)[0]);

  // Verify that we get a blank key handle for the key that was registered.
  EXPECT_TRUE(std::get<2>(response).empty());
}

}  // namespace device
