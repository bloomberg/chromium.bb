// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_POPUP_TOUCH_HANDLE_DRAWABLE_H_
#define CONTENT_BROWSER_ANDROID_POPUP_TOUCH_HANDLE_DRAWABLE_H_

#include "content/browser/renderer_host/input/touch_handle.h"

#include "base/android/jni_android.h"

namespace content {

// Touch handle drawable backed by an Android PopupWindow.
class PopupTouchHandleDrawable : public TouchHandleDrawable {
 public:
  PopupTouchHandleDrawable(base::android::ScopedJavaLocalRef<jobject> drawable,
                           float dpi_scale);
  virtual ~PopupTouchHandleDrawable();

  // TouchHandleDrawable implementation.
  virtual void SetEnabled(bool enabled) OVERRIDE;
  virtual void SetOrientation(TouchHandleOrientation orientation) OVERRIDE;
  virtual void SetAlpha(float alpha) OVERRIDE;
  virtual void SetFocus(const gfx::PointF& position) OVERRIDE;
  virtual void SetVisible(bool visible) OVERRIDE;
  virtual bool IntersectsWith(const gfx::RectF& rect) const OVERRIDE;

  static bool RegisterPopupTouchHandleDrawable(JNIEnv* env);

 private:
  const float dpi_scale_;
  base::android::ScopedJavaGlobalRef<jobject> drawable_;

  DISALLOW_COPY_AND_ASSIGN(PopupTouchHandleDrawable);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_POPUP_TOUCH_HANDLE_DRAWABLE_H_
