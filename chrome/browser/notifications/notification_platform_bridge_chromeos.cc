// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_platform_bridge_chromeos.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/notifications/chrome_ash_message_center_client.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/app_icon_loader.h"
#include "ui/gfx/image/image.h"

namespace {

// TODO(estade): remove this function. NotificationPlatformBridge should either
// get Profile* pointers or, longer term, all profile management should be moved
// up a layer to NativeNotificationDisplayService.
Profile* GetProfileFromId(const std::string& profile_id, bool incognito) {
  ProfileManager* manager = g_browser_process->profile_manager();
  Profile* profile =
      manager->GetProfile(manager->user_data_dir().AppendASCII(profile_id));
  return incognito ? profile->GetOffTheRecordProfile() : profile;
}

}  // namespace

// static
NotificationDisplayService*
NotificationDisplayService::GetForSystemNotifications() {
  // System notifications (such as those for network state) aren't tied to a
  // particular user and can show up before any user is logged in, so fall back
  // to the signin profile, which is guaranteed to exist.
  return NotificationDisplayService::GetForProfile(
      chromeos::ProfileHelper::GetSigninProfile());
}

// static
NotificationPlatformBridge* NotificationPlatformBridge::Create() {
  return new NotificationPlatformBridgeChromeOs();
}

// static
bool NotificationPlatformBridge::CanHandleType(
    NotificationHandler::Type notification_type) {
  return notification_type != NotificationHandler::Type::TRANSIENT;
}

NotificationPlatformBridgeChromeOs::NotificationPlatformBridgeChromeOs()
    : impl_(std::make_unique<ChromeAshMessageCenterClient>(this)) {}

NotificationPlatformBridgeChromeOs::~NotificationPlatformBridgeChromeOs() {}

void NotificationPlatformBridgeChromeOs::Display(
    NotificationHandler::Type notification_type,
    const std::string& profile_id,
    bool is_incognito,
    const message_center::Notification& notification,
    std::unique_ptr<NotificationCommon::Metadata> metadata) {
  auto active_notification = std::make_unique<ProfileNotification>(
      GetProfileFromId(profile_id, is_incognito), notification,
      notification_type);
  impl_->Display(NotificationHandler::Type::MAX, std::string(), false,
                 active_notification->notification(), std::move(metadata));

  std::string profile_notification_id =
      active_notification->notification().id();
  active_notifications_.emplace(profile_notification_id,
                                std::move(active_notification));
}

void NotificationPlatformBridgeChromeOs::Close(
    const std::string& profile_id,
    const std::string& notification_id) {
  impl_->Close(profile_id, notification_id);
}

void NotificationPlatformBridgeChromeOs::GetDisplayed(
    const std::string& profile_id,
    bool incognito,
    const GetDisplayedNotificationsCallback& callback) const {
  impl_->GetDisplayed(profile_id, incognito, callback);
}

void NotificationPlatformBridgeChromeOs::SetReadyCallback(
    NotificationBridgeReadyCallback callback) {
  impl_->SetReadyCallback(std::move(callback));
}

void NotificationPlatformBridgeChromeOs::HandleNotificationClosed(
    const std::string& id,
    bool by_user) {
  auto iter = active_notifications_.find(id);
  DCHECK(iter != active_notifications_.end());
  ProfileNotification* notification = iter->second.get();
  NotificationDisplayServiceFactory::GetForProfile(notification->profile())
      ->ProcessNotificationOperation(
          NotificationCommon::CLOSE, notification->type(),
          notification->notification().origin_url(),
          notification->original_id(), base::nullopt, base::nullopt, by_user);
  active_notifications_.erase(iter);
}

void NotificationPlatformBridgeChromeOs::HandleNotificationClicked(
    const std::string& id) {
  ProfileNotification* notification = GetProfileNotification(id);
  NotificationDisplayServiceFactory::GetForProfile(notification->profile())
      ->ProcessNotificationOperation(NotificationCommon::CLICK,
                                     notification->type(),
                                     notification->notification().origin_url(),
                                     notification->original_id(), base::nullopt,
                                     base::nullopt, base::nullopt);
}

void NotificationPlatformBridgeChromeOs::HandleNotificationButtonClicked(
    const std::string& id,
    int button_index) {
  ProfileNotification* notification = GetProfileNotification(id);
  NotificationDisplayServiceFactory::GetForProfile(notification->profile())
      ->ProcessNotificationOperation(NotificationCommon::CLICK,
                                     notification->type(),
                                     notification->notification().origin_url(),
                                     notification->original_id(), button_index,
                                     base::nullopt, base::nullopt);
}

ProfileNotification* NotificationPlatformBridgeChromeOs::GetProfileNotification(
    const std::string& profile_notification_id) {
  auto iter = active_notifications_.find(profile_notification_id);
  DCHECK(iter != active_notifications_.end());
  return iter->second.get();
}
