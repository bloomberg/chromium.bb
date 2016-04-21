// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/throttled_bluetooth_connection_finder.h"

#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "components/proximity_auth/bluetooth_connection_finder.h"
#include "components/proximity_auth/bluetooth_throttler.h"
#include "components/proximity_auth/fake_connection.h"
#include "components/proximity_auth/remote_device.h"
#include "components/proximity_auth/wire_message.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::NiceMock;
using testing::Return;
using testing::_;

namespace proximity_auth {
namespace {

const int kPollingIntervalSeconds = 7;
const char kUuid[] = "DEADBEEF-CAFE-FEED-FOOD-D15EA5EBEEF";

// A callback that stores a found |connection| into |out|.
void SaveConnection(std::unique_ptr<Connection>* out,
                    std::unique_ptr<Connection> connection) {
  *out = std::move(connection);
}

class MockBluetoothThrottler : public BluetoothThrottler {
 public:
  MockBluetoothThrottler() {}
  ~MockBluetoothThrottler() override {}

  MOCK_CONST_METHOD0(GetDelay, base::TimeDelta());
  MOCK_METHOD1(OnConnection, void(Connection* connection));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockBluetoothThrottler);
};

class FakeBluetoothConnectionFinder : public BluetoothConnectionFinder {
 public:
  FakeBluetoothConnectionFinder()
      : BluetoothConnectionFinder(
            RemoteDevice(),
            device::BluetoothUUID(kUuid),
            base::TimeDelta::FromSeconds(kPollingIntervalSeconds)) {}
  ~FakeBluetoothConnectionFinder() override {}

  void Find(const ConnectionCallback& connection_callback) override {
    connection_callback.Run(
        base::WrapUnique(new FakeConnection(RemoteDevice())));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeBluetoothConnectionFinder);
};

}  // namespace

class ProximityAuthThrottledBluetoothConnectionFinderTest
    : public testing::Test {
 public:
  ProximityAuthThrottledBluetoothConnectionFinderTest()
      : task_runner_(new base::TestSimpleTaskRunner) {}

 protected:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  NiceMock<MockBluetoothThrottler> throttler_;
};

TEST_F(ProximityAuthThrottledBluetoothConnectionFinderTest,
       Find_ExecutesImmediatelyWhenUnthrottled) {
  ON_CALL(throttler_, GetDelay()).WillByDefault(Return(base::TimeDelta()));

  ThrottledBluetoothConnectionFinder connection_finder(
      base::WrapUnique(new FakeBluetoothConnectionFinder), task_runner_,
      &throttler_);
  std::unique_ptr<Connection> connection;
  connection_finder.Find(base::Bind(&SaveConnection, &connection));
  EXPECT_TRUE(connection);
}

TEST_F(ProximityAuthThrottledBluetoothConnectionFinderTest,
       Find_ExecutesAfterADelayWhenThrottled) {
  ON_CALL(throttler_, GetDelay())
      .WillByDefault(Return(base::TimeDelta::FromSeconds(1)));

  ThrottledBluetoothConnectionFinder connection_finder(
      base::WrapUnique(new FakeBluetoothConnectionFinder), task_runner_,
      &throttler_);
  std::unique_ptr<Connection> connection;
  connection_finder.Find(base::Bind(&SaveConnection, &connection));
  EXPECT_FALSE(connection);

  // The connection should be found once the throttling period has elapsed.
  ON_CALL(throttler_, GetDelay()).WillByDefault(Return(base::TimeDelta()));
  task_runner_->RunUntilIdle();
  EXPECT_TRUE(connection);
}

TEST_F(ProximityAuthThrottledBluetoothConnectionFinderTest,
       OnConnection_ForwardsNotificationToThrottler) {
  ON_CALL(throttler_, GetDelay()).WillByDefault(Return(base::TimeDelta()));

  ThrottledBluetoothConnectionFinder connection_finder(
      base::WrapUnique(new FakeBluetoothConnectionFinder), task_runner_,
      &throttler_);
  std::unique_ptr<Connection> connection;
  EXPECT_CALL(throttler_, OnConnection(_));
  connection_finder.Find(base::Bind(&SaveConnection, &connection));
  EXPECT_TRUE(connection);
}

}  // namespace proximity_auth
