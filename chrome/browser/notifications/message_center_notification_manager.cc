// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/message_center_notification_manager.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/pref_names.h"

MessageCenterNotificationManager::MessageCenterNotificationManager() {
}

MessageCenterNotificationManager::~MessageCenterNotificationManager() {
}

bool MessageCenterNotificationManager::CancelById(const std::string& id) {
  // See if this ID hasn't been shown yet.
  if (NotificationUIManagerImpl::CancelById(id))
    return true;

  // If it has been shown, remove it.
  NOTIMPLEMENTED();
  return false;
}

bool MessageCenterNotificationManager::CancelAllBySourceOrigin(
    const GURL& source) {
  // Same pattern as CancelById, but more complicated than the above
  // because there may be multiple notifications from the same source.
  bool removed = NotificationUIManagerImpl::CancelAllBySourceOrigin(source);

  NOTIMPLEMENTED();
  return false || removed;
}

bool MessageCenterNotificationManager::CancelAllByProfile(Profile* profile) {
  // Same pattern as CancelAllBySourceOrigin.
  bool removed = NotificationUIManagerImpl::CancelAllByProfile(profile);

  NOTIMPLEMENTED();
  return false || removed;
}

void MessageCenterNotificationManager::CancelAll() {
  NotificationUIManagerImpl::CancelAll();
  NOTIMPLEMENTED();
}

bool MessageCenterNotificationManager::ShowNotification(
    const Notification& notification, Profile* profile) {
  NOTIMPLEMENTED();
  return true;
}

bool MessageCenterNotificationManager::UpdateNotification(
    const Notification& notification) {
  const string16& replace_id = notification.replace_id();
  DCHECK(!replace_id.empty());

  NOTIMPLEMENTED();
  return false;
}
