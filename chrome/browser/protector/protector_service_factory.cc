// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/protector/protector_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/protector/protector_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"

namespace protector {

// static
ProtectorService* ProtectorServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<ProtectorService*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

//  static
ProtectorServiceFactory* ProtectorServiceFactory::GetInstance() {
  return Singleton<ProtectorServiceFactory>::get();
}

ProtectorServiceFactory::ProtectorServiceFactory()
    : ProfileKeyedServiceFactory(ProfileDependencyManager::GetInstance()) {
  DependsOn(GlobalErrorServiceFactory::GetInstance());
  DependsOn(TemplateURLServiceFactory::GetInstance());
}

ProtectorServiceFactory::~ProtectorServiceFactory() {
}

ProfileKeyedService* ProtectorServiceFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  return new ProtectorService(profile);
}

bool ProtectorServiceFactory::ServiceRedirectedInIncognito() {
  return true;
}

}  // namespace protector
