// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_MULTIDEVICE_SETUP_AUTH_TOKEN_VALIDATOR_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_MULTIDEVICE_SETUP_AUTH_TOKEN_VALIDATOR_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class KeyedService;
class Profile;

namespace chromeos {

namespace multidevice_setup {

class AuthTokenValidator;

// Owns AuthTokenValidator instances and associates them with Profiles.
class AuthTokenValidatorFactory : public BrowserContextKeyedServiceFactory {
 public:
  static AuthTokenValidator* GetForProfile(Profile* profile);

  static AuthTokenValidatorFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<AuthTokenValidatorFactory>;

  AuthTokenValidatorFactory();
  ~AuthTokenValidatorFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(AuthTokenValidatorFactory);
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_MULTIDEVICE_SETUP_AUTH_TOKEN_VALIDATOR_FACTORY_H_
