// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/power/power_data_collector.h"

#include "base/logging.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"

namespace chromeos {

namespace {

// The global PowerDataCollector instance.
PowerDataCollector* g_power_data_collector = NULL;

}  // namespace

// static
void PowerDataCollector::Initialize() {
  // Check that power data collector is initialized only after the
  // DBusThreadManager is initialized.
  CHECK(DBusThreadManager::Get());
  CHECK(g_power_data_collector == NULL);
  g_power_data_collector = new PowerDataCollector();
}

// static
void PowerDataCollector::Shutdown() {
  // Shutdown only if initialized.
  CHECK(g_power_data_collector);
  delete g_power_data_collector;
  g_power_data_collector = NULL;
}

// static
PowerDataCollector* PowerDataCollector::Get() {
  CHECK(g_power_data_collector);
  return g_power_data_collector;
}

void PowerDataCollector::PowerChanged(
    const power_manager::PowerSupplyProperties& prop) {
  PowerSupplySnapshot snapshot;
  snapshot.time = base::TimeTicks::Now();
  snapshot.external_power = (prop.external_power() !=
      power_manager::PowerSupplyProperties::DISCONNECTED);
  snapshot.battery_percent = prop.battery_percent();

  power_supply_data_.push_back(snapshot);
}

PowerDataCollector::PowerDataCollector() {
  DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(this);
}

PowerDataCollector::~PowerDataCollector() {
  DBusThreadManager* dbus_manager = DBusThreadManager::Get();
  CHECK(dbus_manager);
  dbus_manager->GetPowerManagerClient()->RemoveObserver(this);
}

PowerDataCollector::PowerSupplySnapshot::PowerSupplySnapshot()
    : time(base::TimeTicks::Now()),
      external_power(false),
      battery_percent(0) {
}

}  // namespace chromeos
