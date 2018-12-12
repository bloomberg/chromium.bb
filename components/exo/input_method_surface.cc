// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/input_method_surface.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "components/exo/input_method_surface_manager.h"
#include "components/exo/wm_helper.h"
#include "ui/base/class_property.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace {
DEFINE_LOCAL_UI_CLASS_PROPERTY_KEY(exo::InputMethodSurface*,
                                   kInputMethodSurface,
                                   nullptr);
}

DEFINE_UI_CLASS_PROPERTY_TYPE(exo::InputMethodSurface*);

namespace exo {

InputMethodSurface::InputMethodSurface(InputMethodSurfaceManager* manager,
                                       Surface* surface,
                                       double default_device_scale_factor)
    : ClientControlledShellSurface(
          surface,
          true /* can_minimize */,
          ash::kShellWindowId_ArcVirtualKeyboardContainer),
      manager_(manager),
      input_method_bounds_(),
      default_device_scale_factore_(default_device_scale_factor) {
  SetScale(default_device_scale_factor);
  host_window()->SetName("ExoInputMethodSurface");
  host_window()->SetProperty(kInputMethodSurface, this);
}

InputMethodSurface::~InputMethodSurface() {
  if (added_to_manager_)
    manager_->RemoveSurface(this);
}

exo::InputMethodSurface* InputMethodSurface::GetInputMethodSurface() {
  WMHelper* wm_helper = exo::WMHelper::GetInstance();
  if (!wm_helper)
    return nullptr;

  aura::Window* container = wm_helper->GetPrimaryDisplayContainer(
      ash::kShellWindowId_ArcVirtualKeyboardContainer);
  if (!container)
    return nullptr;

  // Host window of InputMethodSurface is grandchild of the container.
  if (container->children().empty())
    return nullptr;

  aura::Window* child = container->children().at(0);

  if (child->children().empty())
    return nullptr;

  aura::Window* host_window = child->children().at(0);
  return host_window->GetProperty(kInputMethodSurface);
}

void InputMethodSurface::OnSurfaceCommit() {
  ClientControlledShellSurface::OnSurfaceCommit();

  if (!added_to_manager_) {
    added_to_manager_ = true;
    manager_->AddSurface(this);
  }

  gfx::Rect new_bounds = root_surface()->hit_test_region().bounds();
  if (input_method_bounds_ != new_bounds) {
    input_method_bounds_ = new_bounds;
    manager_->OnTouchableBoundsChanged(this);

    gfx::RectF bounds(input_method_bounds_);
    bounds.Scale(1.0 / default_device_scale_factore_);
    GetViewAccessibility().OverrideBounds(bounds);
  }
}

gfx::Rect InputMethodSurface::GetBounds() const {
  // TODO(crbug.com/912449): This should return bounds in DIP.
  return input_method_bounds_;
}

}  // namespace exo
