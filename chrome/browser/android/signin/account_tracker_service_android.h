// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_SIGNIN_ACCOUNT_TRACKER_SERVICE_ANDROID_H_
#define CHROME_BROWSER_ANDROID_SIGNIN_ACCOUNT_TRACKER_SERVICE_ANDROID_H_

#include <jni.h>

namespace signin {
namespace android {
// A bridge between the Java and C++ AccountTrackerService.
bool RegisterAccountTrackerService(JNIEnv* env);

}  // namespace android
}  // namespace signin

#endif  // CHROME_BROWSER_ANDROID_SIGNIN_ACCOUNT_TRACKER_SERVICE_ANDROID_H_
