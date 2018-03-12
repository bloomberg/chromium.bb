// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_RECT_H_
#define CHROME_BROWSER_VR_ELEMENTS_RECT_H_

#include "chrome/browser/vr/elements/ui_element.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/point_f.h"

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

  // Syntactic sugar for setting both the edge and center colors simultaneously.
  void SetColor(SkColor color);

  SkColor center_color() const { return center_color_; }
  void SetCenterColor(SkColor color);

  SkColor edge_color() const { return edge_color_; }
  void SetEdgeColor(SkColor color);

  void NotifyClientColorAnimated(SkColor color,
                                 int target_property_id,
                                 cc::KeyframeModel* keyframe_model) override;

  void Render(UiElementRenderer* renderer,
              const CameraModel& model) const override;

  void set_center_point(const gfx::PointF& center_point) {
    center_point_ = center_point;
  }

 private:
  SkColor center_color_ = SK_ColorWHITE;
  SkColor edge_color_ = SK_ColorWHITE;

  // This is the center point of the gradient, in aspect-corrected, local
  // coordinates. That is, {0, 0} is always the center of the quad, and the
  // longer extent always varies between -0.5 and 0.5.
  gfx::PointF center_point_;

  DISALLOW_COPY_AND_ASSIGN(Rect);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_RECT_H_
