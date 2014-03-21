// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_PROFILE_OAUTH2_TOKEN_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SIGNIN_PROFILE_OAUTH2_TOKEN_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class ProfileOAuth2TokenService;
class Profile;

#if defined(OS_ANDROID)
class AndroidProfileOAuth2TokenService;
#else
class MutableProfileOAuth2TokenService;
#endif

// Singleton that owns all ProfileOAuth2TokenServices and associates them with
// Profiles. Listens for the Profile's destruction notification and cleans up
// the associated ProfileOAuth2TokenService.
class ProfileOAuth2TokenServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the instance of ProfileOAuth2TokenService associated with this
  // profile (creating one if none exists). Returns NULL if this profile
  // cannot have a ProfileOAuth2TokenService (for example, if |profile| is
  // incognito).
  static ProfileOAuth2TokenService* GetForProfile(Profile* profile);

  // Returns the platform specific instance of ProfileOAuth2TokenService
  // associated with this profile (creating one if none exists). Returns NULL
  // if this profile cannot have a ProfileOAuth2TokenService (for example,
  // if |profile| is incognito).
  #if defined(OS_ANDROID)
  static AndroidProfileOAuth2TokenService* GetPlatformSpecificForProfile(
      Profile* profile);
  #else
  static MutableProfileOAuth2TokenService* GetPlatformSpecificForProfile(
      Profile* profile);
  #endif

  // Returns an instance of the ProfileOAuth2TokenServiceFactory singleton.
  static ProfileOAuth2TokenServiceFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<ProfileOAuth2TokenServiceFactory>;

#if defined(OS_ANDROID)
  typedef AndroidProfileOAuth2TokenService PlatformSpecificOAuth2TokenService;
#else
  typedef MutableProfileOAuth2TokenService PlatformSpecificOAuth2TokenService;
#endif

  ProfileOAuth2TokenServiceFactory();
  virtual ~ProfileOAuth2TokenServiceFactory();

  // BrowserContextKeyedServiceFactory implementation.
  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(ProfileOAuth2TokenServiceFactory);
};

#endif  // CHROME_BROWSER_SIGNIN_PROFILE_OAUTH2_TOKEN_SERVICE_FACTORY_H_
