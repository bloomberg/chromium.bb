// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/instant_service_factory.h"

#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
InstantService* InstantServiceFactory::GetForProfile(Profile* profile) {
  if (!chrome::IsInstantExtendedAPIEnabled())
    return NULL;

  return static_cast<InstantService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
InstantServiceFactory* InstantServiceFactory::GetInstance() {
  return Singleton<InstantServiceFactory>::get();
}

InstantServiceFactory::InstantServiceFactory()
    : BrowserContextKeyedServiceFactory(
        "InstantService",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(TemplateURLServiceFactory::GetInstance());
#if defined(ENABLE_THEMES)
  DependsOn(ThemeServiceFactory::GetInstance());
#endif
}

InstantServiceFactory::~InstantServiceFactory() {
}

content::BrowserContext* InstantServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

KeyedService* InstantServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return chrome::IsInstantExtendedAPIEnabled() ?
      new InstantService(static_cast<Profile*>(profile)) : NULL;
}
