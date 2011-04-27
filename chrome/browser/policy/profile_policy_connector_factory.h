// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_POLICY_PROFILE_POLICY_CONNECTOR_FACTORY_H_
#define CHROME_BROWSER_POLICY_PROFILE_POLICY_CONNECTOR_FACTORY_H_

#include "base/compiler_specific.h"
#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace policy {

class ProfilePolicyConnector;

// Singleton that owns all ProfilePolicyConnectors and associates them with
// Profiles. Listens for the Profile's destruction notification and cleans up
// the associated ProfilePolicyConnector.

class ProfilePolicyConnectorFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the ProfilePolicyConnector that contains profile-specific
  // policy bits for |profile|.
  static ProfilePolicyConnector* GetForProfile(Profile* profile);

  static ProfilePolicyConnectorFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<ProfilePolicyConnectorFactory>;

  ProfilePolicyConnectorFactory();
  virtual ~ProfilePolicyConnectorFactory();

  // ProfileKeyedServiceFactory:
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(ProfilePolicyConnectorFactory);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_PROFILE_POLICY_CONNECTOR_FACTORY_H_
