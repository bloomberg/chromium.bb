// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/theme_service_factory.h"

#include "base/logging.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/themes/theme_service.h"
#include "content/common/notification_service.h"

#if defined(TOOLKIT_USES_GTK)
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#endif

// static
ThemeService* ThemeServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<ThemeService*>(
      GetInstance()->GetServiceForProfile(profile));
}

// static
const Extension* ThemeServiceFactory::GetThemeForProfile(Profile* profile) {
  std::string id = GetForProfile(profile)->GetThemeID();
  if (id == ThemeService::kDefaultThemeID)
    return NULL;

  return profile->GetExtensionService()->GetExtensionById(id, false);
}

// static
ThemeServiceFactory* ThemeServiceFactory::GetInstance() {
  return Singleton<ThemeServiceFactory>::get();
}

ThemeServiceFactory::ThemeServiceFactory()
    : ProfileKeyedServiceFactory(
        ProfileDependencyManager::GetInstance())
{}

ThemeServiceFactory::~ThemeServiceFactory() {}

ProfileKeyedService* ThemeServiceFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  ThemeService* provider = NULL;
#if defined(TOOLKIT_USES_GTK)
  provider = new GtkThemeService;
#else
  provider = new ThemeService;
#endif
  provider->Init(profile);

  return provider;
}

bool ThemeServiceFactory::ServiceRedirectedInIncognito() {
  return true;
}
