// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/configuration_policy_provider_keeper.h"

#include "base/path_service.h"
#include "chrome/browser/policy/configuration_policy_pref_store.h"
#include "chrome/browser/policy/configuration_policy_provider.h"
#if defined(OS_WIN)
#include "chrome/browser/policy/configuration_policy_provider_win.h"
#elif defined(OS_MACOSX)
#include "chrome/browser/policy/configuration_policy_provider_mac.h"
#elif defined(OS_POSIX)
#include "chrome/browser/policy/config_dir_policy_provider.h"
#endif
#include "chrome/browser/policy/dummy_configuration_policy_provider.h"
#include "chrome/common/chrome_paths.h"

namespace policy {

ConfigurationPolicyProviderKeeper::ConfigurationPolicyProviderKeeper()
    : managed_platform_provider_(CreateManagedPlatformProvider()),
      device_management_provider_(CreateDeviceManagementProvider()),
      recommended_provider_(CreateRecommendedProvider()) {
}

ConfigurationPolicyProviderKeeper::ConfigurationPolicyProviderKeeper(
    ConfigurationPolicyProvider* managed_platform_provider,
    ConfigurationPolicyProvider* device_management_provider,
    ConfigurationPolicyProvider* recommended_provider)
    : managed_platform_provider_(managed_platform_provider),
      device_management_provider_(device_management_provider),
      recommended_provider_(recommended_provider) {
}

ConfigurationPolicyProviderKeeper::~ConfigurationPolicyProviderKeeper() {}

ConfigurationPolicyProvider*
    ConfigurationPolicyProviderKeeper::managed_platform_provider() const {
  return managed_platform_provider_.get();
}

ConfigurationPolicyProvider*
    ConfigurationPolicyProviderKeeper::device_management_provider() const {
  return device_management_provider_.get();
}

ConfigurationPolicyProvider*
    ConfigurationPolicyProviderKeeper::recommended_provider() const {
  return recommended_provider_.get();
}

ConfigurationPolicyProvider*
    ConfigurationPolicyProviderKeeper::CreateManagedPlatformProvider() {
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

ConfigurationPolicyProvider*
    ConfigurationPolicyProviderKeeper::CreateDeviceManagementProvider() {
  return new DummyConfigurationPolicyProvider(
      ConfigurationPolicyPrefStore::GetChromePolicyDefinitionList());
}

ConfigurationPolicyProvider*
    ConfigurationPolicyProviderKeeper::CreateRecommendedProvider() {
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
