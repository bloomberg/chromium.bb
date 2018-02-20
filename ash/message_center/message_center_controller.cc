// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/message_center/message_center_controller.h"

#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/command_line.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/message_center/public/cpp/notifier_id.h"

using message_center::MessageCenter;
using message_center::NotifierId;

namespace ash {

namespace {

// A notification blocker that unconditionally blocks toasts. Implements
// --suppress-message-center-notifications.
class PopupNotificationBlocker : public message_center::NotificationBlocker {
 public:
  PopupNotificationBlocker(MessageCenter* message_center)
      : NotificationBlocker(message_center) {}
  ~PopupNotificationBlocker() override = default;

  bool ShouldShowNotificationAsPopup(
      const message_center::Notification& notification) const override {
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PopupNotificationBlocker);
};

// This notification delegate passes actions back to the client that asked for
// the notification (Chrome).
class AshClientNotificationDelegate
    : public message_center::NotificationDelegate {
 public:
  AshClientNotificationDelegate(const std::string& notification_id,
                                mojom::AshMessageCenterClient* client)
      : notification_id_(notification_id), client_(client) {}

  void Close(bool by_user) override {
    client_->HandleNotificationClosed(notification_id_, by_user);
  }

  void Click() override {
    client_->HandleNotificationClicked(notification_id_);
  }

  void ButtonClick(int button_index) override {
    client_->HandleNotificationButtonClicked(notification_id_, button_index,
                                             base::nullopt);
  }

  void ButtonClickWithReply(int button_index,
                            const base::string16& reply) override {
    client_->HandleNotificationButtonClicked(notification_id_, button_index,
                                             reply);
  }

  void SettingsClick() override {
    client_->HandleNotificationSettingsButtonClicked(notification_id_);
  }

  void DisableNotification() override {
    client_->DisableNotification(notification_id_);
  }

 private:
  ~AshClientNotificationDelegate() override = default;

  std::string notification_id_;
  mojom::AshMessageCenterClient* client_;

  DISALLOW_COPY_AND_ASSIGN(AshClientNotificationDelegate);
};

}  // namespace

MessageCenterController::MessageCenterController()
    : fullscreen_notification_blocker_(MessageCenter::Get()),
      inactive_user_notification_blocker_(MessageCenter::Get()),
      session_state_notification_blocker_(MessageCenter::Get()),
      binding_(this) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSuppressMessageCenterPopups)) {
    all_popup_blocker_ =
        std::make_unique<PopupNotificationBlocker>(MessageCenter::Get());
  }

  message_center::RegisterVectorIcon(kNotificationCaptivePortalIcon);
  message_center::RegisterVectorIcon(kNotificationCellularAlertIcon);
  message_center::RegisterVectorIcon(kNotificationDownloadIcon);
  message_center::RegisterVectorIcon(kNotificationEndOfSupportIcon);
  message_center::RegisterVectorIcon(kNotificationGoogleIcon);
  message_center::RegisterVectorIcon(kNotificationImageIcon);
  message_center::RegisterVectorIcon(kNotificationInstalledIcon);
  message_center::RegisterVectorIcon(kNotificationMobileDataIcon);
  message_center::RegisterVectorIcon(kNotificationMobileDataOffIcon);
  message_center::RegisterVectorIcon(kNotificationPlayPrismIcon);
  message_center::RegisterVectorIcon(kNotificationPrintingDoneIcon);
  message_center::RegisterVectorIcon(kNotificationPrintingIcon);
  message_center::RegisterVectorIcon(kNotificationPrintingWarningIcon);
  message_center::RegisterVectorIcon(kNotificationStorageFullIcon);
  message_center::RegisterVectorIcon(kNotificationVpnIcon);
  message_center::RegisterVectorIcon(kNotificationWarningIcon);
  message_center::RegisterVectorIcon(kNotificationWifiOffIcon);

  // Set the system notification source display name ("Chrome OS" or "Chromium
  // OS").
  message_center::MessageCenter::Get()->SetSystemNotificationAppName(
      l10n_util::GetStringUTF16(IDS_ASH_MESSAGE_CENTER_SYSTEM_APP_NAME));
}

MessageCenterController::~MessageCenterController() = default;

void MessageCenterController::BindRequest(
    mojom::AshMessageCenterControllerRequest request) {
  binding_.Bind(std::move(request));
}

void MessageCenterController::SetNotifierEnabled(const NotifierId& notifier_id,
                                                 bool enabled) {
  client_->SetNotifierEnabled(notifier_id, enabled);
}

void MessageCenterController::SetClient(
    mojom::AshMessageCenterClientAssociatedPtrInfo client) {
  DCHECK(!client_.is_bound());
  client_.Bind(std::move(client));
}

void MessageCenterController::ShowClientNotification(
    const message_center::Notification& notification) {
  DCHECK(client_.is_bound());
  auto message_center_notification =
      std::make_unique<message_center::Notification>(notification);
  message_center_notification->set_delegate(base::WrapRefCounted(
      new AshClientNotificationDelegate(notification.id(), client_.get())));
  MessageCenter::Get()->AddNotification(std::move(message_center_notification));
}

void MessageCenterController::CloseClientNotification(const std::string& id) {
  MessageCenter::Get()->RemoveNotification(id, false /* by_user */);
}

void MessageCenterController::UpdateNotifierIcon(const NotifierId& notifier_id,
                                                 const gfx::ImageSkia& icon) {
  if (notifier_id_)
    notifier_id_->UpdateNotifierIcon(notifier_id, icon);
}

void MessageCenterController::NotifierEnabledChanged(
    const NotifierId& notifier_id,
    bool enabled) {
  if (!enabled)
    MessageCenter::Get()->RemoveNotificationsForNotifierId(notifier_id);
}

void MessageCenterController::SetNotifierSettingsListener(
    NotifierSettingsListener* listener) {
  DCHECK(!listener || !notifier_id_);
  notifier_id_ = listener;

  // |client_| may not be bound in unit tests.
  if (listener && client_.is_bound()) {
    client_->GetNotifierList(base::BindOnce(
        &MessageCenterController::OnGotNotifierList, base::Unretained(this)));
  }
}

void MessageCenterController::OnGotNotifierList(
    std::vector<mojom::NotifierUiDataPtr> ui_data) {
  if (notifier_id_)
    notifier_id_->SetNotifierList(ui_data);
}

}  // namespace ash
