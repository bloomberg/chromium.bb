// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/platform_notification_service_impl.h"

#include "base/metrics/histogram_macros.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/desktop_notification_profile_util.h"
#include "chrome/browser/notifications/notification_object_proxy.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/notifications/persistent_notification_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/desktop_notification_delegate.h"
#include "content/public/browser/notification_event_dispatcher.h"
#include "content/public/browser/platform_notification_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/platform_notification_data.h"
#include "net/base/net_util.h"
#include "ui/message_center/notifier_settings.h"
#include "url/url_constants.h"

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/notifications/notifier_state_tracker.h"
#include "chrome/browser/notifications/notifier_state_tracker_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/info_map.h"
#include "extensions/common/constants.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#endif

#if defined(OS_ANDROID)
#include "base/strings/string_number_conversions.h"
#endif

using content::BrowserContext;
using content::BrowserThread;
using content::PlatformNotificationContext;
using message_center::NotifierId;

namespace {

// Callback to provide when deleting the data associated with persistent Web
// Notifications from the notification database.
void OnPersistentNotificationDataDeleted(bool success) {
  UMA_HISTOGRAM_BOOLEAN("Notifications.PersistentNotificationDataDeleted",
      success);
}

// Persistent notifications fired through the delegate do not care about the
// lifetime of the Service Worker responsible for executing the event.
void OnEventDispatchComplete(content::PersistentNotificationStatus status) {
  UMA_HISTOGRAM_ENUMERATION(
      "Notifications.PersistentWebNotificationClickResult", status,
      content::PersistentNotificationStatus::
          PERSISTENT_NOTIFICATION_STATUS_MAX);
}

void CancelNotification(const std::string& id, ProfileID profile_id) {
  PlatformNotificationServiceImpl::GetInstance()
      ->GetNotificationUIManager()->CancelById(id, profile_id);
}

}  // namespace

// static
PlatformNotificationServiceImpl*
PlatformNotificationServiceImpl::GetInstance() {
  return Singleton<PlatformNotificationServiceImpl>::get();
}

PlatformNotificationServiceImpl::PlatformNotificationServiceImpl()
    : notification_ui_manager_for_tests_(nullptr) {}

PlatformNotificationServiceImpl::~PlatformNotificationServiceImpl() {}

void PlatformNotificationServiceImpl::OnPersistentNotificationClick(
    BrowserContext* browser_context,
    int64_t persistent_notification_id,
    const GURL& origin,
    int action_index) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::NotificationEventDispatcher::GetInstance()
      ->DispatchNotificationClickEvent(
            browser_context,
            persistent_notification_id,
            origin,
            action_index,
            base::Bind(&OnEventDispatchComplete));
}

void PlatformNotificationServiceImpl::OnPersistentNotificationClose(
    BrowserContext* browser_context,
    int64_t persistent_notification_id,
    const GURL& origin) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  PlatformNotificationContext* context =
      BrowserContext::GetStoragePartitionForSite(browser_context, origin)
          ->GetPlatformNotificationContext();

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&PlatformNotificationContext::DeleteNotificationData,
                 context,
                 persistent_notification_id,
                 origin,
                 base::Bind(&OnPersistentNotificationDataDeleted)));
}

blink::WebNotificationPermission
PlatformNotificationServiceImpl::CheckPermissionOnUIThread(
    BrowserContext* browser_context,
    const GURL& origin,
    int render_process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  Profile* profile = Profile::FromBrowserContext(browser_context);
  DCHECK(profile);

#if defined(ENABLE_EXTENSIONS)
  // Extensions support an API permission named "notification". This will grant
  // not only grant permission for using the Chrome App extension API, but also
  // for the Web Notification API.
  if (origin.SchemeIs(extensions::kExtensionScheme)) {
    extensions::ExtensionRegistry* registry =
        extensions::ExtensionRegistry::Get(browser_context);
    extensions::ProcessMap* process_map =
        extensions::ProcessMap::Get(browser_context);

    const extensions::Extension* extension =
        registry->GetExtensionById(origin.host(),
                                   extensions::ExtensionRegistry::ENABLED);

    if (extension &&
        extension->permissions_data()->HasAPIPermission(
            extensions::APIPermission::kNotifications) &&
        process_map->Contains(extension->id(), render_process_id)) {
      NotifierStateTracker* notifier_state_tracker =
          NotifierStateTrackerFactory::GetForProfile(profile);
      DCHECK(notifier_state_tracker);

      NotifierId notifier_id(NotifierId::APPLICATION, extension->id());
      if (notifier_state_tracker->IsNotifierEnabled(notifier_id))
        return blink::WebNotificationPermissionAllowed;
    }
  }
#endif

  ContentSetting setting =
      DesktopNotificationProfileUtil::GetContentSetting(profile, origin);

  if (setting == CONTENT_SETTING_ALLOW)
    return blink::WebNotificationPermissionAllowed;
  if (setting == CONTENT_SETTING_BLOCK)
    return blink::WebNotificationPermissionDenied;

  return blink::WebNotificationPermissionDefault;
}

blink::WebNotificationPermission
PlatformNotificationServiceImpl::CheckPermissionOnIOThread(
    content::ResourceContext* resource_context,
    const GURL& origin,
    int render_process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  ProfileIOData* io_data = ProfileIOData::FromResourceContext(resource_context);
#if defined(ENABLE_EXTENSIONS)
  // Extensions support an API permission named "notification". This will grant
  // not only grant permission for using the Chrome App extension API, but also
  // for the Web Notification API.
  if (origin.SchemeIs(extensions::kExtensionScheme)) {
    extensions::InfoMap* extension_info_map = io_data->GetExtensionInfoMap();
    const extensions::ProcessMap& process_map =
        extension_info_map->process_map();

    const extensions::Extension* extension =
        extension_info_map->extensions().GetByID(origin.host());

    if (extension &&
        extension->permissions_data()->HasAPIPermission(
            extensions::APIPermission::kNotifications) &&
        process_map.Contains(extension->id(), render_process_id)) {
      if (!extension_info_map->AreNotificationsDisabled(extension->id()))
        return blink::WebNotificationPermissionAllowed;
    }
  }
#endif

  // No enabled extensions exist, so check the normal host content settings.
  HostContentSettingsMap* host_content_settings_map =
      io_data->GetHostContentSettingsMap();
  ContentSetting setting = host_content_settings_map->GetContentSetting(
      origin,
      origin,
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
      content_settings::ResourceIdentifier());

  if (setting == CONTENT_SETTING_ALLOW)
    return blink::WebNotificationPermissionAllowed;
  if (setting == CONTENT_SETTING_BLOCK)
    return blink::WebNotificationPermissionDenied;

  return blink::WebNotificationPermissionDefault;
}

void PlatformNotificationServiceImpl::DisplayNotification(
    BrowserContext* browser_context,
    const GURL& origin,
    const SkBitmap& icon,
    const content::PlatformNotificationData& notification_data,
    scoped_ptr<content::DesktopNotificationDelegate> delegate,
    base::Closure* cancel_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  Profile* profile = Profile::FromBrowserContext(browser_context);
  DCHECK(profile);

  NotificationObjectProxy* proxy = new NotificationObjectProxy(delegate.Pass());
  Notification notification = CreateNotificationFromData(
      profile, origin, icon, notification_data, proxy);

  GetNotificationUIManager()->Add(notification, profile);
  if (cancel_callback)
    *cancel_callback =
        base::Bind(&CancelNotification,
                   notification.delegate_id(),
                   NotificationUIManager::GetProfileID(profile));

  profile->GetHostContentSettingsMap()->UpdateLastUsage(
      origin, origin, CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
}

void PlatformNotificationServiceImpl::DisplayPersistentNotification(
    BrowserContext* browser_context,
    int64_t persistent_notification_id,
    const GURL& origin,
    const SkBitmap& icon,
    const content::PlatformNotificationData& notification_data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  Profile* profile = Profile::FromBrowserContext(browser_context);
  DCHECK(profile);

  PersistentNotificationDelegate* delegate = new PersistentNotificationDelegate(
      browser_context, persistent_notification_id, origin);

  Notification notification = CreateNotificationFromData(
      profile, origin, icon, notification_data, delegate);

  // TODO(peter): Remove this mapping when we have reliable id generation for
  // the message_center::Notification objects.
  persistent_notifications_[persistent_notification_id] = notification.id();

  GetNotificationUIManager()->Add(notification, profile);

  profile->GetHostContentSettingsMap()->UpdateLastUsage(
      origin, origin, CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
}

void PlatformNotificationServiceImpl::ClosePersistentNotification(
    BrowserContext* browser_context,
    int64_t persistent_notification_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  Profile* profile = Profile::FromBrowserContext(browser_context);
  DCHECK(profile);

#if defined(OS_ANDROID)
  // TODO(peter): Remove this conversion when the notification ids are being
  // generated by the caller of this method.
  std::string textual_persistent_notification_id =
      base::Int64ToString(persistent_notification_id);
  GetNotificationUIManager()->CancelById(
      textual_persistent_notification_id,
      NotificationUIManager::GetProfileID(profile));
#else
  auto iter = persistent_notifications_.find(persistent_notification_id);
  if (iter == persistent_notifications_.end())
    return;

  GetNotificationUIManager()->CancelById(
      iter->second, NotificationUIManager::GetProfileID(profile));

  persistent_notifications_.erase(iter);
#endif
}

bool PlatformNotificationServiceImpl::GetDisplayedPersistentNotifications(
    BrowserContext* browser_context,
    std::set<std::string>* displayed_notifications) {
  DCHECK(displayed_notifications);

#if !defined(OS_ANDROID)
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (!profile || profile->AsTestingProfile())
    return false;  // Tests will not have a message center.

  // TODO(peter): Filter for persistent notifications only.
  *displayed_notifications = GetNotificationUIManager()->GetAllIdsByProfile(
      NotificationUIManager::GetProfileID(profile));

  return true;
#else
  // Android cannot reliably return the notifications that are currently being
  // displayed on the platform, see the comment in NotificationUIManagerAndroid.
  return false;
#endif  // !defined(OS_ANDROID)
}

Notification PlatformNotificationServiceImpl::CreateNotificationFromData(
    Profile* profile,
    const GURL& origin,
    const SkBitmap& icon,
    const content::PlatformNotificationData& notification_data,
    NotificationDelegate* delegate) const {
  base::string16 display_source = DisplayNameForOrigin(profile, origin);

  // TODO(peter): Icons for Web Notifications are currently always requested for
  // 1x scale, whereas the displays on which they can be displayed can have a
  // different pixel density. Be smarter about this when the API gets updated
  // with a way for developers to specify images of different resolutions.
  Notification notification(origin, notification_data.title,
      notification_data.body, gfx::Image::CreateFrom1xBitmap(icon),
      display_source, notification_data.tag, delegate);

  notification.set_context_message(display_source);
  notification.set_vibration_pattern(notification_data.vibration_pattern);
  notification.set_silent(notification_data.silent);

  std::vector<message_center::ButtonInfo> buttons;
  for (const auto& action : notification_data.actions)
    buttons.push_back(message_center::ButtonInfo(action.title));

  notification.set_buttons(buttons);

  // Web Notifications do not timeout.
  notification.set_never_timeout(true);

  return notification;
}

NotificationUIManager*
PlatformNotificationServiceImpl::GetNotificationUIManager() const {
  if (notification_ui_manager_for_tests_)
    return notification_ui_manager_for_tests_;

  return g_browser_process->notification_ui_manager();
}

void PlatformNotificationServiceImpl::SetNotificationUIManagerForTesting(
    NotificationUIManager* manager) {
  notification_ui_manager_for_tests_ = manager;
}

base::string16 PlatformNotificationServiceImpl::DisplayNameForOrigin(
    Profile* profile,
    const GURL& origin) const {
#if defined(ENABLE_EXTENSIONS)
  // If the source is an extension, lookup the display name.
  if (origin.SchemeIs(extensions::kExtensionScheme)) {
    const extensions::Extension* extension =
        extensions::ExtensionRegistry::Get(profile)->GetExtensionById(
            origin.host(), extensions::ExtensionRegistry::EVERYTHING);
    DCHECK(extension);

    return base::UTF8ToUTF16(extension->name());
  }
#endif

  std::string languages =
      profile->GetPrefs()->GetString(prefs::kAcceptLanguages);

  return WebOriginDisplayName(origin, languages);
}

// static
// TODO(palmer): It might be good to replace this with a call to
// |FormatUrlForSecurityDisplay|. crbug.com/496965
base::string16 PlatformNotificationServiceImpl::WebOriginDisplayName(
    const GURL& origin,
    const std::string& languages) {
  if (origin.SchemeIsHTTPOrHTTPS()) {
    base::string16 formatted_origin;
    if (origin.SchemeIs(url::kHttpScheme)) {
      const url::Parsed& parsed = origin.parsed_for_possibly_invalid_spec();
      const std::string& spec = origin.possibly_invalid_spec();
      formatted_origin.append(
          spec.begin(),
          spec.begin() +
              parsed.CountCharactersBefore(url::Parsed::USERNAME, true));
    }
    formatted_origin.append(net::IDNToUnicode(origin.host(), languages));
    if (origin.has_port()) {
      formatted_origin.push_back(':');
      formatted_origin.append(base::UTF8ToUTF16(origin.port()));
    }
    return formatted_origin;
  }

  // TODO(dewittj): Once file:// URLs are passed in to the origin
  // GURL here, begin returning the path as the display name.
  return net::FormatUrl(origin, languages);
}
