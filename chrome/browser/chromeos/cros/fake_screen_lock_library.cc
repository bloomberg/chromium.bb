// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/fake_screen_lock_library.h"

namespace chromeos {

void FakeScreenLockLibrary::AddObserver(Observer* observer) {
}

void FakeScreenLockLibrary::RemoveObserver(Observer* observer) {
}

void FakeScreenLockLibrary::NotifyScreenLockRequested() {
}

void FakeScreenLockLibrary::NotifyScreenLockCompleted() {
}

void FakeScreenLockLibrary::NotifyScreenUnlockRequested() {
}

void FakeScreenLockLibrary::NotifyScreenUnlockCompleted() {
}

}  // namespace chromeos
