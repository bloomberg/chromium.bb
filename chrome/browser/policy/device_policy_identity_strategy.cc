// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/device_policy_identity_strategy.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/ownership_service.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/net/gaia/token_service.h"
#include "chrome/browser/policy/proto/device_management_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/guid.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "content/common/notification_service.h"
#include "content/common/notification_type.h"

namespace policy {

DevicePolicyIdentityStrategy::DevicePolicyIdentityStrategy() {
}

DevicePolicyIdentityStrategy::~DevicePolicyIdentityStrategy() {
}

std::string DevicePolicyIdentityStrategy::GetDeviceToken() {
  return device_token_;
}

std::string DevicePolicyIdentityStrategy::GetDeviceID() {
  return device_id_;
}

std::string DevicePolicyIdentityStrategy::GetMachineID() {
  return machine_id_;
}

em::DeviceRegisterRequest_Type
DevicePolicyIdentityStrategy::GetPolicyRegisterType() {
  return em::DeviceRegisterRequest::DEVICE;
}

std::string DevicePolicyIdentityStrategy::GetPolicyType() {
  return kChromeDevicePolicyType;
}

void DevicePolicyIdentityStrategy::SetAuthCredentials(
    const std::string& username,
    const std::string& auth_token,
    const std::string& machine_id) {
  username_ = username;
  auth_token_ = auth_token;
  machine_id_ = machine_id;
  device_id_ = guid::GenerateGUID();
  NotifyAuthChanged();
}

void DevicePolicyIdentityStrategy::SetDeviceManagementCredentials(
    const std::string& owner_email,
    const std::string& device_id,
    const std::string& device_token) {
  username_ = owner_email;
  device_id_ = device_id;
  device_token_ = device_token;
  NotifyDeviceTokenChanged();
}

bool DevicePolicyIdentityStrategy::GetCredentials(std::string* username,
                                                  std::string* auth_token) {
  // Need to know the machine id.
  if (machine_id_.empty())
    return false;

  *username = username_;
  *auth_token = auth_token_;

  return !username->empty() && !auth_token->empty();
}

void DevicePolicyIdentityStrategy::OnDeviceTokenAvailable(
    const std::string& token) {
  DCHECK(!machine_id_.empty());

  device_token_ = token;
  NotifyDeviceTokenChanged();
}

}  // namespace policy
