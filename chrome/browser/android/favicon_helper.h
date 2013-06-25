// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_FAVICON_HELPER_H_
#define CHROME_BROWSER_ANDROID_FAVICON_HELPER_H_

#include <jni.h>

#include "base/memory/scoped_ptr.h"
#include "chrome/common/cancelable_task_tracker.h"

class FaviconHelper {
 public:
  FaviconHelper();
  void Destroy(JNIEnv* env, jobject obj);
  jboolean GetFaviconImageForURL(JNIEnv* env, jobject obj, jobject jprofile,
                                 jstring page_url, jint icon_types,
                                 jint desired_size_in_dip,
                                 jobject java_favicon_image_callback);
  static bool RegisterFaviconHelper(JNIEnv* env);
 private:
  scoped_ptr<CancelableTaskTracker> cancelable_task_tracker_;

  virtual ~FaviconHelper();

  DISALLOW_COPY_AND_ASSIGN(FaviconHelper);
};

#endif  // CHROME_BROWSER_ANDROID_FAVICON_HELPER_H_
