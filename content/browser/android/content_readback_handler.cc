// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/content_readback_handler.h"

#include "base/android/jni_android.h"
#include "base/bind.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/copy_output_result.h"
#include "content/browser/android/content_view_core_impl.h"
#include "jni/ContentReadbackHandler_jni.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/android/window_android.h"
#include "ui/android/window_android_compositor.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/geometry/rect.h"

namespace content {

// static
bool ContentReadbackHandler::RegisterContentReadbackHandler(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

ContentReadbackHandler::ContentReadbackHandler(JNIEnv* env, jobject obj)
    : weak_factory_(this) {
  java_obj_.Reset(env, obj);
}

void ContentReadbackHandler::Destroy(JNIEnv* env, jobject obj) {
  delete this;
}

void ContentReadbackHandler::GetContentBitmap(JNIEnv* env,
                                              jobject obj,
                                              jint readback_id,
                                              jfloat scale,
                                              jobject color_type,
                                              jfloat x,
                                              jfloat y,
                                              jfloat width,
                                              jfloat height,
                                              jobject content_view_core) {
  ContentViewCore* view =
      ContentViewCore::GetNativeContentViewCore(env, content_view_core);
  DCHECK(view);

  const ReadbackRequestCallback result_callback =
      base::Bind(&ContentReadbackHandler::OnFinishReadback,
                 weak_factory_.GetWeakPtr(),
                 readback_id);

  SkColorType sk_color_type = gfx::ConvertToSkiaColorType(color_type);
  view->GetScaledContentBitmap(
      scale, sk_color_type, gfx::Rect(x, y, width, height), result_callback);
}

ContentReadbackHandler::~ContentReadbackHandler() {}

void ContentReadbackHandler::OnFinishReadback(int readback_id,
                                              const SkBitmap& bitmap,
                                              ReadbackResponse response) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> java_bitmap;
  if (response == READBACK_SUCCESS)
    java_bitmap = gfx::ConvertToJavaBitmap(&bitmap);

  Java_ContentReadbackHandler_notifyGetBitmapFinished(
      env, java_obj_.obj(), readback_id, java_bitmap.obj(), response);
}

// static
static jlong Init(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  ContentReadbackHandler* content_readback_handler =
      new ContentReadbackHandler(env, obj);
  return reinterpret_cast<intptr_t>(content_readback_handler);
}

}  // namespace content
