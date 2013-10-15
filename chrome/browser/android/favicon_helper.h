// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_FAVICON_HELPER_H_
#define CHROME_BROWSER_ANDROID_FAVICON_HELPER_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/common/cancelable_task_tracker.h"

class FaviconHelper {
 public:
  FaviconHelper();
  void Destroy(JNIEnv* env, jobject obj);
  jboolean GetLocalFaviconImageForURL(JNIEnv* env,
                                      jobject obj,
                                      jobject j_profile,
                                      jstring j_page_url,
                                      jint j_icon_types,
                                      jint j_desired_size_in_dip,
                                      jobject j_favicon_image_callback);
  base::android::ScopedJavaLocalRef<jobject> GetSyncedFaviconImageForURL(
      JNIEnv* env,
      jobject obj,
      jobject jprofile,
      jstring j_page_url);
  jint GetDominantColorForBitmap(JNIEnv* env, jobject obj, jobject bitmap);
  static bool RegisterFaviconHelper(JNIEnv* env);

 private:
  scoped_ptr<CancelableTaskTracker> cancelable_task_tracker_;

  virtual ~FaviconHelper();

  DISALLOW_COPY_AND_ASSIGN(FaviconHelper);
};

#endif  // CHROME_BROWSER_ANDROID_FAVICON_HELPER_H_
