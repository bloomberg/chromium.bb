// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/device_policy_cache.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/task.h"
#include "base/values.h"
#include "chrome/browser/policy/configuration_policy_pref_store.h"
#include "chrome/browser/policy/device_policy_identity_strategy.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/policy/proto/cloud_policy.pb.h"
#include "chrome/browser/policy/proto/device_management_constants.h"
#include "chrome/browser/policy/proto/device_management_local.pb.h"
#include "content/browser/browser_thread.h"
#include "policy/configuration_policy_type.h"

using google::protobuf::RepeatedPtrField;

// Decodes a protobuf integer to an IntegerValue. The caller assumes ownership
// of the return Value*. Returns NULL in case the input value is out of bounds.
static Value* DecodeIntegerValue(google::protobuf::int64 value) {
  if (value < std::numeric_limits<int>::min() ||
      value > std::numeric_limits<int>::max()) {
    LOG(WARNING) << "Integer value " << value
                 << " out of numeric limits, ignoring.";
    return NULL;
  }

  return Value::CreateIntegerValue(static_cast<int>(value));
}

namespace policy {

DevicePolicyCache::DevicePolicyCache(
    DevicePolicyIdentityStrategy* identity_strategy)
    : identity_strategy_(identity_strategy),
      signed_settings_helper_(chromeos::SignedSettingsHelper::Get()),
      starting_up_(true) {
}

DevicePolicyCache::DevicePolicyCache(
    DevicePolicyIdentityStrategy* identity_strategy,
    chromeos::SignedSettingsHelper* signed_settings_helper)
    : identity_strategy_(identity_strategy),
      signed_settings_helper_(signed_settings_helper),
      starting_up_(true) {
}

DevicePolicyCache::~DevicePolicyCache() {
  signed_settings_helper_->CancelCallback(this);
}

void DevicePolicyCache::Load() {
  signed_settings_helper_->StartRetrievePolicyOp(this);
}

void DevicePolicyCache::SetPolicy(const em::PolicyFetchResponse& policy) {
  DCHECK(!starting_up_);
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
  if (starting_up_) {
    starting_up_ = false;
    if (code == chromeos::SignedSettings::NOT_FOUND ||
        code == chromeos::SignedSettings::KEY_UNAVAILABLE ||
        !policy.has_policy_data()) {
      InformNotifier(CloudPolicySubsystem::UNENROLLED,
                     CloudPolicySubsystem::NO_DETAILS);
      return;
    }
    em::PolicyData policy_data;
    if (!policy_data.ParseFromString(policy.policy_data())) {
      LOG(WARNING) << "Failed to parse PolicyData protobuf.";
      InformNotifier(CloudPolicySubsystem::LOCAL_ERROR,
                     CloudPolicySubsystem::POLICY_LOCAL_ERROR);
      return;
    }
    if (!policy_data.has_request_token() ||
        policy_data.request_token().empty()) {
      SetUnmanagedInternal(base::Time::NowFromSystemTime());
      InformNotifier(CloudPolicySubsystem::UNMANAGED,
                     CloudPolicySubsystem::NO_DETAILS);
      // TODO(jkummerow): Reminder: When we want to feed device-wide settings
      // made by a local owner into this cache, we need to call
      // SetPolicyInternal() here.
      return;
    }
    if (!policy_data.has_username() || !policy_data.has_device_id()) {
      InformNotifier(CloudPolicySubsystem::LOCAL_ERROR,
                     CloudPolicySubsystem::POLICY_LOCAL_ERROR);
      return;
    }
    identity_strategy_->SetDeviceManagementCredentials(
        policy_data.username(),
        policy_data.device_id(),
        policy_data.request_token());
    SetPolicyInternal(policy, NULL, false);
  } else {  // In other words, starting_up_ == false.
    if (code != chromeos::SignedSettings::SUCCESS) {
      if (code == chromeos::SignedSettings::BAD_SIGNATURE) {
        InformNotifier(CloudPolicySubsystem::LOCAL_ERROR,
                       CloudPolicySubsystem::SIGNATURE_MISMATCH);
      } else {
        InformNotifier(CloudPolicySubsystem::LOCAL_ERROR,
                       CloudPolicySubsystem::POLICY_LOCAL_ERROR);
      }
      return;
    }
    SetPolicyInternal(policy, NULL, false);
  }
}

void DevicePolicyCache::OnStorePolicyCompleted(
    chromeos::SignedSettings::ReturnCode code) {
  DCHECK(CalledOnValidThread());
  if (code != chromeos::SignedSettings::SUCCESS) {
    if (code == chromeos::SignedSettings::BAD_SIGNATURE) {
      InformNotifier(CloudPolicySubsystem::LOCAL_ERROR,
                     CloudPolicySubsystem::SIGNATURE_MISMATCH);
    } else {
      InformNotifier(CloudPolicySubsystem::LOCAL_ERROR,
                     CloudPolicySubsystem::POLICY_LOCAL_ERROR);
    }
    return;
  }
  signed_settings_helper_->StartRetrievePolicyOp(this);
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
  if (policy.has_policy_refresh_rate()) {
    const em::DevicePolicyRefreshRateProto container =
        policy.policy_refresh_rate();
    if (container.has_policy_refresh_rate()) {
      mandatory->Set(kPolicyPolicyRefreshRate,
                     DecodeIntegerValue(container.policy_refresh_rate()));
    }
  }
}

}  // namespace policy
