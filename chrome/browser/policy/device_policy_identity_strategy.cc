// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/device_policy_identity_strategy.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/login/ownership_service.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/system_access.h"
#include "chrome/browser/net/gaia/token_service.h"
#include "chrome/browser/policy/proto/device_management_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/guid.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "content/common/notification_service.h"
#include "content/common/notification_type.h"

// MachineInfo key names.
static const char kMachineInfoSystemHwqual[] = "hardware_class";
static const char kMachineInfoSerialNumber[] = "serial_number";

namespace policy {

DevicePolicyIdentityStrategy::DevicePolicyIdentityStrategy() {
  chromeos::SystemAccess* sys_lib = chromeos::SystemAccess::GetInstance();

  if (!sys_lib->GetMachineStatistic(kMachineInfoSystemHwqual,
                                    &machine_model_)) {
    LOG(ERROR) << "Failed to get machine model.";
  }
  if (!sys_lib->GetMachineStatistic(kMachineInfoSerialNumber,
                                    &machine_id_)) {
    LOG(ERROR) << "Failed to get machine serial number.";
  }
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

std::string DevicePolicyIdentityStrategy::GetMachineModel() {
  return machine_model_;
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
    const std::string& auth_token) {
  username_ = username;
  auth_token_ = auth_token;
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

void DevicePolicyIdentityStrategy::FetchPolicy() {
  DCHECK(!device_token_.empty());
  NotifyDeviceTokenChanged();
}

bool DevicePolicyIdentityStrategy::GetCredentials(std::string* username,
                                                  std::string* auth_token) {
  *username = username_;
  *auth_token = auth_token_;

  return !username->empty() && !auth_token->empty();
}

void DevicePolicyIdentityStrategy::OnDeviceTokenAvailable(
    const std::string& token) {
  device_token_ = token;
}

}  // namespace policy
