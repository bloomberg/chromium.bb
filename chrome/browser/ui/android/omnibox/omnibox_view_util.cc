// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/omnibox/omnibox_view_util.h"

#include "base/android/jni_string.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "jni/OmniboxViewUtil_jni.h"

// static
jstring SanitizeTextForPaste(JNIEnv* env, jclass clazz, jstring jtext) {
  base::string16 pasted_text(
      base::android::ConvertJavaStringToUTF16(env, jtext));
  pasted_text = OmniboxView::SanitizeTextForPaste(pasted_text);
  return base::android::ConvertUTF16ToJavaString(env, pasted_text).Release();
}

// static
bool OmniboxViewUtil::RegisterOmniboxViewUtil(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
