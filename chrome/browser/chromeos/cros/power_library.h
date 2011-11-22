// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_POWER_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_POWER_LIBRARY_H_
#pragma once

// TODO(sque): Move to chrome/browser/chromeos/system, crosbug.com/16558

#include "base/callback.h"

namespace chromeos {

// This interface defines interaction with the ChromeOS power library APIs.
// Classes can add themselves as observers. Users can get an instance of this
// library class like this: chromeos::CrosLibrary::Get()->GetPowerLibrary()
class PowerLibrary {
 public:
  class Observer {
   public:
    virtual void SystemResumed() = 0;

   protected:
    virtual ~Observer() {}
  };

  virtual ~PowerLibrary() {}

  virtual void Init() = 0;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via CrosLibrary::Get().
  static PowerLibrary* GetImpl(bool stub);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_POWER_LIBRARY_H_
