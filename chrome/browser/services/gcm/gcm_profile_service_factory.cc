// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/services/gcm/gcm_profile_service_factory.h"

#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/services/gcm/gcm_client_factory.h"
#include "chrome/browser/services/gcm/gcm_profile_service.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"

namespace gcm {

// static
GCMProfileService* GCMProfileServiceFactory::GetForProfile(Profile* profile) {
  if (!gcm::GCMProfileService::IsGCMEnabled(profile))
    return NULL;

  return static_cast<GCMProfileService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
GCMProfileServiceFactory* GCMProfileServiceFactory::GetInstance() {
  return Singleton<GCMProfileServiceFactory>::get();
}

GCMProfileServiceFactory::GCMProfileServiceFactory()
    : BrowserContextKeyedServiceFactory(
        "GCMProfileService",
        BrowserContextDependencyManager::GetInstance()) {
}

GCMProfileServiceFactory::~GCMProfileServiceFactory() {
}

BrowserContextKeyedService* GCMProfileServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  if (!gcm::GCMProfileService::IsGCMEnabled(profile))
    return NULL;
  GCMProfileService* service = new GCMProfileService(profile);
  scoped_ptr<GCMClientFactory> gcm_client_factory(new GCMClientFactory);
  service->Initialize(gcm_client_factory.Pass());
  return service;
}

content::BrowserContext* GCMProfileServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

}  // namespace gcm
