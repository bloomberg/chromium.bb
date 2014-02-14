// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/install_tracker_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/install_tracker.h"
#include "chrome/browser/profiles/profile.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"

namespace extensions {

// static
InstallTracker* InstallTrackerFactory::GetForProfile(Profile* profile) {
  return static_cast<InstallTracker*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

InstallTrackerFactory* InstallTrackerFactory::GetInstance() {
  return Singleton<InstallTrackerFactory>::get();
}

InstallTrackerFactory::InstallTrackerFactory()
    : BrowserContextKeyedServiceFactory(
        "InstallTracker",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
}

InstallTrackerFactory::~InstallTrackerFactory() {
}

BrowserContextKeyedService* InstallTrackerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  return new InstallTracker(profile, service->extension_prefs());
}

content::BrowserContext* InstallTrackerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // The installs themselves are routed to the non-incognito profile and so
  // should the install progress.
  return ExtensionsBrowserClient::Get()->GetOriginalContext(context);
}

}  // namespace extensions
