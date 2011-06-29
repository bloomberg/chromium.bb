// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/user_policy_identity_strategy.h"

#include "chrome/browser/policy/proto/device_management_backend.pb.h"
#include "chrome/browser/policy/proto/device_management_constants.h"
#include "chrome/common/guid.h"

namespace policy {

namespace em = enterprise_management;

UserPolicyIdentityStrategy::UserPolicyIdentityStrategy(
    const std::string& user_name,
    const FilePath& cache_file)
    : cache_loaded_(false),
      user_name_(user_name),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  cache_ = new UserPolicyTokenCache(weak_ptr_factory_.GetWeakPtr(), cache_file);
}

UserPolicyIdentityStrategy::~UserPolicyIdentityStrategy() {}

void UserPolicyIdentityStrategy::LoadTokenCache() {
  cache_->Load();
}

std::string UserPolicyIdentityStrategy::GetDeviceToken() {
  return device_token_;
}

std::string UserPolicyIdentityStrategy::GetDeviceID() {
  return device_id_;
}

std::string UserPolicyIdentityStrategy::GetMachineID() {
  return std::string();
}

std::string UserPolicyIdentityStrategy::GetMachineModel() {
  return std::string();
}

em::DeviceRegisterRequest_Type
UserPolicyIdentityStrategy::GetPolicyRegisterType() {
  return em::DeviceRegisterRequest::USER;
}

std::string UserPolicyIdentityStrategy::GetPolicyType() {
  return kChromeUserPolicyType;
}

bool UserPolicyIdentityStrategy::GetCredentials(std::string* username,
                                                std::string* auth_token) {
  *username = user_name_;
  *auth_token = auth_token_;

  return !username->empty() && !auth_token->empty() && !device_id_.empty();
}

void UserPolicyIdentityStrategy::OnDeviceTokenAvailable(
    const std::string& token) {
  DCHECK(!device_id_.empty());
  device_token_ = token;
  cache_->Store(device_token_, device_id_);
  NotifyDeviceTokenChanged();
}

void UserPolicyIdentityStrategy::CheckAndTriggerFetch() {
  if (!user_name_.empty() && !auth_token_.empty() && cache_loaded_) {
    // For user tokens, there is no actual identifier. We generate a random
    // identifier instead each time we ask for the token.
    // This shouldn't be done before the cache is loaded, because there may
    // already be a device id and matching device token stored there.
    device_id_ = guid::GenerateGUID();
    NotifyAuthChanged();
  }
}

void UserPolicyIdentityStrategy::SetAuthToken(const std::string& auth_token) {
  auth_token_ = auth_token;

  // Request a new device management server token, but only in case we
  // don't already have it.
  if (device_token_.empty())
    CheckAndTriggerFetch();
}

void UserPolicyIdentityStrategy::OnTokenCacheLoaded(
    const std::string& token,
    const std::string& device_id) {
  if (cache_loaded_)
    return;
  cache_loaded_ = true;
  if (!token.empty() && !device_id.empty()) {
    device_token_ = token;
    device_id_ = device_id;
    NotifyDeviceTokenChanged();
  } else {
    CheckAndTriggerFetch();
  }
}


}  // namespace policy
