// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/power_library.h"

#include "base/basictypes.h"
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
        resume_status_connection_(NULL),
        status_(chromeos::PowerStatus()) {
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
    if (CrosLibrary::Get()->EnsureLoaded()) {
      power_status_connection_ =
          chromeos::MonitorPowerStatus(&PowerStatusChangedHandler, this);
      resume_status_connection_ =
          chromeos::MonitorResume(&SystemResumedHandler, this);
    }
  }

  virtual void AddObserver(Observer* observer) OVERRIDE {
    observers_.AddObserver(observer);
  }

  virtual void RemoveObserver(Observer* observer) OVERRIDE {
    observers_.RemoveObserver(observer);
  }

  virtual bool line_power_on() const OVERRIDE {
    return status_.line_power_on;
  }

  virtual bool battery_fully_charged() const OVERRIDE {
    return status_.battery_state == chromeos::BATTERY_STATE_FULLY_CHARGED;
  }

  virtual double battery_percentage() const OVERRIDE {
    return status_.battery_percentage;
  }

  virtual bool battery_is_present() const OVERRIDE {
    return status_.battery_is_present;
  }

  virtual base::TimeDelta battery_time_to_empty() const OVERRIDE {
    return base::TimeDelta::FromSeconds(status_.battery_time_to_empty);
  }

  virtual base::TimeDelta battery_time_to_full() const OVERRIDE {
    return base::TimeDelta::FromSeconds(status_.battery_time_to_full);
  }

  virtual void CalculateIdleTime(CalculateIdleTimeCallback* callback) OVERRIDE {
    // TODO(sidor): Examine if it's really a good idea to use void* as a second
    // argument.
    chromeos::GetIdleTime(&GetIdleTimeCallback, callback);
  }

  virtual void EnableScreenLock(bool enable) OVERRIDE {
    if (!CrosLibrary::Get()->EnsureLoaded())
      return;

    // Make sure we run on FILE thread becuase chromeos::EnableScreenLock
    // would write power manager config file to disk.
    if (!BrowserThread::CurrentlyOn(BrowserThread::FILE)) {
      BrowserThread::PostTask(
          BrowserThread::FILE, FROM_HERE,
          NewRunnableMethod(this, &PowerLibraryImpl::EnableScreenLock, enable));
      return;
    }

    chromeos::EnableScreenLock(enable);
  }

  virtual void RequestRestart() OVERRIDE {
    if (CrosLibrary::Get()->EnsureLoaded())
      chromeos::RequestRestart();
  }

  virtual void RequestShutdown() OVERRIDE {
    if (CrosLibrary::Get()->EnsureLoaded())
      chromeos::RequestShutdown();
  }
  // End PowerLibrary implementation.

 private:
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

  static void PowerStatusChangedHandler(void* object,
                                        const chromeos::PowerStatus& status) {
    PowerLibraryImpl* power = static_cast<PowerLibraryImpl*>(object);
    power->UpdatePowerStatus(status);
  }

  static void SystemResumedHandler(void* object) {
    PowerLibraryImpl* power = static_cast<PowerLibraryImpl*>(object);
    power->SystemResumed();
  }

  void UpdatePowerStatus(const chromeos::PowerStatus& status) {
    // Make sure we run on UI thread.
    if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          NewRunnableMethod(
              this, &PowerLibraryImpl::UpdatePowerStatus, status));
      return;
    }

    DVLOG(1) << "Power lpo=" << status.line_power_on
             << " sta=" << status.battery_state
             << " per=" << status.battery_percentage
             << " tte=" << status.battery_time_to_empty
             << " ttf=" << status.battery_time_to_full;
    status_ = status;
    FOR_EACH_OBSERVER(Observer, observers_, PowerChanged(this));
  }

  void SystemResumed() {
    // Make sure we run on the UI thread.
    if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          NewRunnableMethod(this, &PowerLibraryImpl::SystemResumed));
      return;
    }

    FOR_EACH_OBSERVER(Observer, observers_, SystemResumed());
  }

  ObserverList<Observer> observers_;

  // A reference to the power battery API, to allow callbacks when the battery
  // status changes.
  chromeos::PowerStatusConnection power_status_connection_;

  // A reference to the resume alerts.
  chromeos::ResumeConnection resume_status_connection_;

  // The latest power status.
  chromeos::PowerStatus status_;

  DISALLOW_COPY_AND_ASSIGN(PowerLibraryImpl);
};

// The stub implementation runs the battery up and down, pausing at the
// fully charged and fully depleted states.
class PowerLibraryStubImpl : public PowerLibrary {
 public:
  PowerLibraryStubImpl()
      : discharging_(true),
        battery_percentage_(20),
        pause_count_(0) {
    timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(100),
        this,
        &PowerLibraryStubImpl::Update);
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

  virtual bool line_power_on() const OVERRIDE {
    return !discharging_;
  }

  virtual bool battery_fully_charged() const OVERRIDE {
    return battery_percentage_ == 100;
  }

  virtual double battery_percentage() const OVERRIDE {
    return battery_percentage_;
  }

  virtual bool battery_is_present() const OVERRIDE {
    return true;
  }

  virtual base::TimeDelta battery_time_to_empty() const OVERRIDE {
    if (battery_percentage_ == 0)
      return base::TimeDelta::FromSeconds(1);
    else
      return (base::TimeDelta::FromHours(3) * battery_percentage_) / 100;
  }

  virtual base::TimeDelta battery_time_to_full() const OVERRIDE {
    if (battery_percentage_ == 100)
      return base::TimeDelta::FromSeconds(1);
    else
      return base::TimeDelta::FromHours(3) - battery_time_to_empty();
  }

  virtual void CalculateIdleTime(CalculateIdleTimeCallback* callback) OVERRIDE {
    callback->Run(0);
    delete callback;
  }
  virtual void EnableScreenLock(bool enable) OVERRIDE {}
  virtual void RequestRestart() OVERRIDE {}
  virtual void RequestShutdown() OVERRIDE {}
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
    FOR_EACH_OBSERVER(Observer, observers_, PowerChanged(this));
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

// Allows InvokeLater without adding refcounting. This class is a Singleton and
// won't be deleted until it's last InvokeLater is run.
DISABLE_RUNNABLE_METHOD_REFCOUNT(chromeos::PowerLibraryImpl);
