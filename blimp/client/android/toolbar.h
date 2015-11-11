// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_ANDROID_TOOLBAR_H_
#define BLIMP_CLIENT_ANDROID_TOOLBAR_H_

#include "base/android/jni_android.h"
#include "base/macros.h"

class GURL;
class SkBitmap;

namespace blimp {

// The native component of org.chromium.blimp.toolbar.Toolbar.  This handles
// marshalling calls between Java and native.  Specifically, this passes calls
// between Toolbar.java <=> content_lite's NavigationController layer.
class Toolbar {
 public:
  static bool RegisterJni(JNIEnv* env);

  Toolbar(JNIEnv* env, const base::android::JavaParamRef<jobject>& jobj);

  // Methods called from Java via JNI.
  void Destroy(JNIEnv* env, jobject jobj);
  void OnUrlTextEntered(JNIEnv* env, jobject jobj, jstring text);
  void OnReloadPressed(JNIEnv* env, jobject jobj);
  jboolean OnBackPressed(JNIEnv* env, jobject jobj);

  // TODO(dtrainor): To be called by a the content lite NavigationController.
  // Should probably be an overridden delegate method so the network code can be
  // multi platform.
  void OnNavigationStateChanged(const GURL* url,
                                const SkBitmap* favicon,
                                const std::string* title);

 private:
  virtual ~Toolbar();

  // Reference to the Java object which owns this class.
  base::android::ScopedJavaGlobalRef<jobject> java_obj_;

  DISALLOW_COPY_AND_ASSIGN(Toolbar);
};

}  // namespace blimp

#endif  // BLIMP_CLIENT_ANDROID_TOOLBAR_H_
