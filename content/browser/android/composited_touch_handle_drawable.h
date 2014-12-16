// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_COMPOSITED_TOUCH_HANDLE_DRAWABLE_H_
#define CONTENT_BROWSER_ANDROID_COMPOSITED_TOUCH_HANDLE_DRAWABLE_H_

#include "ui/touch_selection/touch_handle.h"

#include "base/android/jni_android.h"
#include "cc/layers/ui_resource_layer.h"

namespace content {

// Touch handle drawable implementation backed by a cc layer.
class CompositedTouchHandleDrawable : public ui::TouchHandleDrawable {
 public:
  CompositedTouchHandleDrawable(cc::Layer* root_layer,
                                float dpi_scale,
                                jobject context);
  virtual ~CompositedTouchHandleDrawable();

  // ui::TouchHandleDrawable implementation.
  virtual void SetEnabled(bool enabled) override;
  virtual void SetOrientation(ui::TouchHandleOrientation orientation) override;
  virtual void SetAlpha(float alpha) override;
  virtual void SetFocus(const gfx::PointF& position) override;
  virtual gfx::RectF GetVisibleBounds() const override;

  static bool RegisterHandleViewResources(JNIEnv* env);

 private:
  void Detach();

  const float dpi_scale_;
  ui::TouchHandleOrientation orientation_;
  gfx::PointF focal_position_;
  gfx::Vector2dF focal_offset_from_origin_;
  scoped_refptr<cc::UIResourceLayer> layer_;

  DISALLOW_COPY_AND_ASSIGN(CompositedTouchHandleDrawable);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_COMPOSITED_TOUCH_HANDLE_DRAWABLE_H_
