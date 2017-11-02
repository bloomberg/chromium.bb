// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/scheduler/device_status_listener.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/power_monitor_test_base.h"
#include "components/download/internal/scheduler/network_status_listener.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::InSequence;
using ConnectionTypeObserver =
    net::NetworkChangeNotifier::ConnectionTypeObserver;
using ConnectionType = net::NetworkChangeNotifier::ConnectionType;

namespace download {
namespace {

MATCHER_P(NetworkStatusEqual, value, "") {
  return arg.network_status == value;
}

MATCHER_P(BatteryStatusEqual, value, "") {
  return arg.battery_status == value;
}

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
    net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(type);
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
  explicit TestDeviceStatusListener()
      : DeviceStatusListener(base::TimeDelta(), base::TimeDelta()) {}

  void BuildNetworkStatusListener() override {
    network_listener_ = base::MakeUnique<NetworkStatusListenerImpl>();
  }
};

class DeviceStatusListenerTest : public testing::Test {
 public:
  void SetUp() override {
    auto power_source = base::MakeUnique<base::PowerMonitorTestSource>();
    power_source_ = power_source.get();
    power_monitor_ =
        base::MakeUnique<base::PowerMonitor>(std::move(power_source));

    listener_ = base::MakeUnique<TestDeviceStatusListener>();
  }

  void TearDown() override { listener_.reset(); }

  // Start the listener with certain network and battery state.
  void StartListener(ConnectionType type, bool on_battery_power) {
    ChangeNetworkType(type);
    SimulateBatteryChange(on_battery_power);
    base::RunLoop().RunUntilIdle();

    EXPECT_CALL(mock_observer_, OnDeviceStatusChanged(_))
        .Times(1)
        .RetiresOnSaturation();
    listener_->Start(&mock_observer_);
    base::RunLoop().RunUntilIdle();
  }

  // Simulates a network change call, the event will be broadcasted
  // asynchronously.
  void ChangeNetworkType(ConnectionType type) {
    test_network_notifier_.ChangeNetworkType(type);
  }

  // Simulates a network change call, the event will be sent to client
  // immediately.
  void ChangeNetworkTypeImmediately(ConnectionType type) {
    DCHECK(listener_.get());
    static_cast<NetworkStatusListener::Observer*>(listener_.get())
        ->OnNetworkChanged(type);
  }

  // Simulates a battery change call.
  void SimulateBatteryChange(bool on_battery_power) {
    power_source_->GeneratePowerStateEvent(on_battery_power);
  }

 protected:
  std::unique_ptr<DeviceStatusListener> listener_;
  MockObserver mock_observer_;

  // Needed for network change notifier and power monitor.
  base::MessageLoop message_loop_;
  TestNetworkChangeNotifier test_network_notifier_;
  std::unique_ptr<base::PowerMonitor> power_monitor_;
  base::PowerMonitorTestSource* power_source_;
};

// Verifies the initial state that the observer should be notified.
TEST_F(DeviceStatusListenerTest, InitialNoOptState) {
  ChangeNetworkType(ConnectionType::CONNECTION_NONE);
  SimulateBatteryChange(true); /* Not charging. */
  EXPECT_EQ(DeviceStatus(), listener_->CurrentDeviceStatus());

  listener_->Start(&mock_observer_);

  // We are in no opt state, notify the observer.
  EXPECT_CALL(mock_observer_, OnDeviceStatusChanged(_)).Times(1);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceStatus(), listener_->CurrentDeviceStatus());
}

TEST_F(DeviceStatusListenerTest, TestValidStateChecks) {
  ChangeNetworkType(ConnectionType::CONNECTION_NONE);
  SimulateBatteryChange(true);
  EXPECT_EQ(DeviceStatus(), listener_->CurrentDeviceStatus());

  {
    EXPECT_CALL(mock_observer_, OnDeviceStatusChanged(_)).Times(0);
    listener_->Start(&mock_observer_);
    EXPECT_FALSE(listener_->is_valid_state());
  }

  {
    EXPECT_CALL(mock_observer_, OnDeviceStatusChanged(_)).Times(1);
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(listener_->is_valid_state());
  }

  // Simulate a connection change directly on the DeviceStatusListener itself to
  // allow validating the state correctly here.
  ChangeNetworkTypeImmediately(ConnectionType::CONNECTION_4G);
  EXPECT_FALSE(listener_->is_valid_state());

  {
    EXPECT_CALL(mock_observer_, OnDeviceStatusChanged(_)).Times(1);
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(listener_->is_valid_state());
  }

  {
    EXPECT_CALL(mock_observer_, OnDeviceStatusChanged(_)).Times(1);
    ChangeNetworkTypeImmediately(ConnectionType::CONNECTION_NONE);
    EXPECT_TRUE(listener_->is_valid_state());
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(listener_->is_valid_state());
  }
}

// Ensures the observer is notified when network condition changes.
TEST_F(DeviceStatusListenerTest, NotifyObserverNetworkChange) {
  listener_->Start(&mock_observer_);

  // Initial states check.
  EXPECT_EQ(NetworkStatus::DISCONNECTED,
            listener_->CurrentDeviceStatus().network_status);

  // Network switch between mobile networks, the observer should be notified
  // only once.
  ChangeNetworkType(ConnectionType::CONNECTION_4G);
  ChangeNetworkType(ConnectionType::CONNECTION_3G);
  ChangeNetworkType(ConnectionType::CONNECTION_2G);

  // Verifies the online signal is sent in a post task after a delay.
  EXPECT_EQ(NetworkStatus::DISCONNECTED,
            listener_->CurrentDeviceStatus().network_status);
  EXPECT_CALL(mock_observer_,
              OnDeviceStatusChanged(NetworkStatusEqual(NetworkStatus::METERED)))
      .Times(1)
      .RetiresOnSaturation();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(NetworkStatus::METERED,
            listener_->CurrentDeviceStatus().network_status);

  // Network is switched between wifi and ethernet, the observer should be
  // notified only once.
  ChangeNetworkType(ConnectionType::CONNECTION_WIFI);
  ChangeNetworkType(ConnectionType::CONNECTION_ETHERNET);

  EXPECT_CALL(mock_observer_, OnDeviceStatusChanged(
                                  NetworkStatusEqual(NetworkStatus::UNMETERED)))
      .Times(1)
      .RetiresOnSaturation();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(NetworkStatus::UNMETERED,
            listener_->CurrentDeviceStatus().network_status);
}

// Ensures the observer is notified when battery condition changes.
TEST_F(DeviceStatusListenerTest, NotifyObserverBatteryChange) {
  InSequence s;
  SimulateBatteryChange(false); /* Charging. */
  EXPECT_EQ(DeviceStatus(), listener_->CurrentDeviceStatus());

  listener_->Start(&mock_observer_);

  EXPECT_CALL(mock_observer_, OnDeviceStatusChanged(
                                  BatteryStatusEqual(BatteryStatus::CHARGING)))
      .Times(1)
      .RetiresOnSaturation();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(BatteryStatus::CHARGING,
            listener_->CurrentDeviceStatus().battery_status);

  EXPECT_CALL(
      mock_observer_,
      OnDeviceStatusChanged(BatteryStatusEqual(BatteryStatus::NOT_CHARGING)))
      .Times(1)
      .RetiresOnSaturation();
  SimulateBatteryChange(true); /* Not charging. */
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(BatteryStatus::NOT_CHARGING,
            listener_->CurrentDeviceStatus().battery_status);

  listener_->Stop();
};

// Verify a sequence of offline->online->offline network state changes.
TEST_F(DeviceStatusListenerTest, OfflineOnlineOffline) {
  StartListener(ConnectionType::CONNECTION_NONE, true);

  // Initial state is offline.
  EXPECT_EQ(NetworkStatus::DISCONNECTED,
            listener_->CurrentDeviceStatus().network_status);
  EXPECT_TRUE(listener_->is_valid_state());
  EXPECT_CALL(mock_observer_, OnDeviceStatusChanged(_)).Times(0);

  // Change to online.
  ChangeNetworkTypeImmediately(ConnectionType::CONNECTION_4G);
  EXPECT_FALSE(listener_->is_valid_state());
  EXPECT_EQ(NetworkStatus::DISCONNECTED,
            listener_->CurrentDeviceStatus().network_status);

  // Change to offline immediately.
  ChangeNetworkTypeImmediately(ConnectionType::CONNECTION_NONE);

  // Since the state changed back to offline before delayed online signal is
  // reported. The state becomes valid again immediately.
  EXPECT_TRUE(listener_->is_valid_state());
  EXPECT_EQ(NetworkStatus::DISCONNECTED,
            listener_->CurrentDeviceStatus().network_status);

  // No more online signal since we are already offline.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(NetworkStatus::DISCONNECTED,
            listener_->CurrentDeviceStatus().network_status);
}

// Verify a sequence of online->offline->online network state changes.
TEST_F(DeviceStatusListenerTest, OnlineOfflineOnline) {
  StartListener(ConnectionType::CONNECTION_3G, true);

  // Initial states is online.
  EXPECT_EQ(NetworkStatus::METERED,
            listener_->CurrentDeviceStatus().network_status);
  EXPECT_TRUE(listener_->is_valid_state());

  // Change to offline. Signal is broadcasted immediately.
  EXPECT_CALL(
      mock_observer_,
      OnDeviceStatusChanged(NetworkStatusEqual(NetworkStatus::DISCONNECTED)))
      .Times(1)
      .RetiresOnSaturation();
  ChangeNetworkTypeImmediately(ConnectionType::CONNECTION_NONE);
  EXPECT_TRUE(listener_->is_valid_state());
  EXPECT_EQ(NetworkStatus::DISCONNECTED,
            listener_->CurrentDeviceStatus().network_status);

  // Change to online. Signal will be broadcasted after a delay.
  ChangeNetworkTypeImmediately(ConnectionType::CONNECTION_WIFI);
  EXPECT_FALSE(listener_->is_valid_state());
  EXPECT_EQ(NetworkStatus::DISCONNECTED,
            listener_->CurrentDeviceStatus().network_status);

  EXPECT_CALL(mock_observer_, OnDeviceStatusChanged(
                                  NetworkStatusEqual(NetworkStatus::UNMETERED)))
      .Times(1)
      .RetiresOnSaturation();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(listener_->is_valid_state());
  EXPECT_EQ(NetworkStatus::UNMETERED,
            listener_->CurrentDeviceStatus().network_status);
}

}  // namespace
}  // namespace download
