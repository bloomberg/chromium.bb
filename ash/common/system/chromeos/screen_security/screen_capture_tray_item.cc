// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/screen_security/screen_capture_tray_item.h"

#include <utility>

#include "ash/common/metrics/user_metrics_action.h"
#include "ash/common/system/system_notifier.h"
#include "ash/common/system/tray/system_tray_notifier.h"
#include "ash/common/wm_shell.h"
#include "ash/resources/grit/ash_resources.h"
#include "ash/strings/grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"

using message_center::Notification;

namespace ash {
namespace {

const char kScreenCaptureNotificationId[] = "chrome://screen/capture";

}  // namespace

ScreenCaptureTrayItem::ScreenCaptureTrayItem(SystemTray* system_tray)
    : ScreenTrayItem(system_tray, UMA_SCREEN_CAPTURE) {
  WmShell::Get()->AddShellObserver(this);
  WmShell::Get()->system_tray_notifier()->AddScreenCaptureObserver(this);
}

ScreenCaptureTrayItem::~ScreenCaptureTrayItem() {
  WmShell::Get()->RemoveShellObserver(this);
  WmShell::Get()->system_tray_notifier()->RemoveScreenCaptureObserver(this);
}

views::View* ScreenCaptureTrayItem::CreateDefaultView(LoginStatus status) {
  set_default_view(new tray::ScreenStatusView(
      this, screen_capture_status_,
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_SCREEN_CAPTURE_STOP)));
  return default_view();
}

void ScreenCaptureTrayItem::CreateOrUpdateNotification() {
  message_center::RichNotificationData data;
  data.buttons.push_back(message_center::ButtonInfo(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_SCREEN_CAPTURE_STOP)));
  ui::ResourceBundle& resource_bundle = ui::ResourceBundle::GetSharedInstance();
  std::unique_ptr<Notification> notification(new Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, kScreenCaptureNotificationId,
      screen_capture_status_, base::string16() /* body is blank */,
      resource_bundle.GetImageNamed(IDR_AURA_UBER_TRAY_SCREENSHARE_DARK),
      base::string16() /* display_source */, GURL(),
      message_center::NotifierId(message_center::NotifierId::SYSTEM_COMPONENT,
                                 system_notifier::kNotifierScreenCapture),
      data, new tray::ScreenNotificationDelegate(this)));
  notification->SetSystemPriority();
  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));
}

std::string ScreenCaptureTrayItem::GetNotificationId() {
  return kScreenCaptureNotificationId;
}

void ScreenCaptureTrayItem::RecordStoppedFromDefaultViewMetric() {
  WmShell::Get()->RecordUserMetricsAction(
      UMA_STATUS_AREA_SCREEN_CAPTURE_DEFAULT_STOP);
}

void ScreenCaptureTrayItem::RecordStoppedFromNotificationViewMetric() {
  WmShell::Get()->RecordUserMetricsAction(
      UMA_STATUS_AREA_SCREEN_CAPTURE_NOTIFICATION_STOP);
}

void ScreenCaptureTrayItem::OnScreenCaptureStart(
    const base::Closure& stop_callback,
    const base::string16& screen_capture_status) {
  screen_capture_status_ = screen_capture_status;

  // We do not want to show the screen capture tray item and the chromecast
  // casting tray item at the same time. We will hide this tray item.
  //
  // This suppression technique is currently dependent on the order
  // that OnScreenCaptureStart and OnCastingSessionStartedOrStopped
  // get invoked. OnCastingSessionStartedOrStopped currently gets
  // called first.
  if (is_casting_)
    return;

  Start(stop_callback);
}

void ScreenCaptureTrayItem::OnScreenCaptureStop() {
  // We do not need to run the stop callback when
  // screen capture is stopped externally.
  set_is_started(false);
  Update();
}

void ScreenCaptureTrayItem::OnCastingSessionStartedOrStopped(bool started) {
  is_casting_ = started;
}

}  // namespace ash
