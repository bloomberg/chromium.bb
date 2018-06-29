// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/quiet_mode_feature_pod_controller.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"

using message_center::MessageCenter;

namespace ash {

QuietModeFeaturePodController::QuietModeFeaturePodController(
    UnifiedSystemTrayController* tray_controller)
    : tray_controller_(tray_controller) {
  MessageCenter::Get()->AddObserver(this);
}

QuietModeFeaturePodController::~QuietModeFeaturePodController() {
  MessageCenter::Get()->RemoveObserver(this);
}

FeaturePodButton* QuietModeFeaturePodController::CreateButton() {
  DCHECK(!button_);
  button_ = new FeaturePodButton(this);
  button_->SetVisible(
      Shell::Get()->session_controller()->ShouldShowNotificationTray() &&
      !Shell::Get()->session_controller()->IsScreenLocked());
  button_->SetLabel(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NOTIFICATIONS_LABEL));
  button_->ShowDetailedViewArrow();
  OnQuietModeChanged(MessageCenter::Get()->IsQuietMode());
  return button_;
}

void QuietModeFeaturePodController::OnIconPressed() {
  MessageCenter* message_center = MessageCenter::Get();
  bool is_quiet_mode = message_center->IsQuietMode();
  message_center->SetQuietMode(!is_quiet_mode);

  // If quiet mode was disabled, show notifier settings as well as enabling
  // quiet mode.
  if (!is_quiet_mode)
    tray_controller_->ShowNotifierSettingsView();
}

void QuietModeFeaturePodController::OnLabelPressed() {
  MessageCenter::Get()->SetQuietMode(true);
  tray_controller_->ShowNotifierSettingsView();
}

SystemTrayItemUmaType QuietModeFeaturePodController::GetUmaType() const {
  return SystemTrayItemUmaType::UMA_NOT_RECORDED;
}

void QuietModeFeaturePodController::OnQuietModeChanged(bool in_quiet_mode) {
  button_->SetVectorIcon(in_quiet_mode
                             ? kNotificationCenterDoNotDisturbOnIcon
                             : kNotificationCenterDoNotDisturbOffIcon);
  button_->SetToggled(in_quiet_mode);
  button_->SetSubLabel(l10n_util::GetStringUTF16(
      in_quiet_mode ? IDS_ASH_STATUS_TRAY_NOTIFICATIONS_DO_NOT_DISTURB_SUBLABEL
                    : IDS_ASH_STATUS_TRAY_NOTIFICATIONS_ON_SUBLABEL));
}

}  // namespace ash
