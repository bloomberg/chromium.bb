// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_ANDROID_TOOLBAR_H_
#define BLIMP_CLIENT_ANDROID_TOOLBAR_H_

#include "base/android/jni_android.h"
#include "base/macros.h"
#include "blimp/client/session/navigation_feature.h"

class GURL;
class SkBitmap;

namespace blimp {

// The native component of org.chromium.blimp.toolbar.Toolbar.  This handles
// marshalling calls between Java and native.  Specifically, this passes calls
// between Toolbar.java <=> NavigationFeature.
class Toolbar : public NavigationFeature::NavigationFeatureDelegate {
 public:
  static bool RegisterJni(JNIEnv* env);

  Toolbar(JNIEnv* env,
          const base::android::JavaParamRef<jobject>& jobj,
          NavigationFeature* navigation_feature);

  // Methods called from Java via JNI.
  void Destroy(JNIEnv* env, jobject jobj);
  void OnUrlTextEntered(JNIEnv* env, jobject jobj, jstring text);
  void OnReloadPressed(JNIEnv* env, jobject jobj);
  void OnForwardPressed(JNIEnv* env, jobject jobj);
  jboolean OnBackPressed(JNIEnv* env, jobject jobj);

  // NavigationFeatureDelegate implementation.
  void OnUrlChanged(int tab_id, const GURL& url) override;
  void OnFaviconChanged(int tab_id, const SkBitmap& favicon) override;
  void OnTitleChanged(int tab_id, const std::string& title) override;
  void OnLoadingChanged(int tab_id, bool loading) override;

 private:
  virtual ~Toolbar();

  NavigationFeature* navigation_feature_;

  // Reference to the Java object which owns this class.
  base::android::ScopedJavaGlobalRef<jobject> java_obj_;

  DISALLOW_COPY_AND_ASSIGN(Toolbar);
};

}  // namespace blimp

#endif  // BLIMP_CLIENT_ANDROID_TOOLBAR_H_
