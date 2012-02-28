// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_window_resizer.h"

#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_property.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/compositor/layer.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/transform.h"

DECLARE_WINDOW_PROPERTY_TYPE(int)

namespace ash {
namespace internal {

namespace {

const aura::WindowProperty<int> kHeightBeforeObscuredProp = {0};
const aura::WindowProperty<int>* const kHeightBeforeObscuredKey =
    &kHeightBeforeObscuredProp;

}  // namespace

// static
bool WorkspaceWindowResizer::scale_windows_ = true;

WorkspaceWindowResizer::WorkspaceWindowResizer(aura::Window* window,
                                               const gfx::Point& location,
                                               int window_component,
                                               int grid_size)
    : WindowResizer(window, location, window_component, grid_size),
      scale_window_(scale_windows_ && is_resizable() &&
                    window_component == HTCAPTION) {
  if (is_resizable() && GetHeightBeforeObscured(window) &&
      (!WindowTouchesBottomOfScreen() ||
       bounds_change() != kBoundsChange_Repositions)) {
    ClearHeightBeforeObscured(window);
  }
  if (scale_window_) {
    // When dragging the caption scale slightly from the center.
    float scale = 1.01f;
    float x_offset =
        static_cast<float>(window->bounds().width()) * (scale - 1) / 2.0f;
    float y_offset =
        (static_cast<float>(window->bounds().width()) * (scale - 1) / 2.0f);
    ui::Transform transform;
    transform.SetTranslate(-x_offset, -y_offset);
    transform.SetScaleX(scale);
    transform.SetScaleY(scale);
    window->layer()->SetTransform(transform);
  }
}

WorkspaceWindowResizer::~WorkspaceWindowResizer() {
}

// static
void WorkspaceWindowResizer::SetScaleWindowsForTest(bool value) {
  scale_windows_ = value;
}

void WorkspaceWindowResizer::CompleteDrag() {
  if (!did_move_or_resize() || grid_size() <= 1) {
    if (scale_window_) {
      ui::ScopedLayerAnimationSettings scoped_setter(
          window()->layer()->GetAnimator());
      window()->layer()->SetTransform(ui::Transform());
    }
    return;
  }

  gfx::Rect new_bounds(GetFinalBounds());
  if (new_bounds.size() != window()->bounds().size()) {
    // Don't attempt to animate a size change.
    window()->SetBounds(new_bounds);
    if (scale_window_)
      window()->layer()->SetTransform(ui::Transform());
    return;
  }

  ui::ScopedLayerAnimationSettings scoped_setter(
      window()->layer()->GetAnimator());
  // Use a small duration since the grid is small.
  scoped_setter.SetTransitionDuration(base::TimeDelta::FromMilliseconds(100));
  window()->SetBounds(new_bounds);
  if (scale_window_)
    window()->layer()->SetTransform(ui::Transform());
}

void WorkspaceWindowResizer::RevertDrag() {
  if (scale_window_)
    window()->layer()->SetTransform(ui::Transform());
  WindowResizer::RevertDrag();
}

gfx::Rect WorkspaceWindowResizer::GetBoundsForDrag(const gfx::Point& location) {
  if (!is_resizable())
    return WindowResizer::GetBoundsForDrag(location);

  gfx::Rect bounds(WindowResizer::GetBoundsForDrag(location));
  AdjustBounds(&bounds);
  return bounds;
}

gfx::Rect WorkspaceWindowResizer::GetFinalBounds() {
  if (grid_size() <= 1 || !GetHeightBeforeObscured(window()))
    return WindowResizer::GetFinalBounds();

  gfx::Rect initial_bounds(window()->bounds());
  bool at_bottom = WindowTouchesBottomOfScreen();
  gfx::Rect bounds(WindowResizer::GetFinalBounds());
  if (at_bottom && bounds.y() != initial_bounds.y()) {
    if (bounds.y() < initial_bounds.y()) {
      bounds.set_height(bounds.height() + grid_size() -
                        (initial_bounds.y() - bounds.y()));
    }
    AdjustBounds(&bounds);
  }
  return bounds;
}

// static
void WorkspaceWindowResizer::SetHeightBeforeObscured(aura::Window* window,
                                                     int height) {
  window->SetProperty(kHeightBeforeObscuredKey, height);
}

// static
void WorkspaceWindowResizer::ClearHeightBeforeObscured(aura::Window* window) {
  window->SetProperty(kHeightBeforeObscuredKey, 0);
}

// static
int WorkspaceWindowResizer::GetHeightBeforeObscured(aura::Window* window) {
  return window->GetProperty(kHeightBeforeObscuredKey);
}

void WorkspaceWindowResizer::AdjustBounds(gfx::Rect* bounds) const {
  gfx::Rect work_area(gfx::Screen::GetMonitorWorkAreaNearestWindow(window()));
  if (bounds->bottom() < work_area.bottom()) {
    int height = GetHeightBeforeObscured(window());
    if (!height)
      return;
    height = std::max(bounds->height(), height);
    bounds->set_height(std::min(work_area.bottom() - bounds->y(), height));
    return;
  }

  if (bounds->bottom() == work_area.bottom())
    return;

  if (!GetHeightBeforeObscured(window()))
    SetHeightBeforeObscured(window(), window()->bounds().height());

  gfx::Size min_size = window()->delegate()->GetMinimumSize();
  bounds->set_height(std::max(0, work_area.bottom() - bounds->y()));
  if (bounds->height() < min_size.height()) {
    bounds->set_height(min_size.height());
    bounds->set_y(work_area.bottom() - min_size.height());
  }
}

bool WorkspaceWindowResizer::WindowTouchesBottomOfScreen() const {
  gfx::Rect work_area(gfx::Screen::GetMonitorWorkAreaNearestWindow(window()));
  return window()->bounds().bottom() == work_area.bottom();
}

}  // namespace internal
}  // namespace ash
