// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_POPUP_TOUCH_HANDLE_DRAWABLE_H_
#define CONTENT_BROWSER_ANDROID_POPUP_TOUCH_HANDLE_DRAWABLE_H_

#include "ui/touch_selection/touch_handle.h"

#include "base/android/jni_android.h"

namespace content {

// Touch handle drawable backed by an Android PopupWindow.
class PopupTouchHandleDrawable : public ui::TouchHandleDrawable {
 public:
  PopupTouchHandleDrawable(base::android::ScopedJavaLocalRef<jobject> drawable,
                           float dpi_scale);
  ~PopupTouchHandleDrawable() override;

  // ui::TouchHandleDrawable implementation.
  void SetEnabled(bool enabled) override;
  void SetOrientation(ui::TouchHandleOrientation orientation) override;
  void SetAlpha(float alpha) override;
  void SetFocus(const gfx::PointF& position) override;
  gfx::RectF GetVisibleBounds() const override;

  static bool RegisterPopupTouchHandleDrawable(JNIEnv* env);

 private:
  const float dpi_scale_;
  base::android::ScopedJavaGlobalRef<jobject> drawable_;

  DISALLOW_COPY_AND_ASSIGN(PopupTouchHandleDrawable);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_POPUP_TOUCH_HANDLE_DRAWABLE_H_
