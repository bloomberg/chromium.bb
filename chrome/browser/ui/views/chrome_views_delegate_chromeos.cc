// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/chrome_views_delegate.h"

#include "base/time/time.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/system/input_device_settings.h"
#include "chromeos/chromeos_constants.h"

base::TimeDelta
ChromeViewsDelegate::GetDefaultTextfieldObscuredRevealDuration() {
  // Enable password echo on login screen when the keyboard driven flag is set.
  if (chromeos::UserManager::IsInitialized() &&
      !chromeos::UserManager::Get()->IsUserLoggedIn() &&
      chromeos::system::keyboard_settings::ForceKeyboardDrivenUINavigation()) {
      return base::TimeDelta::FromSeconds(1);
  }

  return base::TimeDelta();
}
