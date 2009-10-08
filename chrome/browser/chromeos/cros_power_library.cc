// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros_power_library.h"

#include "base/string_util.h"
#include "chrome/browser/chromeos/cros_library.h"

CrosPowerLibrary::CrosPowerLibrary() {
  if (CrosLibrary::loaded()) {
    power_status_connection_ = chromeos::MonitorPowerStatus(
        &PowerStatusChangedHandler, this);
  }
}

CrosPowerLibrary::~CrosPowerLibrary() {
  if (CrosLibrary::loaded())
    chromeos::DisconnectPowerStatus(power_status_connection_);
}

// static
CrosPowerLibrary* CrosPowerLibrary::Get() {
  return Singleton<CrosPowerLibrary>::get();
}

// static
bool CrosPowerLibrary::loaded() {
  return CrosLibrary::loaded();
}

void CrosPowerLibrary::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void CrosPowerLibrary::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool CrosPowerLibrary::line_power_on() const {
  return status_.line_power_on;
}

bool CrosPowerLibrary::battery_fully_charged() const {
  return status_.line_power_on &&
      status_.battery_state == chromeos::BATTERY_STATE_FULLY_CHARGED;
}

double CrosPowerLibrary::battery_percentage() const {
  return status_.battery_percentage;
}

base::TimeDelta CrosPowerLibrary::battery_time_to_empty() const {
  return base::TimeDelta::FromSeconds(status_.battery_time_to_empty);
}

base::TimeDelta CrosPowerLibrary::battery_time_to_full() const {
  return base::TimeDelta::FromSeconds(status_.battery_time_to_full);
}

// static
void CrosPowerLibrary::PowerStatusChangedHandler(void* object,
    const chromeos::PowerStatus& status) {
  CrosPowerLibrary* power = static_cast<CrosPowerLibrary*>(object);
  DLOG(INFO) << "Power" <<
                " lpo=" << status.line_power_on <<
                " sta=" << status.battery_state <<
                " per=" << status.battery_percentage <<
                " tte=" << status.battery_time_to_empty <<
                " ttf=" << status.battery_time_to_full;
  power->status_ = status;
  FOR_EACH_OBSERVER(Observer, power->observers_, PowerChanged(power));
}
