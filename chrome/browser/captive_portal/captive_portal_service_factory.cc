// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/captive_portal/captive_portal_service_factory.h"

#include "chrome/browser/captive_portal/captive_portal_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"

namespace captive_portal {

// static
CaptivePortalService* CaptivePortalServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<CaptivePortalService*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
CaptivePortalServiceFactory* CaptivePortalServiceFactory::GetInstance() {
  return Singleton<CaptivePortalServiceFactory>::get();
}

CaptivePortalServiceFactory::CaptivePortalServiceFactory()
    : ProfileKeyedServiceFactory("CaptivePortalService",
                                 ProfileDependencyManager::GetInstance()) {
}

CaptivePortalServiceFactory::~CaptivePortalServiceFactory() {
}

ProfileKeyedService* CaptivePortalServiceFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  return new CaptivePortalService(profile);
}

bool CaptivePortalServiceFactory::ServiceHasOwnInstanceInIncognito() const {
  return true;
}

}  // namespace captive_portal
