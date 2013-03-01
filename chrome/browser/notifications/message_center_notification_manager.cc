// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/message_center_notification_manager.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/desktop_notification_service_factory.h"
#include "chrome/browser/notifications/message_center_settings_controller.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/common/extensions/extension_set.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/web_contents.h"
#include "ui/message_center/message_center_constants.h"
#include "ui/message_center/message_center_tray.h"

MessageCenterNotificationManager::MessageCenterNotificationManager(
    message_center::MessageCenter* message_center)
  : message_center_(message_center),
    settings_controller_(new MessageCenterSettingsController) {
  message_center_->SetDelegate(this);

#if !defined(OS_CHROMEOS)
  // On Windows, the notification manager owns the tray icon and views.  Other
  // platforms have global ownership and Create will return NULL.
  tray_.reset(message_center::CreateMessageCenterTray());
#endif
}

MessageCenterNotificationManager::~MessageCenterNotificationManager() {
}


////////////////////////////////////////////////////////////////////////////////
// NotificationUIManager

bool MessageCenterNotificationManager::DoesIdExist(const std::string& id) {
  if (NotificationUIManagerImpl::DoesIdExist(id))
    return true;
  NotificationMap::iterator iter = profile_notifications_.find(id);
  if (iter == profile_notifications_.end())
    return false;
  return true;
}

bool MessageCenterNotificationManager::CancelById(const std::string& id) {
  // See if this ID hasn't been shown yet.
  if (NotificationUIManagerImpl::CancelById(id))
    return true;

  // If it has been shown, remove it.
  NotificationMap::iterator iter = profile_notifications_.find(id);
  if (iter == profile_notifications_.end())
    return false;

  RemoveProfileNotification((*iter).second, false);
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
      // This action occurs when extension is unloaded. Closing notifications
      // is not by user, so |false|.
      RemoveProfileNotification((*curiter).second, false);
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
      // This action occurs when profile is unloaded. Closing notifications is
      // not by user, so |false|.
      RemoveProfileNotification((*curiter).second, false);
      removed = true;
    }
  }
  return removed;
}

void MessageCenterNotificationManager::CancelAll() {
  NotificationUIManagerImpl::CancelAll();

  for (NotificationMap::iterator loopiter = profile_notifications_.begin();
       loopiter != profile_notifications_.end(); ) {
    // This action occurs when Chrome is terminating. Closing notifications is
    // not by user, so |false|.
    RemoveProfileNotification((*loopiter++).second, false);
  }
}

////////////////////////////////////////////////////////////////////////////////
// NotificationUIManagerImpl

bool MessageCenterNotificationManager::ShowNotification(
    const Notification& notification, Profile* profile) {
  // There is always space in MessageCenter, it never rejects Notifications.
  AddProfileNotification(
      new ProfileNotification(profile, notification, message_center_));
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
      ProfileNotification* new_notification =
          new ProfileNotification(profile, notification, message_center_);
      profile_notifications_[notification.notification_id()] = new_notification;
      message_center_->UpdateNotification(old_id,
                                          notification.notification_id(),
                                          notification.title(),
                                          notification.body(),
                                          notification.optional_fields());
      new_notification->StartDownloads();
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
  ProfileNotification* profile_notification =
      FindProfileNotification(notification_id);
  std::string extension_id = profile_notification->GetExtensionId();
  DCHECK(!extension_id.empty());  // or UI should not have enabled the command.
  profile_notification->profile()->GetExtensionService()->DisableExtension(
      extension_id, extensions::Extension::DISABLE_USER_ACTION);
}

void MessageCenterNotificationManager::DisableNotificationsFromSource(
    const std::string& notification_id) {
  ProfileNotification* profile_notification =
      FindProfileNotification(notification_id);
  // UI should not have enabled the command if there is no valid source.
  DCHECK(profile_notification->notification().origin_url().is_valid());
  DesktopNotificationService* service =
      DesktopNotificationServiceFactory::GetForProfile(
          profile_notification->profile());
  service->DenyPermission(profile_notification->notification().origin_url());
}

void MessageCenterNotificationManager::NotificationRemoved(
    const std::string& notification_id,
    bool by_user) {
  RemoveProfileNotification(FindProfileNotification(notification_id), by_user);
}

void MessageCenterNotificationManager::ShowSettings(
    const std::string& notification_id) {
  // The per-message-center Settings button passes an empty string.
  if (notification_id.empty()) {
    NOTIMPLEMENTED();
    return;
  }

  ProfileNotification* profile_notification =
      FindProfileNotification(notification_id);
  Browser* browser =
      chrome::FindOrCreateTabbedBrowser(profile_notification->profile(),
                                        chrome::HOST_DESKTOP_TYPE_NATIVE);
  if (profile_notification->GetExtensionId().empty())
    chrome::ShowContentSettings(browser, CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
  else
    chrome::ShowExtensions(browser, std::string());
}

void MessageCenterNotificationManager::ShowSettingsDialog(
    gfx::NativeView context) {
  settings_controller_->ShowSettingsDialog(context);
}

void MessageCenterNotificationManager::OnClicked(
    const std::string& notification_id) {
  FindProfileNotification(notification_id)->notification().Click();
}

void MessageCenterNotificationManager::OnButtonClicked(
    const std::string& notification_id, int button_index) {
  FindProfileNotification(notification_id)->notification().ButtonClick(
      button_index);
}

////////////////////////////////////////////////////////////////////////////////
// ImageDownloads

MessageCenterNotificationManager::ImageDownloads::ImageDownloads(
    message_center::MessageCenter* message_center)
    : message_center_(message_center) {
}

MessageCenterNotificationManager::ImageDownloads::~ImageDownloads() { }

void MessageCenterNotificationManager::ImageDownloads::StartDownloads(
    const Notification& notification) {
  // Notification primary icon.
  StartDownloadWithImage(
      notification,
      &notification.icon(),
      notification.icon_url(),
      message_center::kNotificationIconSize,
      base::Bind(&message_center::MessageCenter::SetNotificationIcon,
                 base::Unretained(message_center_),
                 notification.notification_id()));

  // Notification image.
  StartDownloadByKey(
      notification,
      message_center::kImageUrlKey,
      message_center::kNotificationPreferredImageSize,
      base::Bind(&message_center::MessageCenter::SetNotificationImage,
                 base::Unretained(message_center_),
                 notification.notification_id()));

  // Notification button icons.
  StartDownloadByKey(
      notification,
      message_center::kButtonOneIconUrlKey,
      message_center::kNotificationButtonIconSize,
      base::Bind(&message_center::MessageCenter::SetNotificationButtonIcon,
                 base::Unretained(message_center_),
                 notification.notification_id(), 0));
  StartDownloadByKey(
      notification, message_center::kButtonTwoIconUrlKey,
      message_center::kNotificationButtonIconSize,
      base::Bind(&message_center::MessageCenter::SetNotificationButtonIcon,
                 base::Unretained(message_center_),
                 notification.notification_id(), 1));
}

void MessageCenterNotificationManager::ImageDownloads::StartDownloadWithImage(
    const Notification& notification,
    const gfx::ImageSkia* image,
    const GURL& url,
    int size,
    const SetImageCallback& callback) {
  // Set the image directly if we have it.
  if (image && !image->isNull()) {
    callback.Run(*image);
    return;
  }

  // Leave the image null if there's no URL.
  if (url.is_empty())
    return;

  content::RenderViewHost* host = notification.GetRenderViewHost();
  if (!host) {
    LOG(WARNING) << "Notification needs an image but has no RenderViewHost";
    return;
  }

  content::WebContents* contents =
      content::WebContents::FromRenderViewHost(host);
  if (!contents) {
    LOG(WARNING) << "Notification needs an image but has no WebContents";
    return;
  }

  contents->DownloadFavicon(
      url,
      size,
      base::Bind(
          &MessageCenterNotificationManager::ImageDownloads::DownloadComplete,
          AsWeakPtr(),
          callback));
}

void MessageCenterNotificationManager::ImageDownloads::StartDownloadByKey(
    const Notification& notification,
    const char* key,
    int size,
    const SetImageCallback& callback) {
  const base::DictionaryValue* optional_fields = notification.optional_fields();
  if (optional_fields && optional_fields->HasKey(key)) {
    string16 url;
    optional_fields->GetString(key, &url);
    StartDownloadWithImage(notification, NULL, GURL(url), size, callback);
  }
}

void MessageCenterNotificationManager::ImageDownloads::DownloadComplete(
    const SetImageCallback& callback,
    int download_id,
    const GURL& image_url,
    int requested_size,
    const std::vector<SkBitmap>& bitmaps) {
  if (bitmaps.empty())
    return;
  gfx::ImageSkia image = gfx::ImageSkia::CreateFrom1xBitmap(bitmaps[0]);
  callback.Run(image);
}

////////////////////////////////////////////////////////////////////////////////
// ProfileNotification

MessageCenterNotificationManager::ProfileNotification::ProfileNotification(
    Profile* profile,
    const Notification& notification,
    message_center::MessageCenter* message_center)
    : profile_(profile),
      notification_(notification),
      downloads_(new ImageDownloads(message_center)) {
  DCHECK(profile);
}

MessageCenterNotificationManager::ProfileNotification::~ProfileNotification() {
}

void MessageCenterNotificationManager::ProfileNotification::StartDownloads() {
  downloads_->StartDownloads(notification_);
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
  profile_notification->StartDownloads();
  notification.Display();
}

void MessageCenterNotificationManager::RemoveProfileNotification(
    ProfileNotification* profile_notification,
    bool by_user) {
  profile_notification->notification().Close(by_user);
  std::string id = profile_notification->notification().notification_id();
  message_center_->RemoveNotification(id);
  profile_notifications_.erase(id);
  delete profile_notification;
}

MessageCenterNotificationManager::ProfileNotification*
    MessageCenterNotificationManager::FindProfileNotification(
        const std::string& id) const {
  NotificationMap::const_iterator iter = profile_notifications_.find(id);
  // If the notification is shown in UI, it must be in the map.
  DCHECK(iter != profile_notifications_.end());
  DCHECK((*iter).second);
  return (*iter).second;
}
