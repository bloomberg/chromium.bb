// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_CONTENT_ELEMENT_H_
#define CHROME_BROWSER_VR_ELEMENTS_CONTENT_ELEMENT_H_

#include "base/macros.h"
#include "chrome/browser/vr/content_input_delegate.h"
#include "chrome/browser/vr/elements/ui_element.h"

namespace vr {

class UiElementRenderer;

class ContentElement : public UiElement {
 public:
  explicit ContentElement(ContentInputDelegate* delegate);
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

  void Render(UiElementRenderer* renderer,
              const gfx::Transform& view_proj_matrix) const final;

  void set_texture_id(unsigned int texture_id) { texture_id_ = texture_id; }

 private:
  ContentInputDelegate* delegate_ = nullptr;
  unsigned int texture_id_ = 0;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_CONTENT_ELEMENT_H_
