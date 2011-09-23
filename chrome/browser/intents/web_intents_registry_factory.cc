// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/intents/web_intents_registry.h"
#include "chrome/browser/intents/web_intents_registry_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"

// static
WebIntentsRegistry* WebIntentsRegistryFactory::GetForProfile(Profile* profile) {
  return static_cast<WebIntentsRegistry*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

WebIntentsRegistryFactory::WebIntentsRegistryFactory()
    : ProfileKeyedServiceFactory(ProfileDependencyManager::GetInstance()) {
  // TODO(erg): For Shutdown() order, we need to:
  //     DependsOn(WebDataServiceFactory::GetInstance());
  //     DependsOn(ExtensionServiceFactory::GetInstance());
}

WebIntentsRegistryFactory::~WebIntentsRegistryFactory() {
}

// static
WebIntentsRegistryFactory* WebIntentsRegistryFactory::GetInstance() {
  return Singleton<WebIntentsRegistryFactory>::get();
}

ProfileKeyedService* WebIntentsRegistryFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  WebIntentsRegistry* registry = new WebIntentsRegistry;
  registry->Initialize(profile->GetWebDataService(Profile::EXPLICIT_ACCESS),
                       profile->GetExtensionService());
  return registry;
}

bool WebIntentsRegistryFactory::ServiceRedirectedInIncognito() {
  return false;
}
