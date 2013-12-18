// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/power/power_data_collector.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_dbus_thread_manager.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

class PowerDataCollectorTest : public testing::Test {
 public:
  PowerDataCollectorTest() : power_data_collector_(NULL) {}
  virtual ~PowerDataCollectorTest() {}

  virtual void SetUp() OVERRIDE {
    FakeDBusThreadManager* fake_dbus_thread_manager = new FakeDBusThreadManager;
    fake_dbus_thread_manager->SetFakeClients();
    DBusThreadManager::InitializeForTesting(fake_dbus_thread_manager);
    PowerDataCollector::Initialize();
    power_data_collector_ = PowerDataCollector::Get();
  }

  virtual void TearDown() OVERRIDE {
    PowerDataCollector::Shutdown();
    DBusThreadManager::Shutdown();
    power_data_collector_ = NULL;
  }

 protected:
  PowerDataCollector* power_data_collector_;
};

TEST_F(PowerDataCollectorTest, PowerChanged) {
  power_manager::PowerSupplyProperties prop1, prop2;

  prop1.set_external_power(power_manager::PowerSupplyProperties::DISCONNECTED);
  prop1.set_battery_percent(20.00);

  power_data_collector_->PowerChanged(prop1);
  const std::vector<PowerDataCollector::PowerSupplySnapshot>& data1 =
      power_data_collector_->power_supply_data();
  ASSERT_EQ(static_cast<size_t>(1), data1.size());
  EXPECT_DOUBLE_EQ(prop1.battery_percent(), data1[0].battery_percent);
  EXPECT_FALSE(data1[0].external_power);

  prop2.set_external_power(power_manager::PowerSupplyProperties::AC);
  prop2.set_battery_percent(100.00);

  power_data_collector_->PowerChanged(prop2);
  const std::vector<PowerDataCollector::PowerSupplySnapshot>& data2 =
      power_data_collector_->power_supply_data();
  ASSERT_EQ(static_cast<size_t>(2), data2.size());
  EXPECT_DOUBLE_EQ(prop2.battery_percent(), data1[1].battery_percent);
  EXPECT_TRUE(data2[1].external_power);
}

}  // namespace chromeos
