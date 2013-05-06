// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_MANAGER_FACTORY_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_MANAGER_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class SigninManager;
class SigninManagerBase;
class PrefRegistrySimple;
class Profile;

// Singleton that owns all SigninManagers and associates them with
// Profiles. Listens for the Profile's destruction notification and cleans up
// the associated SigninManager.
class SigninManagerFactory : public ProfileKeyedServiceFactory {
 public:

#if defined(OS_CHROMEOS)
  // Returns the instance of SigninManager associated with this profile
  // (creating one if none exists). Returns NULL if this profile cannot have a
  // SigninManager (for example, if |profile| is incognito).
  static SigninManagerBase* GetForProfile(Profile* profile);

  // Returns the instance of SigninManager associated with this profile. Returns
  // null if no SigninManager instance currently exists (will not create a new
  // instance).
  static SigninManagerBase* GetForProfileIfExists(Profile* profile);
#else
  // On non-ChromeOS platforms, the SigninManager the factory creates will be
  // an instance of the extended SigninManager class.
  static SigninManager* GetForProfile(Profile* profile);
  static SigninManager* GetForProfileIfExists(Profile* profile);
#endif

  // Returns an instance of the SigninManagerFactory singleton.
  static SigninManagerFactory* GetInstance();

  // Implementation of ProfileKeyedServiceFactory (public so tests can call it).
  virtual void RegisterUserPrefs(
      user_prefs::PrefRegistrySyncable* registry) OVERRIDE;

  // Registers the browser-global prefs used by SigninManager.
  static void RegisterPrefs(PrefRegistrySimple* registry);

 private:
  friend struct DefaultSingletonTraits<SigninManagerFactory>;

  SigninManagerFactory();
  virtual ~SigninManagerFactory();

  // ProfileKeyedServiceFactory:
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const OVERRIDE;
};

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_MANAGER_FACTORY_H_
