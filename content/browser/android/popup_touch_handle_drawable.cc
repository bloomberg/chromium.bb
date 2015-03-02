// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/popup_touch_handle_drawable.h"

#include "jni/PopupTouchHandleDrawable_jni.h"

namespace content {

PopupTouchHandleDrawable::PopupTouchHandleDrawable(
    base::android::ScopedJavaLocalRef<jobject> drawable,
    float dpi_scale)
    : dpi_scale_(dpi_scale), drawable_(drawable) {
  DCHECK(drawable.obj());
}

PopupTouchHandleDrawable::~PopupTouchHandleDrawable() {
  // Explicitly disabling ensures that any external references to the Java
  // object are cleared, allowing it to be GC'ed in a timely fashion.
  SetEnabled(false);
}

void PopupTouchHandleDrawable::SetEnabled(bool enabled) {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (enabled)
    Java_PopupTouchHandleDrawable_show(env, drawable_.obj());
  else
    Java_PopupTouchHandleDrawable_hide(env, drawable_.obj());
}

void PopupTouchHandleDrawable::SetOrientation(
    ui::TouchHandleOrientation orientation) {
  JNIEnv* env = base::android::AttachCurrentThread();
  jobject obj = drawable_.obj();
  Java_PopupTouchHandleDrawable_setOrientation(env, obj,
                                               static_cast<int>(orientation));
}

void PopupTouchHandleDrawable::SetAlpha(float alpha) {
  JNIEnv* env = base::android::AttachCurrentThread();
  bool visible = alpha > 0;
  Java_PopupTouchHandleDrawable_setVisible(env, drawable_.obj(), visible);
}

void PopupTouchHandleDrawable::SetFocus(const gfx::PointF& position) {
  const gfx::PointF position_pix = gfx::ScalePoint(position, dpi_scale_);
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PopupTouchHandleDrawable_setFocus(
      env, drawable_.obj(), position_pix.x(), position_pix.y());
}

gfx::RectF PopupTouchHandleDrawable::GetVisibleBounds() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  gfx::RectF unscaled_rect(
      Java_PopupTouchHandleDrawable_getPositionX(env, drawable_.obj()),
      Java_PopupTouchHandleDrawable_getPositionY(env, drawable_.obj()),
      Java_PopupTouchHandleDrawable_getVisibleWidth(env, drawable_.obj()),
      Java_PopupTouchHandleDrawable_getVisibleHeight(env, drawable_.obj()));
  return gfx::ScaleRect(unscaled_rect, 1.f / dpi_scale_);
}

// static
bool PopupTouchHandleDrawable::RegisterPopupTouchHandleDrawable(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace content
