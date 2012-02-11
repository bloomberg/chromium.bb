// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/compact_status_area_layout_manager.h"

#include "ash/shell.h"
#include "base/auto_reset.h"
#include "base/i18n/rtl.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/screen.h"
#include "ui/views/widget/widget.h"

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
  gfx::Rect monitor_bounds = gfx::Screen::GetPrimaryMonitorBounds();
  gfx::Rect widget_bounds = status_widget_->GetRestoredBounds();
  gfx::Size offset = ash::Shell::GetInstance()->compact_status_area_offset();
  if (base::i18n::IsRTL()) {
    // Place the widget in the top-left corner of the screen.
    widget_bounds.set_x(monitor_bounds.x() + offset.width());
  } else {
    // Place the widget in the top-right corner of the screen.
    widget_bounds.set_x(
        monitor_bounds.right() - widget_bounds.width() - offset.width());
  }
  widget_bounds.set_y(offset.height());
  status_widget_->SetBounds(widget_bounds);
}

}  // namespace internal
}  // namespace ash
