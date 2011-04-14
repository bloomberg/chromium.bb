// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/browser_policy_connector.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/cloud_policy_subsystem.h"
#include "chrome/browser/policy/configuration_policy_pref_store.h"
#include "chrome/browser/policy/configuration_policy_provider.h"
#include "chrome/browser/policy/dummy_configuration_policy_provider.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "content/common/notification_service.h"

#if defined(OS_WIN)
#include "chrome/browser/policy/configuration_policy_provider_win.h"
#elif defined(OS_MACOSX)
#include "chrome/browser/policy/configuration_policy_provider_mac.h"
#elif defined(OS_POSIX)
#include "chrome/browser/policy/config_dir_policy_provider.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/policy/device_policy_cache.h"
#include "chrome/browser/policy/device_policy_identity_strategy.h"
#endif

namespace policy {

BrowserPolicyConnector::BrowserPolicyConnector() {
  managed_platform_provider_.reset(CreateManagedPlatformProvider());
  recommended_platform_provider_.reset(CreateRecommendedPlatformProvider());
  registrar_.Add(this, NotificationType::DEFAULT_REQUEST_CONTEXT_AVAILABLE,
                 NotificationService::AllSources());

#if defined(OS_CHROMEOS)
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnableDevicePolicy)) {
    identity_strategy_.reset(new DevicePolicyIdentityStrategy());
    cloud_policy_subsystem_.reset(new CloudPolicySubsystem(
        identity_strategy_.get(),
        new DevicePolicyCache(identity_strategy_.get())));
  }
#endif
}

BrowserPolicyConnector::BrowserPolicyConnector(
    ConfigurationPolicyProvider* managed_platform_provider,
    ConfigurationPolicyProvider* recommended_platform_provider)
    : managed_platform_provider_(managed_platform_provider),
      recommended_platform_provider_(recommended_platform_provider) {}

BrowserPolicyConnector::~BrowserPolicyConnector() {
  if (cloud_policy_subsystem_.get())
    cloud_policy_subsystem_->Shutdown();
  cloud_policy_subsystem_.reset();
#if defined(OS_CHROMEOS)
  identity_strategy_.reset();
#endif
}

ConfigurationPolicyProvider*
    BrowserPolicyConnector::GetManagedPlatformProvider() const {
  return managed_platform_provider_.get();
}

ConfigurationPolicyProvider*
    BrowserPolicyConnector::GetManagedCloudProvider() const {
  if (cloud_policy_subsystem_.get())
    return cloud_policy_subsystem_->GetManagedPolicyProvider();

  return NULL;
}

ConfigurationPolicyProvider*
    BrowserPolicyConnector::GetRecommendedPlatformProvider() const {
  return recommended_platform_provider_.get();
}

ConfigurationPolicyProvider*
    BrowserPolicyConnector::GetRecommendedCloudProvider() const {
  if (cloud_policy_subsystem_.get())
    return cloud_policy_subsystem_->GetRecommendedPolicyProvider();

  return NULL;
}

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

void BrowserPolicyConnector::SetCredentials(const std::string& owner_email,
                                            const std::string& gaia_token) {
#if defined(OS_CHROMEOS)
  if (identity_strategy_.get())
    identity_strategy_->SetAuthCredentials(owner_email, gaia_token);
#endif
}

bool BrowserPolicyConnector::IsEnterpriseManaged() {
#if defined(OS_CHROMEOS)
  return (identity_strategy_.get() &&
          !identity_strategy_->GetDeviceToken().empty());
#else
  return false;
#endif
}

std::string BrowserPolicyConnector::GetEnterpriseDomain() {
  std::string domain;

#if defined(OS_CHROMEOS)
  // TODO(xiyuan): Find a better way to get enterprise domain.
  std::string username;
  std::string auth_token;
  if (identity_strategy_.get() &&
      identity_strategy_->GetCredentials(&username, &auth_token)) {
    size_t pos = username.find('@');
    if (pos != std::string::npos)
      domain = username.substr(pos + 1);
  }
#endif

  return domain;
}

void BrowserPolicyConnector::StopAutoRetry() {
  if (cloud_policy_subsystem_.get())
    cloud_policy_subsystem_->StopAutoRetry();
}

void BrowserPolicyConnector::Observe(NotificationType type,
                                     const NotificationSource& source,
                                     const NotificationDetails& details) {
  if (type == NotificationType::DEFAULT_REQUEST_CONTEXT_AVAILABLE) {
    Initialize(g_browser_process->local_state(),
               Profile::GetDefaultRequestContext());
  } else {
    NOTREACHED();
  }
}

void BrowserPolicyConnector::Initialize(
    PrefService* local_state,
    net::URLRequestContextGetter* request_context) {
  // TODO(jkummerow, mnissler): Move this out of the browser startup path.
  DCHECK(local_state);
  DCHECK(request_context);
  if (cloud_policy_subsystem_.get())
    cloud_policy_subsystem_->Initialize(local_state, request_context);
}

}  // namespace
