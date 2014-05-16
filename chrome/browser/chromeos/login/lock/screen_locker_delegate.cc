// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/lock/screen_locker.h"
#include "chrome/browser/chromeos/login/lock/screen_locker_delegate.h"

namespace chromeos {

ScreenLockerDelegate::ScreenLockerDelegate(ScreenLocker* screen_locker)
    : screen_locker_(screen_locker) {
}

ScreenLockerDelegate::~ScreenLockerDelegate() {
}

void ScreenLockerDelegate::ScreenLockReady() {
  screen_locker_->ScreenLockReady();
}

content::WebUI* ScreenLockerDelegate::GetAssociatedWebUI() {
  return NULL;
}

}  // namespace chromeos
