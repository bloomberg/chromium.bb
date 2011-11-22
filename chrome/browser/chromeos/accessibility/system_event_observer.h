// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_SYSTEM_EVENT_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_SYSTEM_EVENT_OBSERVER_H_
#pragma once

#include "chrome/browser/chromeos/cros/power_library.h"
#include "chrome/browser/chromeos/cros/screen_lock_library.h"

namespace chromeos {
namespace accessibility {

// A singleton class to observe system events like wake up from sleep and
// screen unlock.
class SystemEventObserver : public PowerLibrary::Observer,
                            public ScreenLockLibrary::Observer {
 public:
  virtual ~SystemEventObserver();

  // PowerLibrary::Observer override.
  virtual void SystemResumed() OVERRIDE;

  // ScreenLockLibrary::Observer override.
  virtual void LockScreen(ScreenLockLibrary* screen_lock_library) OVERRIDE;

  // ScreenLockLibrary::Observer override.
  virtual void UnlockScreen(ScreenLockLibrary* screen_lock_library) OVERRIDE;

  // ScreenLockLibrary::Observer override.
  virtual void UnlockScreenFailed(ScreenLockLibrary* screen_lock_library)
      OVERRIDE;

  // Creates the global SystemEventObserver instance.
  static void Initialize();

  // Returns a pointer to the global SystemEventObserver instance.
  // Initialize() should already have been called.
  static SystemEventObserver* GetInstance();

  // Destroys the global SystemEventObserver Instance.
  static void Shutdown();

 private:
  SystemEventObserver();

  DISALLOW_COPY_AND_ASSIGN(SystemEventObserver);
};

}  // namespace accessibility
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_SYSTEM_EVENT_OBSERVER_H_
