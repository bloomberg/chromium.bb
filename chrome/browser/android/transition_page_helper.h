// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_TRANSITION_PAGE_HELPER_H_
#define CHROME_BROWSER_ANDROID_TRANSITION_PAGE_HELPER_H_

#include "base/android/jni_android.h"
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

namespace content {
class WebContents;
}

class TransitionPageHelper {
 public:
  static TransitionPageHelper* FromJavaObject(JNIEnv* env, jobject jobj);
  static bool Register(JNIEnv* env);

  TransitionPageHelper();
  virtual ~TransitionPageHelper();

  void Destroy(JNIEnv* env, jobject jobj);

  void SetWebContents(JNIEnv* env, jobject jobj, jobject jcontent_view_core);
  void ReleaseWebContents(JNIEnv* env, jobject jobj);
  void SetOpacity(JNIEnv* env,
                  jobject jobj,
                  jobject jcontent_view_core,
                  jfloat opacity);

 private:
  scoped_ptr<content::WebContents> web_contents_;

  DISALLOW_COPY_AND_ASSIGN(TransitionPageHelper);
};

#endif  // CHROME_BROWSER_ANDROID_TRANSITION_PAGE_HELPER_H_
