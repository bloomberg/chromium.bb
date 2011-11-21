// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/configuration_policy_pref_store.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/policy/configuration_policy_handler_list.h"
#include "chrome/browser/policy/policy_error_map.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/prefs/pref_value_map.h"
#include "content/public/browser/browser_thread.h"
#include "policy/policy_constants.h"

using content::BrowserThread;

namespace policy {

namespace {

// Policies are loaded early on startup, before PolicyErrorMaps are ready to
// be retrieved. This function is posted to UI to log any errors found on
// Refresh below.
void LogErrors(PolicyErrorMap* errors) {
  PolicyErrorMap::const_iterator iter;
  for (iter = errors->begin(); iter != errors->end(); ++iter) {
    string16 policy = ASCIIToUTF16(GetPolicyName(iter->first));
    DLOG(WARNING) << "Policy " << policy << ": " << iter->second;
  }
}

}  // namespace

ConfigurationPolicyPrefStore::ConfigurationPolicyPrefStore(
    ConfigurationPolicyProvider* provider)
    : provider_(provider),
      initialization_complete_(false) {
  if (provider_) {
    // Read initial policy.
    prefs_.reset(CreatePreferencesFromPolicies());
    registrar_.Init(provider_, this);
    initialization_complete_ = provider_->IsInitializationComplete();
  } else {
    initialization_complete_ = true;
  }
}

ConfigurationPolicyPrefStore::~ConfigurationPolicyPrefStore() {
}

void ConfigurationPolicyPrefStore::AddObserver(PrefStore::Observer* observer) {
  observers_.AddObserver(observer);
}

void ConfigurationPolicyPrefStore::RemoveObserver(
    PrefStore::Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool ConfigurationPolicyPrefStore::IsInitializationComplete() const {
  return initialization_complete_;
}

PrefStore::ReadResult
ConfigurationPolicyPrefStore::GetValue(const std::string& key,
                                       const Value** value) const {
  const Value* stored_value = NULL;
  if (!prefs_.get() || !prefs_->GetValue(key, &stored_value))
    return PrefStore::READ_NO_VALUE;

  if (value)
    *value = stored_value;
  return PrefStore::READ_OK;
}

void ConfigurationPolicyPrefStore::OnUpdatePolicy(
    ConfigurationPolicyProvider* provider) {
  Refresh();
}

void ConfigurationPolicyPrefStore::OnProviderGoingAway(
    ConfigurationPolicyProvider* provider) {
  provider_ = NULL;
}

// static
ConfigurationPolicyPrefStore*
ConfigurationPolicyPrefStore::CreateManagedPlatformPolicyPrefStore() {
  return new ConfigurationPolicyPrefStore(
      g_browser_process->browser_policy_connector()->
          GetManagedPlatformProvider());
}

// static
ConfigurationPolicyPrefStore*
ConfigurationPolicyPrefStore::CreateManagedCloudPolicyPrefStore() {
  return new ConfigurationPolicyPrefStore(
      g_browser_process->browser_policy_connector()->GetManagedCloudProvider());
}

// static
ConfigurationPolicyPrefStore*
ConfigurationPolicyPrefStore::CreateRecommendedPlatformPolicyPrefStore() {
  return new ConfigurationPolicyPrefStore(
      g_browser_process->browser_policy_connector()->
          GetRecommendedPlatformProvider());
}

// static
ConfigurationPolicyPrefStore*
ConfigurationPolicyPrefStore::CreateRecommendedCloudPolicyPrefStore() {
  return new ConfigurationPolicyPrefStore(
      g_browser_process->browser_policy_connector()->
          GetRecommendedCloudProvider());
}

bool
ConfigurationPolicyPrefStore::IsProxyPolicy(ConfigurationPolicyType policy) {
  return policy == kPolicyProxyMode ||
      policy == kPolicyProxyServerMode ||
      policy == kPolicyProxyServer ||
      policy == kPolicyProxyPacUrl ||
      policy == kPolicyProxyBypassList;
}

void ConfigurationPolicyPrefStore::Refresh() {
  if (!provider_)
    return;
  scoped_ptr<PrefValueMap> new_prefs(CreatePreferencesFromPolicies());
  std::vector<std::string> changed_prefs;
  new_prefs->GetDifferingKeys(prefs_.get(), &changed_prefs);
  prefs_.swap(new_prefs);

  // Send out change notifications.
  for (std::vector<std::string>::const_iterator pref(changed_prefs.begin());
       pref != changed_prefs.end();
       ++pref) {
    FOR_EACH_OBSERVER(PrefStore::Observer, observers_,
                      OnPrefValueChanged(*pref));
  }

  // Update the initialization flag.
  if (!initialization_complete_ &&
      provider_->IsInitializationComplete()) {
    initialization_complete_ = true;
    FOR_EACH_OBSERVER(PrefStore::Observer, observers_,
                      OnInitializationCompleted(true));
  }
}

PrefValueMap* ConfigurationPolicyPrefStore::CreatePreferencesFromPolicies() {
  PolicyMap policies;
  if (!provider_->Provide(&policies))
    DLOG(WARNING) << "Failed to get policy from provider.";

  scoped_ptr<PrefValueMap> prefs(new PrefValueMap);
  scoped_ptr<PolicyErrorMap> errors(new PolicyErrorMap);

  const ConfigurationPolicyHandlerList* handler_list =
      g_browser_process->browser_policy_connector()->GetHandlerList();
  handler_list->ApplyPolicySettings(policies, prefs.get(), errors.get());

  // Retrieve and log the errors once the UI loop is ready. This is only an
  // issue during startup.
  BrowserThread::PostTask(BrowserThread::UI,
                          FROM_HERE,
                          base::Bind(&LogErrors,
                                     base::Owned(errors.release())));

  return prefs.release();
}

}  // namespace policy
