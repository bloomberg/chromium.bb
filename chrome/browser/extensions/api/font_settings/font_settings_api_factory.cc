// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/font_settings/font_settings_api_factory.h"

#include "chrome/browser/extensions/api/font_settings/font_settings_api.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"

namespace extensions {

// static
FontSettingsAPI* FontSettingsAPIFactory::GetForProfile(
    Profile* profile) {
  return static_cast<FontSettingsAPI*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
FontSettingsAPIFactory* FontSettingsAPIFactory::GetInstance() {
  return Singleton<FontSettingsAPIFactory>::get();
}

FontSettingsAPIFactory::FontSettingsAPIFactory()
    : ProfileKeyedServiceFactory("FontSettingsAPI",
                                 ProfileDependencyManager::GetInstance()) {
  DependsOn(ExtensionSystemFactory::GetInstance());
}

FontSettingsAPIFactory::~FontSettingsAPIFactory() {
}

ProfileKeyedService* FontSettingsAPIFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  return new FontSettingsAPI(profile);
}

bool FontSettingsAPIFactory::ServiceIsCreatedWithProfile() const {
  return true;
}

bool FontSettingsAPIFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace extensions
