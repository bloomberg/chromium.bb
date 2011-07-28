// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_SCREEN_LOCK_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_SCREEN_LOCK_LIBRARY_H_
#pragma once

namespace chromeos {

// This interface handles the interaction with the ChromeOS screen lock API.
class ScreenLockLibrary {
 public:
  class Observer {
   public:
    virtual ~Observer() {}

    virtual void LockScreen(ScreenLockLibrary* obj) = 0;
    virtual void UnlockScreen(ScreenLockLibrary* obj) = 0;
    virtual void UnlockScreenFailed(ScreenLockLibrary* obj) = 0;
  };

  virtual ~ScreenLockLibrary() {}

  virtual void Init() = 0;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Notifies PowerManager that a user requested to lock the screen.
  virtual void NotifyScreenLockRequested() = 0;

  // Notifies PowerManager that screen lock has been completed.
  virtual void NotifyScreenLockCompleted() = 0;

  // Notifies PowerManager that a user unlocked the screen.
  virtual void NotifyScreenUnlockRequested() = 0;

  // Notifies PowerManager that screen is unlocked.
  virtual void NotifyScreenUnlockCompleted() = 0;

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via CrosLibrary::Get().
  static ScreenLockLibrary* GetImpl(bool stub);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_SCREEN_LOCK_LIBRARY_H_
