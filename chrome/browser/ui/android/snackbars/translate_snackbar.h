// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_SNACKBARS_TRANSLATE_SNACKBAR_H_
#define CHROME_BROWSER_UI_ANDROID_SNACKBARS_TRANSLATE_SNACKBAR_H_

#include <stddef.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"

class TranslateSnackbar {
 public:
  explicit TranslateSnackbar(int snackbar_type);

  void ToggleTranslateOption(JNIEnv* env,
                             const base::android::JavaParamRef<jobject>& obj);

 private:
  int type_;

  DISALLOW_COPY_AND_ASSIGN(TranslateSnackbar);
};

// Registers the native methods through JNI.
bool RegisterTranslateSnackbar(JNIEnv* env);

#endif  // CHROME_BROWSER_UI_ANDROID_SNACKBARS_TRANSLATE_SNACKBAR_H_
