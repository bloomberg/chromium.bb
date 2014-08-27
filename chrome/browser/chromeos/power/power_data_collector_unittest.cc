// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/power_data_collector.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

class PowerDataCollectorTest : public testing::Test {
 public:
  PowerDataCollectorTest() : power_data_collector_(NULL) {}
  virtual ~PowerDataCollectorTest() {}

  virtual void SetUp() OVERRIDE {
    DBusThreadManager::Initialize();
    PowerDataCollector::InitializeForTesting();
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
  const std::deque<PowerDataCollector::PowerSupplySample>& data1 =
      power_data_collector_->power_supply_data();
  ASSERT_EQ(static_cast<size_t>(1), data1.size());
  EXPECT_DOUBLE_EQ(prop1.battery_percent(), data1[0].battery_percent);
  EXPECT_FALSE(data1[0].external_power);

  prop2.set_external_power(power_manager::PowerSupplyProperties::AC);
  prop2.set_battery_percent(100.00);
  power_data_collector_->PowerChanged(prop2);
  const std::deque<PowerDataCollector::PowerSupplySample>& data2 =
      power_data_collector_->power_supply_data();
  ASSERT_EQ(static_cast<size_t>(2), data2.size());
  EXPECT_DOUBLE_EQ(prop2.battery_percent(), data2[1].battery_percent);
  EXPECT_TRUE(data2[1].external_power);
}

TEST_F(PowerDataCollectorTest, SuspendDone) {
  power_data_collector_->SuspendDone(base::TimeDelta::FromSeconds(10));
  const std::deque<PowerDataCollector::SystemResumedSample>& data1 =
      power_data_collector_->system_resumed_data();
  ASSERT_EQ(static_cast<size_t>(1), data1.size());
  ASSERT_EQ(static_cast<int64>(10), data1[0].sleep_duration.InSeconds());

  power_data_collector_->SuspendDone(base::TimeDelta::FromSeconds(20));
  const std::deque<PowerDataCollector::SystemResumedSample>& data2 =
      power_data_collector_->system_resumed_data();
  ASSERT_EQ(static_cast<size_t>(2), data2.size());
  ASSERT_EQ(static_cast<int64>(20), data2[1].sleep_duration.InSeconds());
}

TEST_F(PowerDataCollectorTest, AddSample) {
  std::deque<PowerDataCollector::PowerSupplySample> sample_deque;
  PowerDataCollector::PowerSupplySample sample1, sample2;
  sample1.time = base::Time::FromInternalValue(1000);
  sample2.time = sample1.time +
      base::TimeDelta::FromSeconds(PowerDataCollector::kSampleTimeLimitSec + 1);

  AddSample(&sample_deque, sample1);
  ASSERT_EQ(static_cast<size_t>(1), sample_deque.size());

  AddSample(&sample_deque, sample2);
  ASSERT_EQ(static_cast<size_t>(1), sample_deque.size());
  EXPECT_EQ(sample2.time.ToInternalValue(),
            sample_deque[0].time.ToInternalValue());
}

}  // namespace chromeos
