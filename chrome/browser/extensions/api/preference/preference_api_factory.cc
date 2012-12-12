// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/preference/preference_api_factory.h"

#include "chrome/browser/extensions/api/preference/preference_api.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"

namespace extensions {

// static
PreferenceAPI* PreferenceAPIFactory::GetForProfile(Profile* profile) {
  return static_cast<PreferenceAPI*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
PreferenceAPIFactory* PreferenceAPIFactory::GetInstance() {
  return Singleton<PreferenceAPIFactory>::get();
}

PreferenceAPIFactory::PreferenceAPIFactory()
    : ProfileKeyedServiceFactory("PreferenceAPI",
                                 ProfileDependencyManager::GetInstance()) {
  DependsOn(ExtensionSystemFactory::GetInstance());
}

PreferenceAPIFactory::~PreferenceAPIFactory() {
}

ProfileKeyedService* PreferenceAPIFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  return new PreferenceAPI(profile);
}

bool PreferenceAPIFactory::ServiceIsCreatedWithProfile() const {
  return true;
}

bool PreferenceAPIFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace extensions
