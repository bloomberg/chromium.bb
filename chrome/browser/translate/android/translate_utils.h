// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_INFOBARS_TRANSLATE_UTILS_H_
#define CHROME_BROWSER_UI_ANDROID_INFOBARS_TRANSLATE_UTILS_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"

namespace translate {
class TranslateInfoBarDelegate;
}

class TranslateUtils {
 public:
  static base::android::ScopedJavaLocalRef<jobjectArray> GetJavaLanguages(
      JNIEnv* env,
      translate::TranslateInfoBarDelegate* delegate);
  static base::android::ScopedJavaLocalRef<jobjectArray> GetJavaLanguageCodes(
      JNIEnv* env,
      translate::TranslateInfoBarDelegate* delegate);
};

#endif
