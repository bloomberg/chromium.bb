// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_RECT_H_
#define CHROME_BROWSER_VR_ELEMENTS_RECT_H_

#include "chrome/browser/vr/elements/ui_element.h"
#include "third_party/skia/include/core/SkColor.h"

namespace vr {

// A rect renders a rect via a shader (no texture). This rect can optionally
// have a corner radius and may have a difference center and edge color. If
// these colors are different, we will render a radial gradient between these
// two colors. This radial gradient is not aspect correct; it will be elliptical
// if the rect is stretched. This is intended to serve as a background to be put
// behind other elements.
class Rect : public UiElement {
 public:
  Rect();
  ~Rect() override;

  SkColor center_color() const { return center_color_; }
  void SetCenterColor(SkColor color);

  SkColor edge_color() const { return edge_color_; }
  void SetEdgeColor(SkColor color);

  void NotifyClientColorAnimated(SkColor color,
                                 int target_property_id,
                                 cc::Animation* animation) override;

  void Render(UiElementRenderer* renderer,
              const gfx::Transform& model_view_proj_matrix) const override;

 private:
  SkColor center_color_ = SK_ColorWHITE;
  SkColor edge_color_ = SK_ColorWHITE;

  DISALLOW_COPY_AND_ASSIGN(Rect);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_RECT_H_
