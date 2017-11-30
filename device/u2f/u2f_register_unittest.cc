// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>
#include <utility>

#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "device/u2f/mock_u2f_device.h"
#include "device/u2f/mock_u2f_discovery.h"
#include "device/u2f/u2f_register.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

using ::testing::_;

class U2fRegisterTest : public testing::Test {
 public:
  U2fRegisterTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::UI) {}

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
};

class TestRegisterCallback {
 public:
  TestRegisterCallback()
      : callback_(base::Bind(&TestRegisterCallback::ReceivedCallback,
                             base::Unretained(this))) {}
  ~TestRegisterCallback() = default;

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

TEST_F(U2fRegisterTest, TestRegisterSuccess) {
  std::unique_ptr<MockU2fDevice> device(new MockU2fDevice());
  auto discovery = std::make_unique<MockU2fDiscovery>();

  EXPECT_CALL(*device.get(), GetId())
      .WillRepeatedly(testing::Return("device0"));
  EXPECT_CALL(*device.get(), DeviceTransactPtr(_, _))
      .WillOnce(testing::Invoke(MockU2fDevice::NoErrorRegister));
  EXPECT_CALL(*device.get(), TryWinkRef(_))
      .WillOnce(testing::Invoke(MockU2fDevice::WinkDoNothing));
  EXPECT_CALL(*discovery, Start())
      .WillOnce(testing::Invoke(discovery.get(),
                                &MockU2fDiscovery::StartSuccessAsync));

  TestRegisterCallback cb;
  std::vector<std::unique_ptr<U2fDiscovery>> discoveries;
  MockU2fDiscovery* discovery_weak = discovery.get();
  discoveries.push_back(std::move(discovery));
  std::vector<std::vector<uint8_t>> registration_keys;
  std::unique_ptr<U2fRequest> request = U2fRegister::TryRegistration(
      registration_keys, std::vector<uint8_t>(32), std::vector<uint8_t>(32),
      std::move(discoveries), cb.callback());
  request->Start();
  discovery_weak->AddDevice(std::move(device));
  std::tuple<U2fReturnCode, std::vector<uint8_t>, std::vector<uint8_t>>&
      response = cb.WaitForCallback();
  EXPECT_EQ(U2fReturnCode::SUCCESS, std::get<0>(response));
  ASSERT_GE(1u, std::get<1>(response).size());
  EXPECT_EQ(static_cast<uint8_t>(MockU2fDevice::kRegister),
            std::get<1>(response)[0]);

  // Verify that we get a blank key handle.
  EXPECT_TRUE(std::get<2>(response).empty());
}

TEST_F(U2fRegisterTest, TestDelayedSuccess) {
  std::unique_ptr<MockU2fDevice> device(new MockU2fDevice());
  auto discovery = std::make_unique<MockU2fDiscovery>();

  EXPECT_CALL(*device.get(), GetId())
      .WillRepeatedly(testing::Return("device0"));
  // Go through the state machine twice before success
  EXPECT_CALL(*device.get(), DeviceTransactPtr(_, _))
      .WillOnce(testing::Invoke(MockU2fDevice::NotSatisfied))
      .WillOnce(testing::Invoke(MockU2fDevice::NoErrorRegister));
  EXPECT_CALL(*device.get(), TryWinkRef(_))
      .Times(2)
      .WillRepeatedly(testing::Invoke(MockU2fDevice::WinkDoNothing));
  EXPECT_CALL(*discovery, Start())
      .WillOnce(testing::Invoke(discovery.get(),
                                &MockU2fDiscovery::StartSuccessAsync));
  TestRegisterCallback cb;

  std::vector<std::unique_ptr<U2fDiscovery>> discoveries;
  MockU2fDiscovery* discovery_weak = discovery.get();
  std::vector<std::vector<uint8_t>> registration_keys;
  discoveries.push_back(std::move(discovery));
  std::unique_ptr<U2fRequest> request = U2fRegister::TryRegistration(
      registration_keys, std::vector<uint8_t>(32), std::vector<uint8_t>(32),
      std::move(discoveries), cb.callback());
  request->Start();
  discovery_weak->AddDevice(std::move(device));
  std::tuple<U2fReturnCode, std::vector<uint8_t>, std::vector<uint8_t>>&
      response = cb.WaitForCallback();
  EXPECT_EQ(U2fReturnCode::SUCCESS, std::get<0>(response));
  ASSERT_GE(1u, std::get<1>(response).size());
  EXPECT_EQ(static_cast<uint8_t>(MockU2fDevice::kRegister),
            std::get<1>(response)[0]);

  // Verify that we get a blank key handle.
  EXPECT_TRUE(std::get<2>(response).empty());
}

TEST_F(U2fRegisterTest, TestMultipleDevices) {
  // Second device will have a successful touch
  std::unique_ptr<MockU2fDevice> device0(new MockU2fDevice());
  std::unique_ptr<MockU2fDevice> device1(new MockU2fDevice());
  auto discovery = std::make_unique<MockU2fDiscovery>();

  EXPECT_CALL(*device0.get(), GetId())
      .WillRepeatedly(testing::Return("device0"));
  EXPECT_CALL(*device1.get(), GetId())
      .WillRepeatedly(testing::Return("device1"));
  EXPECT_CALL(*device0.get(), DeviceTransactPtr(_, _))
      .WillOnce(testing::Invoke(MockU2fDevice::NotSatisfied));
  // One wink per device
  EXPECT_CALL(*device0.get(), TryWinkRef(_))
      .WillOnce(testing::Invoke(MockU2fDevice::WinkDoNothing));
  EXPECT_CALL(*device1.get(), DeviceTransactPtr(_, _))
      .WillOnce(testing::Invoke(MockU2fDevice::NoErrorRegister));
  EXPECT_CALL(*device1.get(), TryWinkRef(_))
      .WillOnce(testing::Invoke(MockU2fDevice::WinkDoNothing));
  EXPECT_CALL(*discovery, Start())
      .WillOnce(testing::Invoke(discovery.get(),
                                &MockU2fDiscovery::StartSuccessAsync));

  TestRegisterCallback cb;
  std::vector<std::unique_ptr<U2fDiscovery>> discoveries;
  MockU2fDiscovery* discovery_weak = discovery.get();
  discoveries.push_back(std::move(discovery));
  std::vector<std::vector<uint8_t>> registration_keys;
  std::unique_ptr<U2fRequest> request = U2fRegister::TryRegistration(
      registration_keys, std::vector<uint8_t>(32), std::vector<uint8_t>(32),
      std::move(discoveries), cb.callback());
  request->Start();
  discovery_weak->AddDevice(std::move(device0));
  discovery_weak->AddDevice(std::move(device1));
  std::tuple<U2fReturnCode, std::vector<uint8_t>, std::vector<uint8_t>>&
      response = cb.WaitForCallback();

  EXPECT_EQ(U2fReturnCode::SUCCESS, std::get<0>(response));
  ASSERT_GE(1u, std::get<1>(response).size());
  EXPECT_EQ(static_cast<uint8_t>(MockU2fDevice::kRegister),
            std::get<1>(response)[0]);

  // Verify that we get a blank key handle.
  EXPECT_TRUE(std::get<2>(response).empty());
}

// Tests a scenario where a single device is connected and registration call
// is received with three unknown key handles. We expect that three check only
// sign-in calls be processed before registration.
TEST_F(U2fRegisterTest, TestSingleDeviceRegistrationWithExclusionList) {
  std::vector<uint8_t> unknown_key0(32, 0xB);
  std::vector<uint8_t> unknown_key1(32, 0xC);
  std::vector<uint8_t> unknown_key2(32, 0xD);
  std::vector<std::vector<uint8_t>> handles = {unknown_key0, unknown_key1,
                                               unknown_key2};
  std::unique_ptr<MockU2fDevice> device(new MockU2fDevice());
  auto discovery = std::make_unique<MockU2fDiscovery>();

  EXPECT_CALL(*device.get(), GetId())
      .WillRepeatedly(testing::Return("device0"));
  // DeviceTransact() will be called four times including three check
  // only sign-in calls and one registration call. For the first three calls,
  // device will invoke MockU2fDevice::WrongData as the authenticator did not
  // create the three key handles provided in the exclude list. At the fourth
  // call, MockU2fDevice::NoErrorRegister will be invoked after registration.
  EXPECT_CALL(*device.get(), DeviceTransactPtr(_, _))
      .Times(4)
      .WillOnce(testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(testing::Invoke(MockU2fDevice::NoErrorRegister));
  // TryWink() will be called twice. First during the check only sign-in. After
  // check only sign operation is complete, request state is changed to IDLE,
  // and TryWink() is called again before Register() is called.
  EXPECT_CALL(*device.get(), TryWinkRef(_))
      .WillOnce(testing::Invoke(MockU2fDevice::WinkDoNothing))
      .WillOnce(testing::Invoke(MockU2fDevice::WinkDoNothing));
  EXPECT_CALL(*discovery, Start())
      .WillOnce(testing::Invoke(discovery.get(),
                                &MockU2fDiscovery::StartSuccessAsync));

  TestRegisterCallback cb;
  std::vector<std::unique_ptr<U2fDiscovery>> discoveries;
  MockU2fDiscovery* discovery_weak = discovery.get();
  discoveries.push_back(std::move(discovery));
  std::unique_ptr<U2fRequest> request = U2fRegister::TryRegistration(
      handles, std::vector<uint8_t>(32), std::vector<uint8_t>(32),
      std::move(discoveries), cb.callback());
  discovery_weak->AddDevice(std::move(device));

  std::tuple<U2fReturnCode, std::vector<uint8_t>, std::vector<uint8_t>>&
      response = cb.WaitForCallback();

  EXPECT_EQ(U2fReturnCode::SUCCESS, std::get<0>(response));
  ASSERT_LT(static_cast<size_t>(0), std::get<1>(response).size());
  EXPECT_EQ(static_cast<uint8_t>(MockU2fDevice::kRegister),
            std::get<1>(response)[0]);

  // Verify that we get a blank key handle.
  EXPECT_TRUE(std::get<2>(response).empty());
}

// Tests a scenario where two devices are connected and registration call is
// received with three unknown key handles. We assume that user will proceed the
// registration with second device, "device1".
TEST_F(U2fRegisterTest, TestMultipleDeviceRegistrationWithExclusionList) {
  std::vector<uint8_t> unknown_key0(32, 0xB);
  std::vector<uint8_t> unknown_key1(32, 0xC);
  std::vector<uint8_t> unknown_key2(32, 0xD);
  std::vector<std::vector<uint8_t>> handles = {unknown_key0, unknown_key1,
                                               unknown_key2};
  std::unique_ptr<MockU2fDevice> device0(new MockU2fDevice());
  std::unique_ptr<MockU2fDevice> device1(new MockU2fDevice());
  auto discovery = std::make_unique<MockU2fDiscovery>();

  EXPECT_CALL(*device0.get(), GetId())
      .WillRepeatedly(testing::Return("device0"));
  EXPECT_CALL(*device1.get(), GetId())
      .WillRepeatedly(testing::Return("device1"));

  // DeviceTransact() will be called four times: three times to check for
  // duplicate key handles and once for registration. Since user
  // will register using "device1", the fourth call will invoke
  // MockU2fDevice::NotSatisfied.
  EXPECT_CALL(*device0.get(), DeviceTransactPtr(_, _))
      .WillOnce(testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(testing::Invoke(MockU2fDevice::NotSatisfied));
  // We assume that user registers with second device. Therefore, the fourth
  // DeviceTransact() will invoke MockU2fDevice::NoErrorRegister after
  // successful registration.
  EXPECT_CALL(*device1.get(), DeviceTransactPtr(_, _))
      .WillOnce(testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(testing::Invoke(MockU2fDevice::NoErrorRegister));

  // TryWink() will be called twice on both devices -- during check only
  // sign-in operation and during registration attempt.
  EXPECT_CALL(*device0.get(), TryWinkRef(_))
      .WillOnce(testing::Invoke(MockU2fDevice::WinkDoNothing))
      .WillOnce(testing::Invoke(MockU2fDevice::WinkDoNothing));
  EXPECT_CALL(*device1.get(), TryWinkRef(_))
      .WillOnce(testing::Invoke(MockU2fDevice::WinkDoNothing))
      .WillOnce(testing::Invoke(MockU2fDevice::WinkDoNothing));

  EXPECT_CALL(*discovery, Start())
      .WillOnce(testing::Invoke(discovery.get(),
                                &MockU2fDiscovery::StartSuccessAsync));

  TestRegisterCallback cb;
  std::vector<std::unique_ptr<U2fDiscovery>> discoveries;
  MockU2fDiscovery* discovery_weak = discovery.get();
  discoveries.push_back(std::move(discovery));
  std::unique_ptr<U2fRequest> request = U2fRegister::TryRegistration(
      handles, std::vector<uint8_t>(32), std::vector<uint8_t>(32),
      std::move(discoveries), cb.callback());

  discovery_weak->AddDevice(std::move(device0));
  discovery_weak->AddDevice(std::move(device1));
  std::tuple<U2fReturnCode, std::vector<uint8_t>, std::vector<uint8_t>>&
      response = cb.WaitForCallback();

  EXPECT_EQ(U2fReturnCode::SUCCESS, std::get<0>(response));
  ASSERT_LT(static_cast<size_t>(0), std::get<1>(response).size());
  EXPECT_EQ(static_cast<uint8_t>(MockU2fDevice::kRegister),
            std::get<1>(response)[0]);

  // Verify that we get a blank key handle.
  EXPECT_TRUE(std::get<2>(response).empty());
}

// Tests a scenario where single device is connected and registration is called
// with a key in the exclude list that was created by this device. We assume
// that the duplicate key is the last key handle in the exclude list. Therefore,
// after duplicate key handle is found, the process is expected to terminate
// after calling bogus registration which checks for user presence.
TEST_F(U2fRegisterTest, TestSingleDeviceRegistrationWithDuplicateHandle) {
  std::vector<uint8_t> unknown_key0(32, 0xB);
  std::vector<uint8_t> unknown_key1(32, 0xC);
  std::vector<uint8_t> unknown_key2(32, 0xD);
  std::vector<uint8_t> duplicate_key(32, 0xA);

  std::vector<std::vector<uint8_t>> handles = {unknown_key0, unknown_key1,
                                               unknown_key2, duplicate_key};
  std::unique_ptr<MockU2fDevice> device(new MockU2fDevice());
  auto discovery = std::make_unique<MockU2fDiscovery>();

  EXPECT_CALL(*device.get(), GetId())
      .WillRepeatedly(testing::Return("device0"));

  // For four keys in exclude list, the first three keys will invoke
  // MockU2fDevice::WrongData and the final duplicate key handle will invoke
  // MockU2fDevice::NoErrorSign. Once duplicate key handle is found, bogus
  // registration is called to confirm user presence. This invokes
  // MockU2fDevice::NoErrorRegister.
  EXPECT_CALL(*device.get(), DeviceTransactPtr(_, _))
      .Times(5)
      .WillOnce(testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(testing::Invoke(MockU2fDevice::NoErrorSign))
      .WillOnce(testing::Invoke(MockU2fDevice::NoErrorRegister));

  // Since duplicate key handle is found, registration process is terminated
  // before actual Register() is called on the device. Therefore, TryWink() is
  // invoked once.
  EXPECT_CALL(*device.get(), TryWinkRef(_))
      .WillOnce(testing::Invoke(MockU2fDevice::WinkDoNothing));

  EXPECT_CALL(*discovery, Start())
      .WillOnce(testing::Invoke(discovery.get(),
                                &MockU2fDiscovery::StartSuccessAsync));
  TestRegisterCallback cb;
  std::vector<std::unique_ptr<U2fDiscovery>> discoveries;
  MockU2fDiscovery* discovery_weak = discovery.get();
  discoveries.push_back(std::move(discovery));
  std::unique_ptr<U2fRequest> request = U2fRegister::TryRegistration(
      handles, std::vector<uint8_t>(32), std::vector<uint8_t>(32),
      std::move(discoveries), cb.callback());
  discovery_weak->AddDevice(std::move(device));
  std::tuple<U2fReturnCode, std::vector<uint8_t>, std::vector<uint8_t>>&
      response = cb.WaitForCallback();
  EXPECT_EQ(U2fReturnCode::CONDITIONS_NOT_SATISFIED, std::get<0>(response));
}

// Tests a scenario where one(device1) of the two devices connected has created
// a key handle provided in exclude list. We assume that duplicate key is the
// fourth key handle provided in the exclude list.
TEST_F(U2fRegisterTest, TestMultipleDeviceRegistrationWithDuplicateHandle) {
  std::vector<uint8_t> unknown_key0(32, 0xB);
  std::vector<uint8_t> unknown_key1(32, 0xC);
  std::vector<uint8_t> unknown_key2(32, 0xD);
  std::vector<uint8_t> duplicate_key(32, 0xA);
  std::vector<std::vector<uint8_t>> handles = {unknown_key0, unknown_key1,
                                               unknown_key2, duplicate_key};
  std::unique_ptr<MockU2fDevice> device0(new MockU2fDevice());
  std::unique_ptr<MockU2fDevice> device1(new MockU2fDevice());
  auto discovery = std::make_unique<MockU2fDiscovery>();

  EXPECT_CALL(*device0.get(), GetId())
      .WillRepeatedly(testing::Return("device0"));

  EXPECT_CALL(*device1.get(), GetId())
      .WillRepeatedly(testing::Return("device1"));

  // Since the first device did not create any of the key handles provided in
  // exclude list, we expect that check only sign() should be called
  // four times, and all the calls to DeviceTransact() invoke
  // MockU2fDevice::WrongData.
  EXPECT_CALL(*device0.get(), DeviceTransactPtr(_, _))
      .Times(4)
      .WillOnce(testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(testing::Invoke(MockU2fDevice::WrongData));
  // Since the last key handle in exclude list is a duplicate key, we expect
  // that the first three calls to check only sign() invoke
  // MockU2fDevice::WrongData and that fourth sign() call invoke
  // MockU2fDevice::NoErrorSign. After duplicate key is found, process is
  // terminated after user presence is verified using bogus registration, which
  // invokes MockU2fDevice::NoErrorRegister.
  EXPECT_CALL(*device1.get(), DeviceTransactPtr(_, _))
      .Times(5)
      .WillOnce(testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(testing::Invoke(MockU2fDevice::NoErrorSign))
      .WillOnce(testing::Invoke(MockU2fDevice::NoErrorRegister));

  EXPECT_CALL(*device0.get(), TryWinkRef(_))
      .WillOnce(testing::Invoke(MockU2fDevice::WinkDoNothing));

  EXPECT_CALL(*device1.get(), TryWinkRef(_))
      .WillOnce(testing::Invoke(MockU2fDevice::WinkDoNothing));

  EXPECT_CALL(*discovery, Start())
      .WillOnce(testing::Invoke(discovery.get(),
                                &MockU2fDiscovery::StartSuccessAsync));
  TestRegisterCallback cb;
  std::vector<std::unique_ptr<U2fDiscovery>> discoveries;
  MockU2fDiscovery* discovery_weak = discovery.get();
  discoveries.push_back(std::move(discovery));
  std::unique_ptr<U2fRequest> request = U2fRegister::TryRegistration(
      handles, std::vector<uint8_t>(32), std::vector<uint8_t>(32),
      std::move(discoveries), cb.callback());
  discovery_weak->AddDevice(std::move(device0));
  discovery_weak->AddDevice(std::move(device1));
  std::tuple<U2fReturnCode, std::vector<uint8_t>, std::vector<uint8_t>>&
      response = cb.WaitForCallback();
  EXPECT_EQ(U2fReturnCode::CONDITIONS_NOT_SATISFIED, std::get<0>(response));
}

}  // namespace device
