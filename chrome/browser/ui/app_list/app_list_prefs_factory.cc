// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_prefs_factory.h"

#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_prefs.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/extensions_browser_client.h"

namespace app_list {

// static
AppListPrefs* AppListPrefsFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<AppListPrefs*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
AppListPrefsFactory* AppListPrefsFactory::GetInstance() {
  return base::Singleton<AppListPrefsFactory>::get();
}

void AppListPrefsFactory::SetInstanceForTesting(
    content::BrowserContext* context,
    scoped_ptr<AppListPrefs> prefs) {
  Associate(context, std::move(prefs));
}

AppListPrefsFactory::AppListPrefsFactory()
    : BrowserContextKeyedServiceFactory(
          "AppListPrefs",
          BrowserContextDependencyManager::GetInstance()) {
}

AppListPrefsFactory::~AppListPrefsFactory() {
}

KeyedService* AppListPrefsFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return AppListPrefs::Create(Profile::FromBrowserContext(context)->GetPrefs());
}

content::BrowserContext* AppListPrefsFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return extensions::ExtensionsBrowserClient::Get()->GetOriginalContext(
      context);
}

}  // namespace app_list
