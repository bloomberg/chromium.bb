// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/platform_notification_service_impl.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_object_proxy.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/desktop_notification_delegate.h"
#include "content/public/common/platform_notification_data.h"
#include "ui/message_center/notifier_settings.h"

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/desktop_notification_service_factory.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/info_map.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/permissions/api_permission.h"
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

blink::WebNotificationPermission
PlatformNotificationServiceImpl::CheckPermission(
    content::ResourceContext* resource_context,
    const GURL& origin,
    int render_process_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

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
    int render_process_id,
    base::Closure* cancel_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  Profile* profile = Profile::FromBrowserContext(browser_context);
  DCHECK(profile);

  NotificationObjectProxy* proxy = new NotificationObjectProxy(delegate.Pass());
  base::string16 display_source = DisplayNameForOriginInProcessId(
      profile, origin, render_process_id);

  // TODO(peter): Icons for Web Notifications are currently always requested for
  // 1x scale, whereas the displays on which they can be displayed can have a
  // different pixel density. Be smarter about this when the API gets updated
  // with a way for developers to specify images of different resolutions.
  Notification notification(origin, notification_data.title,
      notification_data.body, gfx::Image::CreateFrom1xBitmap(icon),
      display_source, notification_data.tag, proxy);

  // Web Notifications do not timeout.
  notification.set_never_timeout(true);

  GetNotificationUIManager()->Add(notification, profile);
  if (cancel_callback)
    *cancel_callback =
        base::Bind(&CancelNotification,
                   proxy->id(),
                   NotificationUIManager::GetProfileID(profile));

  profile->GetHostContentSettingsMap()->UpdateLastUsage(
      origin, origin, CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
}

void PlatformNotificationServiceImpl::DisplayPersistentNotification(
    content::BrowserContext* browser_context,
    int64 service_worker_registration_id,
    const GURL& origin,
    const SkBitmap& icon,
    const content::PlatformNotificationData& notification_data,
    int render_process_id) {
  NOTIMPLEMENTED();
}

void PlatformNotificationServiceImpl::ClosePersistentNotification(
    content::BrowserContext* browser_context,
    const std::string& persistent_notification_id) {
  NOTIMPLEMENTED();
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

base::string16 PlatformNotificationServiceImpl::DisplayNameForOriginInProcessId(
    Profile* profile, const GURL& origin, int process_id) {
#if defined(ENABLE_EXTENSIONS)
  // If the source is an extension, lookup the display name.
  if (origin.SchemeIs(extensions::kExtensionScheme)) {
    extensions::InfoMap* extension_info_map =
        extensions::ExtensionSystem::Get(profile)->info_map();
    if (extension_info_map) {
      extensions::ExtensionSet extensions;
      extension_info_map->GetExtensionsWithAPIPermissionForSecurityOrigin(
          origin,
          process_id,
          extensions::APIPermission::kNotifications,
          &extensions);
      DesktopNotificationService* desktop_notification_service =
          DesktopNotificationServiceFactory::GetForProfile(profile);
      DCHECK(desktop_notification_service);

      for (const auto& extension : extensions) {
        NotifierId notifier_id(NotifierId::APPLICATION, extension->id());
        if (desktop_notification_service->IsNotifierEnabled(notifier_id))
          return base::UTF8ToUTF16(extension->name());
      }
    }
  }
#endif

  return base::UTF8ToUTF16(origin.host());
}
