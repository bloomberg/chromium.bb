// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/overflow_bubble.h"

#include "ash/root_window_controller.h"
#include "ash/shelf/overflow_bubble_view.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/tray/system_tray.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/events/event.h"
#include "ui/gfx/screen.h"
#include "ui/views/widget/widget.h"

namespace ash {

OverflowBubble::OverflowBubble()
    : bubble_(NULL),
      anchor_(NULL),
      shelf_view_(NULL) {
}

OverflowBubble::~OverflowBubble() {
  Hide();
}

void OverflowBubble::Show(views::View* anchor, ShelfView* shelf_view) {
  Hide();

  bubble_ = new OverflowBubbleView();
  bubble_->InitOverflowBubble(anchor, shelf_view);
  shelf_view_ = shelf_view;
  anchor_ = anchor;

  Shell::GetInstance()->AddPreTargetHandler(this);

  RootWindowController::ForWindow(anchor->GetWidget()->GetNativeView())->
      GetSystemTray()->InitializeBubbleAnimations(bubble_->GetWidget());
  bubble_->GetWidget()->AddObserver(this);
  bubble_->GetWidget()->Show();
}

void OverflowBubble::Hide() {
  if (!IsShowing())
    return;

  Shell::GetInstance()->RemovePreTargetHandler(this);
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

void OverflowBubble::ProcessPressedEvent(ui::LocatedEvent* event) {
  aura::Window* target = static_cast<aura::Window*>(event->target());
  gfx::Point event_location_in_screen = event->location();
  aura::client::GetScreenPositionClient(target->GetRootWindow())->
      ConvertPointToScreen(target, &event_location_in_screen);
  if (!shelf_view_->IsShowingMenu() &&
      !bubble_->GetBoundsInScreen().Contains(event_location_in_screen) &&
      !anchor_->GetBoundsInScreen().Contains(event_location_in_screen)) {
    HideBubbleAndRefreshButton();
  }
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
  bubble_ = NULL;
  anchor_ = NULL;
  shelf_view_->shelf_layout_manager()->shelf_widget()->shelf()->SchedulePaint();
  shelf_view_ = NULL;
}

}  // namespace ash
