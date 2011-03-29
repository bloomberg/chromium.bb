// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/device_policy_cache.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/task.h"
#include "base/values.h"
#include "chrome/browser/policy/configuration_policy_pref_store.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/policy/proto/cloud_policy.pb.h"
#include "chrome/browser/policy/proto/device_management_constants.h"
#include "chrome/browser/policy/proto/device_management_local.pb.h"
#include "content/browser/browser_thread.h"
#include "policy/configuration_policy_type.h"

using google::protobuf::RepeatedPtrField;

namespace policy {

DevicePolicyCache::DevicePolicyCache()
    : signed_settings_helper_(chromeos::SignedSettingsHelper::Get()) {
}

DevicePolicyCache::DevicePolicyCache(
    chromeos::SignedSettingsHelper* signed_settings_helper)
    : signed_settings_helper_(signed_settings_helper) {
}

DevicePolicyCache::~DevicePolicyCache() {
  signed_settings_helper_->CancelCallback(this);
}

void DevicePolicyCache::Load() {
  // TODO(jkummerow): check if we're unmanaged; if so, set is_unmanaged_ = true
  // and return immediately.

  signed_settings_helper_->StartRetrievePolicyOp(this);
}

void DevicePolicyCache::SetPolicy(const em::PolicyFetchResponse& policy) {
  set_last_policy_refresh_time(base::Time::NowFromSystemTime());
  signed_settings_helper_->StartStorePolicyOp(policy, this);
}

void DevicePolicyCache::SetUnmanaged() {
  LOG(WARNING) << "Tried to set DevicePolicyCache to 'unmanaged'!";
  // This is not supported for DevicePolicyCache.
}

void DevicePolicyCache::OnRetrievePolicyCompleted(
    chromeos::SignedSettings::ReturnCode code,
    const em::PolicyFetchResponse& policy) {
  DCHECK(CalledOnValidThread());
  if (code != chromeos::SignedSettings::SUCCESS) {
    // TODO(jkummerow): We can't really do anything about this error, but
    // we may want to notify the user that something is wrong.
    return;
  }
  SetPolicyInternal(policy, NULL, false);
}

void DevicePolicyCache::OnStorePolicyCompleted(
    chromeos::SignedSettings::ReturnCode code) {
  DCHECK(CalledOnValidThread());
  if (code != chromeos::SignedSettings::SUCCESS) {
    // TODO(jkummerow): We can't really do anything about this error, but
    // we may want to notify the user that something is wrong.
    return;
  }
  Load();
}

bool DevicePolicyCache::DecodePolicyData(const em::PolicyData& policy_data,
                                         PolicyMap* mandatory,
                                         PolicyMap* recommended) {
  em::ChromeDeviceSettingsProto policy;
  if (!policy.ParseFromString(policy_data.policy_value())) {
    LOG(WARNING) << "Failed to parse ChromeDeviceSettingsProto.";
    return false;
  }
  DecodeDevicePolicy(policy, mandatory, recommended);
  return true;
}

// static
void DevicePolicyCache::DecodeDevicePolicy(
    const em::ChromeDeviceSettingsProto& policy,
    PolicyMap* mandatory,
    PolicyMap* recommended) {
  // TODO(jkummerow): Implement this when there are consumers for
  // device-policy-set values in g_browser_process->local_state().
}

}  // namespace policy
