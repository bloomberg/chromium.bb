// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_OAUTH2_TOKEN_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SIGNIN_OAUTH2_TOKEN_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class OAuth2TokenService;
class Profile;

// Singleton that owns all OAuth2TokenServices and associates them with
// Profiles. Listens for the Profile's destruction notification and cleans up
// the associated OAuth2TokenService.
class OAuth2TokenServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the instance of OAuth2TokenService associated with this profile
  // (creating one if none exists). Returns NULL if this profile cannot have a
  // OAuth2TokenService (for example, if |profile| is incognito).
  static OAuth2TokenService* GetForProfile(Profile* profile);

  // Returns an instance of the OAuth2TokenServiceFactory singleton.
  static OAuth2TokenServiceFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<OAuth2TokenServiceFactory>;

  OAuth2TokenServiceFactory();
  virtual ~OAuth2TokenServiceFactory();

  // ProfileKeyedServiceFactory implementation.
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(OAuth2TokenServiceFactory);
};

#endif  // CHROME_BROWSER_SIGNIN_OAUTH2_TOKEN_SERVICE_FACTORY_H_
