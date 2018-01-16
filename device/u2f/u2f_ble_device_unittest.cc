// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/u2f/u2f_ble_device.h"

#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "device/bluetooth/test/bluetooth_test.h"
#include "device/u2f/mock_u2f_ble_connection.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

using ::testing::_;
using ::testing::Invoke;
using ::testing::Test;

namespace {

class TestMessageCallback {
 public:
  void OnMessage(U2fReturnCode code, const std::vector<uint8_t>& data) {
    result_ = std::make_pair(code, data);
    run_loop_->Quit();
  }

  const std::pair<U2fReturnCode, std::vector<uint8_t>>& WaitForResult() {
    run_loop_->Run();
    run_loop_.emplace();
    return result_;
  }

  U2fDevice::MessageCallback GetCallback() {
    return base::BindRepeating(&TestMessageCallback::OnMessage,
                               base::Unretained(this));
  }

 private:
  std::pair<U2fReturnCode, std::vector<uint8_t>> result_;
  base::Optional<base::RunLoop> run_loop_{base::in_place};
};

}  // namespace

class U2fBleDeviceTest : public Test {
 public:
  U2fBleDeviceTest() {
    auto connection = std::make_unique<MockU2fBleConnection>(
        BluetoothTestBase::kTestDeviceAddress1);
    connection_ = connection.get();
    device_ = std::make_unique<U2fBleDevice>(std::move(connection));
    connection_->connection_status_callback() =
        device_->GetConnectionStatusCallbackForTesting();
    connection_->read_callback() = device_->GetReadCallbackForTesting();
  }

  U2fBleDevice* device() { return device_.get(); }
  MockU2fBleConnection* connection() { return connection_; }

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
  MockU2fBleConnection* connection_;
  std::unique_ptr<U2fBleDevice> device_;
};

TEST_F(U2fBleDeviceTest, ConnectionFailureTest) {
  EXPECT_CALL(*connection(), Connect()).WillOnce(Invoke([this] {
    connection()->connection_status_callback().Run(false);
  }));
  device()->Connect();
}

TEST_F(U2fBleDeviceTest, SendPingTest_Failure_Callback) {
  ConnectWithLength(20);

  EXPECT_CALL(*connection(), WriteControlPointPtr(_, _))
      .WillOnce(Invoke(
          [this](const auto& data, auto* cb) { std::move(*cb).Run(false); }));

  TestMessageCallback callback;
  device()->SendPing({'T', 'E', 'S', 'T'}, callback.GetCallback());

  EXPECT_EQ(std::make_pair(U2fReturnCode::FAILURE, std::vector<uint8_t>()),
            callback.WaitForResult());
}

TEST_F(U2fBleDeviceTest, SendPingTest_Failure_Timeout) {
  ConnectWithLength(20);

  EXPECT_CALL(*connection(), WriteControlPointPtr(_, _))
      .WillOnce(Invoke([this](const auto& data, auto* cb) {
        scoped_task_environment_.FastForwardBy(U2fDevice::kDeviceTimeout);
      }));

  TestMessageCallback callback;
  device()->SendPing({'T', 'E', 'S', 'T'}, callback.GetCallback());

  EXPECT_EQ(std::make_pair(U2fReturnCode::FAILURE, std::vector<uint8_t>()),
            callback.WaitForResult());
}

TEST_F(U2fBleDeviceTest, SendPingTest) {
  ConnectWithLength(20);

  const std::vector<uint8_t> ping_data = {'T', 'E', 'S', 'T'};
  EXPECT_CALL(*connection(), WriteControlPointPtr(_, _))
      .WillOnce(Invoke([this](const auto& data, auto* cb) {
        auto almost_time_out =
            U2fDevice::kDeviceTimeout - base::TimeDelta::FromMicroseconds(1);
        scoped_task_environment_.FastForwardBy(almost_time_out);
        connection()->read_callback().Run(data);
        std::move(*cb).Run(true);
      }));

  TestMessageCallback callback;
  device()->SendPing(ping_data, callback.GetCallback());
  EXPECT_EQ(std::make_pair(U2fReturnCode::SUCCESS, ping_data),
            callback.WaitForResult());
}

TEST_F(U2fBleDeviceTest, StaticGetIdTest) {
  std::string address = BluetoothTestBase::kTestDeviceAddress1;
  EXPECT_EQ("ble:" + address, U2fBleDevice::GetId(address));
}

TEST_F(U2fBleDeviceTest, TryWinkTest) {
  base::RunLoop run_loop;
  device()->TryWink(run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(U2fBleDeviceTest, GetIdTest) {
  EXPECT_EQ(std::string("ble:") + BluetoothTestBase::kTestDeviceAddress1,
            device()->GetId());
}

}  // namespace device
