// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_GOOGLE_LOCATION_SETTINGS_HELPER_FACTORY_H_
#define CHROME_BROWSER_ANDROID_GOOGLE_LOCATION_SETTINGS_HELPER_FACTORY_H_

#include "base/android/jni_helper.h"
#include "base/android/scoped_java_ref.h"
#include "base/values.h"

/*
 * Factory class on chrome side to create the chrome java
 * GoogleLocationSettingsHelper object.
 */
class GoogleLocationSettingsHelperFactory {

 public:
  GoogleLocationSettingsHelperFactory();
  ~GoogleLocationSettingsHelperFactory();
  virtual const base::android::ScopedJavaGlobalRef<jobject>&
      GetHelperInstance();
  static bool Register(JNIEnv* env);

 private:
  base::android::ScopedJavaGlobalRef<jobject>
      java_google_location_settings_helper_;

  DISALLOW_COPY_AND_ASSIGN(GoogleLocationSettingsHelperFactory);
};

#endif  // CHROME_BROWSER_ANDROID_GOOGLE_LOCATION_SETTINGS_HELPER_FACTORY_H_
