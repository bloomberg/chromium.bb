// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/networking_private_event_router_factory.h"

#include "chrome/browser/chromeos/extensions/networking_private_event_router.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"

namespace chromeos {

// static
NetworkingPrivateEventRouter*
NetworkingPrivateEventRouterFactory::GetForProfile(Profile* profile) {
  return static_cast<NetworkingPrivateEventRouter*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
NetworkingPrivateEventRouterFactory*
NetworkingPrivateEventRouterFactory::GetInstance() {
  return Singleton<NetworkingPrivateEventRouterFactory>::get();
}

NetworkingPrivateEventRouterFactory::NetworkingPrivateEventRouterFactory()
    : ProfileKeyedServiceFactory(
          "NetworkingPrivateEventRouter",
          ProfileDependencyManager::GetInstance()) {
  DependsOn(extensions::ExtensionSystemFactory::GetInstance());
}

NetworkingPrivateEventRouterFactory::~NetworkingPrivateEventRouterFactory() {
}

ProfileKeyedService*
NetworkingPrivateEventRouterFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  return new NetworkingPrivateEventRouter(profile);
}

bool
NetworkingPrivateEventRouterFactory::ServiceHasOwnInstanceInIncognito() const {
  return true;
}

bool NetworkingPrivateEventRouterFactory::ServiceIsCreatedWithProfile() const {
  return true;
}

bool NetworkingPrivateEventRouterFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace chromeos
