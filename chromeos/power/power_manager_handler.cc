// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/power/power_manager_handler.h"

#include <algorithm>
#include <cmath>

#include "base/logging.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"

namespace chromeos {

namespace {

static PowerManagerHandler* g_power_manager_handler = NULL;

}  // namespace

PowerManagerHandler::Observer::Observer() {
}

PowerManagerHandler::Observer::~Observer() {
}

void PowerManagerHandler::Observer::OnPowerStatusChanged(
    const PowerSupplyStatus& power_status) {
}

// static
void PowerManagerHandler::Initialize() {
  CHECK(!g_power_manager_handler);
  g_power_manager_handler = new PowerManagerHandler();
}

// static
void PowerManagerHandler::Shutdown() {
  CHECK(g_power_manager_handler);
  delete g_power_manager_handler;
  g_power_manager_handler = NULL;
}

// static
bool PowerManagerHandler::IsInitialized() {
  return g_power_manager_handler != NULL;
}

// static
PowerManagerHandler* PowerManagerHandler::Get() {
  CHECK(g_power_manager_handler)
      << "PowerManagerHandler::Get() called before Initialize().";
  return g_power_manager_handler;
}

void PowerManagerHandler::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PowerManagerHandler::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

PowerManagerHandler::PowerManagerHandler() {
  // If the DBusThreadManager or the PowerManagerClient aren't available, there
  // isn't much we can do. This should only happen when running tests.
  if (!DBusThreadManager::IsInitialized() ||
      !DBusThreadManager::Get() ||
      !DBusThreadManager::Get()->GetPowerManagerClient())
    return;
  DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(this);
  DBusThreadManager::Get()->GetPowerManagerClient()->RequestStatusUpdate();
}

PowerManagerHandler::~PowerManagerHandler() {
  if (!DBusThreadManager::IsInitialized() ||
      !DBusThreadManager::Get() ||
      !DBusThreadManager::Get()->GetPowerManagerClient())
    return;
  DBusThreadManager::Get()->GetPowerManagerClient()->RemoveObserver(this);
}

void PowerManagerHandler::RequestStatusUpdate() {
  DBusThreadManager::Get()->GetPowerManagerClient()->RequestStatusUpdate();
}

PowerSupplyStatus PowerManagerHandler::GetPowerSupplyStatus() const {
  return power_supply_status_;
}

void PowerManagerHandler::PowerChanged(
    const PowerSupplyStatus& power_status) {
  power_supply_status_ = power_status;
  FOR_EACH_OBSERVER(Observer, observers_, OnPowerStatusChanged(power_status));
}

}  // namespace chromeos
