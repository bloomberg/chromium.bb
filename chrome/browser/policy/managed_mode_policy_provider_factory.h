// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MANAGED_MODE_POLICY_PROVIDER_FACTORY_H_
#define CHROME_BROWSER_POLICY_MANAGED_MODE_POLICY_PROVIDER_FACTORY_H_
#pragma once

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace policy {
class ManagedModePolicyProvider;
}

class ManagedModePolicyProviderFactory : public ProfileKeyedServiceFactory {
 public:
  static ManagedModePolicyProviderFactory* GetInstance();

  static policy::ManagedModePolicyProvider* GetForProfile(Profile* profile);

 private:
  friend struct DefaultSingletonTraits<ManagedModePolicyProviderFactory>;

  ManagedModePolicyProviderFactory();
  virtual ~ManagedModePolicyProviderFactory();

  virtual ProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(ManagedModePolicyProviderFactory);
};

#endif  // CHROME_BROWSER_POLICY_MANAGED_MODE_POLICY_PROVIDER_FACTORY_H_
