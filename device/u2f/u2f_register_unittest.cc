// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>

#include "base/run_loop.h"
#include "base/test/test_io_thread.h"
#include "device/base/mock_device_client.h"
#include "device/hid/mock_hid_service.h"
#include "device/test/test_device_client.h"
#include "mock_u2f_device.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "u2f_register.h"

namespace device {

class U2fRegisterTest : public testing::Test {
 public:
  U2fRegisterTest() : io_thread_(base::TestIOThread::kAutoStart) {}

  void SetUp() override {
    MockHidService* hid_service = device_client_.hid_service();
    hid_service->FirstEnumerationComplete();
  }

 protected:
  base::MessageLoopForUI message_loop_;
  base::TestIOThread io_thread_;
  device::MockDeviceClient device_client_;
};

class TestRegisterCallback {
 public:
  TestRegisterCallback()
      : callback_(base::Bind(&TestRegisterCallback::ReceivedCallback,
                             base::Unretained(this))) {}
  ~TestRegisterCallback() {}

  void ReceivedCallback(U2fReturnCode status_code,
                        std::vector<uint8_t> response) {
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

TEST_F(U2fRegisterTest, TestRegisterSuccess) {
  std::unique_ptr<MockU2fDevice> device(new MockU2fDevice());
  EXPECT_CALL(*device.get(), DeviceTransactPtr(testing::_, testing::_))
      .WillOnce(testing::Invoke(MockU2fDevice::NoErrorRegister));
  EXPECT_CALL(*device.get(), TryWink(testing::_))
      .WillOnce(testing::Invoke(MockU2fDevice::WinkDoNothing));
  TestRegisterCallback cb;
  std::unique_ptr<U2fRequest> request = U2fRegister::TryRegistration(
      std::vector<uint8_t>(32), std::vector<uint8_t>(32), cb.callback());
  request->Start();
  request->AddDeviceForTesting(std::move(device));
  std::pair<U2fReturnCode, std::vector<uint8_t>>& response =
      cb.WaitForCallback();
  EXPECT_EQ(U2fReturnCode::SUCCESS, response.first);
  ASSERT_LT(static_cast<size_t>(0), response.second.size());
  EXPECT_EQ(static_cast<uint8_t>(MockU2fDevice::kRegister), response.second[0]);
}

TEST_F(U2fRegisterTest, TestDelayedSuccess) {
  std::unique_ptr<MockU2fDevice> device(new MockU2fDevice());

  // Go through the state machine twice before success
  EXPECT_CALL(*device.get(), DeviceTransactPtr(testing::_, testing::_))
      .WillOnce(testing::Invoke(MockU2fDevice::NotSatisfied))
      .WillOnce(testing::Invoke(MockU2fDevice::NoErrorRegister));
  EXPECT_CALL(*device.get(), TryWink(testing::_))
      .Times(2)
      .WillRepeatedly(testing::Invoke(MockU2fDevice::WinkDoNothing));
  TestRegisterCallback cb;

  std::unique_ptr<U2fRequest> request = U2fRegister::TryRegistration(
      std::vector<uint8_t>(32), std::vector<uint8_t>(32), cb.callback());
  request->Start();
  request->AddDeviceForTesting(std::move(device));
  std::pair<U2fReturnCode, std::vector<uint8_t>>& response =
      cb.WaitForCallback();
  EXPECT_EQ(U2fReturnCode::SUCCESS, response.first);
  ASSERT_LT(static_cast<size_t>(0), response.second.size());
  EXPECT_EQ(static_cast<uint8_t>(MockU2fDevice::kRegister), response.second[0]);
}

TEST_F(U2fRegisterTest, TestMultipleDevices) {
  // Second device will have a successful touch
  std::unique_ptr<MockU2fDevice> device0(new MockU2fDevice());
  std::unique_ptr<MockU2fDevice> device1(new MockU2fDevice());

  EXPECT_CALL(*device0.get(), DeviceTransactPtr(testing::_, testing::_))
      .WillOnce(testing::Invoke(MockU2fDevice::NotSatisfied));
  // One wink per device
  EXPECT_CALL(*device0.get(), TryWink(testing::_))
      .WillOnce(testing::Invoke(MockU2fDevice::WinkDoNothing));
  EXPECT_CALL(*device1.get(), DeviceTransactPtr(testing::_, testing::_))
      .WillOnce(testing::Invoke(MockU2fDevice::NoErrorRegister));
  EXPECT_CALL(*device1.get(), TryWink(testing::_))
      .WillOnce(testing::Invoke(MockU2fDevice::WinkDoNothing));

  TestRegisterCallback cb;
  std::unique_ptr<U2fRequest> request = U2fRegister::TryRegistration(
      std::vector<uint8_t>(32), std::vector<uint8_t>(32), cb.callback());
  request->Start();
  request->AddDeviceForTesting(std::move(device0));
  request->AddDeviceForTesting(std::move(device1));
  std::pair<U2fReturnCode, std::vector<uint8_t>>& response =
      cb.WaitForCallback();
  EXPECT_EQ(U2fReturnCode::SUCCESS, response.first);
  ASSERT_LT(static_cast<size_t>(0), response.second.size());
  EXPECT_EQ(static_cast<uint8_t>(MockU2fDevice::kRegister), response.second[0]);
}

}  // namespace device
