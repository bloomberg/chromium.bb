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
  void Destroy(JNIEnv* env, jobject obj);
  jboolean GetLocalFaviconImageForURL(JNIEnv* env,
                                      jobject obj,
                                      jobject j_profile,
                                      jstring j_page_url,
                                      jint j_icon_types,
                                      jint j_desired_size_in_pixel,
                                      jobject j_favicon_image_callback);
  void GetLargestRawFaviconForUrl(JNIEnv* env,
                                  jobject obj,
                                  jobject j_profile,
                                  jstring j_page_url,
                                  jintArray j_icon_types,
                                  jint j_min_size_threshold_px,
                                  jobject j_favicon_image_callback);
  base::android::ScopedJavaLocalRef<jobject> GetSyncedFaviconImageForURL(
      JNIEnv* env,
      jobject obj,
      jobject jprofile,
      jstring j_page_url);
  static bool RegisterFaviconHelper(JNIEnv* env);

 private:
  scoped_ptr<base::CancelableTaskTracker> cancelable_task_tracker_;

  virtual ~FaviconHelper();

  DISALLOW_COPY_AND_ASSIGN(FaviconHelper);
};

#endif  // CHROME_BROWSER_ANDROID_FAVICON_HELPER_H_
