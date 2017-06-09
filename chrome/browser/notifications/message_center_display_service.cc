// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>

#include "chrome/browser/notifications/message_center_display_service.h"

#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_event_dispatcher.h"

MessageCenterDisplayService::MessageCenterDisplayService(
    Profile* profile,
    NotificationUIManager* ui_manager)
    : NotificationDisplayService(profile),
      profile_(profile),
      ui_manager_(ui_manager) {}

MessageCenterDisplayService::~MessageCenterDisplayService() {}

void MessageCenterDisplayService::Display(
    NotificationCommon::Type notification_type,
    const std::string& notification_id,
    const Notification& notification) {
  // TODO(miguelg): MCDS should stop relying on the |notification|'s delegate
  // for Close/Click operations once the Notification object becomes a mojom
  // type.

  NotificationHandler* handler = GetNotificationHandler(notification_type);
  handler->OnShow(profile_, notification_id);
  ui_manager_->Add(notification, profile_);
}

void MessageCenterDisplayService::Close(
    NotificationCommon::Type notification_type,
    const std::string& notification_id) {
  ui_manager_->CancelById(notification_id,
                          NotificationUIManager::GetProfileID(profile_));
}

void MessageCenterDisplayService::GetDisplayed(
    const DisplayedNotificationsCallback& callback) {
  auto displayed_notifications =
      base::MakeUnique<std::set<std::string>>(ui_manager_->GetAllIdsByProfile(
          NotificationUIManager::GetProfileID(profile_)));

  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::BindOnce(callback, base::Passed(&displayed_notifications),
                     true /* supports_synchronization */));
}
