// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_APP_GOOGLE_LOCATION_SETTINGS_HELPER_H_
#define CHROME_BROWSER_ANDROID_APP_GOOGLE_LOCATION_SETTINGS_HELPER_H_

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/android/google_location_settings_helper.h"

// This class is needed to fetch the current location setting and provide it to
// the location infobar.
// TODO(newt): merge this class with GoogleLocationSettingsHelper
class AppGoogleLocationSettingsHelper : public GoogleLocationSettingsHelper {
 public:
  AppGoogleLocationSettingsHelper();
  ~AppGoogleLocationSettingsHelper() override;

  static bool Register(JNIEnv* env);

  // GoogleLocationSettingsHelper implementation:
  bool IsSystemLocationEnabled() override;

 private:
  friend class GoogleLocationSettingsHelper;

  DISALLOW_COPY_AND_ASSIGN(AppGoogleLocationSettingsHelper);
};

#endif  // CHROME_BROWSER_ANDROID_APP_GOOGLE_LOCATION_SETTINGS_HELPER_H_
