// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_BUTTON_H_
#define CHROME_BROWSER_VR_ELEMENTS_BUTTON_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/vr/elements/draw_phase.h"
#include "chrome/browser/vr/elements/invisible_hit_target.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/model/color_scheme.h"
#include "ui/gfx/vector_icon_types.h"

namespace gfx {
class PointF;
}  // namespace gfx

namespace vr {

class Rect;
class VectorIcon;

// Button has rounded rect as background and a vector icon as the foregroud.
// When hovered, background and foreground both move forward on Z axis.
class Button : public UiElement {
 public:
  Button(base::Callback<void()> click_handler, const gfx::VectorIcon& icon);
  ~Button() override;

  void Render(UiElementRenderer* renderer,
              const CameraModel& model) const final;

  Rect* background() const { return background_; }
  VectorIcon* foreground() const { return foreground_; }
  UiElement* hit_plane() const { return hit_plane_; }
  void SetButtonColors(const ButtonColors& colors);

  // TODO(vollick): once all elements are scaled by a ScaledDepthAdjuster, we
  // will never have to change the button hover offset from the default and this
  // method and the associated field can be removed.
  void set_hover_offset(float hover_offset) { hover_offset_ = hover_offset; }

 private:
  void HandleHoverEnter();
  void HandleHoverMove(const gfx::PointF& position);
  void HandleHoverLeave();
  void HandleButtonDown();
  void HandleButtonUp();
  void OnStateUpdated();

  void OnSetDrawPhase() override;
  void OnSetName() override;
  void NotifyClientSizeAnimated(const gfx::SizeF& size,
                                int target_property_id,
                                cc::Animation* animation) override;
  bool down_ = false;

  bool hovered_ = false;
  bool pressed_ = false;
  bool disabled_ = false;
  base::Callback<void()> click_handler_;
  float hover_offset_;
  ButtonColors colors_;

  Rect* background_;
  VectorIcon* foreground_;
  UiElement* hit_plane_;

  DISALLOW_COPY_AND_ASSIGN(Button);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_BUTTON_H_
