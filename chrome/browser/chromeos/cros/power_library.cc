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
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/cros/chromeos_power.h"
#include "third_party/cros/chromeos_resume.h"

using content::BrowserThread;

namespace chromeos {

class PowerLibraryImpl : public PowerLibrary {
 public:
  PowerLibraryImpl()
      : resume_status_connection_(NULL) {
  }

  virtual ~PowerLibraryImpl() {
    if (resume_status_connection_) {
      chromeos::DisconnectResume(resume_status_connection_);
      resume_status_connection_ = NULL;
    }
  }

  // Begin PowerLibrary implementation.
  virtual void Init() OVERRIDE {
    DCHECK(CrosLibrary::Get()->libcros_loaded());
    resume_status_connection_ =
        chromeos::MonitorResume(&SystemResumedHandler, this);
  }

  virtual void AddObserver(Observer* observer) OVERRIDE {
    observers_.AddObserver(observer);
  }

  virtual void RemoveObserver(Observer* observer) OVERRIDE {
    observers_.RemoveObserver(observer);
  }

  // End PowerLibrary implementation.

 private:

  static void SystemResumedHandler(void* object) {
    PowerLibraryImpl* power = static_cast<PowerLibraryImpl*>(object);
    power->SystemResumed();
  }

  void SystemResumed() {
    // Called from SystemResumedHandler, a libcros callback which should
    // always run on UI thread.
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    FOR_EACH_OBSERVER(Observer, observers_, SystemResumed());
  }

  ObserverList<Observer> observers_;

  // A reference to the resume alerts.
  chromeos::ResumeConnection resume_status_connection_;

  DISALLOW_COPY_AND_ASSIGN(PowerLibraryImpl);
};

// The stub implementation runs the battery up and down, pausing at the
// fully charged and fully depleted states.
class PowerLibraryStubImpl : public PowerLibrary {
 public:
  PowerLibraryStubImpl() {
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

  // End PowerLibrary implementation.
 private:
  ObserverList<Observer> observers_;
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
