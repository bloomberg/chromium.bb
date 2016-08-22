// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/shelf/overflow_bubble.h"

#include "ash/common/shelf/overflow_bubble_view.h"
#include "ash/common/shelf/shelf_view.h"
#include "ash/common/shelf/wm_shelf.h"
#include "ash/common/system/tray/tray_background_view.h"
#include "ash/common/wm_shell.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"

namespace ash {

OverflowBubble::OverflowBubble(WmShelf* wm_shelf)
    : wm_shelf_(wm_shelf),
      bubble_(nullptr),
      anchor_(nullptr),
      shelf_view_(nullptr) {
  const bool wants_moves = false;
  WmShell::Get()->AddPointerWatcher(this, wants_moves);
}

OverflowBubble::~OverflowBubble() {
  Hide();
  WmShell::Get()->RemovePointerWatcher(this);
}

void OverflowBubble::Show(views::View* anchor, ShelfView* shelf_view) {
  DCHECK(anchor);
  DCHECK(shelf_view);

  Hide();

  bubble_ = new OverflowBubbleView(wm_shelf_);
  bubble_->InitOverflowBubble(anchor, shelf_view);
  shelf_view_ = shelf_view;
  anchor_ = anchor;

  TrayBackgroundView::InitializeBubbleAnimations(bubble_->GetWidget());
  bubble_->GetWidget()->AddObserver(this);
  bubble_->GetWidget()->Show();
}

void OverflowBubble::Hide() {
  if (!IsShowing())
    return;

  bubble_->GetWidget()->RemoveObserver(this);
  bubble_->GetWidget()->Close();
  bubble_ = NULL;
  anchor_ = NULL;
  shelf_view_ = NULL;
}

void OverflowBubble::HideBubbleAndRefreshButton() {
  if (!IsShowing())
    return;

  views::View* anchor = anchor_;
  Hide();
  // Update overflow button (|anchor|) status when overflow bubble is hidden
  // by outside event of overflow button.
  anchor->SchedulePaint();
}

void OverflowBubble::ProcessPressedEvent(
    const gfx::Point& event_location_in_screen) {
  if (IsShowing() && !shelf_view_->IsShowingMenu() &&
      !bubble_->GetBoundsInScreen().Contains(event_location_in_screen) &&
      !anchor_->GetBoundsInScreen().Contains(event_location_in_screen)) {
    HideBubbleAndRefreshButton();
  }
}

void OverflowBubble::OnPointerEventObserved(
    const ui::PointerEvent& event,
    const gfx::Point& location_in_screen,
    views::Widget* target) {
  if (event.type() == ui::ET_POINTER_DOWN)
    ProcessPressedEvent(location_in_screen);
}

void OverflowBubble::OnWidgetDestroying(views::Widget* widget) {
  DCHECK(widget == bubble_->GetWidget());
  // Update the overflow button in the parent ShelfView.
  anchor_->SchedulePaint();
  bubble_ = nullptr;
  anchor_ = nullptr;
  shelf_view_ = nullptr;
}

}  // namespace ash
