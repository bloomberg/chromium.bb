// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/screen_security/screen_security_notification_controller.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

using message_center::MessageCenter;
using message_center::Notification;

namespace ash {

// It is possible that we are capturing and sharing screen at the same time, so
// we cannot share the notification IDs for capturing and sharing.
const char kScreenCaptureNotificationId[] = "chrome://screen/capture";
const char kScreenShareNotificationId[] = "chrome://screen/share";
const char kNotifierScreenCapture[] = "ash.screen-capture";
const char kNotifierScreenShare[] = "ash.screen-share";

namespace {

class ScreenSecurityNotificationDelegate
    : public message_center::NotificationDelegate {
 public:
  ScreenSecurityNotificationDelegate(base::OnceClosure stop_callback,
                                     bool is_capture);

  // message_center::NotificationDelegate overrides:
  void ButtonClick(int button_index) override;

 protected:
  ~ScreenSecurityNotificationDelegate() override;

 private:
  base::OnceClosure stop_callback_;
  const bool is_capture_;

  DISALLOW_COPY_AND_ASSIGN(ScreenSecurityNotificationDelegate);
};

ScreenSecurityNotificationDelegate::ScreenSecurityNotificationDelegate(
    base::OnceClosure stop_callback,
    bool is_capture)
    : stop_callback_(std::move(stop_callback)), is_capture_(is_capture) {}

ScreenSecurityNotificationDelegate::~ScreenSecurityNotificationDelegate() =
    default;

void ScreenSecurityNotificationDelegate::ButtonClick(int button_index) {
  DCHECK_EQ(0, button_index);
  if (stop_callback_.is_null())
    return;
  std::move(stop_callback_).Run();
  if (is_capture_) {
    Shell::Get()->metrics()->RecordUserMetricsAction(
        UMA_STATUS_AREA_SCREEN_CAPTURE_NOTIFICATION_STOP);
  }
}

void CreateNotification(base::OnceClosure stop_callback,
                        const base::string16& message,
                        bool is_capture) {
  message_center::RichNotificationData data;
  data.buttons.push_back(message_center::ButtonInfo(l10n_util::GetStringUTF16(
      is_capture ? IDS_ASH_STATUS_TRAY_SCREEN_CAPTURE_STOP
                 : IDS_ASH_STATUS_TRAY_SCREEN_SHARE_STOP)));
  std::unique_ptr<Notification> notification =
      Notification::CreateSystemNotification(
          message_center::NOTIFICATION_TYPE_SIMPLE,
          is_capture ? kScreenCaptureNotificationId
                     : kScreenShareNotificationId,
          l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_SCREEN_SHARE_TITLE),
          message, gfx::Image(), base::string16() /* display_source */, GURL(),
          message_center::NotifierId(
              message_center::NotifierId::SYSTEM_COMPONENT,
              is_capture ? kNotifierScreenCapture : kNotifierScreenShare),
          data,
          new ScreenSecurityNotificationDelegate(std::move(stop_callback),
                                                 is_capture),
          kNotificationScreenshareIcon,
          message_center::SystemNotificationWarningLevel::NORMAL);
  notification->SetSystemPriority();
  if (features::IsSystemTrayUnifiedEnabled())
    notification->set_pinned(true);
  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));
}

}  // namespace

ScreenSecurityNotificationController::ScreenSecurityNotificationController() {
  Shell::Get()->AddShellObserver(this);
  Shell::Get()->system_tray_notifier()->AddScreenCaptureObserver(this);
  Shell::Get()->system_tray_notifier()->AddScreenShareObserver(this);
}

ScreenSecurityNotificationController::~ScreenSecurityNotificationController() {
  Shell::Get()->system_tray_notifier()->RemoveScreenShareObserver(this);
  Shell::Get()->system_tray_notifier()->RemoveScreenCaptureObserver(this);
  Shell::Get()->RemoveShellObserver(this);
}

void ScreenSecurityNotificationController::OnScreenCaptureStart(
    const base::Closure& stop_callback,
    const base::string16& screen_capture_status) {
  // We do not want to show the screen capture notification and the chromecast
  // casting tray notification at the same time.
  //
  // This suppression technique is currently dependent on the order
  // that OnScreenCaptureStart and OnCastingSessionStartedOrStopped
  // get invoked. OnCastingSessionStartedOrStopped currently gets
  // called first.
  if (is_casting_)
    return;

  CreateNotification(stop_callback, screen_capture_status,
                     true /* is_capture */);
}

void ScreenSecurityNotificationController::OnScreenCaptureStop() {
  message_center::MessageCenter::Get()->RemoveNotification(
      kScreenCaptureNotificationId, false /* by_user */);
}

void ScreenSecurityNotificationController::OnScreenShareStart(
    const base::Closure& stop_callback,
    const base::string16& helper_name) {
  base::string16 help_label_text;
  if (!helper_name.empty()) {
    help_label_text = l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_SCREEN_SHARE_BEING_HELPED_NAME, helper_name);
  } else {
    help_label_text = l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_SCREEN_SHARE_BEING_HELPED);
  }

  CreateNotification(stop_callback, help_label_text, false /* is_capture */);
}

void ScreenSecurityNotificationController::OnScreenShareStop() {
  message_center::MessageCenter::Get()->RemoveNotification(
      kScreenShareNotificationId, false /* by_user */);
}

void ScreenSecurityNotificationController::OnCastingSessionStartedOrStopped(
    bool started) {
  is_casting_ = started;
}

}  // namespace ash
