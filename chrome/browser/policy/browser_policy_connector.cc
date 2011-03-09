// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/browser_policy_connector.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/cloud_policy_identity_strategy.h"
#include "chrome/browser/policy/cloud_policy_subsystem.h"
#include "chrome/browser/policy/configuration_policy_pref_store.h"
#include "chrome/browser/policy/configuration_policy_provider.h"
#include "chrome/browser/policy/dummy_configuration_policy_provider.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/common/notification_service.h"

#if defined(OS_WIN)
#include "chrome/browser/policy/configuration_policy_provider_win.h"
#elif defined(OS_MACOSX)
#include "chrome/browser/policy/configuration_policy_provider_mac.h"
#elif defined(OS_POSIX)
#include "chrome/browser/policy/config_dir_policy_provider.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/policy/device_policy_identity_strategy.h"
#endif

namespace {

const FilePath::CharType kDevicePolicyCacheFile[] =
    FILE_PATH_LITERAL("Policy");

}  // namespace

namespace policy {

BrowserPolicyConnector::BrowserPolicyConnector() {
  managed_platform_provider_.reset(CreateManagedPlatformProvider());
  recommended_platform_provider_.reset(CreateRecommendedPlatformProvider());
  registrar_.Add(this, NotificationType::DEFAULT_REQUEST_CONTEXT_AVAILABLE,
                 NotificationService::AllSources());

#if defined(OS_CHROMEOS)
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDevicePolicyCacheDir)) {
    FilePath cache_dir(command_line->GetSwitchValuePath(
        switches::kDevicePolicyCacheDir));

    if (!file_util::CreateDirectory(cache_dir)) {
      LOG(WARNING) << "Device policy cache directory "
                   << cache_dir.value()
                   << " is not accessible, skipping initialization.";
    } else {
      identity_strategy_.reset(new DevicePolicyIdentityStrategy());
      cloud_policy_subsystem_.reset(
          new CloudPolicySubsystem(cache_dir.Append(kDevicePolicyCacheFile),
                                   identity_strategy_.get()));
    }
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
  identity_strategy_.reset();
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

// static
void BrowserPolicyConnector::RegisterPrefs(PrefService* local_state) {
  local_state->RegisterIntegerPref(prefs::kPolicyDevicePolicyRefreshRate,
                                   kDefaultPolicyRefreshRateInMilliseconds);
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
    URLRequestContextGetter* request_context) {
  // TODO(jkummerow, mnissler): Move this out of the browser startup path.
  DCHECK(local_state);
  DCHECK(request_context);
  if (cloud_policy_subsystem_.get()) {
    cloud_policy_subsystem_->Initialize(local_state,
                                        prefs::kPolicyDevicePolicyRefreshRate,
                                        request_context);
  }
}

}  // namespace
