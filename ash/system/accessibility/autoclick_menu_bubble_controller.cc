// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/autoclick_menu_bubble_controller.h"

#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/unified_system_tray_view.h"

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
  gfx::Rect work_area =
      Shelf::ForWindow(Shell::GetPrimaryRootWindow())->GetUserWorkAreaBounds();
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
