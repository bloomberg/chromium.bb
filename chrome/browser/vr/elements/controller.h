// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_CONTROLLER_H_
#define CHROME_BROWSER_VR_ELEMENTS_CONTROLLER_H_

#include "base/macros.h"
#include "chrome/browser/vr/elements/ui_element.h"

namespace vr {

// This represents the controller.
class Controller : public UiElement {
 public:
  Controller();
  ~Controller() override;

  void set_touchpad_button_pressed(bool pressed) {
    touchpad_button_pressed_ = pressed;
  }

  void set_app_button_pressed(bool pressed) { app_button_pressed_ = pressed; }

  void set_home_button_pressed(bool pressed) { home_button_pressed_ = pressed; }

  void set_local_transform(const gfx::Transform& transform) {
    local_transform_ = transform;
  }

 private:
  void Render(UiElementRenderer* renderer,
              const gfx::Transform& model_view_proj_matrix) const final;

  gfx::Transform LocalTransform() const override;

  bool touchpad_button_pressed_ = false;
  bool app_button_pressed_ = false;
  bool home_button_pressed_ = false;
  gfx::Transform local_transform_;

  DISALLOW_COPY_AND_ASSIGN(Controller);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_CONTROLLER_H_
