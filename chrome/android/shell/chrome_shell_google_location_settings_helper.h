// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ANDROID_SHELL_CHROME_SHELL_GOOGLE_LOCATION_SETTINGS_HELPER_H_
#define CHROME_ANDROID_SHELL_CHROME_SHELL_GOOGLE_LOCATION_SETTINGS_HELPER_H_

#include "chrome/browser/android/google_location_settings_helper.h"

// Stub implementation of GoogleLocationSettingsHelper for testshell.
class ChromeShellGoogleLocationSettingsHelper
    : public GoogleLocationSettingsHelper {
 public:
  // GoogleLocationSettingsHelper implementation:
  virtual std::string GetAcceptButtonLabel(bool allow) OVERRIDE;
  virtual void ShowGoogleLocationSettings() OVERRIDE;
  virtual bool IsMasterLocationSettingEnabled() OVERRIDE;
  virtual bool IsGoogleAppsLocationSettingEnabled() OVERRIDE;
  virtual bool IsAllowLabel() OVERRIDE;

 protected:
  ChromeShellGoogleLocationSettingsHelper();
  virtual ~ChromeShellGoogleLocationSettingsHelper();

 private:
  friend class GoogleLocationSettingsHelper;

  DISALLOW_COPY_AND_ASSIGN(ChromeShellGoogleLocationSettingsHelper);
};

#endif  // CHROME_ANDROID_SHELL_CHROME_SHELL_GOOGLE_LOCATION_SETTINGS_HELPER_H_
