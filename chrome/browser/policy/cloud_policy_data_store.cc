// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud_policy_data_store.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"

namespace em = enterprise_management;

namespace policy {

CloudPolicyDataStore::~CloudPolicyDataStore() {}

// static
CloudPolicyDataStore* CloudPolicyDataStore::CreateForUserPolicies() {
  return new CloudPolicyDataStore(em::DeviceRegisterRequest::USER,
                                  dm_protocol::kChromeUserPolicyType);
}

// static
CloudPolicyDataStore* CloudPolicyDataStore::CreateForDevicePolicies() {
  return new CloudPolicyDataStore(em::DeviceRegisterRequest::DEVICE,
                                  dm_protocol::kChromeDevicePolicyType);
}

CloudPolicyDataStore::CloudPolicyDataStore(
    const em::DeviceRegisterRequest_Type policy_register_type,
    const std::string& policy_type)
    : user_affiliation_(USER_AFFILIATION_NONE),
      policy_register_type_(policy_register_type),
      policy_type_(policy_type),
      known_machine_id_(false),
      reregister_(false),
      token_cache_loaded_(false),
      policy_fetching_enabled_(true),
      device_mode_(DEVICE_MODE_PENDING) {}

void CloudPolicyDataStore::SetDeviceToken(const std::string& device_token,
                                          bool from_cache) {
  DCHECK(token_cache_loaded_ != from_cache);
  if (!token_cache_loaded_) {
    // The cache should be the first to set the token (it may be empty).
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

void CloudPolicyDataStore::set_machine_id(const std::string& machine_id) {
  machine_id_ = machine_id;
}

void CloudPolicyDataStore::set_machine_model(const std::string& machine_model) {
  machine_model_ = machine_model;
}

const std::string& CloudPolicyDataStore::device_id() const {
  return device_id_;
}

void CloudPolicyDataStore::set_user_name(const std::string& user_name) {
  user_name_ = user_name;
}

void CloudPolicyDataStore::set_user_affiliation(
    UserAffiliation user_affiliation) {
  user_affiliation_ = user_affiliation;
}

void CloudPolicyDataStore::set_known_machine_id(bool known_machine_id) {
  known_machine_id_ = known_machine_id;
}

void CloudPolicyDataStore::set_policy_fetching_enabled(
    bool policy_fetching_enabled) {
  policy_fetching_enabled_ = policy_fetching_enabled;
}

void CloudPolicyDataStore::set_device_mode(DeviceMode device_mode) {
  device_mode_ = device_mode;
}

void CloudPolicyDataStore::set_reregister(bool reregister) {
  reregister_ = reregister;
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

bool CloudPolicyDataStore::policy_fetching_enabled() const {
  return policy_fetching_enabled_;
}

const std::string& CloudPolicyDataStore::user_name() const {
  return user_name_;
}

UserAffiliation CloudPolicyDataStore::user_affiliation() const {
  return user_affiliation_;
}

bool CloudPolicyDataStore::known_machine_id() const {
  return known_machine_id_;
}

DeviceMode CloudPolicyDataStore::device_mode() const {
  return device_mode_;
}

bool CloudPolicyDataStore::reregister() const {
  return reregister_;
}

#if defined(OS_CHROMEOS)
DeviceStatusCollector*
    CloudPolicyDataStore::device_status_collector() {
  return device_status_collector_.get();
}

void CloudPolicyDataStore::set_device_status_collector(
    DeviceStatusCollector* collector) {
  device_status_collector_.reset(collector);
}
#endif  // OS_CHROMEOS

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
