// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/shelf/overflow_bubble.h"

#include "ash/common/shelf/overflow_bubble_view.h"
#include "ash/common/shelf/overflow_button.h"
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
      overflow_button_(nullptr),
      shelf_view_(nullptr) {
  DCHECK(wm_shelf_);
  WmShell::Get()->AddPointerWatcher(this,
                                    views::PointerWatcherEventTypes::BASIC);
}

OverflowBubble::~OverflowBubble() {
  Hide();
  WmShell::Get()->RemovePointerWatcher(this);
}

void OverflowBubble::Show(OverflowButton* overflow_button,
                          ShelfView* shelf_view) {
  DCHECK(overflow_button);
  DCHECK(shelf_view);

  Hide();

  bubble_ = new OverflowBubbleView(wm_shelf_);
  bubble_->InitOverflowBubble(overflow_button, shelf_view);
  shelf_view_ = shelf_view;
  overflow_button_ = overflow_button;

  TrayBackgroundView::InitializeBubbleAnimations(bubble_->GetWidget());
  bubble_->GetWidget()->AddObserver(this);
  bubble_->GetWidget()->Show();

  overflow_button->OnOverflowBubbleShown();
}

void OverflowBubble::Hide() {
  if (!IsShowing())
    return;

  OverflowButton* overflow_button = overflow_button_;

  bubble_->GetWidget()->RemoveObserver(this);
  bubble_->GetWidget()->Close();
  bubble_ = nullptr;
  overflow_button_ = nullptr;
  shelf_view_ = nullptr;

  overflow_button->OnOverflowBubbleHidden();
}

void OverflowBubble::ProcessPressedEvent(
    const gfx::Point& event_location_in_screen) {
  if (IsShowing() && !shelf_view_->IsShowingMenu() &&
      !bubble_->GetBoundsInScreen().Contains(event_location_in_screen) &&
      !overflow_button_->GetBoundsInScreen().Contains(
          event_location_in_screen)) {
    Hide();
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
  overflow_button_->SchedulePaint();
  bubble_ = nullptr;
  overflow_button_ = nullptr;
  shelf_view_ = nullptr;
}

}  // namespace ash
