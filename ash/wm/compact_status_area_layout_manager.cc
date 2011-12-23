// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/compact_status_area_layout_manager.h"

#include "ui/gfx/rect.h"
#include "ui/gfx/screen.h"
#include "ui/views/widget/widget.h"

namespace {
// Padding between the right edge of status area and right edge of screen.
const int kRightEdgePad = 3;
}  // namespace

namespace aura_shell {
namespace internal {

////////////////////////////////////////////////////////////////////////////////
// CompactStatusAreaLayoutManager, public:

CompactStatusAreaLayoutManager::CompactStatusAreaLayoutManager(
    views::Widget* status_widget)
    : status_widget_(status_widget) {
}

CompactStatusAreaLayoutManager::~CompactStatusAreaLayoutManager() {
}

////////////////////////////////////////////////////////////////////////////////
// CompactStatusAreaLayoutManager, aura::LayoutManager implementation:

void CompactStatusAreaLayoutManager::OnWindowResized() {
  LayoutStatusArea();
}

void CompactStatusAreaLayoutManager::OnWindowAddedToLayout(
    aura::Window* child) {
  LayoutStatusArea();
}

void CompactStatusAreaLayoutManager::OnWillRemoveWindowFromLayout(
    aura::Window* child) {
}

void CompactStatusAreaLayoutManager::OnChildWindowVisibilityChanged(
    aura::Window* child, bool visible) {
}

void CompactStatusAreaLayoutManager::SetChildBounds(
    aura::Window* child, const gfx::Rect& requested_bounds) {
  SetChildBoundsDirect(child, requested_bounds);
}

////////////////////////////////////////////////////////////////////////////////
// CompactStatusAreaLayoutManager, private:

void CompactStatusAreaLayoutManager::LayoutStatusArea() {
  // Place the widget in the top-right corner of the screen.
  gfx::Rect monitor_bounds = gfx::Screen::GetPrimaryMonitorBounds();
  gfx::Rect widget_bounds = status_widget_->GetRestoredBounds();
  widget_bounds.set_x(
      monitor_bounds.width() - widget_bounds.width() - kRightEdgePad);
  widget_bounds.set_y(0);
  status_widget_->SetBounds(widget_bounds);
}

}  // internal
}  // aura_shell
