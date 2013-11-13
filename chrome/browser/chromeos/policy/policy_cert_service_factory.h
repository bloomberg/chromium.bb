// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_POLICY_CERT_SERVICE_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_POLICY_CERT_SERVICE_FACTORY_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service_factory.h"

template <typename T> struct DefaultSingletonTraits;

class Profile;

namespace policy {

class PolicyCertService;
class PolicyCertVerifier;

// Factory to create PolicyCertServices.
class PolicyCertServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns an existing PolicyCertService for |profile|. See
  // CreateForProfile.
  static PolicyCertService* GetForProfile(Profile* profile);

  // Creates a new PolicyCertService and returns the associated
  // PolicyCertVerifier. Returns NULL if this service isn't allowed for
  // |profile|, i.e. if NetworkConfigurationUpdater doesn't exist.
  // This service is created separately for the original profile and the
  // incognito profile.
  // Note: NetworkConfigurationUpdater is currently only created for the primary
  // user's profile.
  static scoped_ptr<PolicyCertVerifier> CreateForProfile(Profile* profile);

  static PolicyCertServiceFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<PolicyCertServiceFactory>;

  PolicyCertServiceFactory();
  virtual ~PolicyCertServiceFactory();

  // BrowserContextKeyedServiceFactory:
  virtual BrowserContextKeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const OVERRIDE;
  virtual content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const OVERRIDE;
  virtual void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) OVERRIDE;
  virtual bool ServiceIsNULLWhileTesting() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(PolicyCertServiceFactory);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_POLICY_CERT_SERVICE_FACTORY_H_
