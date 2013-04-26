// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_TOKEN_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SIGNIN_TOKEN_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class TokenService;
class Profile;

// Singleton that owns all TokenServices and associates them with Profiles.
// Listens for the Profile's destruction notification and cleans up the
// associated TokenService.
class TokenServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the instance of TokenService associated with this profile
  // (creating one if none exists). Returns NULL if this profile cannot have a
  // TokenService (for example, if |profile| is incognito).
  static TokenService* GetForProfile(Profile* profile);

  // Returns an instance of the TokenServiceFactory singleton.
  static TokenServiceFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<TokenServiceFactory>;

  TokenServiceFactory();
  virtual ~TokenServiceFactory();

  // ProfileKeyedServiceFactory:
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(TokenServiceFactory);
};

#endif  // CHROME_BROWSER_SIGNIN_TOKEN_SERVICE_FACTORY_H_
