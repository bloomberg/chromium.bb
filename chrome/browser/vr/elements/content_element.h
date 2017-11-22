// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_CONTENT_ELEMENT_H_
#define CHROME_BROWSER_VR_ELEMENTS_CONTENT_ELEMENT_H_

#include "base/macros.h"
#include "chrome/browser/vr/content_input_delegate.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/ui_element_renderer.h"

namespace vr {

class ContentElement : public UiElement {
 public:
  typedef typename base::Callback<void(const gfx::SizeF&)>
      ScreenBoundsChangedCallback;

  ContentElement(ContentInputDelegate* delegate, ScreenBoundsChangedCallback);
  ~ContentElement() override;

  void OnHoverEnter(const gfx::PointF& position) override;
  void OnHoverLeave() override;
  void OnMove(const gfx::PointF& position) override;
  void OnButtonDown(const gfx::PointF& position) override;
  void OnButtonUp(const gfx::PointF& position) override;
  void OnFlingStart(std::unique_ptr<blink::WebGestureEvent> gesture,
                    const gfx::PointF& position) override;
  void OnFlingCancel(std::unique_ptr<blink::WebGestureEvent> gesture,
                     const gfx::PointF& position) override;
  void OnScrollBegin(std::unique_ptr<blink::WebGestureEvent> gesture,
                     const gfx::PointF& position) override;
  void OnScrollUpdate(std::unique_ptr<blink::WebGestureEvent> gesture,
                      const gfx::PointF& position) override;
  void OnScrollEnd(std::unique_ptr<blink::WebGestureEvent> gesture,
                   const gfx::PointF& position) override;
  bool OnBeginFrame(const base::TimeTicks& time,
                    const gfx::Vector3dF& look_at) override;

  void Render(UiElementRenderer* renderer,
              const CameraModel& model) const final;

  void SetTextureId(unsigned int texture_id);
  void SetTextureLocation(UiElementRenderer::TextureLocation location);
  void SetProjectionMatrix(const gfx::Transform& matrix);

 private:
  ContentInputDelegate* delegate_ = nullptr;
  ScreenBoundsChangedCallback bounds_changed_callback_;
  unsigned int texture_id_ = 0;
  UiElementRenderer::TextureLocation texture_location_ =
      UiElementRenderer::kTextureLocationExternal;
  gfx::SizeF last_content_screen_bounds_;
  float last_content_aspect_ratio_ = 0.0f;
  gfx::Transform projection_matrix_;

  DISALLOW_COPY_AND_ASSIGN(ContentElement);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_CONTENT_ELEMENT_H_
