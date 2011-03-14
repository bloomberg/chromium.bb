// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_BRIGHTNESS_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_BRIGHTNESS_LIBRARY_H_
#pragma once

#include "third_party/cros/chromeos_brightness.h"

namespace chromeos {

class BrightnessLibrary {
 public:
  class Observer {
   public:
    virtual void BrightnessChanged(int level, bool user_initiated) = 0;
  };

  virtual ~BrightnessLibrary() {}

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via CrosLibrary::Get().
  static BrightnessLibrary* GetImpl(bool stub);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_BRIGHTNESS_LIBRARY_H_
