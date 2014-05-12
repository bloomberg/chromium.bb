// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/start_page_service_factory.h"

#include "base/command_line.h"
#include "chrome/browser/extensions/install_tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/start_page_service.h"
#include "chrome/common/chrome_switches.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"
#include "ui/app_list/app_list_switches.h"

namespace app_list {

// static
StartPageService* StartPageServiceFactory::GetForProfile(Profile* profile) {
  if (!app_list::switches::IsExperimentalAppListEnabled() &&
      !app_list::switches::IsVoiceSearchEnabled()) {
    return NULL;
  }

  return static_cast<StartPageService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
StartPageServiceFactory* StartPageServiceFactory::GetInstance() {
  return Singleton<StartPageServiceFactory>::get();
}

StartPageServiceFactory::StartPageServiceFactory()
    : BrowserContextKeyedServiceFactory(
        "AppListStartPageService",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(
      extensions::ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
  DependsOn(extensions::InstallTrackerFactory::GetInstance());
}

StartPageServiceFactory::~StartPageServiceFactory() {}

KeyedService* StartPageServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  return new StartPageService(profile);
}

}  // namespace app_list
