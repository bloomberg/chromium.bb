// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_GOOGLE_LOCATION_SETTINGS_HELPER_H_
#define CHROME_BROWSER_ANDROID_GOOGLE_LOCATION_SETTINGS_HELPER_H_

#include "base/android/jni_helper.h"
#include "base/android/scoped_java_ref.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/android/google_location_settings_helper_factory.h"

// This class is needed to fetch the current system location
// setting and update the infobar button label based on that information i.e,
// display "Allow" if google apps setting is set as enabled else, display
// "Settings" with a link to open the system location settings activity.
class GoogleLocationSettingsHelper {
 public:
  GoogleLocationSettingsHelper();
  ~GoogleLocationSettingsHelper();

  static void SetGoogleLocationSettingsHelperFactory(
      GoogleLocationSettingsHelperFactory* factory);
  std::string GetAcceptButtonLabel();
  void ShowGoogleLocationSettings();
  bool IsMasterLocationSettingEnabled();
  bool IsGoogleAppsLocationSettingEnabled();
  static bool Register(JNIEnv* env);

 private:
  base::android::ScopedJavaGlobalRef<jobject>
      java_google_location_settings_helper_;
  static GoogleLocationSettingsHelperFactory* helper_factory_;

  DISALLOW_COPY_AND_ASSIGN(GoogleLocationSettingsHelper);
};

#endif  // CHROME_BROWSER_ANDROID_GOOGLE_LOCATION_SETTINGS_HELPER_H_
