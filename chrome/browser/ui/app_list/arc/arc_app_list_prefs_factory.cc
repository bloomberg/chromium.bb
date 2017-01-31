// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs_factory.h"

#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service_manager.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"

bool ArcAppListPrefsFactory::is_sync_test_ = false;

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

// static
void ArcAppListPrefsFactory::SetFactoryForSyncTest() {
  is_sync_test_ = true;
}

void ArcAppListPrefsFactory::RecreateServiceInstanceForTesting(
    content::BrowserContext* context) {
  Disassociate(context);
  BuildServiceInstanceFor(context);
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
  if (!arc::IsArcAllowedForProfile(profile))
    return nullptr;

  if (is_sync_test_) {
    sync_test_app_instance_holders_[context].reset(
        new arc::InstanceHolder<arc::mojom::AppInstance>());
    return ArcAppListPrefs::Create(
        profile, sync_test_app_instance_holders_[context].get());
  }

  auto* arc_service_manager = arc::ArcServiceManager::Get();
  if (!arc_service_manager)
    return nullptr;

  return ArcAppListPrefs::Create(
      profile, arc_service_manager->arc_bridge_service()->app());
}

content::BrowserContext* ArcAppListPrefsFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // This matches the logic in ExtensionSyncServiceFactory, which uses the
  // orginal browser context.
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}
