// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/power/power_status.h"

#include <algorithm>
#include <cmath>

#include "base/logging.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"

namespace ash {
namespace internal {

namespace {

static PowerStatus* g_power_status = NULL;

}  // namespace

// static
void PowerStatus::Initialize() {
  CHECK(!g_power_status);
  g_power_status = new PowerStatus();
}

// static
void PowerStatus::Shutdown() {
  CHECK(g_power_status);
  delete g_power_status;
  g_power_status = NULL;
}

// static
bool PowerStatus::IsInitialized() {
  return g_power_status != NULL;
}

// static
PowerStatus* PowerStatus::Get() {
  CHECK(g_power_status) << "PowerStatus::Get() called before Initialize().";
  return g_power_status;
}

void PowerStatus::AddObserver(Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void PowerStatus::RemoveObserver(Observer* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

PowerStatus::PowerStatus() {
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->
      AddObserver(this);
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->
      RequestStatusUpdate();
}

PowerStatus::~PowerStatus() {
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->
      RemoveObserver(this);
}

void PowerStatus::RequestStatusUpdate() {
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->
      RequestStatusUpdate();
}

chromeos::PowerSupplyStatus PowerStatus::GetPowerSupplyStatus() const {
  return power_supply_status_;
}

void PowerStatus::PowerChanged(
    const chromeos::PowerSupplyStatus& power_status) {
  power_supply_status_ = power_status;
  FOR_EACH_OBSERVER(Observer, observers_, OnPowerStatusChanged(power_status));
}

}  // namespace internal
}  // namespace ash
