// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/overflow_bubble.h"

#include "ash/focus_cycler.h"
#include "ash/shelf/overflow_bubble_view.h"
#include "ash/shelf/overflow_button.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shell.h"
#include "ash/system/tray/tray_background_view.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"

namespace ash {

OverflowBubble::OverflowBubble(Shelf* shelf)
    : shelf_(shelf), bubble_(nullptr), overflow_button_(nullptr) {
  DCHECK(shelf_);
  Shell::Get()->AddPreTargetHandler(this);
}

OverflowBubble::~OverflowBubble() {
  Hide();
  Shell::Get()->RemovePreTargetHandler(this);
}

void OverflowBubble::Show(OverflowButton* overflow_button,
                          ShelfView* shelf_view) {
  DCHECK(overflow_button);
  DCHECK(shelf_view);

  Hide();

  bubble_ = new OverflowBubbleView(shelf_);
  bubble_->InitOverflowBubble(overflow_button, shelf_view);
  overflow_button_ = overflow_button;

  TrayBackgroundView::InitializeBubbleAnimations(bubble_->GetWidget());
  bubble_->GetWidget()->AddObserver(this);
  bubble_->GetWidget()->Show();
  Shell::Get()->focus_cycler()->AddWidget(bubble_->GetWidget());

  overflow_button->OnOverflowBubbleShown();
}

void OverflowBubble::Hide() {
  if (!IsShowing())
    return;

  OverflowButton* overflow_button = overflow_button_;

  Shell::Get()->focus_cycler()->RemoveWidget(bubble_->GetWidget());
  bubble_->GetWidget()->RemoveObserver(this);
  bubble_->GetWidget()->Close();
  bubble_ = nullptr;
  overflow_button_ = nullptr;

  overflow_button->OnOverflowBubbleHidden();
}

void OverflowBubble::ProcessPressedEvent(ui::LocatedEvent* event) {
  if (!IsShowing() || bubble_->shelf_view()->IsShowingMenu())
    return;

  aura::Window* target = static_cast<aura::Window*>(event->target());
  gfx::Point event_location_in_screen = event->location();
  aura::client::GetScreenPositionClient(target->GetRootWindow())
      ->ConvertPointToScreen(target, &event_location_in_screen);

  if (bubble_->GetBoundsInScreen().Contains(event_location_in_screen) ||
      overflow_button_->GetBoundsInScreen().Contains(
          event_location_in_screen)) {
    return;
  }

  // Do not hide the shelf if one of the buttons on the main shelf was pressed,
  // since the user might want to drag an item onto the overflow bubble.
  // The button itself will close the overflow bubble on the release event.
  if (bubble_->shelf_view()
          ->main_shelf()
          ->GetVisibleItemsBoundsInScreen()
          .Contains(event_location_in_screen)) {
    return;
  }

  Hide();
}

void OverflowBubble::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() == ui::ET_MOUSE_PRESSED)
    ProcessPressedEvent(event);
}

void OverflowBubble::OnTouchEvent(ui::TouchEvent* event) {
  if (event->type() == ui::ET_TOUCH_PRESSED)
    ProcessPressedEvent(event);
}

void OverflowBubble::OnWidgetDestroying(views::Widget* widget) {
  DCHECK(widget == bubble_->GetWidget());
  // Update the overflow button in the parent ShelfView.
  overflow_button_->SchedulePaint();
  bubble_ = nullptr;
  overflow_button_ = nullptr;
}

}  // namespace ash
