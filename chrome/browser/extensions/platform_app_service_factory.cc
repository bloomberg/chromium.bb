// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/platform_app_service_factory.h"

#include "chrome/browser/profiles/profile_dependency_manager.h"

namespace extensions {

// static
PlatformAppService* PlatformAppServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<PlatformAppService*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

PlatformAppServiceFactory* PlatformAppServiceFactory::GetInstance() {
  return Singleton<PlatformAppServiceFactory>::get();
}

PlatformAppServiceFactory::PlatformAppServiceFactory()
    : ProfileKeyedServiceFactory("PlatformAppService",
                                 ProfileDependencyManager::GetInstance()) {
}

PlatformAppServiceFactory::~PlatformAppServiceFactory() {
}

ProfileKeyedService* PlatformAppServiceFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  return new PlatformAppService(profile);
}

bool PlatformAppServiceFactory::ServiceHasOwnInstanceInIncognito() const {
  return true;
}

bool PlatformAppServiceFactory::ServiceIsCreatedWithProfile() const {
  return true;
}

bool PlatformAppServiceFactory::ServiceIsNULLWhileTesting() const {
  return false;
}

}  // namespace extensions
