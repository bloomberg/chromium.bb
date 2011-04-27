// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/profile_policy_connector_factory.h"

#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"

namespace policy {

// static
ProfilePolicyConnector* ProfilePolicyConnectorFactory::GetForProfile(
    Profile* profile) {
  return static_cast<ProfilePolicyConnector*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
ProfilePolicyConnectorFactory* ProfilePolicyConnectorFactory::GetInstance() {
  return Singleton<ProfilePolicyConnectorFactory>::get();
}

ProfilePolicyConnectorFactory::ProfilePolicyConnectorFactory()
    : ProfileKeyedServiceFactory(ProfileDependencyManager::GetInstance()) {
  // TODO(jknotten):
  //     DependsOn(URLRequestContextGetterFactory::GetInstance());
  //     DependsOn(PrefServiceFactory::GetInstance());
  // See comment in profile_policy_connector.h w.r.t. Shutdown()
}

ProfilePolicyConnectorFactory::~ProfilePolicyConnectorFactory() {
}

ProfileKeyedService* ProfilePolicyConnectorFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  return new ProfilePolicyConnector(profile);
}

}  // namespace policy
