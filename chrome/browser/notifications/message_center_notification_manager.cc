// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/message_center_notification_manager.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/stl_util.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/desktop_notification_service_factory.h"
#include "chrome/browser/notifications/fullscreen_notification_blocker.h"
#include "chrome/browser/notifications/message_center_settings_controller.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/screen_lock_notification_blocker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/info_map.h"
#include "extensions/common/extension_set.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/message_center/message_center_style.h"
#include "ui/message_center/message_center_tray.h"
#include "ui/message_center/message_center_types.h"
#include "ui/message_center/notifier_settings.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/notifications/login_state_notification_blocker_chromeos.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#endif

#if defined(USE_ASH)
#include "ash/shell.h"
#include "ash/system/web_notification/web_notification_tray.h"
#endif

#if defined(OS_WIN)
// The first-run balloon will be shown |kFirstRunIdleDelaySeconds| after all
// popups go away and the user has notifications in the message center.
const int kFirstRunIdleDelaySeconds = 1;
#endif

MessageCenterNotificationManager::MessageCenterNotificationManager(
    message_center::MessageCenter* message_center,
    PrefService* local_state,
    scoped_ptr<message_center::NotifierSettingsProvider> settings_provider)
    : message_center_(message_center),
#if defined(OS_WIN)
      first_run_idle_timeout_(
          base::TimeDelta::FromSeconds(kFirstRunIdleDelaySeconds)),
      weak_factory_(this),
#endif
      settings_provider_(settings_provider.Pass()),
      system_observer_(this),
      stats_collector_(message_center),
      google_now_stats_collector_(message_center) {
#if defined(OS_WIN)
  first_run_pref_.Init(prefs::kMessageCenterShowedFirstRunBalloon, local_state);
#endif

  message_center_->AddObserver(this);
  message_center_->SetNotifierSettingsProvider(settings_provider_.get());

#if defined(OS_CHROMEOS)
  blockers_.push_back(
      new LoginStateNotificationBlockerChromeOS(message_center));
#else
  blockers_.push_back(new ScreenLockNotificationBlocker(message_center));
#endif
  blockers_.push_back(new FullscreenNotificationBlocker(message_center));

#if defined(OS_WIN) || defined(OS_MACOSX) \
  || (defined(OS_LINUX) && !defined(OS_CHROMEOS))
  // On Windows, Linux and Mac, the notification manager owns the tray icon and
  // views.Other platforms have global ownership and Create will return NULL.
  tray_.reset(message_center::CreateMessageCenterTray());
#endif
  registrar_.Add(this,
                 chrome::NOTIFICATION_FULLSCREEN_CHANGED,
                 content::NotificationService::AllSources());
}

MessageCenterNotificationManager::~MessageCenterNotificationManager() {
  message_center_->SetNotifierSettingsProvider(NULL);
  message_center_->RemoveObserver(this);

  STLDeleteContainerPairSecondPointers(profile_notifications_.begin(),
                                       profile_notifications_.end());
  profile_notifications_.clear();
}

void MessageCenterNotificationManager::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kMessageCenterShowedFirstRunBalloon,
                                false);
  registry->RegisterBooleanPref(prefs::kMessageCenterShowIcon, true);
  registry->RegisterBooleanPref(prefs::kMessageCenterForcedOnTaskbar, false);
}

////////////////////////////////////////////////////////////////////////////////
// NotificationUIManager

void MessageCenterNotificationManager::Add(const Notification& notification,
                                           Profile* profile) {
  if (Update(notification, profile))
    return;

  DesktopNotificationServiceFactory::GetForProfile(profile)->
      ShowWelcomeNotificationIfNecessary(notification);

  // WARNING: You MUST use AddProfileNotification or update the message center
  // via the notification within a ProfileNotification object or the profile ID
  // will not be correctly set for ChromeOS.
  AddProfileNotification(
      new ProfileNotification(profile, notification, message_center_));
}

bool MessageCenterNotificationManager::Update(const Notification& notification,
                                              Profile* profile) {
  const base::string16& replace_id = notification.replace_id();
  if (replace_id.empty())
    return false;

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
        old_notification->profile() == profile) {
      // Changing the type from non-progress to progress does not count towards
      // the immediate update allowed in the message center.
      std::string old_id =
          old_notification->notification().delegate_id();

      // Add/remove notification in the local list but just update the same
      // one in MessageCenter.
      delete old_notification;
      profile_notifications_.erase(old_id);
      ProfileNotification* new_notification =
          new ProfileNotification(profile, notification, message_center_);
      profile_notifications_[notification.delegate_id()] = new_notification;

      // WARNING: You MUST use AddProfileNotification or update the message
      // center via the notification within a ProfileNotification object or the
      // profile ID will not be correctly set for ChromeOS.
      message_center_->UpdateNotification(
          old_id,
          make_scoped_ptr(new message_center::Notification(
              new_notification->notification())));

      new_notification->StartDownloads();
      return true;
    }
  }
  return false;
}

const Notification* MessageCenterNotificationManager::FindById(
    const std::string& id) const {
  NotificationMap::const_iterator iter = profile_notifications_.find(id);
  if (iter == profile_notifications_.end())
    return NULL;
  return &(iter->second->notification());
}

bool MessageCenterNotificationManager::CancelById(const std::string& id) {
  // See if this ID hasn't been shown yet.
  // If it has been shown, remove it.
  NotificationMap::iterator iter = profile_notifications_.find(id);
  if (iter == profile_notifications_.end())
    return false;

  RemoveProfileNotification(iter->second);
  message_center_->RemoveNotification(id, /* by_user */ false);
  return true;
}

std::set<std::string>
MessageCenterNotificationManager::GetAllIdsByProfileAndSourceOrigin(
    Profile* profile,
    const GURL& source) {

  std::set<std::string> notification_ids;
  for (NotificationMap::iterator iter = profile_notifications_.begin();
       iter != profile_notifications_.end(); iter++) {
    if ((*iter).second->notification().origin_url() == source &&
        profile == (*iter).second->profile()) {
      notification_ids.insert(iter->first);
    }
  }
  return notification_ids;
}

bool MessageCenterNotificationManager::CancelAllBySourceOrigin(
    const GURL& source) {
  // Same pattern as CancelById, but more complicated than the above
  // because there may be multiple notifications from the same source.
  bool removed = false;

  for (NotificationMap::iterator loopiter = profile_notifications_.begin();
       loopiter != profile_notifications_.end(); ) {
    NotificationMap::iterator curiter = loopiter++;
    if ((*curiter).second->notification().origin_url() == source) {
      const std::string id = curiter->first;
      RemoveProfileNotification(curiter->second);
      message_center_->RemoveNotification(id, /* by_user */ false);
      removed = true;
    }
  }
  return removed;
}

bool MessageCenterNotificationManager::CancelAllByProfile(Profile* profile) {
  // Same pattern as CancelAllBySourceOrigin.
  bool removed = false;

  for (NotificationMap::iterator loopiter = profile_notifications_.begin();
       loopiter != profile_notifications_.end(); ) {
    NotificationMap::iterator curiter = loopiter++;
    if (profile == (*curiter).second->profile()) {
      const std::string id = curiter->first;
      RemoveProfileNotification(curiter->second);
      message_center_->RemoveNotification(id, /* by_user */ false);
      removed = true;
    }
  }
  return removed;
}

void MessageCenterNotificationManager::CancelAll() {
  message_center_->RemoveAllNotifications(/* by_user */ false);
}

////////////////////////////////////////////////////////////////////////////////
// MessageCenter::Observer
void MessageCenterNotificationManager::OnNotificationRemoved(
    const std::string& notification_id,
    bool by_user) {
  NotificationMap::const_iterator iter =
      profile_notifications_.find(notification_id);
  if (iter != profile_notifications_.end())
    RemoveProfileNotification(iter->second);

#if defined(OS_WIN)
  CheckFirstRunTimer();
#endif
}

void MessageCenterNotificationManager::OnCenterVisibilityChanged(
    message_center::Visibility visibility) {
#if defined(OS_WIN)
  if (visibility == message_center::VISIBILITY_TRANSIENT)
    CheckFirstRunTimer();
#endif
}

void MessageCenterNotificationManager::OnNotificationUpdated(
    const std::string& notification_id) {
#if defined(OS_WIN)
  CheckFirstRunTimer();
#endif
}

void MessageCenterNotificationManager::EnsureMessageCenterClosed() {
  if (tray_.get())
    tray_->GetMessageCenterTray()->HideMessageCenterBubble();

#if defined(USE_ASH)
  if (ash::Shell::HasInstance()) {
    ash::WebNotificationTray* tray =
        ash::Shell::GetInstance()->GetWebNotificationTray();
    if (tray)
      tray->GetMessageCenterTray()->HideMessageCenterBubble();
  }
#endif
}

void MessageCenterNotificationManager::SetMessageCenterTrayDelegateForTest(
    message_center::MessageCenterTrayDelegate* delegate) {
  tray_.reset(delegate);
}

void MessageCenterNotificationManager::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_FULLSCREEN_CHANGED) {
    const bool is_fullscreen = *content::Details<bool>(details).ptr();

    if (is_fullscreen && tray_.get() && tray_->GetMessageCenterTray())
      tray_->GetMessageCenterTray()->HidePopupBubble();
  }
}

////////////////////////////////////////////////////////////////////////////////
// ImageDownloads

MessageCenterNotificationManager::ImageDownloads::ImageDownloads(
    message_center::MessageCenter* message_center,
    ImageDownloadsObserver* observer)
    : message_center_(message_center),
      pending_downloads_(0),
      observer_(observer) {
}

MessageCenterNotificationManager::ImageDownloads::~ImageDownloads() { }

void MessageCenterNotificationManager::ImageDownloads::StartDownloads(
    const Notification& notification) {
  // In case all downloads are synchronous, assume a pending download.
  AddPendingDownload();

  // Notification primary icon.
  StartDownloadWithImage(
      notification,
      &notification.icon(),
      notification.icon_url(),
      base::Bind(&message_center::MessageCenter::SetNotificationIcon,
                 base::Unretained(message_center_),
                 notification.delegate_id()));

  // Notification image.
  StartDownloadWithImage(
      notification,
      NULL,
      notification.image_url(),
      base::Bind(&message_center::MessageCenter::SetNotificationImage,
                 base::Unretained(message_center_),
                 notification.delegate_id()));

  // Notification button icons.
  StartDownloadWithImage(
      notification,
      NULL,
      notification.button_one_icon_url(),
      base::Bind(&message_center::MessageCenter::SetNotificationButtonIcon,
                 base::Unretained(message_center_),
                 notification.delegate_id(),
                 0));
  StartDownloadWithImage(
      notification,
      NULL,
      notification.button_two_icon_url(),
      base::Bind(&message_center::MessageCenter::SetNotificationButtonIcon,
                 base::Unretained(message_center_),
                 notification.delegate_id(),
                 1));

  // This should tell the observer we're done if everything was synchronous.
  PendingDownloadCompleted();
}

void MessageCenterNotificationManager::ImageDownloads::StartDownloadWithImage(
    const Notification& notification,
    const gfx::Image* image,
    const GURL& url,
    const SetImageCallback& callback) {
  // Set the image directly if we have it.
  if (image && !image->IsEmpty()) {
    callback.Run(*image);
    return;
  }

  // Leave the image null if there's no URL.
  if (url.is_empty())
    return;

  content::WebContents* contents = notification.GetWebContents();
  if (!contents) {
    LOG(WARNING) << "Notification needs an image but has no WebContents";
    return;
  }

  AddPendingDownload();

  contents->DownloadImage(
      url,
      false,  // Not a favicon
      0,  // No maximum size
      base::Bind(
          &MessageCenterNotificationManager::ImageDownloads::DownloadComplete,
          AsWeakPtr(),
          callback));
}

void MessageCenterNotificationManager::ImageDownloads::DownloadComplete(
    const SetImageCallback& callback,
    int download_id,
    int http_status_code,
    const GURL& image_url,
    const std::vector<SkBitmap>& bitmaps,
    const std::vector<gfx::Size>& original_bitmap_sizes) {
  PendingDownloadCompleted();

  if (bitmaps.empty())
    return;
  gfx::Image image = gfx::Image::CreateFrom1xBitmap(bitmaps[0]);
  callback.Run(image);
}

// Private methods.

void MessageCenterNotificationManager::ImageDownloads::AddPendingDownload() {
  ++pending_downloads_;
}

void
MessageCenterNotificationManager::ImageDownloads::PendingDownloadCompleted() {
  DCHECK(pending_downloads_ > 0);
  if (--pending_downloads_ == 0 && observer_)
    observer_->OnDownloadsCompleted();
}

////////////////////////////////////////////////////////////////////////////////
// ProfileNotification

MessageCenterNotificationManager::ProfileNotification::ProfileNotification(
    Profile* profile,
    const Notification& notification,
    message_center::MessageCenter* message_center)
    : profile_(profile),
      notification_(notification),
      downloads_(new ImageDownloads(message_center, this)) {
  DCHECK(profile);
#if defined(OS_CHROMEOS)
  notification_.set_profile_id(multi_user_util::GetUserIDFromProfile(profile));
#endif
}

MessageCenterNotificationManager::ProfileNotification::~ProfileNotification() {
}

void MessageCenterNotificationManager::ProfileNotification::StartDownloads() {
  downloads_->StartDownloads(notification_);
}

void
MessageCenterNotificationManager::ProfileNotification::OnDownloadsCompleted() {
  notification_.DoneRendering();
}

std::string
    MessageCenterNotificationManager::ProfileNotification::GetExtensionId() {
  extensions::InfoMap* extension_info_map =
      extensions::ExtensionSystem::Get(profile())->info_map();
  extensions::ExtensionSet extensions;
  extension_info_map->GetExtensionsWithAPIPermissionForSecurityOrigin(
      notification().origin_url(), notification().process_id(),
      extensions::APIPermission::kNotification, &extensions);

  DesktopNotificationService* desktop_service =
      DesktopNotificationServiceFactory::GetForProfile(profile());
  for (extensions::ExtensionSet::const_iterator iter = extensions.begin();
       iter != extensions.end(); ++iter) {
    if (desktop_service->IsNotifierEnabled(message_center::NotifierId(
            message_center::NotifierId::APPLICATION, (*iter)->id()))) {
      return (*iter)->id();
    }
  }
  return std::string();
}

////////////////////////////////////////////////////////////////////////////////
// private

void MessageCenterNotificationManager::AddProfileNotification(
    ProfileNotification* profile_notification) {
  const Notification& notification = profile_notification->notification();
  std::string id = notification.delegate_id();
  // Notification ids should be unique.
  DCHECK(profile_notifications_.find(id) == profile_notifications_.end());
  profile_notifications_[id] = profile_notification;

  // Create the copy for message center, and ensure the extension ID is correct.
  scoped_ptr<message_center::Notification> message_center_notification(
      new message_center::Notification(notification));
  message_center_->AddNotification(message_center_notification.Pass());

  profile_notification->StartDownloads();
}

void MessageCenterNotificationManager::RemoveProfileNotification(
    ProfileNotification* profile_notification) {
  std::string id = profile_notification->notification().delegate_id();
  profile_notifications_.erase(id);
  delete profile_notification;
}

MessageCenterNotificationManager::ProfileNotification*
    MessageCenterNotificationManager::FindProfileNotification(
        const std::string& id) const {
  NotificationMap::const_iterator iter = profile_notifications_.find(id);
  if (iter == profile_notifications_.end())
    return NULL;

  return (*iter).second;
}
