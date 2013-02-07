// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_NETWORKING_PRIVATE_EVENT_ROUTER_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_NETWORKING_PRIVATE_EVENT_ROUTER_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace chromeos {

class NetworkingPrivateEventRouter;

// This is a factory class used by the ProfileDependencyManager to instantiate
// the networking event router per profile (since the extension event router is
// per profile).
class NetworkingPrivateEventRouterFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the NetworkingPrivateEventRouter for |profile|, creating it if
  // it is not yet created.
  static NetworkingPrivateEventRouter* GetForProfile(Profile* profile);

  // Returns the NetworkingPrivateEventRouterFactory instance.
  static NetworkingPrivateEventRouterFactory* GetInstance();

 protected:
  // ProfileKeyedBaseFactory overrides:
  virtual bool ServiceHasOwnInstanceInIncognito() const OVERRIDE;
  virtual bool ServiceIsCreatedWithProfile() const OVERRIDE;
  virtual bool ServiceIsNULLWhileTesting() const OVERRIDE;

 private:
  friend struct DefaultSingletonTraits<NetworkingPrivateEventRouterFactory>;

  NetworkingPrivateEventRouterFactory();
  virtual ~NetworkingPrivateEventRouterFactory();

  // ProfileKeyedServiceFactory:
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(NetworkingPrivateEventRouterFactory);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_NETWORKING_PRIVATE_EVENT_ROUTER_FACTORY_H_
