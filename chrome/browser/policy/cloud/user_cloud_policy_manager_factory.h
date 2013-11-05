// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_USER_CLOUD_POLICY_MANAGER_FACTORY_H_
#define CHROME_BROWSER_POLICY_CLOUD_USER_CLOUD_POLICY_MANAGER_FACTORY_H_

#include <map>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "components/browser_context_keyed_service/browser_context_keyed_base_factory.h"

class Profile;

namespace base {
class SequencedTaskRunner;
}

namespace policy {

class UserCloudPolicyManager;

// BrowserContextKeyedBaseFactory implementation for UserCloudPolicyManager
// instances that initialize per-profile cloud policy settings on the desktop
// platforms.
//
// UserCloudPolicyManager is handled different than other
// BrowserContextKeyedServices because it is a dependency of PrefService.
// Therefore, lifetime of instances is managed by Profile, Profile startup code
// invokes CreateForProfile() explicitly, takes ownership, and the instance
// is only deleted after PrefService destruction.
//
// TODO(mnissler): Remove the special lifetime management in favor of
// PrefService directly depending on UserCloudPolicyManager once the former has
// been converted to a BrowserContextKeyedService.
// See also http://crbug.com/131843 and http://crbug.com/131844.
class UserCloudPolicyManagerFactory : public BrowserContextKeyedBaseFactory {
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
  static scoped_ptr<UserCloudPolicyManager> CreateForOriginalProfile(
      Profile* profile,
      bool force_immediate_load,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner);

  static UserCloudPolicyManager* RegisterForOffTheRecordProfile(
      Profile* original_profile,
      Profile* off_the_record_profile);

 private:
  friend class UserCloudPolicyManager;
  friend struct DefaultSingletonTraits<UserCloudPolicyManagerFactory>;

  UserCloudPolicyManagerFactory();
  virtual ~UserCloudPolicyManagerFactory();

  // See comments for the static versions above.
  UserCloudPolicyManager* GetManagerForProfile(Profile* profile);

  scoped_ptr<UserCloudPolicyManager> CreateManagerForOriginalProfile(
      Profile* profile,
      bool force_immediate_load,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner);

  UserCloudPolicyManager* RegisterManagerForOffTheRecordProfile(
      Profile* original_profile,
      Profile* off_the_record_profile);

  // BrowserContextKeyedBaseFactory:
  virtual void BrowserContextShutdown(
      content::BrowserContext* context) OVERRIDE;
  virtual void SetEmptyTestingFactory(
      content::BrowserContext* context) OVERRIDE;
  virtual void CreateServiceNow(content::BrowserContext* context) OVERRIDE;

  // Invoked by UserCloudPolicyManager to register/unregister instances.
  void Register(Profile* profile, UserCloudPolicyManager* instance);
  void Unregister(Profile* profile, UserCloudPolicyManager* instance);

  typedef std::map<Profile*, UserCloudPolicyManager*> ManagerMap;

  ManagerMap managers_;

  DISALLOW_COPY_AND_ASSIGN(UserCloudPolicyManagerFactory);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_USER_CLOUD_POLICY_MANAGER_FACTORY_H_
