// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/window_selector_window.h"

#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/overview/scoped_transform_overview_window.h"
#include "grit/ash_resources.h"
#include "ui/aura/window.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/transform.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/widget/widget.h"

namespace ash {

WindowSelectorWindow::WindowSelectorWindow(aura::Window* window)
    : transform_window_(window) {
  window->AddObserver(this);
}

WindowSelectorWindow::~WindowSelectorWindow() {
  if (transform_window_.window())
    transform_window_.window()->RemoveObserver(this);
}

aura::Window* WindowSelectorWindow::GetRootWindow() {
  return transform_window_.window()->GetRootWindow();
}

bool WindowSelectorWindow::HasSelectableWindow(const aura::Window* window) {
  return transform_window_.window() == window;
}

bool WindowSelectorWindow::Contains(const aura::Window* target) {
  return transform_window_.Contains(target);
}

void WindowSelectorWindow::RestoreWindowOnExit(aura::Window* window) {
  transform_window_.RestoreWindowOnExit();
}

aura::Window* WindowSelectorWindow::SelectionWindow() {
  return transform_window_.window();
}

void WindowSelectorWindow::RemoveWindow(const aura::Window* window) {
  DCHECK_EQ(transform_window_.window(), window);
  transform_window_.window()->RemoveObserver(this);
  transform_window_.OnWindowDestroyed();
  WindowSelectorItem::RemoveWindow(window);
}

bool WindowSelectorWindow::empty() const {
  return transform_window_.window() == NULL;
}

void WindowSelectorWindow::PrepareForOverview() {
  transform_window_.PrepareForOverview();
}

void WindowSelectorWindow::SetItemBounds(aura::Window* root_window,
                                         const gfx::Rect& target_bounds,
                                         bool animate) {
  gfx::Rect src_rect = transform_window_.GetBoundsInScreen();
  set_bounds(ScopedTransformOverviewWindow::
        ShrinkRectToFitPreservingAspectRatio(src_rect, target_bounds));
  transform_window_.SetTransform(root_window,
      ScopedTransformOverviewWindow::GetTransformForRect(src_rect, bounds()),
      animate);
}

void WindowSelectorWindow::SetOpacity(float opacity) {
  transform_window_.window()->layer()->SetOpacity(opacity);
  WindowSelectorItem::SetOpacity(opacity);
}

}  // namespace ash
