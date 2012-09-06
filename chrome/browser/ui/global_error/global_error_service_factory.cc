// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_error/global_error_service_factory.h"

#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/ui/global_error/global_error_service.h"

// static
GlobalErrorService* GlobalErrorServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<GlobalErrorService*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
GlobalErrorServiceFactory* GlobalErrorServiceFactory::GetInstance() {
  return Singleton<GlobalErrorServiceFactory>::get();
}

GlobalErrorServiceFactory::GlobalErrorServiceFactory()
    : ProfileKeyedServiceFactory("GlobalErrorService",
                                 ProfileDependencyManager::GetInstance()) {
}

GlobalErrorServiceFactory::~GlobalErrorServiceFactory() {
}

ProfileKeyedService* GlobalErrorServiceFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  return new GlobalErrorService(profile);
}

bool GlobalErrorServiceFactory::ServiceRedirectedInIncognito() const {
  return true;
}
