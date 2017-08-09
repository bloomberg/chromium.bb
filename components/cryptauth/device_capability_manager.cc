// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cryptauth/device_capability_manager.h"

#include "base/logging.h"
#include "components/cryptauth/proto/cryptauth_api.pb.h"

namespace cryptauth {

DeviceCapabilityManager::DeviceCapabilityManager(
    CryptAuthClientFactory* cryptauth_client_factory)
    : crypt_auth_client_factory_(cryptauth_client_factory),
      weak_ptr_factory_(this) {}

DeviceCapabilityManager::~DeviceCapabilityManager() {}

void DeviceCapabilityManager::SetCapabilityEnabled(
    const std::string& public_key,
    Capability capability,
    bool enabled,
    const base::Closure& success_callback,
    const base::Callback<void(const std::string&)>& error_callback) {
  pending_requests_.emplace(base::MakeUnique<Request>(
      RequestType::SET_CAPABILITY_ENABLED, Capability::CAPABILITY_UNLOCK_KEY,
      public_key, enabled, success_callback, error_callback));
  ProcessRequestQueue();
}

void DeviceCapabilityManager::FindEligibleDevicesForCapability(
    Capability capability,
    const base::Callback<void(const std::vector<ExternalDeviceInfo>&,
                              const std::vector<IneligibleDevice>&)>&
        success_callback,
    const base::Callback<void(const std::string&)>& error_callback) {
  pending_requests_.emplace(base::MakeUnique<Request>(
      RequestType::FIND_ELIGIBLE_DEVICES_FOR_CAPABILITY,
      Capability::CAPABILITY_UNLOCK_KEY, success_callback, error_callback));
  ProcessRequestQueue();
}

void DeviceCapabilityManager::IsCapabilityPromotable(
    const std::string& public_key,
    Capability capability,
    const base::Callback<void(bool)>& success_callback,
    const base::Callback<void(const std::string&)>& error_callback) {
  pending_requests_.emplace(base::MakeUnique<Request>(
      RequestType::IS_CAPABILITY_PROMOTABLE, capability, public_key,
      success_callback, error_callback));
  ProcessRequestQueue();
}

DeviceCapabilityManager::Request::Request(
    RequestType request_type,
    Capability capability,
    std::string public_key,
    bool enabled,
    const base::Closure& set_capability_callback,
    const base::Callback<void(const std::string&)>& error_callback)
    : request_type(request_type),
      error_callback(error_callback),
      capability(capability),
      public_key(public_key),
      set_capability_callback(set_capability_callback),
      enabled(enabled) {}

DeviceCapabilityManager::Request::Request(
    RequestType request_type,
    Capability capability,
    const base::Callback<void(const std::vector<ExternalDeviceInfo>&,
                              const std::vector<IneligibleDevice>&)>&
        find_eligible_devices_callback,
    const base::Callback<void(const std::string&)>& error_callback)
    : request_type(request_type),
      error_callback(error_callback),
      capability(capability),
      find_eligible_devices_callback(find_eligible_devices_callback) {}

DeviceCapabilityManager::Request::Request(
    RequestType request_type,
    Capability capability,
    std::string public_key,
    const base::Callback<void(bool)> is_device_promotable_callback,
    const base::Callback<void(const std::string&)>& error_callback)
    : request_type(request_type),
      error_callback(error_callback),
      capability(capability),
      public_key(public_key),
      is_device_promotable_callback(is_device_promotable_callback) {}

DeviceCapabilityManager::Request::~Request() {}

void DeviceCapabilityManager::CreateNewCryptAuthClient() {
  DCHECK(!current_cryptauth_client_);
  current_cryptauth_client_ = crypt_auth_client_factory_->CreateInstance();
}

void DeviceCapabilityManager::ProcessSetCapabilityEnabledRequest() {
  DCHECK(current_request_->capability == Capability::CAPABILITY_UNLOCK_KEY);
  SetUnlockKeyCapability();
}

void DeviceCapabilityManager::ProcessFindEligibleDevicesForCapability() {
  DCHECK(current_request_->capability == Capability::CAPABILITY_UNLOCK_KEY);
  FindEligibleUnlockDevices();
}

void DeviceCapabilityManager::ProcessIsCapabilityPromotableRequest() {
  DCHECK(current_request_->capability == Capability::CAPABILITY_UNLOCK_KEY);
  IsDeviceUnlockPromotable();
}

void DeviceCapabilityManager::SetUnlockKeyCapability() {
  CreateNewCryptAuthClient();

  ToggleEasyUnlockRequest request;
  request.set_enable(current_request_->enabled);
  request.set_apply_to_all(true);
  request.set_public_key(current_request_->public_key);

  current_cryptauth_client_->ToggleEasyUnlock(
      request,
      base::Bind(&DeviceCapabilityManager::OnToggleEasyUnlockResponse,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&DeviceCapabilityManager::OnErrorResponse,
                 weak_ptr_factory_.GetWeakPtr()));
}

void DeviceCapabilityManager::FindEligibleUnlockDevices() {
  CreateNewCryptAuthClient();

  current_cryptauth_client_->FindEligibleUnlockDevices(
      FindEligibleUnlockDevicesRequest(),
      base::Bind(&DeviceCapabilityManager::OnFindEligibleUnlockDevicesResponse,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&DeviceCapabilityManager::OnErrorResponse,
                 weak_ptr_factory_.GetWeakPtr()));
}

void DeviceCapabilityManager::IsDeviceUnlockPromotable() {
  CreateNewCryptAuthClient();

  FindEligibleForPromotionRequest request;
  request.set_promoter_public_key(current_request_->public_key);

  current_cryptauth_client_->FindEligibleForPromotion(
      request,
      base::Bind(&DeviceCapabilityManager::OnIsDeviceUnlockPromotableResponse,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&DeviceCapabilityManager::OnErrorResponse,
                 weak_ptr_factory_.GetWeakPtr()));
}

void DeviceCapabilityManager::OnToggleEasyUnlockResponse(
    const ToggleEasyUnlockResponse& response) {
  current_cryptauth_client_.reset();
  current_request_->set_capability_callback.Run();
  current_request_.reset();
  ProcessRequestQueue();
}

void DeviceCapabilityManager::OnFindEligibleUnlockDevicesResponse(
    const FindEligibleUnlockDevicesResponse& response) {
  current_cryptauth_client_.reset();
  current_request_->find_eligible_devices_callback.Run(
      std::vector<ExternalDeviceInfo>(response.eligible_devices().begin(),
                                      response.eligible_devices().end()),
      std::vector<IneligibleDevice>(response.ineligible_devices().begin(),
                                    response.ineligible_devices().end()));
  current_request_.reset();
  ProcessRequestQueue();
}

void DeviceCapabilityManager::OnIsDeviceUnlockPromotableResponse(
    const FindEligibleForPromotionResponse& response) {
  current_cryptauth_client_.reset();
  current_request_->is_device_promotable_callback.Run(
      response.may_show_promo());
  current_request_.reset();
  ProcessRequestQueue();
}

void DeviceCapabilityManager::OnErrorResponse(const std::string& response) {
  current_cryptauth_client_.reset();
  current_request_->error_callback.Run(response);
  current_request_.reset();
  ProcessRequestQueue();
}

void DeviceCapabilityManager::ProcessRequestQueue() {
  if (current_request_ || pending_requests_.empty())
    return;

  current_request_ = std::move(pending_requests_.front());
  pending_requests_.pop();

  switch (current_request_->request_type) {
    case RequestType::SET_CAPABILITY_ENABLED:
      ProcessSetCapabilityEnabledRequest();
      return;
    case RequestType::FIND_ELIGIBLE_DEVICES_FOR_CAPABILITY:
      ProcessFindEligibleDevicesForCapability();
      return;
    case RequestType::IS_CAPABILITY_PROMOTABLE:
      ProcessIsCapabilityPromotableRequest();
      return;
    default:
      NOTREACHED();
      return;
  }
}

}  // namespace cryptauth