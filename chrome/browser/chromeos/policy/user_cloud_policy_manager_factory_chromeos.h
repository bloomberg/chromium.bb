// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_USER_CLOUD_POLICY_MANAGER_FACTORY_CHROMEOS_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_USER_CLOUD_POLICY_MANAGER_FACTORY_CHROMEOS_H_

#include <map>

#include "base/basictypes.h"
#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_base_factory.h"

class Profile;

namespace content {
class BrowserContext;
}

namespace policy {

class UserCloudPolicyManagerChromeOS;

// ProfileKeyedBaseFactory implementation for UserCloudPolicyManagerChromeOS
// instances that initialize per-profile cloud policy settings on ChromeOS.
//
// UserCloudPolicyManagerChromeOS is handled different than other
// ProfileKeyedServices because it is a dependency of PrefService. Therefore,
// lifetime of instances is managed by Profile, Profile startup code invokes
// CreateForProfile() explicitly, takes ownership, and the instance is only
// deleted after PrefService destruction.
//
// TODO(mnissler): Remove the special lifetime management in favor of
// PrefService directly depending on UserCloudPolicyManagerChromeOS once the
// former has been converted to a ProfileKeyedService.
// See also http://crbug.com/131843 and http://crbug.com/131844.
class UserCloudPolicyManagerFactoryChromeOS : public ProfileKeyedBaseFactory {
 public:
  // Returns an instance of the UserCloudPolicyManagerFactoryChromeOS singleton.
  static UserCloudPolicyManagerFactoryChromeOS* GetInstance();

  // Returns the UserCloudPolicyManagerChromeOS instance associated with
  // |profile|.
  static UserCloudPolicyManagerChromeOS* GetForProfile(Profile* profile);

  // Creates an instance for |profile|. Note that the caller is responsible for
  // managing the lifetime of the instance. Subsequent calls to GetForProfile()
  // will return the created instance as long as it lives.
  //
  // If |force_immediate_load| is true, policy is loaded synchronously from
  // UserCloudPolicyStore at startup.
  static scoped_ptr<UserCloudPolicyManagerChromeOS> CreateForProfile(
      Profile* profile,
      bool force_immediate_load);

 private:
  friend struct DefaultSingletonTraits<UserCloudPolicyManagerFactoryChromeOS>;

  UserCloudPolicyManagerFactoryChromeOS();
  virtual ~UserCloudPolicyManagerFactoryChromeOS();

  // See comments for the static versions above.
  UserCloudPolicyManagerChromeOS* GetManagerForProfile(Profile* profile);
  scoped_ptr<UserCloudPolicyManagerChromeOS> CreateManagerForProfile(
      Profile* profile,
      bool force_immediate_load);

  // ProfileKeyedBaseFactory:
  virtual void ProfileShutdown(content::BrowserContext* context) OVERRIDE;
  virtual void ProfileDestroyed(content::BrowserContext* context) OVERRIDE;
  virtual void SetEmptyTestingFactory(
      content::BrowserContext* context) OVERRIDE;
  virtual void CreateServiceNow(content::BrowserContext* context) OVERRIDE;

  typedef std::map<Profile*, UserCloudPolicyManagerChromeOS*> ManagerMap;
  ManagerMap managers_;

  DISALLOW_COPY_AND_ASSIGN(UserCloudPolicyManagerFactoryChromeOS);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_USER_CLOUD_POLICY_MANAGER_FACTORY_CHROMEOS_H_
