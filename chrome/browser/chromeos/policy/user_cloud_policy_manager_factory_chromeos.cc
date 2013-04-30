// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_factory_chromeos.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_store_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/policy/cloud/cloud_policy_manager.h"
#include "chrome/browser/policy/cloud/device_management_service.h"
#include "chrome/browser/policy/cloud/resource_cache.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/chromeos_paths.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "net/url_request/url_request_context_getter.h"

namespace policy {

namespace {

// Subdirectory in the user's profile for storing legacy user policies.
const base::FilePath::CharType kDeviceManagementDir[] =
    FILE_PATH_LITERAL("Device Management");
// File in the above directory for storing legacy user policy dmtokens.
const base::FilePath::CharType kToken[] = FILE_PATH_LITERAL("Token");
// This constant is used to build two different paths. It can be a file inside
// kDeviceManagementDir where legacy user policy data is stored, and it can be
// a directory inside the profile directory where other resources are stored.
const base::FilePath::CharType kPolicy[] = FILE_PATH_LITERAL("Policy");
// Directory under kPolicy, in the user's profile dir, where external policy
// resources are stored.
const base::FilePath::CharType kResourceDir[] = FILE_PATH_LITERAL("Resources");

}  // namespace

// static
UserCloudPolicyManagerFactoryChromeOS*
    UserCloudPolicyManagerFactoryChromeOS::GetInstance() {
  return Singleton<UserCloudPolicyManagerFactoryChromeOS>::get();
}

// static
UserCloudPolicyManagerChromeOS*
    UserCloudPolicyManagerFactoryChromeOS::GetForProfile(
        Profile* profile) {
  return GetInstance()->GetManagerForProfile(profile);
}

// static
scoped_ptr<UserCloudPolicyManagerChromeOS>
    UserCloudPolicyManagerFactoryChromeOS::CreateForProfile(
        Profile* profile,
        bool force_immediate_load) {
  return GetInstance()->CreateManagerForProfile(profile, force_immediate_load);
}

UserCloudPolicyManagerFactoryChromeOS::UserCloudPolicyManagerFactoryChromeOS()
    : ProfileKeyedBaseFactory("UserCloudPolicyManagerChromeOS",
                              ProfileDependencyManager::GetInstance()) {}

UserCloudPolicyManagerFactoryChromeOS::
    ~UserCloudPolicyManagerFactoryChromeOS() {}

UserCloudPolicyManagerChromeOS*
    UserCloudPolicyManagerFactoryChromeOS::GetManagerForProfile(
        Profile* profile) {
  // Get the manager for the original profile, since the PolicyService is
  // also shared between the incognito Profile and the original Profile.
  ManagerMap::const_iterator it = managers_.find(profile->GetOriginalProfile());
  return it != managers_.end() ? it->second : NULL;
}

scoped_ptr<UserCloudPolicyManagerChromeOS>
    UserCloudPolicyManagerFactoryChromeOS::CreateManagerForProfile(
        Profile* profile,
        bool force_immediate_load) {
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  // Don't initialize cloud policy for the signin profile.
  if (chromeos::ProfileHelper::IsSigninProfile(profile))
    return scoped_ptr<UserCloudPolicyManagerChromeOS>();

  // |user| should never be NULL except for the signin profile. This object is
  // created as part of the Profile creation, which happens right after
  // sign-in. The just-signed-in User is the active user during that time.
  chromeos::UserManager* user_manager = chromeos::UserManager::Get();
  chromeos::User* user = user_manager->GetActiveUser();
  CHECK(user);

  // TODO(joaodasilva): Initialize user policy for all accounts once the session
  // manager is ready for multi-profile policy. Note that there can still be
  // only one delegate for the global user policy provider.
  // http://crbug.com/230349
  const bool is_primary_user =
      chromeos::UserManager::Get()->GetLoggedInUsers().size() == 1;
  // Only USER_TYPE_REGULAR users have user cloud policy.
  // USER_TYPE_RETAIL_MODE, USER_TYPE_KIOSK_APP and USER_TYPE_GUEST are not
  // signed in and can't authenticate the policy registration.
  // USER_TYPE_PUBLIC_ACCOUNT gets its policy from the
  // DeviceLocalAccountPolicyService.
  // USER_TYPE_LOCALLY_MANAGED gets its policy from the
  // ManagedModePolicyProvider.
  const std::string& username = user->email();
  if (!is_primary_user ||
      user->GetType() != chromeos::User::USER_TYPE_REGULAR ||
      BrowserPolicyConnector::IsNonEnterpriseUser(username)) {
    return scoped_ptr<UserCloudPolicyManagerChromeOS>();
  }

  BrowserPolicyConnector* connector =
      g_browser_process->browser_policy_connector();
  UserAffiliation affiliation = connector->GetUserAffiliation(username);
  const bool is_managed_user = affiliation == USER_AFFILIATION_MANAGED;
  const bool is_browser_restart =
      command_line->HasSwitch(chromeos::switches::kLoginUser) &&
      !command_line->HasSwitch(chromeos::switches::kLoginPassword);
  const bool wait_for_initial_policy = is_managed_user && !is_browser_restart;

  DeviceManagementService* device_management_service =
      connector->device_management_service();
  if (wait_for_initial_policy)
    device_management_service->ScheduleInitialization(0);

  base::FilePath profile_dir = profile->GetPath();
  const base::FilePath legacy_dir = profile_dir.Append(kDeviceManagementDir);
  const base::FilePath policy_cache_file = legacy_dir.Append(kPolicy);
  const base::FilePath token_cache_file = legacy_dir.Append(kToken);
  const base::FilePath resource_cache_dir =
      profile_dir.Append(kPolicy).Append(kResourceDir);
  base::FilePath policy_key_dir;
  CHECK(PathService::Get(chromeos::DIR_USER_POLICY_KEYS, &policy_key_dir));

  scoped_ptr<CloudPolicyStore> store(
      new UserCloudPolicyStoreChromeOS(
          chromeos::DBusThreadManager::Get()->GetCryptohomeClient(),
          chromeos::DBusThreadManager::Get()->GetSessionManagerClient(),
          username, policy_key_dir, token_cache_file, policy_cache_file));
  scoped_ptr<ResourceCache> resource_cache;
  if (command_line->HasSwitch(switches::kEnableComponentCloudPolicy))
    resource_cache.reset(new ResourceCache(resource_cache_dir));
  scoped_ptr<UserCloudPolicyManagerChromeOS> manager(
      new UserCloudPolicyManagerChromeOS(store.Pass(),
                                         resource_cache.Pass(),
                                         wait_for_initial_policy));
  manager->Init();
  manager->Connect(g_browser_process->local_state(),
                   device_management_service,
                   g_browser_process->system_request_context(),
                   affiliation);

  DCHECK(managers_.find(profile) == managers_.end());
  managers_[profile] = manager.get();
  return manager.Pass();
}

void UserCloudPolicyManagerFactoryChromeOS::ProfileShutdown(
    content::BrowserContext* context) {
  Profile* profile = static_cast<Profile*>(context);
  if (profile->IsOffTheRecord())
    return;
  UserCloudPolicyManagerChromeOS* manager = GetManagerForProfile(profile);
  if (manager)
    manager->Shutdown();
}

void UserCloudPolicyManagerFactoryChromeOS::ProfileDestroyed(
    content::BrowserContext* context) {
  Profile* profile = static_cast<Profile*>(context);
  managers_.erase(profile);
  ProfileKeyedBaseFactory::ProfileDestroyed(context);
}

void UserCloudPolicyManagerFactoryChromeOS::SetEmptyTestingFactory(
    content::BrowserContext* context) {}

void UserCloudPolicyManagerFactoryChromeOS::CreateServiceNow(
    content::BrowserContext* context) {}

}  // namespace policy
