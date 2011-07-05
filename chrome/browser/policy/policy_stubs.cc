// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/policy/configuration_policy_pref_store.h"
#include "chrome/browser/policy/cloud_policy_subsystem.h"

// Policies are optionally built, hence the following stubs.

namespace policy {

// static
BrowserPolicyConnector* BrowserPolicyConnector::Create() {
  return NULL;
}

// static
BrowserPolicyConnector* BrowserPolicyConnector::CreateForTests() {
  return NULL;
}

BrowserPolicyConnector::~BrowserPolicyConnector() {
}

// static
const ConfigurationPolicyProvider::PolicyDefinitionList*
ConfigurationPolicyPrefStore::GetChromePolicyDefinitionList() {
  return NULL;
}

// static
ConfigurationPolicyPrefStore* ConfigurationPolicyPrefStore::Create(
    ConfigurationPolicyProvider* provider) {
  return NULL;
}

// static
ConfigurationPolicyPrefStore*
ConfigurationPolicyPrefStore::CreateManagedPlatformPolicyPrefStore() {
  return NULL;
}

// static
ConfigurationPolicyPrefStore*
ConfigurationPolicyPrefStore::CreateManagedCloudPolicyPrefStore(
    Profile* profile) {
  return NULL;
}

// static
ConfigurationPolicyPrefStore*
ConfigurationPolicyPrefStore::CreateRecommendedPlatformPolicyPrefStore() {
  return NULL;
}

// static
ConfigurationPolicyPrefStore*
ConfigurationPolicyPrefStore::CreateRecommendedCloudPolicyPrefStore(
    Profile* profile) {
  return NULL;
}

// static
void CloudPolicySubsystem::RegisterPrefs(PrefService* local_state) {
  // don't need prefs for things we don't use.
}

}  // namespace policy
