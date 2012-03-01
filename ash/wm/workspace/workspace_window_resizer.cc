// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_window_resizer.h"

#include "ash/wm/window_util.h"
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

WorkspaceWindowResizer::WorkspaceWindowResizer(aura::Window* window,
                                               const gfx::Point& location,
                                               int window_component,
                                               int grid_size)
    : WindowResizer(window, location, window_component, grid_size),
      constrain_size_(wm::IsWindowNormal(window)) {
  if (is_resizable() && GetHeightBeforeObscured(window) &&
      constrain_size_ &&
      (!WindowTouchesBottomOfScreen() ||
       bounds_change() != kBoundsChange_Repositions)) {
    ClearHeightBeforeObscured(window);
  }
}

WorkspaceWindowResizer::~WorkspaceWindowResizer() {
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
  if (!constrain_size_)
    return;

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
