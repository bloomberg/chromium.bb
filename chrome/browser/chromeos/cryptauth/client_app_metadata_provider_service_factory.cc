// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cryptauth/client_app_metadata_provider_service_factory.h"

#include "chrome/browser/chromeos/cryptauth/client_app_metadata_provider_service.h"
#include "chrome/browser/gcm/instance_id/instance_id_profile_service_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/network/network_handler.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace chromeos {

// static
ClientAppMetadataProviderService*
ClientAppMetadataProviderServiceFactory::GetForProfile(Profile* profile) {
  if (profile->IsOffTheRecord())
    return nullptr;

  return static_cast<ClientAppMetadataProviderService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ClientAppMetadataProviderServiceFactory*
ClientAppMetadataProviderServiceFactory::GetInstance() {
  return base::Singleton<ClientAppMetadataProviderServiceFactory>::get();
}

ClientAppMetadataProviderServiceFactory::
    ClientAppMetadataProviderServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "ClientAppMetadataProviderService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(instance_id::InstanceIDProfileServiceFactory::GetInstance());
}

ClientAppMetadataProviderServiceFactory::
    ~ClientAppMetadataProviderServiceFactory() = default;

KeyedService* ClientAppMetadataProviderServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  return new ClientAppMetadataProviderService(
      profile->GetPrefs(), NetworkHandler::Get()->network_state_handler(),
      instance_id::InstanceIDProfileServiceFactory::GetForProfile(profile));
}

}  // namespace chromeos
