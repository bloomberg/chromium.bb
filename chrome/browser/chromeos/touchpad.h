// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_TOUCHPAD_H_
#define CHROME_BROWSER_CHROMEOS_TOUCHPAD_H_

#include <string>

// For Synaptics touchpads, we use synclient to change settings on-the-fly.
// See "man synaptics" for a list of settings that can be changed.
// Since we are doing a system call to run the synclient binary, we make sure
// that we are running on the File thread so that we don't block the UI thread.
class Touchpad {
 public:
  // This methods makes a system call to synclient to change touchpad settings.
  // The system call will be invoked on the file thread.
  static void SetSynclientParam(const std::string& param, double value);

  // Set tap-to-click to value stored in preferences.
  static void SetTapToClick(bool value);

  // Set vertical edge scrolling to value stored in preferences.
  static void SetVertEdgeScroll(bool value);

  // Set touchpad speed factor to value stored in preferences.
  static void SetSpeedFactor(int value);

  // Set tap sensitivity to value stored in preferences.
  static void SetSensitivity(int value);
};

#endif  // CHROME_BROWSER_CHROMEOS_TOUCHPAD_H_
