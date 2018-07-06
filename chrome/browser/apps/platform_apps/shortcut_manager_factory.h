// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_PLATFORM_APPS_SHORTCUT_MANAGER_FACTORY_H_
#define CHROME_BROWSER_APPS_PLATFORM_APPS_SHORTCUT_MANAGER_FACTORY_H_

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace base {
template <typename Type>
struct DefaultSingletonTraits;
}

class Profile;

class AppShortcutManager;

// Singleton that owns all AppShortcutManagers and associates them with
// Profiles. Listens for the Profile's destruction notification and cleans up
// the associated AppShortcutManager.
// AppShortcutManagers should not exist in incognito profiles.
class AppShortcutManagerFactory : public BrowserContextKeyedServiceFactory {
 public:
  static AppShortcutManager* GetForProfile(Profile* profile);

  static AppShortcutManagerFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<AppShortcutManagerFactory>;

  AppShortcutManagerFactory();
  ~AppShortcutManagerFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};
#endif  // CHROME_BROWSER_APPS_PLATFORM_APPS_SHORTCUT_MANAGER_FACTORY_H_
