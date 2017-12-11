// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_platform_bridge_message_center.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification_display_service_impl.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "content/public/browser/browser_thread.h"
#include "ui/message_center/notification.h"

namespace {

// A NotificationDelegate that passes through click actions to the notification
// display service (and on to the appropriate handler). This is a temporary
// class to ease the transition from NotificationDelegate to
// NotificationHandler.
// TODO(estade): also handle other NotificationDelegate actions as needed.
class PassThroughDelegate : public message_center::NotificationDelegate {
 public:
  PassThroughDelegate(Profile* profile,
                      const message_center::Notification& notification,
                      NotificationHandler::Type notification_type)
      : profile_(profile),
        notification_(notification),
        notification_type_(notification_type) {
    DCHECK_NE(notification_type, NotificationHandler::Type::TRANSIENT);
  }

  void SettingsClick() override {
    NotificationDisplayServiceImpl::GetForProfile(profile_)
        ->ProcessNotificationOperation(
            NotificationCommon::SETTINGS, notification_type_,
            notification_.origin_url(), notification_.id(), base::nullopt,
            base::nullopt, base::nullopt /* by_user */);
  }

  void DisableNotification() override {
    NotificationDisplayServiceImpl::GetForProfile(profile_)
        ->ProcessNotificationOperation(
            NotificationCommon::DISABLE_PERMISSION, notification_type_,
            notification_.origin_url(), notification_.id(),
            base::nullopt /* action_index */, base::nullopt /* reply */,
            base::nullopt /* by_user */);
  }

  void Close(bool by_user) override {
    NotificationDisplayServiceImpl::GetForProfile(profile_)
        ->ProcessNotificationOperation(
            NotificationCommon::CLOSE, notification_type_,
            notification_.origin_url(), notification_.id(),
            base::nullopt /* action_index */, base::nullopt /* reply */,
            by_user);
  }

  void Click() override {
    NotificationDisplayServiceImpl::GetForProfile(profile_)
        ->ProcessNotificationOperation(
            NotificationCommon::CLICK, notification_type_,
            notification_.origin_url(), notification_.id(),
            base::nullopt /* action_index */, base::nullopt /* reply */,
            base::nullopt /* by_user */);
  }

  void ButtonClick(int action_index) override {
    NotificationDisplayServiceImpl::GetForProfile(profile_)
        ->ProcessNotificationOperation(
            NotificationCommon::CLICK, notification_type_,
            notification_.origin_url(), notification_.id(), action_index,
            base::nullopt /* reply */, base::nullopt /* by_user */);
  }

  void ButtonClickWithReply(int action_index,
                            const base::string16& reply) override {
    NotificationDisplayServiceImpl::GetForProfile(profile_)
        ->ProcessNotificationOperation(
            NotificationCommon::CLICK, notification_type_,
            notification_.origin_url(), notification_.id(), action_index, reply,
            base::nullopt /* by_user */);
  }

 protected:
  ~PassThroughDelegate() override = default;

 private:
  Profile* profile_;
  message_center::Notification notification_;
  NotificationHandler::Type notification_type_;

  DISALLOW_COPY_AND_ASSIGN(PassThroughDelegate);
};

}  // namespace

NotificationPlatformBridgeMessageCenter::
    NotificationPlatformBridgeMessageCenter(Profile* profile)
    : profile_(profile) {}

NotificationPlatformBridgeMessageCenter::
    ~NotificationPlatformBridgeMessageCenter() = default;

void NotificationPlatformBridgeMessageCenter::Display(
    NotificationHandler::Type notification_type,
    const std::string& /* profile_id */,
    bool /* is_incognito */,
    const message_center::Notification& notification,
    std::unique_ptr<NotificationCommon::Metadata> /* metadata */) {
  NotificationUIManager* ui_manager =
      g_browser_process->notification_ui_manager();
  if (!ui_manager)
    return;  // The process is shutting down.

  if (notification.delegate() ||
      notification_type == NotificationHandler::Type::TRANSIENT) {
    ui_manager->Add(notification, profile_);
    return;
  }

  // If there's no delegate, replace it with a PassThroughDelegate so clicks
  // go back to the appropriate handler.
  message_center::Notification notification_with_delegate(notification);
  notification_with_delegate.set_delegate(base::WrapRefCounted(
      new PassThroughDelegate(profile_, notification, notification_type)));
  ui_manager->Add(notification_with_delegate, profile_);
}

void NotificationPlatformBridgeMessageCenter::Close(
    const std::string& /* profile_id */,
    const std::string& notification_id) {
  NotificationUIManager* ui_manager =
      g_browser_process->notification_ui_manager();
  if (!ui_manager)
    return;  // the process is shutting down

  ui_manager->CancelById(notification_id,
                         NotificationUIManager::GetProfileID(profile_));
}

void NotificationPlatformBridgeMessageCenter::GetDisplayed(
    const std::string& /* profile_id */,
    bool /* incognito */,
    const GetDisplayedNotificationsCallback& callback) const {
  auto displayed_notifications = std::make_unique<std::set<std::string>>(
      g_browser_process->notification_ui_manager()->GetAllIdsByProfile(
          NotificationUIManager::GetProfileID(profile_)));

  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::BindOnce(callback, base::Passed(&displayed_notifications),
                     true /* supports_synchronization */));
}

void NotificationPlatformBridgeMessageCenter::SetReadyCallback(
    NotificationBridgeReadyCallback callback) {
  std::move(callback).Run(true /* success */);
}
