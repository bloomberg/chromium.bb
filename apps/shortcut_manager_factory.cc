// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/shortcut_manager_factory.h"

#include "apps/shortcut_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"

namespace apps {

// static
ShortcutManager* ShortcutManagerFactory::GetForProfile(Profile* profile) {
  return static_cast<ShortcutManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

ShortcutManagerFactory* ShortcutManagerFactory::GetInstance() {
  return Singleton<ShortcutManagerFactory>::get();
}

ShortcutManagerFactory::ShortcutManagerFactory()
    : BrowserContextKeyedServiceFactory(
        "ShortcutManager",
        BrowserContextDependencyManager::GetInstance()) {
}

ShortcutManagerFactory::~ShortcutManagerFactory() {
}

BrowserContextKeyedService* ShortcutManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new ShortcutManager(static_cast<Profile*>(profile));
}

bool ShortcutManagerFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace apps
