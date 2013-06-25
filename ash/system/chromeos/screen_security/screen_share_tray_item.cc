// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/screen_security/screen_share_tray_item.h"

#include "ash/shell.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {
namespace internal {

ScreenShareTrayItem::ScreenShareTrayItem(SystemTray* system_tray)
    : ScreenTrayItem(system_tray) {
  Shell::GetInstance()->system_tray_notifier()->
      AddScreenShareObserver(this);
}

ScreenShareTrayItem::~ScreenShareTrayItem() {
  Shell::GetInstance()->system_tray_notifier()->
      RemoveScreenShareObserver(this);
}

views::View* ScreenShareTrayItem::CreateTrayView(user::LoginStatus status) {
  set_tray_view(
      new tray::ScreenTrayView(this, IDR_AURA_UBER_TRAY_DISPLAY_LIGHT));
  return tray_view();
}

views::View* ScreenShareTrayItem::CreateDefaultView(user::LoginStatus status) {
  set_default_view(new tray::ScreenStatusView(
      this,
      tray::ScreenStatusView::VIEW_DEFAULT,
      IDR_AURA_UBER_TRAY_DISPLAY,
      l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_SCREEN_SHARE_BEING_HELPED),
      l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_SCREEN_SHARE_STOP)));
  return default_view();
}

views::View* ScreenShareTrayItem::CreateNotificationView(
    user::LoginStatus status) {
  base::string16 help_label_text;
  if (!helper_name_.empty()) {
    help_label_text = l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_SCREEN_SHARE_BEING_HELPED_NAME,
        helper_name_);
  } else {
    help_label_text = l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_SCREEN_SHARE_BEING_HELPED);
  }

  set_notification_view(new tray::ScreenNotificationView(
      this,
      IDR_AURA_UBER_TRAY_DISPLAY,
      help_label_text,
      l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_SCREEN_SHARE_STOP)));
  return notification_view();
}

void ScreenShareTrayItem::OnScreenShareStart(
    const base::Closure& stop_callback,
    const base::string16& helper_name) {
  helper_name_ = helper_name;
  Start(stop_callback);
}

void ScreenShareTrayItem::OnScreenShareStop() {
  // We do not need to run the stop callback
  // when screening is stopped externally.
  set_is_started(false);
  Update();
}

}  // namespace internal
}  // namespace ash
