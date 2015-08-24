// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/android/variations_associated_data_android.h"

#include <string>

#include "base/android/jni_string.h"
#include "components/variations/variations_associated_data.h"
#include "jni/VariationsAssociatedData_jni.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;

namespace variations {

namespace android {

jstring GetVariationParamValue(JNIEnv* env,
                               jclass clazz,
                               jstring jtrial_name,
                               jstring jparam_name) {
  std::string trial_name(ConvertJavaStringToUTF8(env, jtrial_name));
  std::string param_name(ConvertJavaStringToUTF8(env, jparam_name));
  std::string param_value =
      variations::GetVariationParamValue(trial_name, param_name);
  return ConvertUTF8ToJavaString(env, param_value).Release();
}

bool RegisterVariationsAssociatedData(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android

}  // namespace variations
