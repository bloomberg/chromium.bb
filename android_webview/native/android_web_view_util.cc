// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/android_web_view_util.h"

#include "android_webview/native/aw_browser_dependency_factory.h"
#include "base/android/jni_android.h"
#include "jni/AndroidWebViewUtil_jni.h"

static jint CreateNativeWebContents(
    JNIEnv* env, jclass clazz, jboolean incognito) {
  android_webview::AwBrowserDependencyFactory* browser_deps =
      android_webview::AwBrowserDependencyFactory::GetInstance();
  return reinterpret_cast<jint>(browser_deps->CreateWebContents(incognito));
}

bool RegisterAndroidWebViewUtil(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
