// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/ntp/content_suggestions_notification_helper.h"

#include <limits>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/strings/utf_string_conversions.h"
#include "jni/ContentSuggestionsNotificationHelper_jni.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

namespace ntp_snippets {

void ContentSuggestionsNotificationHelper::OpenURL(const GURL& url) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ContentSuggestionsNotificationHelper_openUrl(
      env, base::android::ConvertUTF8ToJavaString(env, url.spec()));
}

void ContentSuggestionsNotificationHelper::SendNotification(
    const GURL& url,
    const base::string16& title,
    const base::string16& text,
    const gfx::Image& image,
    base::Time timeout_at) {
  JNIEnv* env = base::android::AttachCurrentThread();
  SkBitmap skimage = image.AsImageSkia().GetRepresentation(1.0f).sk_bitmap();
  if (skimage.empty())
    return;

  jint timeout_at_millis = timeout_at.ToJavaTime();
  if (timeout_at == base::Time::Max()) {
    timeout_at_millis = std::numeric_limits<jint>::max();
  }

  Java_ContentSuggestionsNotificationHelper_showNotification(
      env, base::android::ConvertUTF8ToJavaString(env, url.spec()),
      base::android::ConvertUTF16ToJavaString(env, title),
      base::android::ConvertUTF16ToJavaString(env, text),
      gfx::ConvertToJavaBitmap(&skimage), timeout_at_millis);
}

void ContentSuggestionsNotificationHelper::HideAllNotifications() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ContentSuggestionsNotificationHelper_hideAllNotifications(env);
}

}  // namespace ntp_snippets
