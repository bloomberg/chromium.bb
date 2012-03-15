// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_KIOSK_MODE_KIOSK_MODE_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_KIOSK_MODE_KIOSK_MODE_HELPER_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"

namespace base {
template <typename T> struct DefaultLazyInstanceTraits;
}

// This class centralizes all our code to get KioskMode settings; since
// KioskMode interferes with normal operations all over Chrome, having all
// data about it pulled from a central location would make future
// refactorings easier. This class also handles getting trust for the policies
// via it's init method.
//
// Note: If Initialize is not called before the various Getters, we'll return
// invalid values.
//
// TODO(rkc): Once the enterprise policy side of this code is checked in, add
// code to pull from the enterprise policy instead of flags and constants.
namespace chromeos {

class KioskModeHelper {
 public:
  // This method checks if Kiosk Mode is enabled or not.
  static bool IsKioskModeEnabled();

  static KioskModeHelper* Get();

  // Initialize the settings helper; this will wait till trust is established
  // for the enterprise policies then call the callback to notify the caller.
  void Initialize(const base::Closure& notify_initialized);
  bool is_initialized() const { return is_initialized_; }

  // The path to the screensaver extension.
  std::string GetScreensaverPath() const;
  // The timeout before which we'll start showing the screensaver.
  int64 GetScreensaverTimeout() const;
  // The time to logout the user in on idle.
  int64 GetIdleLogoutTimeout() const;
  // The time to show the countdown timer for.
  int64 GetIdleLogoutWarningTimeout() const;

 private:
  friend struct base::DefaultLazyInstanceTraits<KioskModeHelper>;
  KioskModeHelper();

  bool is_initialized_;

  DISALLOW_COPY_AND_ASSIGN(KioskModeHelper);
};


}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_KIOSK_MODE_KIOSK_MODE_HELPER_H_
