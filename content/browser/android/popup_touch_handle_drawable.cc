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
}

void PopupTouchHandleDrawable::SetEnabled(bool enabled) {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (enabled)
    Java_PopupTouchHandleDrawable_show(env, drawable_.obj());
  else
    Java_PopupTouchHandleDrawable_hide(env, drawable_.obj());
}

void PopupTouchHandleDrawable::SetOrientation(
    TouchHandleOrientation orientation) {
  JNIEnv* env = base::android::AttachCurrentThread();
  jobject obj = drawable_.obj();
  switch (orientation) {
    case TOUCH_HANDLE_LEFT:
      Java_PopupTouchHandleDrawable_setLeftOrientation(env, obj);
      break;

    case TOUCH_HANDLE_RIGHT:
      Java_PopupTouchHandleDrawable_setRightOrientation(env, obj);
      break;

    case TOUCH_HANDLE_CENTER:
      Java_PopupTouchHandleDrawable_setCenterOrientation(env, obj);
      break;

    case TOUCH_HANDLE_ORIENTATION_UNDEFINED:
      NOTREACHED() << "Invalid touch handle orientation.";
  };
}

void PopupTouchHandleDrawable::SetAlpha(float alpha) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PopupTouchHandleDrawable_setOpacity(env, drawable_.obj(), alpha);
}

void PopupTouchHandleDrawable::SetFocus(const gfx::PointF& position) {
  const gfx::PointF position_pix = gfx::ScalePoint(position, dpi_scale_);
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PopupTouchHandleDrawable_setFocus(
      env, drawable_.obj(), position_pix.x(), position_pix.y());
}

void PopupTouchHandleDrawable::SetVisible(bool visible) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PopupTouchHandleDrawable_setVisible(env, drawable_.obj(), visible);
}

bool PopupTouchHandleDrawable::IntersectsWith(const gfx::RectF& rect) const {
  const gfx::RectF rect_pix = gfx::ScaleRect(rect, dpi_scale_);
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_PopupTouchHandleDrawable_intersectsWith(env,
                                                      drawable_.obj(),
                                                      rect_pix.x(),
                                                      rect_pix.y(),
                                                      rect_pix.width(),
                                                      rect_pix.height());
}

// static
bool PopupTouchHandleDrawable::RegisterPopupTouchHandleDrawable(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace content
