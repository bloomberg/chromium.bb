// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud_policy_data_store.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"
#include "chrome/browser/policy/proto/device_management_constants.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/system/statistics_provider.h"
#endif

namespace {

#if defined(OS_CHROMEOS)
// MachineInfo key names.
const char kMachineInfoSystemHwqual[] = "hardware_class";

// These are the machine serial number keys that we check in order until we
// find a non-empty serial number. The VPD spec says the serial number should be
// in the "serial_number" key for v2+ VPDs. However, we cannot check this first,
// since we'd get the "serial_number" value from the SMBIOS (yes, there's a name
// clash here!), which is different from the serial number we want and not
// actually per-device. So, we check the the legacy keys first. If we find a
// serial number for these, we use it, otherwise we must be on a newer device
// that provides the correct data in "serial_number".
const char* kMachineInfoSerialNumberKeys[] = {
  "sn",            // ZGB
  "Product_S/N",   // Alex
  "Product_SN",    // Mario
  "serial_number"  // VPD v2+ devices
};
#endif

}  // namespace

namespace policy {

CloudPolicyDataStore::~CloudPolicyDataStore() {}

// static
CloudPolicyDataStore* CloudPolicyDataStore::CreateForUserPolicies() {
  return new CloudPolicyDataStore(em::DeviceRegisterRequest::USER,
                                  kChromeUserPolicyType,
                                  "", "");
}

// static
CloudPolicyDataStore* CloudPolicyDataStore::CreateForDevicePolicies() {
  std::string machine_model;
  std::string machine_id;

#if defined(OS_CHROMEOS)
  chromeos::system::StatisticsProvider* provider =
      chromeos::system::StatisticsProvider::GetInstance();
  if (!provider->GetMachineStatistic(kMachineInfoSystemHwqual,
                                     &machine_model)) {
    LOG(ERROR) << "Failed to get machine model.";
  }
  for (unsigned int i = 0; i < arraysize(kMachineInfoSerialNumberKeys); i++) {
    if (provider->GetMachineStatistic(kMachineInfoSerialNumberKeys[i],
                                      &machine_id) &&
        !machine_id.empty()) {
      break;
    }
  }

  if (machine_id.empty())
    LOG(ERROR) << "Failed to get machine serial number.";
#endif

  return new CloudPolicyDataStore(em::DeviceRegisterRequest::DEVICE,
                                  kChromeDevicePolicyType,
                                  machine_model,
                                  machine_id);
}

CloudPolicyDataStore::CloudPolicyDataStore(
    const em::DeviceRegisterRequest_Type policy_register_type,
    const std::string& policy_type,
    const std::string& machine_model,
    const std::string& machine_id)
    : policy_register_type_(policy_register_type),
      policy_type_(policy_type),
      machine_model_(machine_model),
      machine_id_(machine_id),
      token_cache_loaded_(false) {}

void CloudPolicyDataStore::SetDeviceToken(const std::string& device_token,
                                          bool from_cache) {
  DCHECK(token_cache_loaded_ != from_cache);
  if (!token_cache_loaded_) {
    // The cache should be the first to set the token. (It may be "")
    DCHECK(from_cache);
    token_cache_loaded_ = true;
  } else {
    // The cache should never set the token later.
    DCHECK(!from_cache);
  }
  device_token_ = device_token;
  token_cache_loaded_ = true;
  NotifyDeviceTokenChanged();
}

void CloudPolicyDataStore::SetGaiaToken(const std::string& gaia_token) {
  DCHECK(!user_name_.empty());
  gaia_token_ = gaia_token;
  NotifyCredentialsChanged();
}

void CloudPolicyDataStore::SetOAuthToken(const std::string& oauth_token) {
  DCHECK(!user_name_.empty());
  oauth_token_ = oauth_token;
  NotifyCredentialsChanged();
}

void CloudPolicyDataStore::Reset() {
  user_name_ = "";
  gaia_token_ = "";
  device_id_ = "";
  device_token_ = "";
}

void CloudPolicyDataStore::SetupForTesting(const std::string& device_token,
                                           const std::string& device_id,
                                           const std::string& user_name,
                                           const std::string& gaia_token,
                                           bool token_cache_loaded) {
  device_id_ = device_id;
  user_name_ = user_name;
  gaia_token_ = gaia_token;
  device_token_ = device_token;
  token_cache_loaded_ = token_cache_loaded;
}

void CloudPolicyDataStore::set_device_id(const std::string& device_id) {
  device_id_ = device_id;
}

const std::string& CloudPolicyDataStore::device_id() const {
  return device_id_;
}

void CloudPolicyDataStore::set_user_name(const std::string& user_name) {
  user_name_ = user_name;
}

const std::string& CloudPolicyDataStore::device_token() const {
  return device_token_;
}

const std::string& CloudPolicyDataStore::gaia_token() const {
  return gaia_token_;
}

const std::string& CloudPolicyDataStore::oauth_token() const {
  return oauth_token_;
}

bool CloudPolicyDataStore::has_auth_token() const {
  return !oauth_token_.empty() || !gaia_token_.empty();
}

const std::string& CloudPolicyDataStore::machine_id() const {
  return machine_id_;
}

const std::string& CloudPolicyDataStore::machine_model() const {
  return machine_model_;
}

em::DeviceRegisterRequest_Type
CloudPolicyDataStore::policy_register_type() const {
  return policy_register_type_;
}

const std::string& CloudPolicyDataStore::policy_type() const {
  return policy_type_;
}

bool CloudPolicyDataStore::token_cache_loaded() const {
  return token_cache_loaded_;
}

const std::string& CloudPolicyDataStore::user_name() const {
  return user_name_;
}

void CloudPolicyDataStore::AddObserver(
    CloudPolicyDataStore::Observer* observer) {
  observer_list_.AddObserver(observer);
}

void CloudPolicyDataStore::RemoveObserver(
    CloudPolicyDataStore::Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void CloudPolicyDataStore::NotifyCredentialsChanged() {
  FOR_EACH_OBSERVER(Observer, observer_list_, OnCredentialsChanged());
}

void CloudPolicyDataStore::NotifyDeviceTokenChanged() {
  FOR_EACH_OBSERVER(Observer, observer_list_, OnDeviceTokenChanged());
}

}  // namespace policy
