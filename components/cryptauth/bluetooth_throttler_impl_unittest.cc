// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cryptauth/bluetooth_throttler_impl.h"

#include <utility>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "components/cryptauth/fake_connection.h"
#include "components/cryptauth/wire_message.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cryptauth {
namespace {

class TestBluetoothThrottler : public BluetoothThrottlerImpl {
 public:
  explicit TestBluetoothThrottler(std::unique_ptr<base::TickClock> clock)
      : BluetoothThrottlerImpl(std::move(clock)) {}
  ~TestBluetoothThrottler() override {}

  // Increase visibility for testing.
  using BluetoothThrottlerImpl::GetCooldownTimeDelta;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestBluetoothThrottler);
};

}  // namespace

class CryptAuthBluetoothThrottlerImplTest : public testing::Test {
 public:
  CryptAuthBluetoothThrottlerImplTest()
      : clock_(new base::SimpleTestTickClock),
        throttler_(base::WrapUnique(clock_)) {
    // The throttler treats null times as special, so start with a non-null
    // time.
    clock_->Advance(base::TimeDelta::FromSeconds(1));
  }

  void PerformConnectionStateTransition(Connection::Status old_status,
                                        Connection::Status new_status) {
    FakeConnection connection((RemoteDevice()));
    throttler_.OnConnection(&connection);
    static_cast<ConnectionObserver*>(&throttler_)
        ->OnConnectionStatusChanged(&connection, old_status, new_status);
  }

 protected:
  // The clock is owned by the |throttler_|.
  base::SimpleTestTickClock* clock_;
  TestBluetoothThrottler throttler_;
};

TEST_F(CryptAuthBluetoothThrottlerImplTest,
       GetDelay_FirstConnectionIsNotThrottled) {
  EXPECT_EQ(base::TimeDelta(), throttler_.GetDelay());
}

TEST_F(CryptAuthBluetoothThrottlerImplTest,
       GetDelay_ConnectionAfterDisconnectIsThrottled) {
  // Simulate a connection followed by a disconnection.
  PerformConnectionStateTransition(Connection::CONNECTED,
                                   Connection::DISCONNECTED);
  EXPECT_GT(throttler_.GetDelay(), base::TimeDelta());
}

TEST_F(CryptAuthBluetoothThrottlerImplTest,
       GetDelay_ConnectionAfterIsProgressDisconnectIsThrottled) {
  // Simulate an attempt to connect (in progress connection) followed by a
  // disconnection.
  PerformConnectionStateTransition(Connection::IN_PROGRESS,
                                   Connection::DISCONNECTED);
  EXPECT_GT(throttler_.GetDelay(), base::TimeDelta());
}

TEST_F(CryptAuthBluetoothThrottlerImplTest,
       GetDelay_DelayedConnectionAfterDisconnectIsNotThrottled) {
  // Simulate a connection followed by a disconnection, then allow the cooldown
  // period to elapse.
  PerformConnectionStateTransition(Connection::CONNECTED,
                                   Connection::DISCONNECTED);
  clock_->Advance(throttler_.GetCooldownTimeDelta());
  EXPECT_EQ(base::TimeDelta(), throttler_.GetDelay());
}

TEST_F(CryptAuthBluetoothThrottlerImplTest,
       GetDelay_DelayedConnectionAfterInProgressDisconnectIsNotThrottled) {
  // Simulate an attempt to connect (in progress connection) followed by a
  // disconnection, then allow the cooldown period to elapse.
  PerformConnectionStateTransition(Connection::IN_PROGRESS,
                                   Connection::DISCONNECTED);
  clock_->Advance(throttler_.GetCooldownTimeDelta());
  EXPECT_EQ(base::TimeDelta(), throttler_.GetDelay());
}

}  // namespace cryptauth
