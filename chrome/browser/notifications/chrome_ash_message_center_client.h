// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_CHROME_ASH_MESSAGE_CENTER_CLIENT_H_
#define CHROME_BROWSER_NOTIFICATIONS_CHROME_ASH_MESSAGE_CENTER_CLIENT_H_

#include "ash/public/interfaces/ash_message_center_controller.mojom.h"
#include "chrome/browser/notifications/notification_platform_bridge.h"
#include "chrome/browser/notifications/notification_platform_bridge_chromeos.h"
#include "chrome/browser/notifications/notifier_controller.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "ui/message_center/public/cpp/notifier_id.h"

// This class serves as Chrome's AshMessageCenterClient, as well as the
// NotificationPlatformBridge for ChromeOS. It dispatches notifications to Ash
// and handles interactions with those notifications, plus it keeps track of
// NotifierControllers to provide notifier settings information to Ash (visible
// in NotifierSettingsView).
class ChromeAshMessageCenterClient : public NotificationPlatformBridge,
                                     public ash::mojom::AshMessageCenterClient,
                                     public NotifierController::Observer {
 public:
  explicit ChromeAshMessageCenterClient(
      NotificationPlatformBridgeDelegate* delegate);

  ~ChromeAshMessageCenterClient() override;

  // NotificationPlatformBridge:
  void Display(NotificationHandler::Type notification_type,
               const std::string& profile_id,
               bool is_incognito,
               const message_center::Notification& notification,
               std::unique_ptr<NotificationCommon::Metadata> metadata) override;
  void Close(const std::string& profile_id,
             const std::string& notification_id) override;
  void GetDisplayed(const std::string& profile_id,
                    bool incognito,
                    GetDisplayedNotificationsCallback callback) const override;
  void SetReadyCallback(NotificationBridgeReadyCallback callback) override;

  // ash::mojom::AshMessageCenterClient:
  void HandleNotificationClosed(const std::string& id, bool by_user) override;
  void HandleNotificationClicked(const std::string& id) override;
  void HandleNotificationButtonClicked(
      const std::string& id,
      int button_index,
      const base::Optional<base::string16>& reply) override;
  void HandleNotificationSettingsButtonClicked(const std::string& id) override;
  void DisableNotification(const std::string& id) override;
  void SetNotifierEnabled(const message_center::NotifierId& notifier_id,
                          bool enabled) override;
  void GetNotifierList(GetNotifierListCallback callback) override;

  // NotifierController::Observer:
  void OnIconImageUpdated(const message_center::NotifierId& notifier_id,
                          const gfx::ImageSkia& icon) override;
  void OnNotifierEnabledChanged(const message_center::NotifierId& notifier_id,
                                bool enabled) override;

 private:
  NotificationPlatformBridgeDelegate* delegate_;

  // Notifier source for each notifier type.
  std::map<message_center::NotifierId::NotifierType,
           std::unique_ptr<NotifierController>>
      sources_;

  ash::mojom::AshMessageCenterControllerPtr controller_;
  mojo::AssociatedBinding<ash::mojom::AshMessageCenterClient> binding_;

  DISALLOW_COPY_AND_ASSIGN(ChromeAshMessageCenterClient);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_CHROME_ASH_MESSAGE_CENTER_CLIENT_H_
