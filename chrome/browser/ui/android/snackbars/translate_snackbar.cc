// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/snackbars/translate_snackbar.h"

#include "base/memory/ptr_util.h"
#include "jni/TranslateSnackbarController_jni.h"

TranslateSnackbar::TranslateSnackbar(int snackbar_type) {
  type_ = snackbar_type;
}

void TranslateSnackbar::ToggleTranslateOption(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  // TODO(ramyasharma): Implement.
}

// Native JNI methods -------------------------------------------------------

// static
bool RegisterTranslateSnackbar(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
