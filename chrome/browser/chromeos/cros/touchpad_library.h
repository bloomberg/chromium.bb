// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_TOUCHPAD_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_TOUCHPAD_LIBRARY_H_
#pragma once

#include "third_party/cros/chromeos_touchpad.h"

namespace chromeos {

// This interface defines interaction with the ChromeOS synaptics library APIs.
// Users can get an instance of this library class like this:
//   chromeos::CrosLibrary::Get()->GetTouchpadLibrary()
class TouchpadLibrary {
 public:
  virtual ~TouchpadLibrary() {}
  virtual void SetSensitivity(int value) = 0;
  virtual void SetTapToClick(bool enabled) = 0;

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via CrosLibrary::Get().
  static TouchpadLibrary* GetImpl(bool stub);
};


}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_TOUCHPAD_LIBRARY_H_
