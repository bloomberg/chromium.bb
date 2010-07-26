// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_MOCK_SCREEN_LOCK_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_MOCK_SCREEN_LOCK_LIBRARY_H_
#pragma once

#include "chrome/browser/chromeos/cros/screen_lock_library.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockScreenLockLibrary : public ScreenLockLibrary {
 public:
  MockScreenLockLibrary() {}
  virtual ~MockScreenLockLibrary() {}
  MOCK_METHOD1(AddObserver, void(Observer*));
  MOCK_METHOD1(RemoveObserver, void(Observer*));

  MOCK_METHOD0(NotifyScreenLockRequested, void());
  MOCK_METHOD0(NotifyScreenLockCompleted, void());
  MOCK_METHOD0(NotifyScreenUnlockRequested, void());
  MOCK_METHOD0(NotifyScreenUnlockCompleted, void());
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_MOCK_SCREEN_LOCK_LIBRARY_H_
