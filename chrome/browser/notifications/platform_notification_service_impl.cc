// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/platform_notification_service_impl.h"

#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/desktop_notification_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/desktop_notification_delegate.h"
#include "content/public/common/show_desktop_notification_params.h"

#if defined(ENABLE_EXTENSIONS)
#include "extensions/browser/info_map.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/permissions/api_permission.h"
#endif

using content::BrowserThread;

// static
PlatformNotificationServiceImpl*
PlatformNotificationServiceImpl::GetInstance() {
  return Singleton<PlatformNotificationServiceImpl>::get();
}

PlatformNotificationServiceImpl::PlatformNotificationServiceImpl() {}

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
    const content::ShowDesktopNotificationHostMsgParams& params,
    scoped_ptr<content::DesktopNotificationDelegate> delegate,
    int render_process_id,
    base::Closure* cancel_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  Profile* profile = Profile::FromBrowserContext(browser_context);
  DCHECK(profile);

  DesktopNotificationService* service =
      DesktopNotificationServiceFactory::GetForProfile(profile);
  DCHECK(service);

  service->ShowDesktopNotification(
      params, render_process_id, delegate.Pass(), cancel_callback);
  profile->GetHostContentSettingsMap()->UpdateLastUsage(
      params.origin, params.origin, CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
}

void PlatformNotificationServiceImpl::DisplayPersistentNotification(
    content::BrowserContext* browser_context,
    int64 service_worker_registration_id,
    const content::ShowDesktopNotificationHostMsgParams& params,
    int render_process_id) {
  NOTIMPLEMENTED();
}

void PlatformNotificationServiceImpl::ClosePersistentNotification(
    content::BrowserContext* browser_context,
    const std::string& persistent_notification_id) {
  NOTIMPLEMENTED();
}
