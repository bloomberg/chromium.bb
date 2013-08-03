// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/sync_notifier/chrome_notifier_service_factory.h"

#include "base/command_line.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/sync_notifier/chrome_notifier_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"

namespace notifier {

// static
ChromeNotifierService* ChromeNotifierServiceFactory::GetForProfile(
    Profile* profile, Profile::ServiceAccessType sat) {
  return static_cast<ChromeNotifierService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ChromeNotifierServiceFactory* ChromeNotifierServiceFactory::GetInstance() {
  return Singleton<ChromeNotifierServiceFactory>::get();
}

// static
bool ChromeNotifierServiceFactory::UseSyncedNotifications(
    CommandLine* command_line) {
  if (command_line->HasSwitch(switches::kDisableSyncSyncedNotifications))
    return false;
  if (command_line->HasSwitch(switches::kEnableSyncSyncedNotifications))
    return true;

  // enable it by default for canary and dev
  chrome::VersionInfo::Channel channel = chrome::VersionInfo::GetChannel();
  if (channel == chrome::VersionInfo::CHANNEL_UNKNOWN ||
      channel == chrome::VersionInfo::CHANNEL_DEV ||
      channel == chrome::VersionInfo::CHANNEL_CANARY)
    return true;
  else

  return false;
}

ChromeNotifierServiceFactory::ChromeNotifierServiceFactory()
    : BrowserContextKeyedServiceFactory(
        "ChromeNotifierService",
        BrowserContextDependencyManager::GetInstance()) {}

ChromeNotifierServiceFactory::~ChromeNotifierServiceFactory() {
}

BrowserContextKeyedService*
ChromeNotifierServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  NotificationUIManager* notification_manager =
      g_browser_process->notification_ui_manager();
  ChromeNotifierService* chrome_notifier_service =
      new ChromeNotifierService(static_cast<Profile*>(profile),
                                notification_manager);
  return chrome_notifier_service;
}

}  // namespace notifier
