// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/content_readback_handler.h"

#include "base/android/jni_android.h"
#include "base/bind.h"
#include "content/browser/android/content_view_core_impl.h"
#include "jni/ContentReadbackHandler_jni.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/rect.h"

namespace content {

// static
bool ContentReadbackHandler::RegisterContentReadbackHandler(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

ContentReadbackHandler::ContentReadbackHandler(JNIEnv* env, jobject obj)
    : weak_factory_(this) {
  java_obj_.Reset(env, obj);
}

ContentReadbackHandler::~ContentReadbackHandler() {}

void ContentReadbackHandler::Destroy(JNIEnv* env, jobject obj) {
  delete this;
}

void ContentReadbackHandler::OnFinishContentReadback(int readback_id,
                                                     bool success,
                                                     const SkBitmap& bitmap) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> java_bitmap;
  if (success)
    java_bitmap = gfx::ConvertToJavaBitmap(&bitmap);

  Java_ContentReadbackHandler_notifyGetContentBitmapFinished(
      env, java_obj_.obj(), readback_id, success, java_bitmap.obj());
}

void ContentReadbackHandler::GetContentBitmap(JNIEnv* env,
                                              jobject obj,
                                              jint readback_id,
                                              jfloat scale,
                                              jobject config,
                                              jfloat x,
                                              jfloat y,
                                              jfloat width,
                                              jfloat height,
                                              jobject content_view_core) {
  ContentViewCore* view =
      ContentViewCore::GetNativeContentViewCore(env, content_view_core);
  DCHECK(view);

  base::Callback<void(bool, const SkBitmap&)> result_callback =
      base::Bind(&ContentReadbackHandler::OnFinishContentReadback,
                 weak_factory_.GetWeakPtr(),
                 readback_id);

  view->GetScaledContentBitmap(
      scale, config, gfx::Rect(x, y, width, height), result_callback);
  return;
}

// static
static jlong Init(JNIEnv* env, jobject obj) {
  ContentReadbackHandler* content_readback_handler =
      new ContentReadbackHandler(env, obj);
  return reinterpret_cast<intptr_t>(content_readback_handler);
}

}  // namespace content
