// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/browser_policy_connector.h"

#include "base/command_line.h"
#include "base/path_service.h"
#include "chrome/browser/net/gaia/token_service.h"
#include "chrome/browser/policy/cloud_policy_data_store.h"
#include "chrome/browser/policy/cloud_policy_provider.h"
#include "chrome/browser/policy/cloud_policy_provider_impl.h"
#include "chrome/browser/policy/cloud_policy_subsystem.h"
#include "chrome/browser/policy/configuration_policy_pref_store.h"
#include "chrome/browser/policy/configuration_policy_provider.h"
#include "chrome/browser/policy/dummy_cloud_policy_provider.h"
#include "chrome/browser/policy/dummy_configuration_policy_provider.h"
#include "chrome/browser/policy/user_policy_cache.h"
#include "chrome/browser/policy/user_policy_token_cache.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/pref_names.h"
#include "content/common/notification_details.h"
#include "content/common/notification_source.h"

#if defined(OS_WIN)
#include "chrome/browser/policy/configuration_policy_provider_win.h"
#elif defined(OS_MACOSX)
#include "chrome/browser/policy/configuration_policy_provider_mac.h"
#elif defined(OS_POSIX)
#include "chrome/browser/policy/config_dir_policy_provider.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/policy/device_policy_cache.h"
#endif

namespace policy {

namespace {

// Subdirectory in the user's profile for storing user policies.
const FilePath::CharType kPolicyDir[] = FILE_PATH_LITERAL("Device Management");
// File in the above directory for stroing user policy dmtokens.
const FilePath::CharType kTokenCacheFile[] = FILE_PATH_LITERAL("Token");
// File in the above directory for storing user policy data.
const FilePath::CharType kPolicyCacheFile[] = FILE_PATH_LITERAL("Policy");

// The following constants define delays applied before the initial policy fetch
// on startup. (So that displaying Chrome's GUI does not get delayed.)
// Delay in milliseconds from startup.
const int64 kServiceInitializationStartupDelay = 5000;

}  // namespace

// static
BrowserPolicyConnector* BrowserPolicyConnector::Create() {
  return new BrowserPolicyConnector();
}

BrowserPolicyConnector::~BrowserPolicyConnector() {
  // Shutdown device cloud policy.
#if defined(OS_CHROMEOS)
  if (device_cloud_policy_subsystem_.get())
    device_cloud_policy_subsystem_->Shutdown();
  device_cloud_policy_subsystem_.reset();
  device_data_store_.reset();
#endif

  // Shutdown user cloud policy.
  if (user_cloud_policy_subsystem_.get())
    user_cloud_policy_subsystem_->Shutdown();
  user_cloud_policy_subsystem_.reset();
  user_policy_token_cache_.reset();
  user_data_store_.reset();
}

ConfigurationPolicyProvider*
    BrowserPolicyConnector::GetManagedPlatformProvider() const {
  return managed_platform_provider_.get();
}

ConfigurationPolicyProvider*
    BrowserPolicyConnector::GetManagedCloudProvider() const {
  return managed_cloud_provider_.get();
}

ConfigurationPolicyProvider*
    BrowserPolicyConnector::GetRecommendedPlatformProvider() const {
  return recommended_platform_provider_.get();
}

ConfigurationPolicyProvider*
    BrowserPolicyConnector::GetRecommendedCloudProvider() const {
  return recommended_cloud_provider_.get();
}

void BrowserPolicyConnector::RegisterForDevicePolicy(
    const std::string& owner_email,
    const std::string& token,
    TokenType token_type) {
#if defined(OS_CHROMEOS)
  if (device_data_store_.get()) {
    device_data_store_->set_user_name(owner_email);
    switch (token_type) {
      case TOKEN_TYPE_OAUTH:
        device_data_store_->SetOAuthToken(token);
        break;
      case TOKEN_TYPE_GAIA:
        device_data_store_->SetGaiaToken(token);
        break;
      default:
        NOTREACHED() << "Invalid token type " << token_type;
    }
  }
#endif
}

bool BrowserPolicyConnector::IsEnterpriseManaged() {
#if defined(OS_CHROMEOS)
  return install_attributes_.get() && install_attributes_->IsEnterpriseDevice();
#else
  return false;
#endif
}

EnterpriseInstallAttributes::LockResult
    BrowserPolicyConnector::LockDevice(const std::string& user) {
#if defined(OS_CHROMEOS)
  if (install_attributes_.get())
    return install_attributes_->LockDevice(user);
#endif

  return EnterpriseInstallAttributes::LOCK_BACKEND_ERROR;
}

std::string BrowserPolicyConnector::GetEnterpriseDomain() {
#if defined(OS_CHROMEOS)
  if (install_attributes_.get())
    return install_attributes_->GetDomain();
#endif

  return std::string();
}

void BrowserPolicyConnector::ResetDevicePolicy() {
#if defined(OS_CHROMEOS)
  if (device_cloud_policy_subsystem_.get())
    device_cloud_policy_subsystem_->Reset();
#endif
}

void BrowserPolicyConnector::FetchDevicePolicy() {
#if defined(OS_CHROMEOS)
  if (device_data_store_.get()) {
    DCHECK(!device_data_store_->device_token().empty());
    device_data_store_->NotifyDeviceTokenChanged();
  }
#endif
}

void BrowserPolicyConnector::FetchUserPolicy() {
#if defined(OS_CHROMEOS)
  if (user_data_store_.get()) {
    DCHECK(!user_data_store_->device_token().empty());
    user_data_store_->NotifyDeviceTokenChanged();
  }
#endif
}

void BrowserPolicyConnector::ScheduleServiceInitialization(
    int64 delay_milliseconds) {
  if (user_cloud_policy_subsystem_.get()) {
    user_cloud_policy_subsystem_->
        ScheduleServiceInitialization(delay_milliseconds);
  }
#if defined(OS_CHROMEOS)
  if (device_cloud_policy_subsystem_.get()) {
    device_cloud_policy_subsystem_->
        ScheduleServiceInitialization(delay_milliseconds);
  }
#endif
}
void BrowserPolicyConnector::InitializeUserPolicy(const std::string& user_name,
                                                  const FilePath& policy_dir,
                                                  TokenService* token_service) {
  // Throw away the old backend.
  user_cloud_policy_subsystem_.reset();
  user_policy_token_cache_.reset();
  user_data_store_.reset();
  registrar_.RemoveAll();

  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDeviceManagementUrl)) {
    token_service_ = token_service;
    if (token_service_) {
      registrar_.Add(this,
                     chrome::NOTIFICATION_TOKEN_AVAILABLE,
                     Source<TokenService>(token_service_));
    }

    FilePath policy_cache_dir = policy_dir.Append(kPolicyDir);
    UserPolicyCache* user_policy_cache =
        new UserPolicyCache(policy_cache_dir.Append(kPolicyCacheFile));
    user_data_store_.reset(CloudPolicyDataStore::CreateForUserPolicies());
    user_policy_token_cache_.reset(
        new UserPolicyTokenCache(user_data_store_.get(),
                                 policy_cache_dir.Append(kTokenCacheFile)));

    // Prepending user caches meaning they will take precedence of device policy
    // caches.
    managed_cloud_provider_->PrependCache(user_policy_cache);
    recommended_cloud_provider_->PrependCache(user_policy_cache);
    user_cloud_policy_subsystem_.reset(new CloudPolicySubsystem(
        user_data_store_.get(),
        user_policy_cache));

    // Initiate the DM-Token load.
    user_policy_token_cache_->Load();

    user_data_store_->set_user_name(user_name);
    user_data_store_->set_user_affiliation(GetUserAffiliation(user_name));

    if (token_service_ &&
        token_service_->HasTokenForService(
            GaiaConstants::kDeviceManagementService)) {
      user_data_store_->SetGaiaToken(token_service_->GetTokenForService(
              GaiaConstants::kDeviceManagementService));
    }

    user_cloud_policy_subsystem_->CompleteInitialization(
        prefs::kUserPolicyRefreshRate,
        kServiceInitializationStartupDelay);
  }
}

void BrowserPolicyConnector::RegisterForUserPolicy(
    const std::string& oauth_token) {
  if (user_data_store_.get())
    user_data_store_->SetOAuthToken(oauth_token);
}

const CloudPolicyDataStore*
    BrowserPolicyConnector::GetDeviceCloudPolicyDataStore() const {
#if defined(OS_CHROMEOS)
  return device_data_store_.get();
#else
  return NULL;
#endif
}

const CloudPolicyDataStore*
    BrowserPolicyConnector::GetUserCloudPolicyDataStore() const {
  return user_data_store_.get();
}

BrowserPolicyConnector::BrowserPolicyConnector()
    : ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {
  managed_platform_provider_.reset(CreateManagedPlatformProvider());
  recommended_platform_provider_.reset(CreateRecommendedPlatformProvider());

  managed_cloud_provider_.reset(new CloudPolicyProviderImpl(
      ConfigurationPolicyPrefStore::GetChromePolicyDefinitionList(),
      CloudPolicyCacheBase::POLICY_LEVEL_MANDATORY));
  recommended_cloud_provider_.reset(new CloudPolicyProviderImpl(
      ConfigurationPolicyPrefStore::GetChromePolicyDefinitionList(),
      CloudPolicyCacheBase::POLICY_LEVEL_RECOMMENDED));

#if defined(OS_CHROMEOS)
  InitializeDevicePolicy();
#endif
}

BrowserPolicyConnector::BrowserPolicyConnector(
    ConfigurationPolicyProvider* managed_platform_provider,
    ConfigurationPolicyProvider* recommended_platform_provider,
    CloudPolicyProvider* managed_cloud_provider,
    CloudPolicyProvider* recommended_cloud_provider)
    : managed_platform_provider_(managed_platform_provider),
      recommended_platform_provider_(recommended_platform_provider),
      managed_cloud_provider_(managed_cloud_provider),
      recommended_cloud_provider_(recommended_cloud_provider),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {}

void BrowserPolicyConnector::Observe(int type,
                                     const NotificationSource& source,
                                     const NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (type == chrome::NOTIFICATION_TOKEN_AVAILABLE) {
    const TokenService* token_source =
        Source<const TokenService>(source).ptr();
    DCHECK_EQ(token_service_, token_source);
    const TokenService::TokenAvailableDetails* token_details =
        Details<const TokenService::TokenAvailableDetails>(details).ptr();
    if (token_details->service() == GaiaConstants::kDeviceManagementService) {
      if (user_data_store_.get()) {
        user_data_store_->SetGaiaToken(token_details->token());
      }
    }
  } else {
    NOTREACHED();
  }
}

void BrowserPolicyConnector::InitializeDevicePolicy() {
#if defined(OS_CHROMEOS)
  // Throw away the old backend.
  device_cloud_policy_subsystem_.reset();
  device_data_store_.reset();

  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnableDevicePolicy)) {
    device_data_store_.reset(CloudPolicyDataStore::CreateForDevicePolicies());
    chromeos::CryptohomeLibrary* cryptohome = NULL;
    if (chromeos::CrosLibrary::Get()->EnsureLoaded())
      cryptohome = chromeos::CrosLibrary::Get()->GetCryptohomeLibrary();
    install_attributes_.reset(new EnterpriseInstallAttributes(cryptohome));
    DevicePolicyCache* device_policy_cache =
        new DevicePolicyCache(device_data_store_.get(),
                              install_attributes_.get());

    managed_cloud_provider_->AppendCache(device_policy_cache);
    recommended_cloud_provider_->AppendCache(device_policy_cache);

    device_cloud_policy_subsystem_.reset(new CloudPolicySubsystem(
        device_data_store_.get(),
        device_policy_cache));

    // Initialize the subsystem once the message loops are spinning.
    MessageLoop::current()->PostTask(
        FROM_HERE,
        method_factory_.NewRunnableMethod(
            &BrowserPolicyConnector::InitializeDevicePolicySubsystem));
  }
#endif
}

void BrowserPolicyConnector::InitializeDevicePolicySubsystem() {
#if defined(OS_CHROMEOS)
  if (device_cloud_policy_subsystem_.get()) {
    device_cloud_policy_subsystem_->CompleteInitialization(
        prefs::kDevicePolicyRefreshRate,
        kServiceInitializationStartupDelay);
  }
#endif
}

CloudPolicyDataStore::UserAffiliation
    BrowserPolicyConnector::GetUserAffiliation(const std::string& user_name) {
#if defined(OS_CHROMEOS)
  if (install_attributes_.get()) {
    size_t pos = user_name.find('@');
    if (pos != std::string::npos &&
        user_name.substr(pos + 1) == install_attributes_->GetDomain()) {
      return CloudPolicyDataStore::USER_AFFILIATION_MANAGED;
    }
  }
#endif

  return CloudPolicyDataStore::USER_AFFILIATION_NONE;
}

// static
BrowserPolicyConnector* BrowserPolicyConnector::CreateForTests() {
  const ConfigurationPolicyProvider::PolicyDefinitionList*
      policy_list = ConfigurationPolicyPrefStore::
          GetChromePolicyDefinitionList();
  return new BrowserPolicyConnector(
      new policy::DummyConfigurationPolicyProvider(policy_list),
      new policy::DummyConfigurationPolicyProvider(policy_list),
      new policy::DummyCloudPolicyProvider(policy_list),
      new policy::DummyCloudPolicyProvider(policy_list));
}

// static
ConfigurationPolicyProvider*
    BrowserPolicyConnector::CreateManagedPlatformProvider() {
  const ConfigurationPolicyProvider::PolicyDefinitionList* policy_list =
      ConfigurationPolicyPrefStore::GetChromePolicyDefinitionList();
#if defined(OS_WIN)
  return new ConfigurationPolicyProviderWin(policy_list);
#elif defined(OS_MACOSX)
  return new ConfigurationPolicyProviderMac(policy_list);
#elif defined(OS_POSIX)
  FilePath config_dir_path;
  if (PathService::Get(chrome::DIR_POLICY_FILES, &config_dir_path)) {
    return new ConfigDirPolicyProvider(
        policy_list,
        config_dir_path.Append(FILE_PATH_LITERAL("managed")));
  } else {
    return new DummyConfigurationPolicyProvider(policy_list);
  }
#else
  return new DummyConfigurationPolicyProvider(policy_list);
#endif
}

// static
ConfigurationPolicyProvider*
    BrowserPolicyConnector::CreateRecommendedPlatformProvider() {
  const ConfigurationPolicyProvider::PolicyDefinitionList* policy_list =
      ConfigurationPolicyPrefStore::GetChromePolicyDefinitionList();
#if defined(OS_POSIX) && !defined(OS_MACOSX)
  FilePath config_dir_path;
  if (PathService::Get(chrome::DIR_POLICY_FILES, &config_dir_path)) {
    return new ConfigDirPolicyProvider(
        policy_list,
        config_dir_path.Append(FILE_PATH_LITERAL("recommended")));
  } else {
    return new DummyConfigurationPolicyProvider(policy_list);
  }
#else
  return new DummyConfigurationPolicyProvider(policy_list);
#endif
}

}  // namespace
