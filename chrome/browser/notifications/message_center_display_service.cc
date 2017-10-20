// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/message_center_display_service.h"

#include <memory>
#include <set>
#include <string>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_event_dispatcher.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_delegate.h"

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
                      NotificationCommon::Type notification_type)
      : profile_(profile),
        notification_(notification),
        notification_type_(notification_type) {}

  void Close(bool by_user) override {
    NotificationDisplayServiceFactory::GetForProfile(profile_)
        ->ProcessNotificationOperation(
            NotificationCommon::CLOSE, notification_type_,
            notification_.origin_url().possibly_invalid_spec(),
            notification_.id(), base::nullopt, base::nullopt, by_user);
  }

  void Click() override {
    NotificationDisplayServiceFactory::GetForProfile(profile_)
        ->ProcessNotificationOperation(
            NotificationCommon::CLICK, notification_type_,
            notification_.origin_url().possibly_invalid_spec(),
            notification_.id(), base::nullopt, base::nullopt, base::nullopt);
  }

  void ButtonClick(int button_index) override {
    NotificationDisplayServiceFactory::GetForProfile(profile_)
        ->ProcessNotificationOperation(
            NotificationCommon::CLICK, notification_type_,
            notification_.origin_url().possibly_invalid_spec(),
            notification_.id(), button_index, base::nullopt, base::nullopt);
  }

 protected:
  ~PassThroughDelegate() override = default;

 private:
  Profile* profile_;
  message_center::Notification notification_;
  NotificationCommon::Type notification_type_;

  DISALLOW_COPY_AND_ASSIGN(PassThroughDelegate);
};

}  // namespace

MessageCenterDisplayService::MessageCenterDisplayService(Profile* profile)
    : NotificationDisplayService(profile), profile_(profile) {}

MessageCenterDisplayService::~MessageCenterDisplayService() {}

void MessageCenterDisplayService::Display(
    NotificationCommon::Type notification_type,
    const std::string& notification_id,
    const message_center::Notification& notification,
    std::unique_ptr<NotificationCommon::Metadata> metadata) {
  // TODO(miguelg): MCDS should stop relying on the |notification|'s delegate
  // for Close/Click operations once the Notification object becomes a mojom
  // type.

  // This can be called when the browser is shutting down and the
  // NotificationUiManager has already destructed.
  NotificationUIManager* ui_manager =
      g_browser_process->notification_ui_manager();
  if (!ui_manager)
    return;

  NotificationHandler* handler = GetNotificationHandler(notification_type);
  handler->OnShow(profile_, notification_id);

  if (notification.delegate()) {
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

void MessageCenterDisplayService::Close(
    NotificationCommon::Type notification_type,
    const std::string& notification_id) {
  // This can be called when the browser is shutting down and the
  // NotificationUiManager has already destructed.
  NotificationUIManager* ui_manager =
      g_browser_process->notification_ui_manager();
  if (ui_manager) {
    ui_manager->CancelById(notification_id,
                           NotificationUIManager::GetProfileID(profile_));
  }
}

void MessageCenterDisplayService::GetDisplayed(
    const DisplayedNotificationsCallback& callback) {
  auto displayed_notifications = std::make_unique<std::set<std::string>>(
      g_browser_process->notification_ui_manager()->GetAllIdsByProfile(
          NotificationUIManager::GetProfileID(profile_)));

  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::BindOnce(callback, base::Passed(&displayed_notifications),
                     true /* supports_synchronization */));
}
