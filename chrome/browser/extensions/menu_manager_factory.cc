// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/menu_manager_factory.h"

#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"

namespace extensions {

// static
MenuManager* MenuManagerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<MenuManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
MenuManagerFactory* MenuManagerFactory::GetInstance() {
  return Singleton<MenuManagerFactory>::get();
}

MenuManagerFactory::MenuManagerFactory()
    : BrowserContextKeyedServiceFactory(
        "MenuManager",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ExtensionSystemFactory::GetInstance());
}

MenuManagerFactory::~MenuManagerFactory() {}

BrowserContextKeyedService*
    MenuManagerFactory::BuildServiceInstanceFor(
        content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new MenuManager(
      profile,
      ExtensionSystem::Get(profile)->state_store());
}

content::BrowserContext* MenuManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

bool MenuManagerFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool MenuManagerFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace extensions
