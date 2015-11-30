// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_FAVICON_HELPER_H_
#define CHROME_BROWSER_ANDROID_FAVICON_HELPER_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/memory/scoped_ptr.h"
#include "base/task/cancelable_task_tracker.h"

class FaviconHelper {
 public:
  FaviconHelper();
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  jboolean GetLocalFaviconImageForURL(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& j_profile,
      const base::android::JavaParamRef<jstring>& j_page_url,
      jint j_icon_types,
      jint j_desired_size_in_pixel,
      const base::android::JavaParamRef<jobject>& j_favicon_image_callback);
  base::android::ScopedJavaLocalRef<jobject> GetSyncedFaviconImageForURL(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& jprofile,
      const base::android::JavaParamRef<jstring>& j_page_url);
  void EnsureIconIsAvailable(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& j_profile,
      const base::android::JavaParamRef<jobject>& j_web_contents,
      const base::android::JavaParamRef<jstring>& j_page_url,
      const base::android::JavaParamRef<jstring>& j_icon_url,
      jboolean j_is_large_icon,
      const base::android::JavaParamRef<jobject>& j_availability_callback);
  static bool RegisterFaviconHelper(JNIEnv* env);

 private:
  scoped_ptr<base::CancelableTaskTracker> cancelable_task_tracker_;

  virtual ~FaviconHelper();

  DISALLOW_COPY_AND_ASSIGN(FaviconHelper);
};

#endif  // CHROME_BROWSER_ANDROID_FAVICON_HELPER_H_
