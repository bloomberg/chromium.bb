// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/scheduler/device_status_listener.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/power_monitor_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::InSequence;
using ConnectionTypeObserver =
    net::NetworkChangeNotifier::ConnectionTypeObserver;
using ConnectionType = net::NetworkChangeNotifier::ConnectionType;

namespace download {
namespace {

// NetworkChangeNotifier that can change network type in tests.
class TestNetworkChangeNotifier : public net::NetworkChangeNotifier {
 public:
  TestNetworkChangeNotifier()
      : net::NetworkChangeNotifier(),
        conn_type_(ConnectionType::CONNECTION_UNKNOWN) {}

  // net::NetworkChangeNotifier implementation.
  ConnectionType GetCurrentConnectionType() const override {
    return conn_type_;
  }

  // Changes the network type.
  void ChangeNetworkType(ConnectionType type) {
    conn_type_ = type;
    net::NetworkChangeNotifier::NotifyObserversOfConnectionTypeChangeForTests(
        type);
  }

 private:
  ConnectionType conn_type_;

  DISALLOW_COPY_AND_ASSIGN(TestNetworkChangeNotifier);
};

class MockObserver : public DeviceStatusListener::Observer {
 public:
  MOCK_METHOD1(OnDeviceStatusChanged, void(const DeviceStatus&));
};

// Test target that only loads default implementation of NetworkStatusListener.
class TestDeviceStatusListener : public DeviceStatusListener {
 public:
  explicit TestDeviceStatusListener(const base::TimeDelta& delay)
      : DeviceStatusListener(delay) {}

  void BuildNetworkStatusListener() override {
    network_listener_ = base::MakeUnique<NetworkStatusListenerImpl>();
  }
};

class DeviceStatusListenerTest : public testing::Test {
 public:
  void SetUp() override {
    power_monitor_ = base::MakeUnique<base::PowerMonitor>(
        base::MakeUnique<base::PowerMonitorTestSource>());

    listener_ = base::MakeUnique<TestDeviceStatusListener>(
        base::TimeDelta::FromSeconds(0));
  }

  void TearDown() override { listener_.reset(); }

  // Simulates a network change call.
  void ChangeNetworkType(ConnectionType type) {
    test_network_notifier_.ChangeNetworkType(type);
  }

  // Simulates a battery change call.
  void SimulateBatteryChange(bool on_battery_power) {
    static_cast<base::PowerObserver*>(listener_.get())
        ->OnPowerStateChange(on_battery_power);
  }

 protected:
  std::unique_ptr<DeviceStatusListener> listener_;
  MockObserver mock_observer_;

  // Needed for network change notifier and power monitor.
  base::MessageLoop message_loop_;
  TestNetworkChangeNotifier test_network_notifier_;
  std::unique_ptr<base::PowerMonitor> power_monitor_;
};

// Ensures the observer is notified when network condition changes.
TEST_F(DeviceStatusListenerTest, NotifyObserverNetworkChange) {
  listener_->Start(&mock_observer_);

  // Initial states check.
  DeviceStatus status = listener_->CurrentDeviceStatus();
  EXPECT_EQ(NetworkStatus::DISCONNECTED, status.network_status);

  // Network switch between mobile networks, the observer should be notified
  // only once.
  status.network_status = NetworkStatus::METERED;
  EXPECT_CALL(mock_observer_, OnDeviceStatusChanged(status))
      .Times(1)
      .RetiresOnSaturation();

  ChangeNetworkType(ConnectionType::CONNECTION_4G);
  ChangeNetworkType(ConnectionType::CONNECTION_3G);
  ChangeNetworkType(ConnectionType::CONNECTION_2G);

  // Verifies the online signal is sent in a post task after a delay.
  EXPECT_EQ(NetworkStatus::DISCONNECTED,
            listener_->CurrentDeviceStatus().network_status);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(NetworkStatus::METERED,
            listener_->CurrentDeviceStatus().network_status);

  // Network is switched between wifi and ethernet, the observer should be
  // notified only once.
  status.network_status = NetworkStatus::UNMETERED;
  EXPECT_CALL(mock_observer_, OnDeviceStatusChanged(status))
      .Times(1)
      .RetiresOnSaturation();

  ChangeNetworkType(ConnectionType::CONNECTION_WIFI);
  ChangeNetworkType(ConnectionType::CONNECTION_ETHERNET);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(NetworkStatus::UNMETERED,
            listener_->CurrentDeviceStatus().network_status);
}

// Ensures the observer is notified when battery condition changes.
TEST_F(DeviceStatusListenerTest, NotifyObserverBatteryChange) {
  InSequence s;
  listener_->Start(&mock_observer_);
  DeviceStatus status = listener_->CurrentDeviceStatus();
  status.battery_status = BatteryStatus::NOT_CHARGING;
  EXPECT_CALL(mock_observer_, OnDeviceStatusChanged(status))
      .RetiresOnSaturation();
  SimulateBatteryChange(true);

  status.battery_status = BatteryStatus::CHARGING;
  EXPECT_CALL(mock_observer_, OnDeviceStatusChanged(status))
      .RetiresOnSaturation();
  SimulateBatteryChange(false);
  listener_->Stop();
};

}  // namespace
}  // namespace download
