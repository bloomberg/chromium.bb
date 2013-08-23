// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/privet_notifications_factory.h"

#include "base/command_line.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/local_discovery/privet_notifications.h"
#include "chrome/common/chrome_switches.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"

namespace local_discovery {

PrivetNotificationServiceFactory*
PrivetNotificationServiceFactory::GetInstance() {
  return Singleton<PrivetNotificationServiceFactory>::get();
}

PrivetNotificationServiceFactory::PrivetNotificationServiceFactory()
    : BrowserContextKeyedServiceFactory(
        "PrivetNotificationService",
        BrowserContextDependencyManager::GetInstance()) {
}

PrivetNotificationServiceFactory::~PrivetNotificationServiceFactory() {
}

BrowserContextKeyedService*
PrivetNotificationServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new PrivetNotificationService(
      profile, g_browser_process->notification_ui_manager());
}

bool
PrivetNotificationServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableDeviceDiscovery);
}

}  // namespace local_discovery
