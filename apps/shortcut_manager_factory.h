// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_SHORTCUT_MANAGER_FACTORY_H_
#define APPS_SHORTCUT_MANAGER_FACTORY_H_

#include "components/browser_context_keyed_service/browser_context_keyed_service_factory.h"

template<typename Type> struct DefaultSingletonTraits;

class Profile;

namespace apps {

class ShortcutManager;

// Singleton that owns all ShortcutManagers and associates them with
// Profiles. Listens for the Profile's destruction notification and cleans up
// the associated ShortcutManager.
// ShortcutManagers should not exist in incognito profiles.
class ShortcutManagerFactory : public BrowserContextKeyedServiceFactory {
 public:
  static ShortcutManager* GetForProfile(Profile* profile);

  static ShortcutManagerFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<ShortcutManagerFactory>;

  ShortcutManagerFactory();
  virtual ~ShortcutManagerFactory();

  // BrowserContextKeyedServiceFactory:
  virtual BrowserContextKeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const OVERRIDE;
  virtual bool ServiceIsCreatedWithBrowserContext() const OVERRIDE;
};

}  // namespace apps

#endif  // APPS_SHORTCUT_MANAGER_FACTORY_H_
