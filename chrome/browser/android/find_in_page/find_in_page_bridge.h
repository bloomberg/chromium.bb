// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_FIND_IN_PAGE_FIND_IN_PAGE_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_FIND_IN_PAGE_FIND_IN_PAGE_BRIDGE_H_

#include <jni.h>

#include "base/android/jni_weak_ref.h"
#include "content/public/browser/web_contents.h"

class FindInPageBridge {
 public:
  FindInPageBridge(JNIEnv* env, jobject obj, jobject j_web_contents);
  void Destroy(JNIEnv*, jobject);

  void StartFinding(JNIEnv* env,
                    jobject obj,
                    jstring search_string,
                    jboolean forward_direction,
                    jboolean case_sensitive);

  void StopFinding(JNIEnv* env, jobject obj);

  base::android::ScopedJavaLocalRef<jstring> GetPreviousFindText(JNIEnv* env,
                                                                 jobject obj);

  void RequestFindMatchRects(JNIEnv* env, jobject obj, jint current_version);

  void ActivateNearestFindResult(JNIEnv* env, jobject obj, jfloat x, jfloat y);

  void ActivateFindInPageResultForAccessibility(JNIEnv* env, jobject obj);

  static bool RegisterFindInPageBridge(JNIEnv* env);

 private:
  content::WebContents* web_contents_;
  JavaObjectWeakGlobalRef weak_java_ref_;

  DISALLOW_COPY_AND_ASSIGN(FindInPageBridge);
};

#endif  // CHROME_BROWSER_ANDROID_FIND_IN_PAGE_FIND_IN_PAGE_BRIDGE_H_
