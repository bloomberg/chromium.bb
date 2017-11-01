// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/controller.h"

#include "chrome/browser/vr/controller_mesh.h"
#include "chrome/browser/vr/ui_element_renderer.h"

namespace vr {

Controller::Controller() {
  set_name(kController);
  set_hit_testable(false);
  SetVisible(true);
}

Controller::~Controller() = default;

void Controller::Render(UiElementRenderer* renderer,
                        const gfx::Transform& model_view_proj_matrix) const {
  ControllerMesh::State state;
  if (touchpad_button_pressed_) {
    state = ControllerMesh::TOUCHPAD;
  } else if (app_button_pressed_) {
    state = ControllerMesh::APP;
  } else if (home_button_pressed_) {
    state = ControllerMesh::SYSTEM;
  } else {
    state = ControllerMesh::IDLE;
  }

  renderer->DrawController(state, computed_opacity(), model_view_proj_matrix);
}

gfx::Transform Controller::LocalTransform() const {
  return local_transform_;
}

}  // namespace vr
