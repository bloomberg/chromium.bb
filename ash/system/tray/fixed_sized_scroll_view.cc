// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/fixed_sized_scroll_view.h"

namespace ash {
namespace internal {

FixedSizedScrollView::FixedSizedScrollView() {
  set_focusable(true);
  set_notify_enter_exit_on_child(true);
}

FixedSizedScrollView::~FixedSizedScrollView() {
}

void FixedSizedScrollView::SetContentsView(views::View* view) {
  SetContents(view);
  view->SetBoundsRect(gfx::Rect(view->GetPreferredSize()));
}

void FixedSizedScrollView::SetFixedSize(const gfx::Size& size) {
  if (fixed_size_ == size)
    return;
  fixed_size_ = size;
  PreferredSizeChanged();
}

gfx::Size FixedSizedScrollView::GetPreferredSize() {
  gfx::Size size = fixed_size_.IsEmpty() ?
      contents()->GetPreferredSize() : fixed_size_;
  gfx::Insets insets = GetInsets();
  size.Enlarge(insets.width(), insets.height());
  return size;
}

void FixedSizedScrollView::Layout() {
  gfx::Rect bounds = gfx::Rect(contents()->GetPreferredSize());
  bounds.set_width(std::max(0, width() - GetScrollBarWidth()));
  contents()->SetBoundsRect(bounds);

  views::ScrollView::Layout();
  if (!vertical_scroll_bar()->visible()) {
    gfx::Rect bounds = contents()->bounds();
    bounds.set_width(bounds.width() + GetScrollBarWidth());
    contents()->SetBoundsRect(bounds);
  }
}

void FixedSizedScrollView::OnMouseEntered(const ui::MouseEvent& event) {
  // TODO(sad): This is done to make sure that the scroll view scrolls on
  // mouse-wheel events. This is ugly, and Ben thinks this is weird. There
  // should be a better fix for this.
  RequestFocus();
}

void FixedSizedScrollView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  gfx::Rect bounds = gfx::Rect(contents()->GetPreferredSize());
  bounds.set_width(std::max(0, width() - GetScrollBarWidth()));
  contents()->SetBoundsRect(bounds);
}

void FixedSizedScrollView::OnPaintFocusBorder(gfx::Canvas* canvas) {
  // Do not paint the focus border.
}

}  // namespace internal
}  // namespace ash
