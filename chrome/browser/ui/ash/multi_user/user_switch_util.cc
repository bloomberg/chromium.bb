// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/multi_user/user_switch_util.h"

#include "ash/common/system/chromeos/screen_security/screen_tray_item.h"
#include "ash/common/system/tray/system_tray.h"
#include "ash/common/wm/overview/window_selector_controller.h"
#include "ash/common/wm_shell.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "ui/base/l10n/l10n_util.h"

void TrySwitchingActiveUser(const base::Callback<void()> on_switch) {
  // Some unit tests do not have a shell. In that case simply execute.
  if (!ash::Shell::HasInstance()) {
    on_switch.Run();
    return;
  }

  // Cancel overview mode when switching user profiles.
  ash::WindowSelectorController* controller =
      ash::WmShell::Get()->window_selector_controller();
  if (controller->IsSelecting())
    controller->ToggleOverview();

  // If neither screen sharing nor capturing is going on we can immediately
  // switch users.
  ash::SystemTray* system_tray =
      ash::Shell::GetInstance()->GetPrimarySystemTray();
  if (!system_tray->GetScreenShareItem()->is_started() &&
      !system_tray->GetScreenCaptureItem()->is_started()) {
    on_switch.Run();
    return;
  }
  if (chrome::ShowQuestionMessageBox(
          nullptr, l10n_util::GetStringUTF16(IDS_DESKTOP_CASTING_ACTIVE_TITLE),
          l10n_util::GetStringUTF16(IDS_DESKTOP_CASTING_ACTIVE_MESSAGE)) ==
      chrome::MESSAGE_BOX_RESULT_YES) {
    // Stop screen sharing and capturing.
    ash::SystemTray* system_tray =
        ash::Shell::GetInstance()->GetPrimarySystemTray();
    if (system_tray->GetScreenShareItem()->is_started())
      system_tray->GetScreenShareItem()->Stop();
    if (system_tray->GetScreenCaptureItem()->is_started())
      system_tray->GetScreenCaptureItem()->Stop();

    on_switch.Run();
  }
}
