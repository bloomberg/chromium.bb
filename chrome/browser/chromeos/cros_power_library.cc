// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros_power_library.h"

#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/chromeos/cros_library.h"

// Allows InvokeLater without adding refcounting. This class is a Singleton and
// won't be deleted until it's last InvokeLater is run.
template <>
struct RunnableMethodTraits<CrosPowerLibrary> {
  void RetainCallee(CrosPowerLibrary* obj) {}
  void ReleaseCallee(CrosPowerLibrary* obj) {}
};

CrosPowerLibrary::CrosPowerLibrary() : status_(chromeos::PowerStatus()) {
  if (CrosLibrary::loaded()) {
    Init();
  }
}

CrosPowerLibrary::~CrosPowerLibrary() {
  if (CrosLibrary::loaded()) {
    chromeos::DisconnectPowerStatus(power_status_connection_);
  }
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

bool CrosPowerLibrary::battery_is_present() const {
  return status_.battery_is_present;
}

bool CrosPowerLibrary::battery_fully_charged() const {
  return status_.battery_state == chromeos::BATTERY_STATE_FULLY_CHARGED;
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
  power->UpdatePowerStatus(status);
}

void CrosPowerLibrary::Init() {
  power_status_connection_ = chromeos::MonitorPowerStatus(
      &PowerStatusChangedHandler, this);
}

void CrosPowerLibrary::UpdatePowerStatus(const chromeos::PowerStatus& status) {
  // Make sure we run on UI thread.
  if (!ChromeThread::CurrentlyOn(ChromeThread::UI)) {
    MessageLoop* loop = ChromeThread::GetMessageLoop(ChromeThread::UI);
    if (loop)
      loop->PostTask(FROM_HERE, NewRunnableMethod(this,
          &CrosPowerLibrary::UpdatePowerStatus, status));
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
