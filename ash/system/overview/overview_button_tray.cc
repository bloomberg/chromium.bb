// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/overview/overview_button_tray.h"

#include "ash/session/session_state_delegate.h"
#include "ash/shelf/shelf_types.h"
#include "ash/shelf/shelf_util.h"
#include "ash/shell.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/tray_utils.h"
#include "ash/wm/maximize_mode/maximize_mode_controller.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"

namespace {

// Predefined padding for the icon used in this tray. These are to be set to the
// border of the icon, depending on the current shelf_alignment()
const int kHorizontalShelfHorizontalPadding = 8;
const int kHorizontalShelfVerticalPadding = 4;
const int kVerticalShelfHorizontalPadding = 2;
const int kVerticalShelfVerticalPadding = 5;

}  // namespace

namespace ash {

OverviewButtonTray::OverviewButtonTray(StatusAreaWidget* status_area_widget)
    : TrayBackgroundView(status_area_widget), icon_(NULL) {
  SetContentsBackground();

  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  icon_ = new views::ImageView();
  icon_->SetImage(
      bundle.GetImageNamed(IDR_AURA_UBER_TRAY_OVERVIEW_MODE).ToImageSkia());
  SetIconBorderForShelfAlignment();
  tray_container()->AddChildView(icon_);

  Shell::GetInstance()->AddShellObserver(this);
  Shell::GetInstance()->session_state_delegate()->AddSessionStateObserver(this);
}

OverviewButtonTray::~OverviewButtonTray() {
  Shell::GetInstance()->RemoveShellObserver(this);
  Shell::GetInstance()->session_state_delegate()->RemoveSessionStateObserver(
      this);
}

void OverviewButtonTray::UpdateAfterLoginStatusChange(
    user::LoginStatus status) {
  UpdateIconVisibility();
}

bool OverviewButtonTray::PerformAction(const ui::Event& event) {
  WindowSelectorController* controller =
      Shell::GetInstance()->window_selector_controller();
  controller->ToggleOverview();
  SetDrawBackgroundAsActive(controller->IsSelecting());
  Shell::GetInstance()->metrics()->RecordUserMetricsAction(UMA_TRAY_OVERVIEW);
  return true;
}

void OverviewButtonTray::SessionStateChanged(
    SessionStateDelegate::SessionState state) {
  UpdateIconVisibility();
}

void OverviewButtonTray::OnMaximizeModeStarted() {
  UpdateIconVisibility();
}

void OverviewButtonTray::OnMaximizeModeEnded() {
  UpdateIconVisibility();
}

void OverviewButtonTray::OnOverviewModeEnded() {
  SetDrawBackgroundAsActive(false);
}

bool OverviewButtonTray::ClickedOutsideBubble() {
  // This class has no bubbles dismiss, but acknowledge that the message was
  // handled.
  return true;
}

base::string16 OverviewButtonTray::GetAccessibleNameForTray() {
  return l10n_util::GetStringUTF16(IDS_ASH_OVERVIEW_BUTTON_ACCESSIBLE_NAME);
}

void OverviewButtonTray::HideBubbleWithView(
    const views::TrayBubbleView* bubble_view) {
  // This class has no bubbles to hide.
}

void OverviewButtonTray::SetShelfAlignment(wm::ShelfAlignment alignment) {
  if (alignment == shelf_alignment())
    return;

  TrayBackgroundView::SetShelfAlignment(alignment);
  SetIconBorderForShelfAlignment();
}

void OverviewButtonTray::SetIconBorderForShelfAlignment() {
  if (IsHorizontalAlignment(shelf_alignment())) {
    icon_->SetBorder(views::Border::CreateEmptyBorder(
        kHorizontalShelfVerticalPadding,
        kHorizontalShelfHorizontalPadding,
        kHorizontalShelfVerticalPadding,
        kHorizontalShelfHorizontalPadding));
  } else {
    icon_->SetBorder(views::Border::CreateEmptyBorder(
        kVerticalShelfVerticalPadding,
        kVerticalShelfHorizontalPadding,
        kVerticalShelfVerticalPadding,
        kVerticalShelfHorizontalPadding));
  }
}

void OverviewButtonTray::UpdateIconVisibility() {
  // The visibility of the OverviewButtonTray has diverge from
  // WindowSelectorController::CanSelect. The visibility of the button should
  // not change during transient times in which CanSelect is false. Such as when
  // a modal dialog is present.
  Shell* shell = Shell::GetInstance();
  SessionStateDelegate* session_state_delegate =
      shell->session_state_delegate();

  SetVisible(
      shell->maximize_mode_controller()->IsMaximizeModeWindowManagerEnabled() &&
      session_state_delegate->IsActiveUserSessionStarted() &&
      !session_state_delegate->IsScreenLocked() &&
      session_state_delegate->GetSessionState() ==
          SessionStateDelegate::SESSION_STATE_ACTIVE &&
      shell->system_tray_delegate()->GetUserLoginStatus() !=
          user::LOGGED_IN_KIOSK_APP);
}

}  // namespace ash
