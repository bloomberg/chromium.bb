// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/managed_mode_policy_provider_factory.h"

#include "chrome/browser/policy/managed_mode_policy_provider.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"

using policy::ManagedModePolicyProvider;

// static
ManagedModePolicyProviderFactory*
    ManagedModePolicyProviderFactory::GetInstance() {
  return Singleton<ManagedModePolicyProviderFactory>::get();
}

// static
ManagedModePolicyProvider*
    ManagedModePolicyProviderFactory::GetForProfile(Profile* profile) {
  return static_cast<ManagedModePolicyProvider*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

ManagedModePolicyProviderFactory::ManagedModePolicyProviderFactory()
    : ProfileKeyedServiceFactory("ManagedModePolicyProvider",
                                 ProfileDependencyManager::GetInstance()) {}
ManagedModePolicyProviderFactory::~ManagedModePolicyProviderFactory() {}

ProfileKeyedService* ManagedModePolicyProviderFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  return ManagedModePolicyProvider::Create(profile);
}
