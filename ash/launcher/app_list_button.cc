// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/launcher/app_list_button.h"

#include "ash/launcher/launcher_button_host.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/base/animation/throb_animation.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/shadow_value.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/gfx/transform_util.h"
#include "ui/views/controls/image_view.h"

namespace ash {

namespace internal {

AppListButton::AppListButton(views::ButtonListener* listener,
                             LauncherButtonHost* host)
    : views::ImageButton(listener),
      host_(host) {}

AppListButton::~AppListButton() {
}

bool AppListButton::OnMousePressed(const views::MouseEvent& event) {
  ImageButton::OnMousePressed(event);
  host_->MousePressedOnButton(this, event);
  return true;
}

void AppListButton::OnMouseReleased(const views::MouseEvent& event) {
  ImageButton::OnMouseReleased(event);
  host_->MouseReleasedOnButton(this, false);
}

void AppListButton::OnMouseCaptureLost() {
  host_->MouseReleasedOnButton(this, true);
  ImageButton::OnMouseCaptureLost();
}

bool AppListButton::OnMouseDragged(const views::MouseEvent& event) {
  ImageButton::OnMouseDragged(event);
  host_->MouseDraggedOnButton(this, event);
  return true;
}

void AppListButton::OnMouseMoved(const views::MouseEvent& event) {
  ImageButton::OnMouseMoved(event);
  host_->MouseMovedOverButton(this);
}

void AppListButton::OnMouseEntered(const views::MouseEvent& event) {
  ImageButton::OnMouseEntered(event);
  host_->MouseEnteredButton(this);
}

void AppListButton::OnMouseExited(const views::MouseEvent& event) {
  ImageButton::OnMouseExited(event);
  host_->MouseExitedButton(this);
}

void AppListButton::GetAccessibleState(ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_PUSHBUTTON;
  state->name = host_->GetAccessibleName(this);
}

}  // namespace internal
}  // namespace ash
