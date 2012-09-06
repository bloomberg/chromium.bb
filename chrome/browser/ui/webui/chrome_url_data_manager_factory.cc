// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chrome_url_data_manager_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/profiles/profile_keyed_service.h"

// static
ChromeURLDataManager* ChromeURLDataManagerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<ChromeURLDataManager*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
ChromeURLDataManagerFactory* ChromeURLDataManagerFactory::GetInstance() {
  return Singleton<ChromeURLDataManagerFactory>::get();
}

ChromeURLDataManagerFactory::ChromeURLDataManagerFactory()
    : ProfileKeyedServiceFactory("ChromeURLDataManager",
                                 ProfileDependencyManager::GetInstance()) {
}

ChromeURLDataManagerFactory::~ChromeURLDataManagerFactory() {
}

ProfileKeyedService* ChromeURLDataManagerFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  return new ChromeURLDataManager(
      profile->GetChromeURLDataManagerBackendGetter());
}

bool ChromeURLDataManagerFactory::ServiceHasOwnInstanceInIncognito() const {
  return true;
}
