// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/popup_touch_handle_drawable.h"

#include "jni/PopupTouchHandleDrawable_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace android_webview {

PopupTouchHandleDrawable::PopupTouchHandleDrawable(
    JNIEnv* env,
    jobject obj,
    float dip_scale,
    float horizontal_padding_ratio)
    : java_ref_(env, obj)
    , dip_scale_(dip_scale)
    , drawable_horizontal_padding_ratio_(horizontal_padding_ratio) {
  DCHECK(!java_ref_.is_empty());
}

PopupTouchHandleDrawable::~PopupTouchHandleDrawable() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj.is_null())
    Java_PopupTouchHandleDrawable_destroy(env, obj);
}

bool PopupTouchHandleDrawable::RegisterPopupTouchHandleDrawable(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void PopupTouchHandleDrawable::SetEnabled(bool enabled) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  if (enabled)
    Java_PopupTouchHandleDrawable_show(env, obj);
  else
    Java_PopupTouchHandleDrawable_hide(env, obj);
}

void PopupTouchHandleDrawable::SetOrientation(
    ui::TouchHandleOrientation orientation,
    bool mirror_vertical,
    bool mirror_horizontal) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj.is_null()) {
    Java_PopupTouchHandleDrawable_setOrientation(
        env, obj, static_cast<int>(orientation), mirror_vertical,
        mirror_horizontal);
  }
}

void PopupTouchHandleDrawable::SetOrigin(const gfx::PointF& origin) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj.is_null()) {
    const gfx::PointF origin_pix = gfx::ScalePoint(origin, dip_scale_);
    Java_PopupTouchHandleDrawable_setOrigin(env, obj, origin_pix.x(),
                                            origin_pix.y());
  }
}

void PopupTouchHandleDrawable::SetAlpha(float alpha) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  bool visible = alpha > 0;
  if (!obj.is_null())
    Java_PopupTouchHandleDrawable_setVisible(env, obj, visible);
}

gfx::RectF PopupTouchHandleDrawable::GetVisibleBounds() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return gfx::RectF();
  gfx::RectF unscaled_rect(
      Java_PopupTouchHandleDrawable_getPositionX(env, obj),
      Java_PopupTouchHandleDrawable_getPositionY(env, obj),
      Java_PopupTouchHandleDrawable_getVisibleWidth(env, obj),
      Java_PopupTouchHandleDrawable_getVisibleHeight(env, obj));
  return gfx::ScaleRect(unscaled_rect, 1.f / dip_scale_);
}

float PopupTouchHandleDrawable::GetDrawableHorizontalPaddingRatio() const {
  return drawable_horizontal_padding_ratio_;
}

static jlong Init(JNIEnv* env,
                  const JavaParamRef<jobject>& obj,
                  const JavaParamRef<jobject>& content_view_core,
                  const jfloat dip_scale,
                  const jfloat horizontal_padding_ratio) {
  DCHECK(content_view_core.obj());
  return reinterpret_cast<intptr_t>(
      new PopupTouchHandleDrawable(env, obj, dip_scale,
          horizontal_padding_ratio));
}

}  // namespace content
