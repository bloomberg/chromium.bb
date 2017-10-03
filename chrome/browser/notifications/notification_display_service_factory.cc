// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_display_service_factory.h"

#include "base/command_line.h"
#include "base/memory/singleton.h"
#include "base/win/windows_version.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/message_center_display_service.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/features.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

#if BUILDFLAG(ENABLE_NATIVE_NOTIFICATIONS)
#include "chrome/browser/notifications/native_notification_display_service.h"
#endif

// static
NotificationDisplayService* NotificationDisplayServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<NotificationDisplayService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true /* create */));
}

// static
NotificationDisplayServiceFactory*
NotificationDisplayServiceFactory::GetInstance() {
  return base::Singleton<NotificationDisplayServiceFactory>::get();
}

NotificationDisplayServiceFactory::NotificationDisplayServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "NotificationDisplayService",
          BrowserContextDependencyManager::GetInstance()) {}

// Selection of the implementation works as follows:
//   - Android always uses the NativeNotificationDisplayService.
//   - Mac uses the NativeNotificationDisplayService by default but
//     can revert to MessageCenterDisplayService via
//     chrome://flags#enable-native-notifications or Finch
//   - Linux uses MessageCenterDisplayService by default but can switch
//     to NativeNotificationDisplayService via
//     chrome://flags#enable-native-notifications
//   - Windows 10 update 2016/07 and above use MessageCenterDisplayService by
//     default but can switch to NativeNotificationDisplayService via
//     chrome://flags#enable-native-notifications
//   - All other platforms always use the MessageCenterDisplayService.
KeyedService* NotificationDisplayServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
#if BUILDFLAG(ENABLE_NATIVE_NOTIFICATIONS)
#if defined(OS_WIN)
  if (base::win::GetVersion() >= base::win::VERSION_WIN10_RS1 &&
      base::FeatureList::IsEnabled(features::kNativeNotifications)) {
    return new NativeNotificationDisplayService(
        Profile::FromBrowserContext(context),
        g_browser_process->notification_platform_bridge());
  }
#elif defined(OS_ANDROID)
  DCHECK(base::FeatureList::IsEnabled(features::kNativeNotifications));
  return new NativeNotificationDisplayService(
      Profile::FromBrowserContext(context),
      g_browser_process->notification_platform_bridge());
#endif
  if (base::FeatureList::IsEnabled(features::kNativeNotifications) &&
      g_browser_process->notification_platform_bridge()) {
    return new NativeNotificationDisplayService(
        Profile::FromBrowserContext(context),
        g_browser_process->notification_platform_bridge());
  }
#endif  // BUILDFLAG(ENABLE_NATIVE_NOTIFICATIONS)
  return new MessageCenterDisplayService(Profile::FromBrowserContext(context));
}

content::BrowserContext*
NotificationDisplayServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}
