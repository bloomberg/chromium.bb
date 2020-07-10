// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_platform_bridge_chromeos.h"

#include <memory>

#include "base/callback_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/notifications/chrome_ash_message_center_client.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/notification_display_service_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/app_icon_loader.h"
#include "ui/gfx/image/image.h"

// static
std::unique_ptr<NotificationPlatformBridge>
NotificationPlatformBridge::Create() {
  return std::make_unique<NotificationPlatformBridgeChromeOs>();
}

// static
bool NotificationPlatformBridge::CanHandleType(
    NotificationHandler::Type notification_type) {
  return true;
}

NotificationPlatformBridgeChromeOs::NotificationPlatformBridgeChromeOs()
    : impl_(std::make_unique<ChromeAshMessageCenterClient>(this)) {}

NotificationPlatformBridgeChromeOs::~NotificationPlatformBridgeChromeOs() {}

void NotificationPlatformBridgeChromeOs::Display(
    NotificationHandler::Type notification_type,
    Profile* profile,
    const message_center::Notification& notification,
    std::unique_ptr<NotificationCommon::Metadata> metadata) {
  auto active_notification = std::make_unique<ProfileNotification>(
      profile, notification, notification_type);
  impl_->Display(active_notification->notification());

  std::string profile_notification_id =
      active_notification->notification().id();
  active_notifications_.erase(profile_notification_id);
  active_notifications_.emplace(profile_notification_id,
                                std::move(active_notification));
}

void NotificationPlatformBridgeChromeOs::Close(
    Profile* profile,
    const std::string& notification_id) {
  const std::string profile_notification_id =
      ProfileNotification::GetProfileNotificationId(
          notification_id, NotificationUIManager::GetProfileID(profile));

  impl_->Close(profile_notification_id);
}

void NotificationPlatformBridgeChromeOs::GetDisplayed(
    Profile* profile,
    GetDisplayedNotificationsCallback callback) const {
  // Right now, this is only used to get web notifications that were created by
  // and have outlived a previous browser process. Ash itself doesn't outlive
  // the browser process, so there's no need to implement.
  std::set<std::string> displayed_notifications;
  std::move(callback).Run(std::move(displayed_notifications), false);
}

void NotificationPlatformBridgeChromeOs::SetReadyCallback(
    NotificationBridgeReadyCallback callback) {
  // We don't handle the absence of Ash or a failure to open a Mojo connection,
  // so just assume the client is ready.
  std::move(callback).Run(true);
}

void NotificationPlatformBridgeChromeOs::DisplayServiceShutDown(
    Profile* profile) {
  // Notify delegates/handlers of the service shutdown and remove the
  // notifications associated with the profile whose service is being destroyed.
  // Otherwise Ash might asynchronously notify the bridge of operations on a
  // notification associated with a profile that has already been destroyed).
  std::list<std::string> ids_to_close;
  for (const auto& iter : active_notifications_) {
    if (iter.second->profile() == profile)
      ids_to_close.push_back(iter.second->notification().id());
  }

  for (auto id : ids_to_close)
    HandleNotificationClosed(id, false);
}

void NotificationPlatformBridgeChromeOs::HandleNotificationClosed(
    const std::string& id,
    bool by_user) {
  auto iter = active_notifications_.find(id);
  if (iter == active_notifications_.end())
    return;
  ProfileNotification* notification = iter->second.get();

  if (notification->type() == NotificationHandler::Type::TRANSIENT) {
    notification->notification().delegate()->Close(by_user);
  } else {
    NotificationDisplayServiceImpl::GetForProfile(notification->profile())
        ->ProcessNotificationOperation(
            NotificationCommon::OPERATION_CLOSE, notification->type(),
            notification->notification().origin_url(),
            notification->original_id(), base::nullopt, base::nullopt, by_user);
  }
  active_notifications_.erase(iter);
}

void NotificationPlatformBridgeChromeOs::HandleNotificationClicked(
    const std::string& id) {
  ProfileNotification* notification = GetProfileNotification(id);
  if (!notification)
    return;

  if (notification->type() == NotificationHandler::Type::TRANSIENT) {
    notification->notification().delegate()->Click(base::nullopt,
                                                   base::nullopt);
  } else {
    NotificationDisplayServiceImpl::GetForProfile(notification->profile())
        ->ProcessNotificationOperation(
            NotificationCommon::OPERATION_CLICK, notification->type(),
            notification->notification().origin_url(),
            notification->original_id(), base::nullopt, base::nullopt,
            base::nullopt);
  }
}

void NotificationPlatformBridgeChromeOs::HandleNotificationButtonClicked(
    const std::string& id,
    int button_index,
    const base::Optional<base::string16>& reply) {
  ProfileNotification* notification = GetProfileNotification(id);
  if (!notification)
    return;

  if (notification->type() == NotificationHandler::Type::TRANSIENT) {
    notification->notification().delegate()->Click(button_index, reply);
  } else {
    NotificationDisplayServiceImpl::GetForProfile(notification->profile())
        ->ProcessNotificationOperation(
            NotificationCommon::OPERATION_CLICK, notification->type(),
            notification->notification().origin_url(),
            notification->original_id(), button_index, reply, base::nullopt);
  }
}

void NotificationPlatformBridgeChromeOs::
    HandleNotificationSettingsButtonClicked(const std::string& id) {
  ProfileNotification* notification = GetProfileNotification(id);
  if (!notification)
    return;

  if (notification->type() == NotificationHandler::Type::TRANSIENT) {
    notification->notification().delegate()->SettingsClick();
  } else {
    NotificationDisplayServiceImpl::GetForProfile(notification->profile())
        ->ProcessNotificationOperation(
            NotificationCommon::OPERATION_SETTINGS, notification->type(),
            notification->notification().origin_url(),
            notification->original_id(), base::nullopt, base::nullopt,
            base::nullopt);
  }
}

void NotificationPlatformBridgeChromeOs::DisableNotification(
    const std::string& id) {
  ProfileNotification* notification = GetProfileNotification(id);
  if (!notification)
    return;

  DCHECK_NE(NotificationHandler::Type::TRANSIENT, notification->type());
  NotificationDisplayServiceImpl::GetForProfile(notification->profile())
      ->ProcessNotificationOperation(
          NotificationCommon::OPERATION_DISABLE_PERMISSION,
          notification->type(), notification->notification().origin_url(),
          notification->original_id(), base::nullopt, base::nullopt,
          base::nullopt);
}

ProfileNotification* NotificationPlatformBridgeChromeOs::GetProfileNotification(
    const std::string& profile_notification_id) {
  auto iter = active_notifications_.find(profile_notification_id);
  if (iter == active_notifications_.end())
    return nullptr;
  return iter->second.get();
}
