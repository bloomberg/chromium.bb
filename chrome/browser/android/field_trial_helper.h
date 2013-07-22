// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_FIELD_TRIAL_HELPER_H_
#define CHROME_BROWSER_ANDROID_FIELD_TRIAL_HELPER_H_

#include <jni.h>

namespace chrome {
namespace android {

bool RegisterFieldTrialHelper(JNIEnv* env);

}  // namespace android
}  // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_FIELD_TRIAL_HELPER_H_
