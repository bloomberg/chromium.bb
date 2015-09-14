// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_SIGNIN_ACCOUNT_TRACKER_SERVICE_ANDROID_H_
#define CHROME_BROWSER_ANDROID_SIGNIN_ACCOUNT_TRACKER_SERVICE_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/profiles/profile.h"
#include "components/signin/core/browser/account_tracker_service.h"

// Android wrapper of the AccountTrackerService which provides access from Java
// layer. Note that on Android, there's only a single profile, and therefore a
// single instance of this wrapper. The same name of the Java class is
// AccountTrackerService.
class AccountTrackerServiceAndroid {
 public:
  AccountTrackerServiceAndroid(JNIEnv* env, jobject obj);

  // Registers the AccountTrackerServiceAndroid's native methods through JNI.
  static bool Register(JNIEnv* env);

  void SeedAccountsInfo(JNIEnv* env,
                        jobject obj,
                        jobjectArray gaiaIds,
                        jobjectArray accountNames);

 private:
  ~AccountTrackerServiceAndroid();

  base::android::ScopedJavaGlobalRef<jobject> java_account_tracker_service_;
};

#endif  // CHROME_BROWSER_ANDROID_SIGNIN_ACCOUNT_TRACKER_SERVICE_ANDROID_H_
