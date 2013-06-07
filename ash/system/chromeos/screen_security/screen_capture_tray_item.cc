// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/screen_security/screen_capture_tray_item.h"

#include "ash/shell.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {
namespace internal {

ScreenCaptureTrayItem::ScreenCaptureTrayItem(SystemTray* system_tray)
    : ScreenTrayItem(system_tray) {
  Shell::GetInstance()->system_tray_notifier()->
      AddScreenCaptureObserver(this);
}

ScreenCaptureTrayItem::~ScreenCaptureTrayItem() {
  Shell::GetInstance()->system_tray_notifier()->
      RemoveScreenCaptureObserver(this);
}

views::View* ScreenCaptureTrayItem::CreateTrayView(user::LoginStatus status) {
  set_tray_view(
      new tray::ScreenTrayView(this, IDR_AURA_UBER_TRAY_DISPLAY_LIGHT));
  return tray_view();
}

views::View* ScreenCaptureTrayItem::CreateDefaultView(
    user::LoginStatus status) {
  set_default_view(new tray::ScreenStatusView(
      this,
      tray::ScreenStatusView::VIEW_DEFAULT,
      IDR_AURA_UBER_TRAY_DISPLAY,
      screen_capture_status_,
      l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_SCREEN_CAPTURE_STOP)));
  return default_view();
}

views::View* ScreenCaptureTrayItem::CreateNotificationView(
    user::LoginStatus status) {
  set_notification_view(new tray::ScreenNotificationView(
      this,
      IDR_AURA_UBER_TRAY_DISPLAY,
      screen_capture_status_,
      l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_SCREEN_CAPTURE_STOP)));
  return notification_view();
}

void ScreenCaptureTrayItem::OnScreenCaptureStart(
    const base::Closure& stop_callback,
    const base::string16& screen_capture_status) {
  screen_capture_status_ = screen_capture_status;
  Start(stop_callback);
}

void ScreenCaptureTrayItem::OnScreenCaptureStop() {
  // We do not need to run the stop callback when
  // screen capture is stopped externally.
  set_is_started(false);
  Update();
}

}  // namespace internal
}  // namespace ash
