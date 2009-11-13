// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power_library.h"

#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/chromeos/cros_library.h"

// Allows InvokeLater without adding refcounting. This class is a Singleton and
// won't be deleted until it's last InvokeLater is run.
template <>
struct RunnableMethodTraits<chromeos::PowerLibrary> {
  void RetainCallee(chromeos::PowerLibrary* obj) {}
  void ReleaseCallee(chromeos::PowerLibrary* obj) {}
};

namespace chromeos {

PowerLibrary::PowerLibrary() : status_(chromeos::PowerStatus()) {
  if (CrosLibrary::EnsureLoaded()) {
    Init();
  }
}

PowerLibrary::~PowerLibrary() {
  if (CrosLibrary::EnsureLoaded()) {
    chromeos::DisconnectPowerStatus(power_status_connection_);
  }
}

// static
PowerLibrary* PowerLibrary::Get() {
  return Singleton<PowerLibrary>::get();
}

// static
bool PowerLibrary::EnsureLoaded() {
  return CrosLibrary::EnsureLoaded();
}

void PowerLibrary::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PowerLibrary::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool PowerLibrary::line_power_on() const {
  return status_.line_power_on;
}

bool PowerLibrary::battery_is_present() const {
  return status_.battery_is_present;
}

bool PowerLibrary::battery_fully_charged() const {
  return status_.battery_state == chromeos::BATTERY_STATE_FULLY_CHARGED;
}

double PowerLibrary::battery_percentage() const {
  return status_.battery_percentage;
}

base::TimeDelta PowerLibrary::battery_time_to_empty() const {
  return base::TimeDelta::FromSeconds(status_.battery_time_to_empty);
}

base::TimeDelta PowerLibrary::battery_time_to_full() const {
  return base::TimeDelta::FromSeconds(status_.battery_time_to_full);
}

// static
void PowerLibrary::PowerStatusChangedHandler(void* object,
    const chromeos::PowerStatus& status) {
  PowerLibrary* power = static_cast<PowerLibrary*>(object);
  power->UpdatePowerStatus(status);
}

void PowerLibrary::Init() {
  power_status_connection_ = chromeos::MonitorPowerStatus(
      &PowerStatusChangedHandler, this);
}

void PowerLibrary::UpdatePowerStatus(const chromeos::PowerStatus& status) {
  // Make sure we run on UI thread.
  if (!ChromeThread::CurrentlyOn(ChromeThread::UI)) {
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableMethod(this, &PowerLibrary::UpdatePowerStatus, status));
    return;
  }

  DLOG(INFO) << "Power" <<
                " lpo=" << status.line_power_on <<
                " sta=" << status.battery_state <<
                " per=" << status.battery_percentage <<
                " tte=" << status.battery_time_to_empty <<
                " ttf=" << status.battery_time_to_full;
  status_ = status;
  FOR_EACH_OBSERVER(Observer, observers_, PowerChanged(this));
}

}  // namespace chromeos
