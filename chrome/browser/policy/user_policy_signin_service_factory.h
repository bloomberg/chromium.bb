// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_USER_POLICY_SIGNIN_SERVICE_FACTORY_H_
#define CHROME_BROWSER_POLICY_USER_POLICY_SIGNIN_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace policy {

class UserPolicySigninService;

// Singleton that owns all UserPolicySigninServices and creates/deletes them as
// new Profiles are created/shutdown.
class UserPolicySigninServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns an instance of the UserPolicySigninServiceFactory singleton.
  static UserPolicySigninServiceFactory* GetInstance();

  // Returns the instance of UserPolicySigninService for the passed |profile|.
  // Used primarily for testing.
  static UserPolicySigninService* GetForProfile(Profile* profile);

 protected:
  // ProfileKeyedServiceFactory implementation.
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE;

  // Overridden to cause this object to be created when the profile is created.
  virtual bool ServiceIsCreatedWithProfile() const OVERRIDE;

  // Register the preferences related to cloud-based user policy.
  virtual void RegisterUserPrefs(PrefService* user_prefs) OVERRIDE;

 private:
  friend struct DefaultSingletonTraits<UserPolicySigninServiceFactory>;

  UserPolicySigninServiceFactory();
  virtual ~UserPolicySigninServiceFactory();

  DISALLOW_COPY_AND_ASSIGN(UserPolicySigninServiceFactory);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_USER_POLICY_SIGNIN_SERVICE_FACTORY_H_
