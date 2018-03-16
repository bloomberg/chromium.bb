// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/fido_ble_device.h"

#include "base/optional.h"
#include "base/test/scoped_task_environment.h"
#include "device/bluetooth/test/bluetooth_test.h"
#include "device/fido/fido_constants.h"
#include "device/fido/mock_fido_ble_connection.h"
#include "device/fido/test_callback_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {
namespace {

using ::testing::_;
using ::testing::Invoke;
using ::testing::Test;
using TestDeviceCallbackReceiver =
    test::TestCallbackReceiver<base::Optional<std::vector<uint8_t>>>;

}  // namespace

class FidoBleDeviceTest : public Test {
 public:
  FidoBleDeviceTest() {
    auto connection = std::make_unique<MockFidoBleConnection>(
        BluetoothTestBase::kTestDeviceAddress1);
    connection_ = connection.get();
    device_ = std::make_unique<FidoBleDevice>(std::move(connection));
    connection_->connection_status_callback() =
        device_->GetConnectionStatusCallbackForTesting();
    connection_->read_callback() = device_->GetReadCallbackForTesting();
  }

  FidoBleDevice* device() { return device_.get(); }
  MockFidoBleConnection* connection() { return connection_; }

  void ConnectWithLength(uint16_t length) {
    EXPECT_CALL(*connection(), Connect()).WillOnce(Invoke([this] {
      connection()->connection_status_callback().Run(true);
    }));

    EXPECT_CALL(*connection(), ReadControlPointLengthPtr(_))
        .WillOnce(Invoke([length](auto* cb) { std::move(*cb).Run(length); }));

    device()->Connect();
  }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_{
      base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME};

 private:
  MockFidoBleConnection* connection_;
  std::unique_ptr<FidoBleDevice> device_;
};

TEST_F(FidoBleDeviceTest, ConnectionFailureTest) {
  EXPECT_CALL(*connection(), Connect()).WillOnce(Invoke([this] {
    connection()->connection_status_callback().Run(false);
  }));
  device()->Connect();
}

TEST_F(FidoBleDeviceTest, SendPingTest_Failure_Callback) {
  ConnectWithLength(20);

  EXPECT_CALL(*connection(), WriteControlPointPtr(_, _))
      .WillOnce(Invoke(
          [this](const auto& data, auto* cb) { std::move(*cb).Run(false); }));

  TestDeviceCallbackReceiver callback_receiver;
  device()->SendPing({'T', 'E', 'S', 'T'}, callback_receiver.callback());

  callback_receiver.WaitForCallback();
  EXPECT_FALSE(std::get<0>(*callback_receiver.result()));
}

TEST_F(FidoBleDeviceTest, SendPingTest_Failure_Timeout) {
  ConnectWithLength(20);

  EXPECT_CALL(*connection(), WriteControlPointPtr(_, _))
      .WillOnce(Invoke([this](const auto& data, auto* cb) {
        scoped_task_environment_.FastForwardBy(kDeviceTimeout);
      }));

  TestDeviceCallbackReceiver callback_receiver;
  device()->SendPing({'T', 'E', 'S', 'T'}, callback_receiver.callback());

  callback_receiver.WaitForCallback();
  EXPECT_FALSE(std::get<0>(*callback_receiver.result()));
}

TEST_F(FidoBleDeviceTest, SendPingTest) {
  ConnectWithLength(20);

  const std::vector<uint8_t> ping_data = {'T', 'E', 'S', 'T'};
  EXPECT_CALL(*connection(), WriteControlPointPtr(_, _))
      .WillOnce(Invoke([this](const auto& data, auto* cb) {
        auto almost_time_out =
            kDeviceTimeout - base::TimeDelta::FromMicroseconds(1);
        scoped_task_environment_.FastForwardBy(almost_time_out);
        connection()->read_callback().Run(data);
        std::move(*cb).Run(true);
      }));

  TestDeviceCallbackReceiver callback_receiver;
  device()->SendPing(ping_data, callback_receiver.callback());

  callback_receiver.WaitForCallback();
  const auto& result = std::get<0>(*callback_receiver.result());
  ASSERT_TRUE(result);
  EXPECT_EQ(ping_data, *result);
}

TEST_F(FidoBleDeviceTest, StaticGetIdTest) {
  std::string address = BluetoothTestBase::kTestDeviceAddress1;
  EXPECT_EQ("ble:" + address, FidoBleDevice::GetId(address));
}

TEST_F(FidoBleDeviceTest, TryWinkTest) {
  test::TestCallbackReceiver<> closure_receiver;
  device()->TryWink(closure_receiver.callback());
  closure_receiver.WaitForCallback();
}

TEST_F(FidoBleDeviceTest, GetIdTest) {
  EXPECT_EQ(std::string("ble:") + BluetoothTestBase::kTestDeviceAddress1,
            device()->GetId());
}

}  // namespace device
