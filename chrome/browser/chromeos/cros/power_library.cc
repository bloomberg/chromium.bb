// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/power_library.h"

#include <algorithm>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/observer_list.h"
#include "base/time.h"
#include "base/timer.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "content/browser/browser_thread.h"
#include "third_party/cros/chromeos_power.h"
#include "third_party/cros/chromeos_resume.h"

namespace chromeos {

class PowerLibraryImpl : public PowerLibrary {
 public:
  PowerLibraryImpl()
      : power_status_connection_(NULL),
        resume_status_connection_(NULL) {
  }

  virtual ~PowerLibraryImpl() {
    if (power_status_connection_) {
      chromeos::DisconnectPowerStatus(power_status_connection_);
      power_status_connection_ = NULL;
    }
    if (resume_status_connection_) {
      chromeos::DisconnectResume(resume_status_connection_);
      resume_status_connection_ = NULL;
    }
  }

  // Begin PowerLibrary implementation.
  virtual void Init() OVERRIDE {
    DCHECK(CrosLibrary::Get()->libcros_loaded());
    power_status_connection_ =
        chromeos::MonitorPowerStatus(&PowerStatusChangedHandler, this);
    resume_status_connection_ =
        chromeos::MonitorResume(&SystemResumedHandler, this);
  }

  virtual void AddObserver(Observer* observer) OVERRIDE {
    observers_.AddObserver(observer);
  }

  virtual void RemoveObserver(Observer* observer) OVERRIDE {
    observers_.RemoveObserver(observer);
  }

  virtual void CalculateIdleTime(CalculateIdleTimeCallback* callback) OVERRIDE {
    // TODO(sidor): Examine if it's really a good idea to use void* as a second
    // argument.
    chromeos::GetIdleTime(&GetIdleTimeCallback, callback);
  }

  virtual void EnableScreenLock(bool enable) OVERRIDE {
    // Called when the screen preference is changed, which should always
    // run on UI thread.
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    // Post the task to FILE thread as chromeos::EnableScreenLock
    // would write power manager config file to disk.
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&PowerLibraryImpl::DoEnableScreenLock, enable));
  }

  virtual void RequestRestart() OVERRIDE {
    chromeos::RequestRestart();
  }

  virtual void RequestShutdown() OVERRIDE {
    chromeos::RequestShutdown();
  }

  virtual void RequestStatusUpdate() OVERRIDE {
    // TODO(stevenjb): chromeos::RetrievePowerInformation has been deprecated;
    // we should add a mechanism to immediately request an update, probably
    // when we migrate the DBus code from libcros to here.
  }

  // End PowerLibrary implementation.

 private:
  static void DoEnableScreenLock(bool enable) {
    chromeos::EnableScreenLock(enable);
  }

  static void GetIdleTimeCallback(void* object,
                                 int64_t time_idle_ms,
                                 bool success) {
    DCHECK(object);
    CalculateIdleTimeCallback* notify =
        static_cast<CalculateIdleTimeCallback*>(object);
    if (success) {
      notify->Run(time_idle_ms/1000);
    } else {
      LOG(ERROR) << "Power manager failed to calculate idle time.";
      notify->Run(-1);
    }
    delete notify;
  }

  static void PowerStatusChangedHandler(
      void* object, const chromeos::PowerStatus& power_status) {
    // TODO(sque): this is a temporary copy-over from libcros.  Soon libcros
    // will be removed and this will not be necessary.
    PowerSupplyStatus status;
    status.line_power_on         = power_status.line_power_on;
    status.battery_is_present    = power_status.battery_is_present;
    status.battery_is_full       =
        (power_status.battery_state == chromeos::BATTERY_STATE_FULLY_CHARGED);
    status.battery_seconds_to_empty = power_status.battery_time_to_empty;
    status.battery_seconds_to_full  = power_status.battery_time_to_full;
    status.battery_percentage    = power_status.battery_percentage;

    PowerLibraryImpl* power = static_cast<PowerLibraryImpl*>(object);
    power->UpdatePowerStatus(status);
  }

  static void SystemResumedHandler(void* object) {
    PowerLibraryImpl* power = static_cast<PowerLibraryImpl*>(object);
    power->SystemResumed();
  }

  void UpdatePowerStatus(const PowerSupplyStatus& status) {
    // Called from PowerStatusChangedHandler, a libcros callback which
    // should always run on UI thread.
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

    DVLOG(1) << "Power line_power_on = " << status.line_power_on
             << " percentage = " << status.battery_percentage
             << " seconds_to_empty = " << status.battery_seconds_to_empty
             << " seconds_to_full = " << status.battery_seconds_to_full;
    FOR_EACH_OBSERVER(Observer, observers_, PowerChanged(status));
  }

  void SystemResumed() {
    // Called from SystemResumedHandler, a libcros callback which should
    // always run on UI thread.
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    FOR_EACH_OBSERVER(Observer, observers_, SystemResumed());
  }

  ObserverList<Observer> observers_;

  // A reference to the power battery API, to allow callbacks when the battery
  // status changes.
  chromeos::PowerStatusConnection power_status_connection_;

  // A reference to the resume alerts.
  chromeos::ResumeConnection resume_status_connection_;

  DISALLOW_COPY_AND_ASSIGN(PowerLibraryImpl);
};

// The stub implementation runs the battery up and down, pausing at the
// fully charged and fully depleted states.
class PowerLibraryStubImpl : public PowerLibrary {
 public:
  PowerLibraryStubImpl()
      : discharging_(true),
        battery_percentage_(80),
        pause_count_(0) {
  }

  virtual ~PowerLibraryStubImpl() {}

  // Begin PowerLibrary implementation.
  virtual void Init() OVERRIDE {}
  virtual void AddObserver(Observer* observer) OVERRIDE {
    observers_.AddObserver(observer);
  }

  virtual void RemoveObserver(Observer* observer) OVERRIDE {
    observers_.RemoveObserver(observer);
  }

  virtual void CalculateIdleTime(CalculateIdleTimeCallback* callback) OVERRIDE {
    callback->Run(0);
    delete callback;
  }

  virtual void EnableScreenLock(bool enable) OVERRIDE {}

  virtual void RequestRestart() OVERRIDE {}

  virtual void RequestShutdown() OVERRIDE {}

  virtual void RequestStatusUpdate() OVERRIDE {
    if (!timer_.IsRunning()) {
      timer_.Start(
          FROM_HERE,
          base::TimeDelta::FromMilliseconds(100),
          this,
          &PowerLibraryStubImpl::Update);
    } else {
      timer_.Stop();
    }
  }

  // End PowerLibrary implementation.

 private:
  void Update() {
    // We pause at 0 and 100% so that it's easier to check those conditions.
    if (pause_count_ > 1) {
      pause_count_--;
      return;
    }

    if (battery_percentage_ == 0 || battery_percentage_ == 100) {
      if (pause_count_) {
        pause_count_ = 0;
        discharging_ = !discharging_;
      } else {
        pause_count_ = 20;
        return;
      }
    }
    battery_percentage_ += (discharging_ ? -1 : 1);

    PowerSupplyStatus status;
    status.line_power_on = !discharging_;
    status.battery_is_present = true;
    status.battery_percentage = battery_percentage_;
    status.battery_seconds_to_empty =
        std::max(1, battery_percentage_ * 180 / 100);
    status.battery_seconds_to_full =
        std::max(static_cast<int64>(1), 180 - status.battery_seconds_to_empty);

    FOR_EACH_OBSERVER(Observer, observers_, PowerChanged(status));
  }

  bool discharging_;
  int battery_percentage_;
  int pause_count_;
  ObserverList<Observer> observers_;
  base::RepeatingTimer<PowerLibraryStubImpl> timer_;
};

// static
PowerLibrary* PowerLibrary::GetImpl(bool stub) {
  PowerLibrary* impl;
  if (stub)
    impl = new PowerLibraryStubImpl();
  else
    impl = new PowerLibraryImpl();
  impl->Init();
  return impl;
}

}  // namespace chromeos
