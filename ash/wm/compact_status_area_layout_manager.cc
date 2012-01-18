// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/compact_status_area_layout_manager.h"

#include "base/auto_reset.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/screen.h"
#include "ui/views/widget/widget.h"

namespace {

// Padding between the right edge of status area and right edge of screen.
const int kRightEdgePad = 3;

// Padding between the top of the status area and the top of the screen.
const int kTopEdgePad = 2;

}  // namespace

namespace ash {
namespace internal {

////////////////////////////////////////////////////////////////////////////////
// CompactStatusAreaLayoutManager, public:

CompactStatusAreaLayoutManager::CompactStatusAreaLayoutManager(
    views::Widget* status_widget)
    : in_layout_(false),
      status_widget_(status_widget) {
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
  if (!in_layout_)
    LayoutStatusArea();
}

////////////////////////////////////////////////////////////////////////////////
// CompactStatusAreaLayoutManager, private:

void CompactStatusAreaLayoutManager::LayoutStatusArea() {
  AutoReset<bool> auto_reset_in_layout(&in_layout_, true);
  // Place the widget in the top-right corner of the screen.
  gfx::Rect monitor_bounds = gfx::Screen::GetPrimaryMonitorBounds();
  gfx::Rect widget_bounds = status_widget_->GetRestoredBounds();
  widget_bounds.set_x(
      monitor_bounds.width() - widget_bounds.width() - kRightEdgePad);
  widget_bounds.set_y(kTopEdgePad);
  status_widget_->SetBounds(widget_bounds);
}

}  // namespace internal
}  // namespace ash
