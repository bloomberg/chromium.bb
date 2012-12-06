// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/cookies/cookies_api_factory.h"

#include "chrome/browser/extensions/api/cookies/cookies_api.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"

namespace extensions {

// static
CookiesAPIFactory* CookiesAPIFactory::GetInstance() {
  return Singleton<CookiesAPIFactory>::get();
}

CookiesAPIFactory::CookiesAPIFactory()
    : ProfileKeyedServiceFactory("CookiesAPI",
                                 ProfileDependencyManager::GetInstance()) {
  DependsOn(extensions::ExtensionSystemFactory::GetInstance());
}

CookiesAPIFactory::~CookiesAPIFactory() {
}

ProfileKeyedService* CookiesAPIFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  return new CookiesAPI(profile);
}

bool CookiesAPIFactory::ServiceIsCreatedWithProfile() const {
  return true;
}

bool CookiesAPIFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace extensions
