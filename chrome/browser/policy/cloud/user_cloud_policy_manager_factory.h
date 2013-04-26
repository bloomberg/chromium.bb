// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_USER_CLOUD_POLICY_MANAGER_FACTORY_H_
#define CHROME_BROWSER_POLICY_CLOUD_USER_CLOUD_POLICY_MANAGER_FACTORY_H_

#include <map>

#include "base/basictypes.h"
#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_base_factory.h"

class Profile;

namespace policy {

class UserCloudPolicyManager;

// ProfileKeyedBaseFactory implementation for UserCloudPolicyManager
// instances that initialize per-profile cloud policy settings on the desktop
// platforms.
//
// UserCloudPolicyManager is handled different than other ProfileKeyedServices
// because it is a dependency of PrefService. Therefore, lifetime of instances
// is managed by Profile, Profile startup code invokes CreateForProfile()
// explicitly, takes ownership, and the instance is only deleted after
// PrefService destruction.
//
// TODO(mnissler): Remove the special lifetime management in favor of
// PrefService directly depending on UserCloudPolicyManager once the former has
// been converted to a ProfileKeyedService. See also http://crbug.com/131843 and
// http://crbug.com/131844.
class UserCloudPolicyManagerFactory : public ProfileKeyedBaseFactory {
 public:
  // Returns an instance of the UserCloudPolicyManagerFactory singleton.
  static UserCloudPolicyManagerFactory* GetInstance();

  // Returns the UserCloudPolicyManager instance associated with |profile|.
  static UserCloudPolicyManager* GetForProfile(Profile* profile);

  // Creates an instance for |profile|. Note that the caller is responsible for
  // managing the lifetime of the instance. Subsequent calls to GetForProfile()
  // will return the created instance as long as it lives.
  //
  // If |force_immediate_load| is true, policy is loaded synchronously from
  // UserCloudPolicyStore at startup.
  static scoped_ptr<UserCloudPolicyManager> CreateForProfile(
      Profile* profile,
      bool force_immediate_load);

 private:
  friend class UserCloudPolicyManager;
  friend struct DefaultSingletonTraits<UserCloudPolicyManagerFactory>;

  UserCloudPolicyManagerFactory();
  virtual ~UserCloudPolicyManagerFactory();

  // See comments for the static versions above.
  UserCloudPolicyManager* GetManagerForProfile(Profile* profile);
  scoped_ptr<UserCloudPolicyManager> CreateManagerForProfile(
      Profile* profile,
      bool force_immediate_load);

  // ProfileKeyedBaseFactory:
  virtual void ProfileShutdown(content::BrowserContext* profile) OVERRIDE;
  virtual void SetEmptyTestingFactory(
      content::BrowserContext* profile) OVERRIDE;
  virtual void CreateServiceNow(content::BrowserContext* profile) OVERRIDE;

  // Invoked by UserCloudPolicyManager to register/unregister instances.
  void Register(Profile* profile, UserCloudPolicyManager* instance);
  void Unregister(Profile* profile, UserCloudPolicyManager* instance);

  typedef std::map<Profile*, UserCloudPolicyManager*> ManagerMap;
  ManagerMap managers_;

  DISALLOW_COPY_AND_ASSIGN(UserCloudPolicyManagerFactory);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_USER_CLOUD_POLICY_MANAGER_FACTORY_H_
