// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/ntp/content_suggestions_notification_helper.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "jni/ContentSuggestionsNotificationHelper_jni.h"

namespace ntp_snippets {

void ContentSuggestionsNotificationHelper::OpenURL(const GURL& url) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ContentSuggestionsNotificationHelper_openUrl(
      env, base::android::ConvertUTF8ToJavaString(env, url.spec()));
}

}  // namespace ntp_snippets
