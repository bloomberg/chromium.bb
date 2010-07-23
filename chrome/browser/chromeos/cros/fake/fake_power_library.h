// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_FAKE_FAKE_POWER_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_FAKE_FAKE_POWER_LIBRARY_H_

#include "chrome/browser/chromeos/cros/power_library.h"

namespace chromeos {

class FakePowerLibrary : public PowerLibrary {
 public:
  FakePowerLibrary() {}
  virtual ~FakePowerLibrary() {}
  virtual void AddObserver(Observer* observer) {}
  virtual void RemoveObserver(Observer* observer) {}
  virtual bool line_power_on() const { return true; }
  virtual bool battery_fully_charged() const { return true; }
  virtual double battery_percentage() const { return 100; }
  virtual bool battery_is_present() const { return true; }
  virtual base::TimeDelta battery_time_to_empty() const {
    return base::TimeDelta::FromSeconds(0);
  }
  virtual base::TimeDelta battery_time_to_full() const {
    return base::TimeDelta::FromSeconds(0);
  }
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_FAKE_FAKE_POWER_LIBRARY_H_
