// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/omnibox/omnibox_api_factory.h"

#include "chrome/browser/extensions/api/omnibox/omnibox_api.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"

namespace extensions {

// static
OmniboxAPI* OmniboxAPIFactory::GetForProfile(Profile* profile) {
  return static_cast<OmniboxAPI*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
OmniboxAPIFactory* OmniboxAPIFactory::GetInstance() {
  return Singleton<OmniboxAPIFactory>::get();
}

OmniboxAPIFactory::OmniboxAPIFactory()
    : ProfileKeyedServiceFactory("OmniboxAPI",
                                 ProfileDependencyManager::GetInstance()) {
}

OmniboxAPIFactory::~OmniboxAPIFactory() {
}

ProfileKeyedService* OmniboxAPIFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  return new OmniboxAPI(profile);
}

bool OmniboxAPIFactory::ServiceIsCreatedWithProfile() const {
  return true;
}

bool OmniboxAPIFactory::ServiceRedirectedInIncognito() const {
  return true;
}

}  // namespace extensions
