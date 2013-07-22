// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/field_trial_helper.h"

#include <jni.h>

#include "base/android/jni_string.h"
#include "base/metrics/field_trial.h"
#include "jni/FieldTrialHelper_jni.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;

static jstring GetFieldTrialFullName(JNIEnv* env,
                                     jclass clazz,
                                     jstring jtrial_name) {
  std::string trial_name(ConvertJavaStringToUTF8(env, jtrial_name));
  return ConvertUTF8ToJavaString(
      env,
      base::FieldTrialList::FindFullName(trial_name)).Release();
}

namespace chrome {
namespace android {

bool RegisterFieldTrialHelper(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android
}  // namespace chrome
