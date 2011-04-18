// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/device_policy_cache.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/task.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros_settings_names.h"
#include "chrome/browser/chromeos/login/ownership_service.h"
#include "chrome/browser/chromeos/login/signed_settings_helper.h"
#include "chrome/browser/chromeos/user_cros_settings_provider.h"
#include "chrome/browser/policy/configuration_policy_pref_store.h"
#include "chrome/browser/policy/device_policy_identity_strategy.h"
#include "chrome/browser/policy/enterprise_install_attributes.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"
#include "chrome/browser/policy/proto/device_management_constants.h"
#include "chrome/browser/policy/proto/device_management_local.pb.h"
#include "content/browser/browser_thread.h"
#include "policy/configuration_policy_type.h"

namespace {

// Stores policy, updates the owner key if required and reports the status
// through a callback.
class StorePolicyOperation : public chromeos::SignedSettingsHelper::Callback,
                             public chromeos::OwnerManager::KeyUpdateDelegate {
 public:
  typedef Callback1<chromeos::SignedSettings::ReturnCode>::Type Callback;

  StorePolicyOperation(chromeos::SignedSettingsHelper* signed_settings_helper,
                       const em::PolicyFetchResponse& policy,
                       Callback* callback)
      : signed_settings_helper_(signed_settings_helper),
        policy_(policy),
        callback_(callback) {
    signed_settings_helper_->StartStorePolicyOp(policy, this);
  }
  virtual ~StorePolicyOperation() {
    signed_settings_helper_->CancelCallback(this);
  }

  // SignedSettingsHelper implementation:
  virtual void OnStorePolicyCompleted(
      chromeos::SignedSettings::ReturnCode code) OVERRIDE {
    if (code != chromeos::SignedSettings::SUCCESS) {
      callback_->Run(code);
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
      UpdateUserCrosSettings();
      callback_->Run(chromeos::SignedSettings::SUCCESS);
      delete this;
      return;
    }
  }

  // OwnerManager::KeyUpdateDelegate implementation:
  virtual void OnKeyUpdated() OVERRIDE {
    UpdateUserCrosSettings();
    callback_->Run(chromeos::SignedSettings::SUCCESS);
    delete this;
  }

 private:
  void UpdateUserCrosSettings() {
    // TODO(mnissler): Find a better way. This is a hack that updates the
    // UserCrosSettingsProvider's cache, since it is unable to notice we've
    // updated policy information.
    chromeos::UserCrosSettingsProvider().Reload();
  }

  chromeos::SignedSettingsHelper* signed_settings_helper_;
  em::PolicyFetchResponse policy_;
  scoped_ptr<Callback> callback_;

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
    DevicePolicyIdentityStrategy* identity_strategy,
    EnterpriseInstallAttributes* install_attributes)
    : identity_strategy_(identity_strategy),
      install_attributes_(install_attributes),
      signed_settings_helper_(chromeos::SignedSettingsHelper::Get()),
      starting_up_(true),
      ALLOW_THIS_IN_INITIALIZER_LIST(callback_factory_(this)) {
}

DevicePolicyCache::DevicePolicyCache(
    DevicePolicyIdentityStrategy* identity_strategy,
    EnterpriseInstallAttributes* install_attributes,
    chromeos::SignedSettingsHelper* signed_settings_helper)
    : identity_strategy_(identity_strategy),
      install_attributes_(install_attributes),
      signed_settings_helper_(signed_settings_helper),
      starting_up_(true),
      ALLOW_THIS_IN_INITIALIZER_LIST(callback_factory_(this)) {
}

DevicePolicyCache::~DevicePolicyCache() {
  signed_settings_helper_->CancelCallback(this);
}

void DevicePolicyCache::Load() {
  signed_settings_helper_->StartRetrievePolicyOp(this);
}

void DevicePolicyCache::SetPolicy(const em::PolicyFetchResponse& policy) {
  DCHECK(!starting_up_);

  // Make sure we have an enterprise device.
  std::string registration_user(install_attributes_->GetRegistrationUser());
  if (registration_user.empty()) {
    LOG(WARNING) << "Refusing to accept policy on non-enterprise device.";
    InformNotifier(CloudPolicySubsystem::LOCAL_ERROR,
                   CloudPolicySubsystem::POLICY_LOCAL_ERROR);
    return;
  }

  // Check the user this policy is for against the device-locked name.
  em::PolicyData policy_data;
  if (!policy_data.ParseFromString(policy.policy_data())) {
    LOG(WARNING) << "Invalid policy protobuf";
    InformNotifier(CloudPolicySubsystem::LOCAL_ERROR,
                   CloudPolicySubsystem::POLICY_LOCAL_ERROR);
    return;
  }

  if (registration_user != policy_data.username()) {
    LOG(WARNING) << "Refusing policy blob for " << policy_data.username()
                 << " which doesn't match " << registration_user;
    InformNotifier(CloudPolicySubsystem::LOCAL_ERROR,
                   CloudPolicySubsystem::POLICY_LOCAL_ERROR);
    return;
  }

  set_last_policy_refresh_time(base::Time::NowFromSystemTime());

  // Start a store operation.
  new StorePolicyOperation(signed_settings_helper_,
                           policy,
                           callback_factory_.NewCallback(
                               &DevicePolicyCache::PolicyStoreOpCompleted));
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
}

}  // namespace policy
