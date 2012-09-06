// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/discovery/suggested_links_registry_factory.h"

#include "chrome/browser/extensions/api/discovery/suggested_links_registry.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"

namespace extensions {

// static
SuggestedLinksRegistry* SuggestedLinksRegistryFactory::GetForProfile(
    Profile* profile) {
  return static_cast<SuggestedLinksRegistry*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
SuggestedLinksRegistryFactory* SuggestedLinksRegistryFactory::GetInstance() {
  return Singleton<SuggestedLinksRegistryFactory>::get();
}

bool SuggestedLinksRegistryFactory::ServiceIsCreatedWithProfile() const {
  return true;
}

SuggestedLinksRegistryFactory::SuggestedLinksRegistryFactory()
    : ProfileKeyedServiceFactory("SuggestedLinksRegistry",
                                 ProfileDependencyManager::GetInstance()) {
  DependsOn(ExtensionSystemFactory::GetInstance());
}

SuggestedLinksRegistryFactory::~SuggestedLinksRegistryFactory() {
}

ProfileKeyedService* SuggestedLinksRegistryFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  return new SuggestedLinksRegistry();
}

bool SuggestedLinksRegistryFactory::ServiceRedirectedInIncognito() const {
  return true;
}

}  // namespace extensions
