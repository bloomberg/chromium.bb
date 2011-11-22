// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_service_mock_builder.h"

#include "chrome/browser/policy/configuration_policy_pref_store.h"
#include "chrome/browser/prefs/command_line_pref_store.h"
#include "chrome/browser/prefs/default_pref_store.h"
#include "chrome/browser/prefs/pref_notifier_impl.h"
#include "chrome/browser/prefs/pref_value_store.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/testing_pref_store.h"
#include "chrome/common/json_pref_store.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

PrefServiceMockBuilder::PrefServiceMockBuilder()
  : user_prefs_(new TestingPrefStore) {
}

PrefServiceMockBuilder::~PrefServiceMockBuilder() {}

PrefServiceMockBuilder&
PrefServiceMockBuilder::WithManagedPlatformPrefs(PrefStore* store) {
  managed_platform_prefs_ = store;
  return *this;
}

PrefServiceMockBuilder&
PrefServiceMockBuilder::WithManagedCloudPrefs(PrefStore* store) {
  managed_cloud_prefs_ = store;
  return *this;
}

PrefServiceMockBuilder&
PrefServiceMockBuilder::WithExtensionPrefs(PrefStore* store) {
  extension_prefs_ = store;
  return *this;
}

PrefServiceMockBuilder&
PrefServiceMockBuilder::WithCommandLinePrefs(PrefStore* store) {
  command_line_prefs_ = store;
  return *this;
}

PrefServiceMockBuilder&
PrefServiceMockBuilder::WithUserPrefs(PersistentPrefStore* store) {
  user_prefs_ = store;
  return *this;
}

PrefServiceMockBuilder&
PrefServiceMockBuilder::WithRecommendedPlatformPrefs(PrefStore* store) {
  recommended_platform_prefs_ = store;
  return *this;
}

PrefServiceMockBuilder&
PrefServiceMockBuilder::WithRecommendedCloudPrefs(PrefStore* store) {
  recommended_cloud_prefs_ = store;
  return *this;
}

#if defined(ENABLE_CONFIGURATION_POLICY)
PrefServiceMockBuilder&
PrefServiceMockBuilder::WithManagedPlatformProvider(
    policy::ConfigurationPolicyProvider* provider) {
  managed_platform_prefs_ = new policy::ConfigurationPolicyPrefStore(provider);
  return *this;
}

PrefServiceMockBuilder&
PrefServiceMockBuilder::WithManagedCloudProvider(
    policy::ConfigurationPolicyProvider* provider) {
  managed_cloud_prefs_ = new policy::ConfigurationPolicyPrefStore(provider);
  return *this;
}

PrefServiceMockBuilder&
PrefServiceMockBuilder::WithRecommendedPlatformProvider(
    policy::ConfigurationPolicyProvider* provider) {
  recommended_platform_prefs_ =
      new policy::ConfigurationPolicyPrefStore(provider);
  return *this;
}

PrefServiceMockBuilder&
PrefServiceMockBuilder::WithRecommendedCloudProvider(
    policy::ConfigurationPolicyProvider* provider) {
  recommended_cloud_prefs_ = new policy::ConfigurationPolicyPrefStore(provider);
  return *this;
}
#endif

PrefServiceMockBuilder&
PrefServiceMockBuilder::WithCommandLine(CommandLine* command_line) {
  command_line_prefs_ = new CommandLinePrefStore(command_line);
  return *this;
}

PrefServiceMockBuilder&
PrefServiceMockBuilder::WithUserFilePrefs(const FilePath& prefs_file) {
  user_prefs_ =
      new JsonPrefStore(prefs_file,
                        BrowserThread::GetMessageLoopProxyForThread(
                            BrowserThread::FILE));
  return *this;
}

PrefService* PrefServiceMockBuilder::Create() {
  DefaultPrefStore* default_pref_store = new DefaultPrefStore();
  PrefNotifierImpl* pref_notifier = new PrefNotifierImpl();
  PrefService* pref_service =
      new PrefService(
          pref_notifier,
          new PrefValueStore(
              managed_platform_prefs_.get(),
              managed_cloud_prefs_.get(),
              extension_prefs_.get(),
              command_line_prefs_.get(),
              user_prefs_.get(),
              recommended_platform_prefs_.get(),
              recommended_cloud_prefs_.get(),
              default_pref_store,
              NULL,
              pref_notifier),
          user_prefs_.get(),
          default_pref_store,
          NULL,
          false);
  managed_platform_prefs_ = NULL;
  managed_cloud_prefs_ = NULL;
  extension_prefs_ = NULL;
  command_line_prefs_ = NULL;
  user_prefs_ = NULL;
  recommended_platform_prefs_ = NULL;
  recommended_cloud_prefs_ = NULL;
  user_prefs_ = new TestingPrefStore;
  return pref_service;
}
