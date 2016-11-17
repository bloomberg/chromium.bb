// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/tray/fixed_sized_scroll_view.h"

#include "ash/common/material_design/material_design_controller.h"
#include "ui/views/controls/scrollbar/overlay_scroll_bar.h"

namespace ash {

namespace {

bool UseMd() {
  return MaterialDesignController::IsSystemTrayMenuMaterial();
}

}  // namespace

FixedSizedScrollView::FixedSizedScrollView() {
  set_notify_enter_exit_on_child(true);
  if (UseMd())
    SetVerticalScrollBar(new views::OverlayScrollBar(false));
}

FixedSizedScrollView::~FixedSizedScrollView() {}

void FixedSizedScrollView::SetContentsView(views::View* view) {
  SetContents(view);
  if (!UseMd())
    view->SetBoundsRect(gfx::Rect(view->GetPreferredSize()));
}

void FixedSizedScrollView::SetFixedSize(const gfx::Size& size) {
  DCHECK(!UseMd());
  if (fixed_size_ == size)
    return;
  fixed_size_ = size;
  PreferredSizeChanged();
}

void FixedSizedScrollView::set_fixed_size(const gfx::Size& size) {
  DCHECK(!UseMd());
  fixed_size_ = size;
}

gfx::Size FixedSizedScrollView::GetPreferredSize() const {
  if (UseMd())
    return views::View::GetPreferredSize();

  gfx::Size size =
      fixed_size_.IsEmpty() ? contents()->GetPreferredSize() : fixed_size_;
  gfx::Insets insets = GetInsets();
  size.Enlarge(insets.width(), insets.height());
  return size;
}

void FixedSizedScrollView::Layout() {
  if (UseMd())
    return views::ScrollView::Layout();

  gfx::Rect bounds = gfx::Rect(contents()->GetPreferredSize());
  bounds.set_width(std::max(0, width() - GetScrollBarWidth()));
  // Keep the origin of the contents unchanged so that the list will not scroll
  // away from the current visible region user is viewing. ScrollView::Layout()
  // will make sure the contents line up with its viewport properly if
  // the contents moves out of the viewport region.
  bounds.set_origin(contents()->bounds().origin());
  contents()->SetBoundsRect(bounds);

  views::ScrollView::Layout();
  if (!vertical_scroll_bar()->visible()) {
    gfx::Rect bounds = contents()->bounds();
    bounds.set_width(bounds.width() + GetScrollBarWidth());
    contents()->SetBoundsRect(bounds);
  }
}

void FixedSizedScrollView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  if (UseMd())
    return;

  gfx::Rect bounds = gfx::Rect(contents()->GetPreferredSize());
  bounds.set_width(std::max(0, width() - GetScrollBarWidth()));
  contents()->SetBoundsRect(bounds);
}

}  // namespace ash
