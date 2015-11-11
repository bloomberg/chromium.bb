// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/android/toolbar.h"

#include "base/android/jni_string.h"
#include "jni/Toolbar_jni.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/android/java_bitmap.h"
#include "url/gurl.h"

namespace blimp {

// static
static jlong Init(JNIEnv* env, const JavaParamRef<jobject>& jobj) {
  return reinterpret_cast<intptr_t>(new Toolbar(env, jobj));
}

// static
bool Toolbar::RegisterJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

Toolbar::Toolbar(JNIEnv* env,
                 const base::android::JavaParamRef<jobject>& jobj) {
  java_obj_.Reset(env, jobj);
}

Toolbar::~Toolbar() {}

void Toolbar::Destroy(JNIEnv* env, jobject jobj) {
  delete this;
}

void Toolbar::OnUrlTextEntered(JNIEnv* env, jobject jobj, jstring text) {
  // TODO(dtrainor): Notify the content lite NavigationController.
}

void Toolbar::OnReloadPressed(JNIEnv* env, jobject jobj) {
  // TODO(dtrainor): Notify the content lite NavigationController.
}

jboolean Toolbar::OnBackPressed(JNIEnv* env, jobject jobj) {
  // TODO(dtrainor): Notify the content lite NavigationController.
  // TODO(dtrainor): Find a way to determine whether or not we're at the end of
  // our history stack.
  return true;
}

void Toolbar::OnNavigationStateChanged(const GURL* url,
                                       const SkBitmap* favicon,
                                       const std::string* title) {
  JNIEnv* env = base::android::AttachCurrentThread();

  ScopedJavaLocalRef<jstring> jurl;
  ScopedJavaLocalRef<jobject> jfavicon;
  ScopedJavaLocalRef<jstring> jtitle;

  if (url)
    jurl = base::android::ConvertUTF8ToJavaString(env, url->spec());

  if (favicon)
    jfavicon = gfx::ConvertToJavaBitmap(favicon);

  if (title)
    jtitle = base::android::ConvertUTF8ToJavaString(env, *title);

  Java_Toolbar_onNavigationStateChanged(env,
                                        java_obj_.obj(),
                                        jurl.obj(),
                                        jfavicon.obj(),
                                        jtitle.obj());
}

}  // namespace blimp
