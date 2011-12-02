// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/device_policy_cache.h"

#include <limits>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros_settings.h"
#include "chrome/browser/chromeos/dbus/dbus_thread_manager.h"
#include "chrome/browser/chromeos/dbus/update_engine_client.h"
#include "chrome/browser/chromeos/login/ownership_service.h"
#include "chrome/browser/chromeos/login/signed_settings_helper.h"
#include "chrome/browser/policy/cloud_policy_data_store.h"
#include "chrome/browser/policy/enterprise_install_attributes.h"
#include "chrome/browser/policy/enterprise_metrics.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"
#include "chrome/browser/policy/proto/device_management_constants.h"
#include "chrome/browser/policy/proto/device_management_local.pb.h"
#include "policy/configuration_policy_type.h"

namespace {

// Stores policy, updates the owner key if required and reports the status
// through a callback.
class StorePolicyOperation : public chromeos::OwnerManager::KeyUpdateDelegate {
 public:
  typedef base::Callback<void(chromeos::SignedSettings::ReturnCode)> Callback;

  StorePolicyOperation(chromeos::SignedSettingsHelper* signed_settings_helper,
                       const em::PolicyFetchResponse& policy,
                       const Callback& callback)
      : signed_settings_helper_(signed_settings_helper),
        policy_(policy),
        callback_(callback),
        weak_ptr_factory_(this) {
    signed_settings_helper_->StartStorePolicyOp(
        policy,
        base::Bind(&StorePolicyOperation::OnStorePolicyCompleted,
                   weak_ptr_factory_.GetWeakPtr()));
  }
  virtual ~StorePolicyOperation() {
  }

  void OnStorePolicyCompleted(chromeos::SignedSettings::ReturnCode code) {
    if (code != chromeos::SignedSettings::SUCCESS) {
      callback_.Run(code);
      delete this;
      return;
    }

    if (policy_.has_new_public_key()) {
      // The session manager has successfully done a key rotation. Replace the
      // owner key also in chrome.
      const std::string& new_key = policy_.new_public_key();
      const std::vector<uint8> new_key_data(new_key.c_str(),
                                            new_key.c_str() + new_key.size());
      chromeos::OwnershipService::GetSharedInstance()->StartUpdateOwnerKey(
          new_key_data, this);
      return;
    } else {
      chromeos::CrosSettings::Get()->ReloadProviders();
      callback_.Run(chromeos::SignedSettings::SUCCESS);
      delete this;
      return;
    }
  }

  // OwnerManager::KeyUpdateDelegate implementation:
  virtual void OnKeyUpdated() OVERRIDE {
    chromeos::CrosSettings::Get()->ReloadProviders();
    callback_.Run(chromeos::SignedSettings::SUCCESS);
    delete this;
  }

 private:

  chromeos::SignedSettingsHelper* signed_settings_helper_;
  em::PolicyFetchResponse policy_;
  Callback callback_;

  base::WeakPtrFactory<StorePolicyOperation> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(StorePolicyOperation);
};

// Decodes a protobuf integer to an IntegerValue. The caller assumes ownership
// of the return Value*. Returns NULL in case the input value is out of bounds.
Value* DecodeIntegerValue(google::protobuf::int64 value) {
  if (value < std::numeric_limits<int>::min() ||
      value > std::numeric_limits<int>::max()) {
    LOG(WARNING) << "Integer value " << value
                 << " out of numeric limits, ignoring.";
    return NULL;
  }

  return Value::CreateIntegerValue(static_cast<int>(value));
}

}  // namespace

namespace policy {

DevicePolicyCache::DevicePolicyCache(
    CloudPolicyDataStore* data_store,
    EnterpriseInstallAttributes* install_attributes)
    : data_store_(data_store),
      install_attributes_(install_attributes),
      signed_settings_helper_(chromeos::SignedSettingsHelper::Get()),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
}

DevicePolicyCache::DevicePolicyCache(
    CloudPolicyDataStore* data_store,
    EnterpriseInstallAttributes* install_attributes,
    chromeos::SignedSettingsHelper* signed_settings_helper)
    : data_store_(data_store),
      install_attributes_(install_attributes),
      signed_settings_helper_(signed_settings_helper),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
}

DevicePolicyCache::~DevicePolicyCache() {
}

void DevicePolicyCache::Load() {
  signed_settings_helper_->StartRetrievePolicyOp(
      base::Bind(&DevicePolicyCache::OnRetrievePolicyCompleted,
                 weak_ptr_factory_.GetWeakPtr()));
}

void DevicePolicyCache::SetPolicy(const em::PolicyFetchResponse& policy) {
  DCHECK(IsReady());

  // Make sure we have an enterprise device.
  std::string registration_user(install_attributes_->GetRegistrationUser());
  if (registration_user.empty()) {
    LOG(WARNING) << "Refusing to accept policy on non-enterprise device.";
    UMA_HISTOGRAM_ENUMERATION(kMetricPolicy,
                              kMetricPolicyFetchNonEnterpriseDevice,
                              kMetricPolicySize);
    InformNotifier(CloudPolicySubsystem::LOCAL_ERROR,
                   CloudPolicySubsystem::POLICY_LOCAL_ERROR);
    return;
  }

  // Check the user this policy is for against the device-locked name.
  em::PolicyData policy_data;
  if (!policy_data.ParseFromString(policy.policy_data())) {
    LOG(WARNING) << "Invalid policy protobuf";
    UMA_HISTOGRAM_ENUMERATION(kMetricPolicy, kMetricPolicyFetchInvalidPolicy,
                              kMetricPolicySize);
    InformNotifier(CloudPolicySubsystem::LOCAL_ERROR,
                   CloudPolicySubsystem::POLICY_LOCAL_ERROR);
    return;
  }

  if (registration_user != policy_data.username()) {
    LOG(WARNING) << "Refusing policy blob for " << policy_data.username()
                 << " which doesn't match " << registration_user;
    UMA_HISTOGRAM_ENUMERATION(kMetricPolicy, kMetricPolicyFetchUserMismatch,
                              kMetricPolicySize);
    InformNotifier(CloudPolicySubsystem::LOCAL_ERROR,
                   CloudPolicySubsystem::POLICY_LOCAL_ERROR);
    return;
  }

  set_last_policy_refresh_time(base::Time::NowFromSystemTime());

  // Start a store operation.
  StorePolicyOperation::Callback callback =
      base::Bind(&DevicePolicyCache::PolicyStoreOpCompleted,
                 weak_ptr_factory_.GetWeakPtr());
  new StorePolicyOperation(signed_settings_helper_, policy, callback);
}

void DevicePolicyCache::SetUnmanaged() {
  LOG(WARNING) << "Tried to set DevicePolicyCache to 'unmanaged'!";
  // This is not supported for DevicePolicyCache.
}

void DevicePolicyCache::OnRetrievePolicyCompleted(
    chromeos::SignedSettings::ReturnCode code,
    const em::PolicyFetchResponse& policy) {
  DCHECK(CalledOnValidThread());
  if (!IsReady()) {
    std::string device_token;
    InstallInitialPolicy(code, policy, &device_token);
    // We need to call SetDeviceToken unconditionally to indicate the cache has
    // finished loading.
    data_store_->SetDeviceToken(device_token, true);
    SetReady();
  } else {  // In other words, IsReady() == true
    if (code != chromeos::SignedSettings::SUCCESS) {
      if (code == chromeos::SignedSettings::BAD_SIGNATURE) {
        UMA_HISTOGRAM_ENUMERATION(kMetricPolicy, kMetricPolicyFetchBadSignature,
                                  kMetricPolicySize);
        InformNotifier(CloudPolicySubsystem::LOCAL_ERROR,
                       CloudPolicySubsystem::SIGNATURE_MISMATCH);
      } else {
        UMA_HISTOGRAM_ENUMERATION(kMetricPolicy, kMetricPolicyFetchOtherFailed,
                                  kMetricPolicySize);
        InformNotifier(CloudPolicySubsystem::LOCAL_ERROR,
                       CloudPolicySubsystem::POLICY_LOCAL_ERROR);
      }
      return;
    }
    bool ok = SetPolicyInternal(policy, NULL, false);
    if (ok) {
      UMA_HISTOGRAM_ENUMERATION(kMetricPolicy, kMetricPolicyFetchOK,
                                kMetricPolicySize);
    }
  }
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

void DevicePolicyCache::PolicyStoreOpCompleted(
    chromeos::SignedSettings::ReturnCode code) {
  DCHECK(CalledOnValidThread());
  if (code != chromeos::SignedSettings::SUCCESS) {
    UMA_HISTOGRAM_ENUMERATION(kMetricPolicy, kMetricPolicyStoreFailed,
                              kMetricPolicySize);
    if (code == chromeos::SignedSettings::BAD_SIGNATURE) {
      UMA_HISTOGRAM_ENUMERATION(kMetricPolicy, kMetricPolicyFetchBadSignature,
                                kMetricPolicySize);
      InformNotifier(CloudPolicySubsystem::LOCAL_ERROR,
                     CloudPolicySubsystem::SIGNATURE_MISMATCH);
    } else {
      UMA_HISTOGRAM_ENUMERATION(kMetricPolicy, kMetricPolicyFetchOtherFailed,
                                kMetricPolicySize);
      InformNotifier(CloudPolicySubsystem::LOCAL_ERROR,
                     CloudPolicySubsystem::POLICY_LOCAL_ERROR);
    }
    return;
  }
  UMA_HISTOGRAM_ENUMERATION(kMetricPolicy, kMetricPolicyStoreSucceeded,
                            kMetricPolicySize);
  signed_settings_helper_->StartRetrievePolicyOp(
      base::Bind(&DevicePolicyCache::OnRetrievePolicyCompleted,
                 weak_ptr_factory_.GetWeakPtr()));
}

void DevicePolicyCache::InstallInitialPolicy(
    chromeos::SignedSettings::ReturnCode code,
    const em::PolicyFetchResponse& policy,
    std::string* device_token) {
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
    UMA_HISTOGRAM_ENUMERATION(kMetricPolicy, kMetricPolicyLoadFailed,
                              kMetricPolicySize);
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
    UMA_HISTOGRAM_ENUMERATION(kMetricPolicy, kMetricPolicyLoadFailed,
                              kMetricPolicySize);
    InformNotifier(CloudPolicySubsystem::LOCAL_ERROR,
                   CloudPolicySubsystem::POLICY_LOCAL_ERROR);
    return;
  }
  UMA_HISTOGRAM_ENUMERATION(kMetricPolicy, kMetricPolicyLoadSucceeded,
                            kMetricPolicySize);
  data_store_->set_user_name(policy_data.username());
  data_store_->set_device_id(policy_data.device_id());
  *device_token = policy_data.request_token();
  base::Time timestamp;
  if (SetPolicyInternal(policy, &timestamp, true))
    set_last_policy_refresh_time(timestamp);
}

// static
void DevicePolicyCache::DecodeDevicePolicy(
    const em::ChromeDeviceSettingsProto& policy,
    PolicyMap* mandatory,
    PolicyMap* recommended) {
  if (policy.has_device_policy_refresh_rate()) {
    const em::DevicePolicyRefreshRateProto container =
        policy.device_policy_refresh_rate();
    if (container.has_device_policy_refresh_rate()) {
      mandatory->Set(kPolicyDevicePolicyRefreshRate,
                     DecodeIntegerValue(
                         container.device_policy_refresh_rate()));
    }
  }

  if (policy.has_device_proxy_settings()) {
    const em::DeviceProxySettingsProto container =
        policy.device_proxy_settings();
    if (container.has_proxy_mode()) {
      recommended->Set(kPolicyProxyMode,
                       Value::CreateStringValue(container.proxy_mode()));
    }
    if (container.has_proxy_server()) {
      recommended->Set(kPolicyProxyServer,
                       Value::CreateStringValue(container.proxy_server()));
    }
    if (container.has_proxy_pac_url()) {
      recommended->Set(kPolicyProxyPacUrl,
                       Value::CreateStringValue(container.proxy_pac_url()));
    }
    if (container.has_proxy_bypass_list()) {
      recommended->Set(kPolicyProxyBypassList,
                       Value::CreateStringValue(container.proxy_bypass_list()));
    }
  }

  if (policy.has_release_channel() &&
      policy.release_channel().has_release_channel()) {
    std::string channel(policy.release_channel().release_channel());
    mandatory->Set(kPolicyChromeOsReleaseChannel,
                   Value::CreateStringValue(channel));
    // TODO(dubroy): Once http://crosbug.com/17015 is implemented, we won't
    // have to pass the channel in here, only ping the update engine to tell
    // it to fetch the channel from the policy.
    chromeos::DBusThreadManager::Get()->GetUpdateEngineClient()
        ->SetReleaseTrack(channel);
  }

  if (policy.has_open_network_configuration() &&
      policy.open_network_configuration().has_open_network_configuration()) {
    std::string config(
        policy.open_network_configuration().open_network_configuration());
    mandatory->Set(kPolicyDeviceOpenNetworkConfiguration,
                   Value::CreateStringValue(config));
  }
}

}  // namespace policy
