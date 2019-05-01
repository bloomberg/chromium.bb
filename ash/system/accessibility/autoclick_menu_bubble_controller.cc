// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/autoclick_menu_bubble_controller.h"

#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/unified_system_tray_view.h"
#include "ash/wm/collision_detection/collision_detection_utils.h"
#include "ash/wm/work_area_insets.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event_utils.h"

namespace ash {

namespace {
// Autoclick menu constants.
const int kAutoclickMenuWidth = 321;
}  // namespace

AutoclickMenuBubbleController::AutoclickMenuBubbleController() {}

AutoclickMenuBubbleController::~AutoclickMenuBubbleController() {
  if (bubble_widget_ && !bubble_widget_->IsClosed())
    bubble_widget_->CloseNow();
}

void AutoclickMenuBubbleController::SetEventType(
    mojom::AutoclickEventType type) {
  if (menu_view_) {
    menu_view_->UpdateEventType(type);
  }
}

void AutoclickMenuBubbleController::SetPosition(
    mojom::AutoclickMenuPosition position) {
  if (!menu_view_ || !bubble_view_)
    return;

  menu_view_->UpdatePosition(position);
  // TODO(katie): On first load it can display over top of the shelf on Chrome
  // OS emulated on Linux (not reproduced on Eve). There must be a race
  // condition with the user work area bounds loading.

  // TODO(katie): Support multiple displays.
  gfx::Rect work_area = WorkAreaInsets::ForWindow(Shell::GetPrimaryRootWindow())
                            ->user_work_area_bounds();
  gfx::Rect new_position;
  switch (position) {
    case mojom::AutoclickMenuPosition::kBottomRight:
      new_position = gfx::Rect(work_area.width(), work_area.height(), 0, 0);
      break;
    case mojom::AutoclickMenuPosition::kBottomLeft:
      new_position = gfx::Rect(work_area.x(), work_area.height(), 0, 0);
      break;
    case mojom::AutoclickMenuPosition::kTopLeft:
      // Setting the top to 1 instead of 0 so that the view is drawn on screen.
      new_position = gfx::Rect(work_area.x(), 1, 0, 0);
      break;
    case mojom::AutoclickMenuPosition::kTopRight:
      // Setting the top to 1 instead of 0 so that the view is drawn on screen.
      new_position = gfx::Rect(work_area.width(), 1, 0, 0);
      break;
  }
  bubble_view_->MoveToPosition(new_position);
}

void AutoclickMenuBubbleController::ShowBubble(
    mojom::AutoclickEventType type,
    mojom::AutoclickMenuPosition position) {
  // Ignore if bubble widget already exists.
  if (bubble_widget_)
    return;

  DCHECK(!bubble_view_);

  TrayBubbleView::InitParams init_params;
  init_params.delegate = this;
  // Anchor within the overlay container.
  init_params.parent_window = Shell::GetContainer(
      Shell::GetPrimaryRootWindow(), kShellWindowId_AutoclickContainer);
  init_params.anchor_mode = TrayBubbleView::AnchorMode::kRect;
  init_params.insets =
      gfx::Insets(kUnifiedMenuPadding, kUnifiedMenuPadding,
                  kUnifiedMenuPadding - 1, kUnifiedMenuPadding - 1);
  init_params.min_width = kAutoclickMenuWidth;
  init_params.max_width = kAutoclickMenuWidth;
  init_params.corner_radius = kUnifiedTrayCornerRadius;
  init_params.has_shadow = false;
  bubble_view_ = new AutoclickMenuBubbleView(init_params);

  menu_view_ = new AutoclickMenuView(type, position);
  menu_view_->SetBackground(UnifiedSystemTrayView::CreateBackground());
  menu_view_->SetBorder(
      views::CreateEmptyBorder(kUnifiedTopShortcutSpacing, 0, 0, 0));
  bubble_view_->AddChildView(menu_view_);
  bubble_view_->set_color(SK_ColorTRANSPARENT);
  bubble_view_->layer()->SetFillsBoundsOpaquely(false);

  bubble_widget_ = views::BubbleDialogDelegateView::CreateBubble(bubble_view_);
  TrayBackgroundView::InitializeBubbleAnimations(bubble_widget_);
  CollisionDetectionUtils::MarkWindowPriorityForCollisionDetection(
      bubble_widget_->GetNativeWindow(),
      CollisionDetectionUtils::RelativePriority::kAutomaticClicksMenu);
  bubble_view_->InitializeAndShowBubble();

  if (app_list_features::IsBackgroundBlurEnabled()) {
    bubble_widget_->client_view()->layer()->SetBackgroundBlur(
        kUnifiedMenuBackgroundBlur);
  }

  SetPosition(position);
}

void AutoclickMenuBubbleController::CloseBubble() {
  if (!bubble_widget_ || bubble_widget_->IsClosed())
    return;
  bubble_widget_->Close();
}

void AutoclickMenuBubbleController::ClickOnBubble(gfx::Point location_in_dips,
                                                  int mouse_event_flags) {
  if (!bubble_widget_)
    return;

  // Change the event location bounds to be relative to the menu bubble.
  location_in_dips -= bubble_view_->GetBoundsInScreen().OffsetFromOrigin();

  // Generate synthesized mouse events for the click.
  const ui::MouseEvent press_event(ui::ET_MOUSE_PRESSED, location_in_dips,
                                   location_in_dips, ui::EventTimeForNow(),
                                   mouse_event_flags | ui::EF_LEFT_MOUSE_BUTTON,
                                   ui::EF_LEFT_MOUSE_BUTTON);
  const ui::MouseEvent release_event(
      ui::ET_MOUSE_RELEASED, location_in_dips, location_in_dips,
      ui::EventTimeForNow(), mouse_event_flags | ui::EF_LEFT_MOUSE_BUTTON,
      ui::EF_LEFT_MOUSE_BUTTON);

  // Send the press/release events to the widget's root view for processing.
  bubble_widget_->GetRootView()->OnMousePressed(press_event);
  bubble_widget_->GetRootView()->OnMouseReleased(release_event);
}

bool AutoclickMenuBubbleController::ContainsPointInScreen(
    const gfx::Point& point) {
  return bubble_view_ && bubble_view_->GetBoundsInScreen().Contains(point);
}

void AutoclickMenuBubbleController::BubbleViewDestroyed() {
  bubble_view_ = nullptr;
  bubble_widget_ = nullptr;
  menu_view_ = nullptr;
}

}  // namespace ash
