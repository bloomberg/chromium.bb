// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_service_mock_builder.h"

#include "chrome/browser/browser_thread.h"
#include "chrome/browser/policy/configuration_policy_pref_store.h"
#include "chrome/browser/prefs/command_line_pref_store.h"
#include "chrome/browser/prefs/default_pref_store.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/testing_pref_store.h"
#include "chrome/common/json_pref_store.h"

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
PrefServiceMockBuilder::WithDeviceManagementPrefs(PrefStore* store) {
  device_management_prefs_ = store;
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
PrefServiceMockBuilder::WithRecommendedPrefs(PrefStore* store) {
  recommended_prefs_ = store;
  return *this;
}

PrefServiceMockBuilder&
PrefServiceMockBuilder::WithManagedPlatformProvider(
    policy::ConfigurationPolicyProvider* provider) {
  managed_platform_prefs_ =
      new policy::ConfigurationPolicyPrefStore(provider);
  return *this;
}

PrefServiceMockBuilder&
PrefServiceMockBuilder::WithDeviceManagementProvider(
    policy::ConfigurationPolicyProvider* provider) {
  device_management_prefs_ =
      new policy::ConfigurationPolicyPrefStore(provider);
  return *this;
}

PrefServiceMockBuilder&
PrefServiceMockBuilder::WithRecommendedProvider(
    policy::ConfigurationPolicyProvider* provider) {
  recommended_prefs_ =
      new policy::ConfigurationPolicyPrefStore(provider);
  return *this;
}

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
  PrefService* pref_service =
      new PrefService(managed_platform_prefs_.get(),
                      device_management_prefs_.get(),
                      extension_prefs_.get(),
                      command_line_prefs_.get(),
                      user_prefs_.get(),
                      recommended_prefs_.get(),
                      new DefaultPrefStore());
  managed_platform_prefs_ = NULL;
  device_management_prefs_ = NULL;
  extension_prefs_ = NULL;
  command_line_prefs_ = NULL;
  user_prefs_ = NULL;
  recommended_prefs_ = NULL;
  user_prefs_ = new TestingPrefStore;
  return pref_service;
}
