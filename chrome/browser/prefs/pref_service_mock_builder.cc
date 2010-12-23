// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_service_mock_builder.h"

#include "chrome/browser/browser_thread.h"
#include "chrome/browser/policy/configuration_policy_pref_store.h"
#include "chrome/browser/prefs/command_line_pref_store.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/testing_pref_store.h"
#include "chrome/common/json_pref_store.h"

PrefServiceMockBuilder::PrefServiceMockBuilder()
  : user_prefs_(new TestingPrefStore) {
}

PrefServiceMockBuilder&
PrefServiceMockBuilder::WithManagedPlatformPrefs(PrefStore* store) {
  managed_platform_prefs_.reset(store);
  return *this;
}

PrefServiceMockBuilder&
PrefServiceMockBuilder::WithDeviceManagementPrefs(PrefStore* store) {
  device_management_prefs_.reset(store);
  return *this;
}

PrefServiceMockBuilder&
PrefServiceMockBuilder::WithExtensionPrefs(PrefStore* store) {
  extension_prefs_.reset(store);
  return *this;
}

PrefServiceMockBuilder&
PrefServiceMockBuilder::WithCommandLinePrefs(PrefStore* store) {
  command_line_prefs_.reset(store);
  return *this;
}

PrefServiceMockBuilder&
PrefServiceMockBuilder::WithUserPrefs(PersistentPrefStore* store) {
  user_prefs_.reset(store);
  return *this;
}

PrefServiceMockBuilder&
PrefServiceMockBuilder::WithRecommendedPrefs(PrefStore* store) {
  recommended_prefs_.reset(store);
  return *this;
}

PrefServiceMockBuilder&
PrefServiceMockBuilder::WithManagedPlatformProvider(
    policy::ConfigurationPolicyProvider* provider) {
  managed_platform_prefs_.reset(
      new policy::ConfigurationPolicyPrefStore(provider));
  return *this;
}

PrefServiceMockBuilder&
PrefServiceMockBuilder::WithDeviceManagementProvider(
    policy::ConfigurationPolicyProvider* provider) {
  device_management_prefs_.reset(
      new policy::ConfigurationPolicyPrefStore(provider));
  return *this;
}

PrefServiceMockBuilder&
PrefServiceMockBuilder::WithRecommendedProvider(
    policy::ConfigurationPolicyProvider* provider) {
  recommended_prefs_.reset(
      new policy::ConfigurationPolicyPrefStore(provider));
  return *this;
}

PrefServiceMockBuilder&
PrefServiceMockBuilder::WithCommandLine(CommandLine* command_line) {
  command_line_prefs_.reset(new CommandLinePrefStore(command_line));
  return *this;
}

PrefServiceMockBuilder&
PrefServiceMockBuilder::WithUserFilePrefs(const FilePath& prefs_file) {
  user_prefs_.reset(
      new JsonPrefStore(prefs_file,
                        BrowserThread::GetMessageLoopProxyForThread(
                            BrowserThread::FILE)));
  return *this;
}

PrefService* PrefServiceMockBuilder::Create() {
  PrefService* pref_service =
      new PrefService(managed_platform_prefs_.release(),
                      device_management_prefs_.release(),
                      extension_prefs_.release(),
                      command_line_prefs_.release(),
                      user_prefs_.release(),
                      recommended_prefs_.release());
  user_prefs_.reset(new TestingPrefStore);
  return pref_service;
}
