// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_formatter/android/component_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "components/url_formatter/url_formatter_android.h"

namespace url_formatter {

namespace android {

bool RegisterUrlFormatter(JNIEnv* env) {
  constexpr base::android::RegistrationMethod register_methods[] = {
    {"UrlFormatter", url_formatter::android::RegisterUrlFormatterNatives},
  };

  return base::android::RegisterNativeMethods(
      env, register_methods,
      arraysize(register_methods));
}

}  // namespace android

}  // namespace url_formatter
