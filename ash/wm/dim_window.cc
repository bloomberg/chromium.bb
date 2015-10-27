// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/dim_window.h"
#include "base/time/time.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window_property.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/wm/core/visibility_controller.h"
#include "ui/wm/core/window_animations.h"

DECLARE_WINDOW_PROPERTY_TYPE(ash::DimWindow*);

namespace ash {
namespace {

DEFINE_LOCAL_WINDOW_PROPERTY_KEY(DimWindow*, kDimWindowKey, nullptr);

const int kDefaultDimAnimationDurationMs = 200;

const float kDefaultDimOpacity = 0.5f;

}  // namespace

// static
DimWindow* DimWindow::Get(aura::Window* container) {
  return container->GetProperty(kDimWindowKey);
}

DimWindow::DimWindow(aura::Window* parent)
    : aura::Window(nullptr), parent_(parent) {
  SetType(ui::wm::WINDOW_TYPE_NORMAL);
  Init(ui::LAYER_SOLID_COLOR);
  wm::SetWindowVisibilityChangesAnimated(this);
  wm::SetWindowVisibilityAnimationType(
      this, wm::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE);
  wm::SetWindowVisibilityAnimationDuration(
      this, base::TimeDelta::FromMilliseconds(kDefaultDimAnimationDurationMs));

  SetDimOpacity(kDefaultDimOpacity);

  parent->AddChild(this);
  parent->AddObserver(this);
  parent->SetProperty(kDimWindowKey, this);
  parent->StackChildAtTop(this);

  SetBounds(parent->bounds());
}

DimWindow::~DimWindow() {
  if (parent_) {
    parent_->ClearProperty(kDimWindowKey);
    parent_->RemoveObserver(this);
    parent_ = nullptr;
  }
}

void DimWindow::SetDimOpacity(float target_opacity) {
  layer()->SetColor(SkColorSetA(SK_ColorBLACK, 255 * target_opacity));
}

void DimWindow::OnWindowBoundsChanged(aura::Window* window,
                                      const gfx::Rect& old_bounds,
                                      const gfx::Rect& new_bounds) {
  if (window == parent_)
    SetBounds(new_bounds);
}

void DimWindow::OnWindowDestroying(Window* window) {
  if (window == parent_) {
    window->ClearProperty(kDimWindowKey);
    window->RemoveObserver(this);
    parent_ = nullptr;
  }
}

}  // namespace ash
