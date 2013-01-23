// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/message_center_notification_manager.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension_set.h"
#include "chrome/common/pref_names.h"

MessageCenterNotificationManager::MessageCenterNotificationManager(
    message_center::MessageCenter* message_center)
  : message_center_(message_center) {
  message_center_->SetDelegate(this);
}

MessageCenterNotificationManager::~MessageCenterNotificationManager() {
}

////////////////////////////////////////////////////////////////////////////////
// NotificationUIManager

bool MessageCenterNotificationManager::CancelById(const std::string& id) {
  // See if this ID hasn't been shown yet.
  if (NotificationUIManagerImpl::CancelById(id))
    return true;

  // If it has been shown, remove it.
  NotificationMap::iterator iter = profile_notifications_.find(id);
  if (iter == profile_notifications_.end())
    return false;

  RemoveProfileNotification((*iter).second);
  return true;
}

bool MessageCenterNotificationManager::CancelAllBySourceOrigin(
    const GURL& source) {
  // Same pattern as CancelById, but more complicated than the above
  // because there may be multiple notifications from the same source.
  bool removed = NotificationUIManagerImpl::CancelAllBySourceOrigin(source);

  for (NotificationMap::iterator loopiter = profile_notifications_.begin();
       loopiter != profile_notifications_.end(); ) {
    NotificationMap::iterator curiter = loopiter++;
    if ((*curiter).second->notification().origin_url() == source) {
      RemoveProfileNotification((*curiter).second);
      removed = true;
    }
  }
  return removed;
}

bool MessageCenterNotificationManager::CancelAllByProfile(Profile* profile) {
  // Same pattern as CancelAllBySourceOrigin.
  bool removed = NotificationUIManagerImpl::CancelAllByProfile(profile);

  for (NotificationMap::iterator loopiter = profile_notifications_.begin();
       loopiter != profile_notifications_.end(); ) {
    NotificationMap::iterator curiter = loopiter++;
    if ((*curiter).second->profile()->IsSameProfile(profile)) {
      RemoveProfileNotification((*curiter).second);
      removed = true;
    }
  }
  return removed;
}

void MessageCenterNotificationManager::CancelAll() {
  NotificationUIManagerImpl::CancelAll();

  for (NotificationMap::iterator loopiter = profile_notifications_.begin();
       loopiter != profile_notifications_.end(); ) {
    RemoveProfileNotification((*loopiter++).second);
  }
}

////////////////////////////////////////////////////////////////////////////////
// NotificationUIManagerImpl

bool MessageCenterNotificationManager::ShowNotification(
    const Notification& notification, Profile* profile) {
  // There is always space in MessageCenter, it never rejects Notifications.
  AddProfileNotification(new ProfileNotification(profile, notification));
  return true;
}

bool MessageCenterNotificationManager::UpdateNotification(
    const Notification& notification, Profile* profile) {
  const string16& replace_id = notification.replace_id();
  DCHECK(!replace_id.empty());
  const GURL origin_url = notification.origin_url();
  DCHECK(origin_url.is_valid());

  // Since replace_id is provided by arbitrary JS, we need to use origin_url
  // (which is an app url in case of app/extension) to scope the replace ids
  // in the given profile.
  for (NotificationMap::iterator iter = profile_notifications_.begin();
       iter != profile_notifications_.end(); ++iter) {
    ProfileNotification* old_notification = (*iter).second;
    if (old_notification->notification().replace_id() == replace_id &&
        old_notification->notification().origin_url() == origin_url &&
        old_notification->profile()->IsSameProfile(profile)) {
      std::string old_id =
          old_notification->notification().notification_id();
      DCHECK(message_center_->GetNotificationList()->HasNotification(old_id));

      // Add/remove notification in the local list but just update the same
      // one in MessageCenter.
      old_notification->notification().Close(false); // Not by user.
      delete old_notification;
      profile_notifications_.erase(old_id);
      profile_notifications_[notification.notification_id()] =
          new ProfileNotification(profile, notification);

      message_center_->UpdateNotification(old_id,
                                          notification.notification_id(),
                                          notification.title(),
                                          notification.body(),
                                          notification.optional_fields());
      notification.Display();
      return true;
    }
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// MessageCenter::Delegate

void MessageCenterNotificationManager::DisableExtension(
    const std::string& notification_id) {
  NOTIMPLEMENTED();
}

void MessageCenterNotificationManager::DisableNotificationsFromSource(
    const std::string& notification_id) {
  NOTIMPLEMENTED();
}

void MessageCenterNotificationManager::NotificationRemoved(
    const std::string& notification_id) {
  NOTIMPLEMENTED();
}

void MessageCenterNotificationManager::ShowSettings(
    const std::string& notification_id) {
  NOTIMPLEMENTED();
}

void MessageCenterNotificationManager::OnClicked(
    const std::string& notification_id) {
  NOTIMPLEMENTED();
}

void MessageCenterNotificationManager::OnButtonClicked(
    const std::string& notification_id, int button_index) {
  NOTIMPLEMENTED();
}

////////////////////////////////////////////////////////////////////////////////
// ProfileNotification

MessageCenterNotificationManager::ProfileNotification::ProfileNotification(
    Profile* profile, const Notification& notification)
  : profile_(profile),
    notification_(notification) {
  DCHECK(profile);
}

std::string
    MessageCenterNotificationManager::ProfileNotification::GetExtensionId() {
  const ExtensionURLInfo url(notification().origin_url());
  const ExtensionService* service = profile()->GetExtensionService();
  const extensions::Extension* extension =
      service->extensions()->GetExtensionOrAppByURL(url);
  return extension ? extension->id() : std::string();
}

////////////////////////////////////////////////////////////////////////////////
// private

void MessageCenterNotificationManager::AddProfileNotification(
    ProfileNotification* profile_notification) {
  const Notification& notification = profile_notification->notification();
  std::string id = notification.notification_id();
  // Notification ids should be unique.
  DCHECK(profile_notifications_.find(id) == profile_notifications_.end());
  profile_notifications_[id] = profile_notification;

  message_center_->AddNotification(notification.type(),
                                   notification.notification_id(),
                                   notification.title(),
                                   notification.body(),
                                   notification.display_source(),
                                   profile_notification->GetExtensionId(),
                                   notification.optional_fields());
  notification.Display();
}

void MessageCenterNotificationManager::RemoveProfileNotification(
    ProfileNotification* profile_notification) {
  profile_notification->notification().Close(false); // Not by user.
  std::string id = profile_notification->notification().notification_id();
  message_center_->RemoveNotification(id);
  profile_notifications_.erase(id);
  delete profile_notification;
}
