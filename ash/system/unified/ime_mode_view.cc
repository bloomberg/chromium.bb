// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/ime_mode_view.h"

#include "ash/ime/ime_controller.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_utils.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ui/views/controls/label.h"

namespace ash {

ImeModeView::ImeModeView() : TrayItemView(nullptr) {
  SetVisible(false);
  CreateLabel();
  SetupLabelForTray(label());
  Update();

  Shell::Get()->system_tray_notifier()->AddIMEObserver(this);
  Shell::Get()->tablet_mode_controller()->AddObserver(this);
}

ImeModeView::~ImeModeView() {
  if (Shell::Get()->tablet_mode_controller())
    Shell::Get()->tablet_mode_controller()->RemoveObserver(this);
  Shell::Get()->system_tray_notifier()->RemoveIMEObserver(this);
}

void ImeModeView::OnIMERefresh() {
  Update();
}

void ImeModeView::OnIMEMenuActivationChanged(bool is_active) {
  ime_menu_on_shelf_activated_ = is_active;
  Update();
}

void ImeModeView::OnTabletModeStarted() {
  Update();
}

void ImeModeView::OnTabletModeEnded() {
  Update();
}

void ImeModeView::OnSessionStateChanged(session_manager::SessionState state) {
  Update();
}

void ImeModeView::Update() {
  // Do not show IME mode icon in tablet mode as it's less useful and screen
  // space is limited.
  if (Shell::Get()
          ->tablet_mode_controller()
          ->IsTabletModeWindowManagerEnabled()) {
    SetVisible(false);
    return;
  }

  ImeController* ime_controller = Shell::Get()->ime_controller();

  size_t ime_count = ime_controller->available_imes().size();
  SetVisible(!ime_menu_on_shelf_activated_ &&
             (ime_count > 1 || ime_controller->managed_by_policy()));

  label()->SetText(ime_controller->current_ime().short_name);
  label()->SetEnabledColor(
      TrayIconColor(Shell::Get()->session_controller()->GetSessionState()));
  Layout();
}

}  // namespace ash
