// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/java_browser_view_renderer_helper.h"

#include "base/debug/trace_event.h"
#include "jni/JavaBrowserViewRendererHelper_jni.h"

using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace android_webview {

JavaBrowserViewRendererHelper::JavaBrowserViewRendererHelper() {
}

JavaBrowserViewRendererHelper::~JavaBrowserViewRendererHelper() {
}

// static
JavaBrowserViewRendererHelper* JavaBrowserViewRendererHelper::GetInstance() {
  static JavaBrowserViewRendererHelper* g_instance =
      new JavaBrowserViewRendererHelper;
  return g_instance;
}

ScopedJavaLocalRef<jobject> JavaBrowserViewRendererHelper::CreateBitmap(
    JNIEnv* env,
    int width,
    int height,
    const base::android::JavaRef<jobject>& jcanvas,
    void* owner_key) {
  TRACE_EVENT0("android_webview", "RendererHelper::CreateBitmap");
  return width <= 0 || height <= 0 ? ScopedJavaLocalRef<jobject>() :
      Java_JavaBrowserViewRendererHelper_createBitmap(
          env, width, height, jcanvas.obj(),
          reinterpret_cast<jint>(owner_key));
}

void JavaBrowserViewRendererHelper::DrawBitmapIntoCanvas(
    JNIEnv* env,
    const JavaRef<jobject>& jbitmap,
    const JavaRef<jobject>& jcanvas,
    int x,
    int y) {
  TRACE_EVENT0("android_webview", "RendererHelper::DrawBitmapIntoCanvas");
  Java_JavaBrowserViewRendererHelper_drawBitmapIntoCanvas(
      env, jbitmap.obj(), jcanvas.obj(), x, y);
}

ScopedJavaLocalRef<jobject>
JavaBrowserViewRendererHelper::RecordBitmapIntoPicture(
    JNIEnv* env,
    const JavaRef<jobject>& jbitmap) {
  TRACE_EVENT0("android_webview", "RendererHelper::RecordBitmapIntoPicture");
  return Java_JavaBrowserViewRendererHelper_recordBitmapIntoPicture(
      env, jbitmap.obj());
}

bool RegisterJavaBrowserViewRendererHelper(JNIEnv* env) {
  return RegisterNativesImpl(env) >= 0;
}

}  // namespace android_webview
