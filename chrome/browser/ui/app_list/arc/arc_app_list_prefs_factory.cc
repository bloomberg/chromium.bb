// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs_factory.h"

#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"

// static
ArcAppListPrefs* ArcAppListPrefsFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ArcAppListPrefs*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ArcAppListPrefsFactory* ArcAppListPrefsFactory::GetInstance() {
  return base::Singleton<ArcAppListPrefsFactory>::get();
}

ArcAppListPrefsFactory::ArcAppListPrefsFactory()
    : BrowserContextKeyedServiceFactory(
          "ArcAppListPrefs",
          BrowserContextDependencyManager::GetInstance()) {
}

ArcAppListPrefsFactory::~ArcAppListPrefsFactory() {
}

KeyedService* ArcAppListPrefsFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  return ArcAppListPrefs::Create(profile->GetPath(), profile->GetPrefs());
}

content::BrowserContext* ArcAppListPrefsFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // This matches the logic in ExtensionSyncServiceFactory, which uses the
  // orginal browser context.
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}
