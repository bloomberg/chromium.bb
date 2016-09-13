// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/message_center_display_service.h"

#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/profiles/profile.h"

MessageCenterDisplayService::MessageCenterDisplayService(
    Profile* profile,
    NotificationUIManager* ui_manager)
    : profile_(profile), ui_manager_(ui_manager) {}

MessageCenterDisplayService::~MessageCenterDisplayService() {}

void MessageCenterDisplayService::Display(
    NotificationCommon::Type notification_type,
    const std::string& notification_id,
    const Notification& notification) {
  // TODO(miguelg): MCDS should stop relying on the |notification|'s delegate
  // for Close/Click operations once the Notification object becomes a mojom
  // type.
  ui_manager_->Add(notification, profile_);
}

void MessageCenterDisplayService::Close(
    NotificationCommon::Type notification_type,
    const std::string& notification_id) {
  ui_manager_->CancelById(notification_id,
                          NotificationUIManager::GetProfileID(profile_));
}

bool MessageCenterDisplayService::GetDisplayed(
    std::set<std::string>* notifications) const {
  DCHECK(notifications);
  for (auto notification_id : ui_manager_->GetAllIdsByProfile(
           NotificationUIManager::GetProfileID(profile_))) {
    notifications->insert(notification_id);
  }
  return true;
}
