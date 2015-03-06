// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/platform_notification_service_impl.h"

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
#include "content/public/common/platform_notification_data.h"
#include "net/base/net_util.h"
#include "ui/message_center/notifier_settings.h"
#include "url/url_constants.h"

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/desktop_notification_service_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/info_map.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#endif

using content::BrowserThread;
using message_center::NotifierId;

namespace {

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
    content::BrowserContext* browser_context,
    int64 service_worker_registration_id,
    const std::string& notification_id,
    const GURL& origin,
    const content::PlatformNotificationData& notification_data,
    const base::Callback<void(content::PersistentNotificationStatus)>&
        callback) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::NotificationEventDispatcher::GetInstance()
      ->DispatchNotificationClickEvent(
            browser_context,
            origin,
            service_worker_registration_id,
            notification_id,
            notification_data,
            callback);
}

blink::WebNotificationPermission
PlatformNotificationServiceImpl::CheckPermissionOnUIThread(
    content::BrowserContext* browser_context,
    const GURL& origin,
    int render_process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  Profile* profile = Profile::FromBrowserContext(browser_context);
  DCHECK(profile);

#if defined(ENABLE_EXTENSIONS)
  // Extensions support an API permission named "notification". This will grant
  // not only grant permission for using the Chrome App extension API, but also
  // for the Web Notification API.
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(browser_context);
  extensions::ProcessMap* process_map =
      extensions::ProcessMap::Get(browser_context);
  extensions::ExtensionSet extensions;

  DesktopNotificationService* desktop_notification_service =
      DesktopNotificationServiceFactory::GetForProfile(profile);
  DCHECK(desktop_notification_service);

  // If |origin| is an enabled extension, only select that one. Otherwise select
  // all extensions whose web content matches |origin|.
  if (origin.SchemeIs(extensions::kExtensionScheme)) {
    const extensions::Extension* extension = registry->GetExtensionById(
        origin.host(), extensions::ExtensionRegistry::ENABLED);
    if (extension)
      extensions.Insert(extension);
    } else {
      for (const auto& extension : registry->enabled_extensions()) {
        if (extension->web_extent().MatchesSecurityOrigin(origin))
          extensions.Insert(extension);
    }
  }

  // Check if any of the selected extensions have the "notification" API
  // permission, are active in |render_process_id| and has not been manually
  // disabled by the user. If all of that is true, grant permission.
  for (const auto& extension : extensions) {
    if (!extension->permissions_data()->HasAPIPermission(
        extensions::APIPermission::kNotifications))
      continue;

    if (!process_map->Contains(extension->id(), render_process_id))
      continue;

    NotifierId notifier_id(NotifierId::APPLICATION, extension->id());
    if (!desktop_notification_service->IsNotifierEnabled(notifier_id))
      continue;

    return blink::WebNotificationPermissionAllowed;
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
  extensions::InfoMap* extension_info_map = io_data->GetExtensionInfoMap();

  // We want to see if there is an extension that hasn't been manually disabled
  // that has the notifications permission and applies to this security origin.
  // First, get the list of extensions with permission for the origin.
  extensions::ExtensionSet extensions;
  extension_info_map->GetExtensionsWithAPIPermissionForSecurityOrigin(
      origin,
      render_process_id,
      extensions::APIPermission::kNotifications,
      &extensions);
  for (const auto& extension : extensions) {
    if (!extension_info_map->AreNotificationsDisabled(extension->id()))
      return blink::WebNotificationPermissionAllowed;
  }
#endif

  // No enabled extensions exist, so check the normal host content settings.
  HostContentSettingsMap* host_content_settings_map =
      io_data->GetHostContentSettingsMap();
  ContentSetting setting = host_content_settings_map->GetContentSetting(
      origin,
      origin,
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
      NO_RESOURCE_IDENTIFIER);

  if (setting == CONTENT_SETTING_ALLOW)
    return blink::WebNotificationPermissionAllowed;
  if (setting == CONTENT_SETTING_BLOCK)
    return blink::WebNotificationPermissionDenied;

  return blink::WebNotificationPermissionDefault;
}

void PlatformNotificationServiceImpl::DisplayNotification(
    content::BrowserContext* browser_context,
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
    content::BrowserContext* browser_context,
    int64 service_worker_registration_id,
    const GURL& origin,
    const SkBitmap& icon,
    const content::PlatformNotificationData& notification_data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  Profile* profile = Profile::FromBrowserContext(browser_context);
  DCHECK(profile);

  PersistentNotificationDelegate* delegate = new PersistentNotificationDelegate(
      browser_context,
      service_worker_registration_id,
      origin,
      notification_data);

  Notification notification = CreateNotificationFromData(
      profile, origin, icon, notification_data, delegate);

  GetNotificationUIManager()->Add(notification, profile);

  profile->GetHostContentSettingsMap()->UpdateLastUsage(
      origin, origin, CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
}

void PlatformNotificationServiceImpl::ClosePersistentNotification(
    content::BrowserContext* browser_context,
    const std::string& persistent_notification_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  Profile* profile = Profile::FromBrowserContext(browser_context);
  DCHECK(profile);

  GetNotificationUIManager()->CancelById(
      persistent_notification_id, NotificationUIManager::GetProfileID(profile));
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
  notification.set_silent(notification_data.silent);

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
