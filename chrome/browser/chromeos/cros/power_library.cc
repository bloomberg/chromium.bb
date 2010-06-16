// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/power_library.h"

#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/chromeos/cros/cros_library.h"

// Allows InvokeLater without adding refcounting. This class is a Singleton and
// won't be deleted until it's last InvokeLater is run.
DISABLE_RUNNABLE_METHOD_REFCOUNT(chromeos::PowerLibraryImpl);

namespace chromeos {

PowerLibraryImpl::PowerLibraryImpl()
    : power_status_connection_(NULL),
      status_(chromeos::PowerStatus()) {
  if (CrosLibrary::Get()->EnsureLoaded()) {
    Init();
  }
}

PowerLibraryImpl::~PowerLibraryImpl() {
  if (power_status_connection_) {
    chromeos::DisconnectPowerStatus(power_status_connection_);
  }
}

void PowerLibraryImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PowerLibraryImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool PowerLibraryImpl::line_power_on() const {
  return status_.line_power_on;
}

bool PowerLibraryImpl::battery_is_present() const {
  return status_.battery_is_present;
}

bool PowerLibraryImpl::battery_fully_charged() const {
  return status_.battery_state == chromeos::BATTERY_STATE_FULLY_CHARGED;
}

double PowerLibraryImpl::battery_percentage() const {
  return status_.battery_percentage;
}

base::TimeDelta PowerLibraryImpl::battery_time_to_empty() const {
  return base::TimeDelta::FromSeconds(status_.battery_time_to_empty);
}

base::TimeDelta PowerLibraryImpl::battery_time_to_full() const {
  return base::TimeDelta::FromSeconds(status_.battery_time_to_full);
}

// static
void PowerLibraryImpl::PowerStatusChangedHandler(void* object,
    const chromeos::PowerStatus& status) {
  PowerLibraryImpl* power = static_cast<PowerLibraryImpl*>(object);
  power->UpdatePowerStatus(status);
}

void PowerLibraryImpl::Init() {
  power_status_connection_ = chromeos::MonitorPowerStatus(
      &PowerStatusChangedHandler, this);
}

void PowerLibraryImpl::UpdatePowerStatus(const chromeos::PowerStatus& status) {
  // Make sure we run on UI thread.
  if (!ChromeThread::CurrentlyOn(ChromeThread::UI)) {
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableMethod(this, &PowerLibraryImpl::UpdatePowerStatus, status));
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
