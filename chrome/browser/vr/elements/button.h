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
  Button(base::Callback<void()> click_handler,
         DrawPhase draw_phase,
         float width,
         float height,
         float hover_offset,
         const gfx::VectorIcon& icon);
  ~Button() override;

  void Render(UiElementRenderer* renderer,
              const CameraModel& model) const final;

  Rect* background() const { return background_; }
  VectorIcon* foreground() const { return foreground_; }
  UiElement* hit_plane() const { return hit_plane_; }
  void SetButtonColors(const ButtonColors& colors);

 private:
  void HandleHoverEnter();
  void HandleHoverMove(const gfx::PointF& position);
  void HandleHoverLeave();
  void HandleButtonDown();
  void HandleButtonUp();
  void OnStateUpdated();

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
