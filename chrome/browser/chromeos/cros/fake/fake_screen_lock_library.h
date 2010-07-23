// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_FAKE_FAKE_SCREEN_LOCK_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_FAKE_FAKE_SCREEN_LOCK_LIBRARY_H_

#include "chrome/browser/chromeos/cros/screen_lock_library.h"

namespace chromeos {

class FakeScreenLockLibrary : public ScreenLockLibrary {
 public:
  FakeScreenLockLibrary() {}
  virtual ~FakeScreenLockLibrary() {}
  virtual void AddObserver(Observer* observer) {}
  virtual void RemoveObserver(Observer* observer) {}
  virtual void NotifyScreenLockRequested() {}
  virtual void NotifyScreenLockCompleted() {}
  virtual void NotifyScreenUnlockRequested() {}
  virtual void NotifyScreenUnlockCompleted() {}
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_FAKE_FAKE_SCREEN_LOCK_LIBRARY_H_
