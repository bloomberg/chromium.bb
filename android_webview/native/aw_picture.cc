// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/aw_picture.h"

#include "android_webview/native/java_browser_view_renderer_helper.h"
#include "base/bind.h"
#include "jni/AwPicture_jni.h"
#include "third_party/skia/include/core/SkPicture.h"

namespace android_webview {

AwPicture::AwPicture(skia::RefPtr<SkPicture> picture)
    : picture_(picture) {
  DCHECK(picture_);
}

AwPicture::~AwPicture() {}

void AwPicture::Destroy(JNIEnv* env, jobject obj) {
  delete this;
}

jint AwPicture::GetWidth(JNIEnv* env, jobject obj) {
  return picture_->width();
}

jint AwPicture::GetHeight(JNIEnv* env, jobject obj) {
  return picture_->height();
}

namespace {
bool RenderPictureToCanvas(SkPicture* picture, SkCanvas* canvas) {
  picture->draw(canvas);
  return true;
}
}

void AwPicture::Draw(JNIEnv* env, jobject obj, jobject canvas,
                     jint left, jint top, jint right, jint bottom) {
  bool ok = JavaBrowserViewRendererHelper::GetInstance()
                ->RenderViaAuxilaryBitmapIfNeeded(
                      canvas,
                      gfx::Vector2d(),
                      gfx::Rect(left, top, right - left, bottom - top),
                      base::Bind(&RenderPictureToCanvas,
                                 base::Unretained(picture_.get())));
  LOG_IF(ERROR, !ok) << "Couldn't draw picture";
}

bool RegisterAwPicture(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android_webview
