// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_PROFILE_OAUTH2_TOKEN_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SIGNIN_PROFILE_OAUTH2_TOKEN_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class ProfileOAuth2TokenService;
class Profile;

// Singleton that owns all ProfileOAuth2TokenServices and associates them with
// Profiles. Listens for the Profile's destruction notification and cleans up
// the associated ProfileOAuth2TokenService.
class ProfileOAuth2TokenServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the instance of ProfileOAuth2TokenService associated with this
  // profile (creating one if none exists). Returns NULL if this profile
  // cannot have a ProfileOAuth2TokenService (for example, if |profile| is
  // incognito).  On Android, returns the AndroidProfileOAuth2TokenService
  // specialization.
  static ProfileOAuth2TokenService* GetForProfile(Profile* profile);

  // Returns an instance of the ProfileOAuth2TokenServiceFactory singleton.
  static ProfileOAuth2TokenServiceFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<ProfileOAuth2TokenServiceFactory>;

  ProfileOAuth2TokenServiceFactory();
  virtual ~ProfileOAuth2TokenServiceFactory();

  // ProfileKeyedServiceFactory implementation.
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(ProfileOAuth2TokenServiceFactory);
};

#endif  // CHROME_BROWSER_SIGNIN_PROFILE_OAUTH2_TOKEN_SERVICE_FACTORY_H_
