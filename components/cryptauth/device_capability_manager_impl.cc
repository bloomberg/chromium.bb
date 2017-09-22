// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cryptauth/device_capability_manager_impl.h"

#include "base/logging.h"
#include "components/cryptauth/proto/cryptauth_api.pb.h"

namespace cryptauth {

// static
DeviceCapabilityManagerImpl::Factory*
    DeviceCapabilityManagerImpl::Factory::factory_instance_ = nullptr;

// static
std::unique_ptr<DeviceCapabilityManager>
DeviceCapabilityManagerImpl::Factory::NewInstance(
    CryptAuthClientFactory* cryptauth_client_factory) {
  if (!factory_instance_) {
    factory_instance_ = new Factory();
  }
  return factory_instance_->BuildInstance(cryptauth_client_factory);
}

void DeviceCapabilityManagerImpl::Factory::SetInstanceForTesting(
    Factory* factory) {
  factory_instance_ = factory;
}

std::unique_ptr<DeviceCapabilityManager>
DeviceCapabilityManagerImpl::Factory::BuildInstance(
    CryptAuthClientFactory* cryptauth_client_factory) {
  return base::WrapUnique(
      new DeviceCapabilityManagerImpl(cryptauth_client_factory));
}

DeviceCapabilityManagerImpl::DeviceCapabilityManagerImpl(
    CryptAuthClientFactory* cryptauth_client_factory)
    : crypt_auth_client_factory_(cryptauth_client_factory),
      weak_ptr_factory_(this) {}

DeviceCapabilityManagerImpl::~DeviceCapabilityManagerImpl() {}

void DeviceCapabilityManagerImpl::SetCapabilityEnabled(
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

void DeviceCapabilityManagerImpl::FindEligibleDevicesForCapability(
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

void DeviceCapabilityManagerImpl::IsCapabilityPromotable(
    const std::string& public_key,
    Capability capability,
    const base::Callback<void(bool)>& success_callback,
    const base::Callback<void(const std::string&)>& error_callback) {
  pending_requests_.emplace(base::MakeUnique<Request>(
      RequestType::IS_CAPABILITY_PROMOTABLE, capability, public_key,
      success_callback, error_callback));
  ProcessRequestQueue();
}

DeviceCapabilityManagerImpl::Request::Request(
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

DeviceCapabilityManagerImpl::Request::Request(
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

DeviceCapabilityManagerImpl::Request::Request(
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

DeviceCapabilityManagerImpl::Request::~Request() {}

void DeviceCapabilityManagerImpl::CreateNewCryptAuthClient() {
  DCHECK(!current_cryptauth_client_);
  current_cryptauth_client_ = crypt_auth_client_factory_->CreateInstance();
}

void DeviceCapabilityManagerImpl::ProcessSetCapabilityEnabledRequest() {
  DCHECK(current_request_->capability == Capability::CAPABILITY_UNLOCK_KEY);
  SetUnlockKeyCapability();
}

void DeviceCapabilityManagerImpl::ProcessFindEligibleDevicesForCapability() {
  DCHECK(current_request_->capability == Capability::CAPABILITY_UNLOCK_KEY);
  FindEligibleUnlockDevices();
}

void DeviceCapabilityManagerImpl::ProcessIsCapabilityPromotableRequest() {
  DCHECK(current_request_->capability == Capability::CAPABILITY_UNLOCK_KEY);
  IsDeviceUnlockPromotable();
}

void DeviceCapabilityManagerImpl::SetUnlockKeyCapability() {
  CreateNewCryptAuthClient();

  ToggleEasyUnlockRequest request;
  request.set_enable(current_request_->enabled);
  request.set_apply_to_all(true);
  request.set_public_key(current_request_->public_key);

  current_cryptauth_client_->ToggleEasyUnlock(
      request,
      base::Bind(&DeviceCapabilityManagerImpl::OnToggleEasyUnlockResponse,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&DeviceCapabilityManagerImpl::OnErrorResponse,
                 weak_ptr_factory_.GetWeakPtr()));
}

void DeviceCapabilityManagerImpl::FindEligibleUnlockDevices() {
  CreateNewCryptAuthClient();

  current_cryptauth_client_->FindEligibleUnlockDevices(
      FindEligibleUnlockDevicesRequest(),
      base::Bind(
          &DeviceCapabilityManagerImpl::OnFindEligibleUnlockDevicesResponse,
          weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&DeviceCapabilityManagerImpl::OnErrorResponse,
                 weak_ptr_factory_.GetWeakPtr()));
}

void DeviceCapabilityManagerImpl::IsDeviceUnlockPromotable() {
  CreateNewCryptAuthClient();

  FindEligibleForPromotionRequest request;
  request.set_promoter_public_key(current_request_->public_key);

  current_cryptauth_client_->FindEligibleForPromotion(
      request,
      base::Bind(
          &DeviceCapabilityManagerImpl::OnIsDeviceUnlockPromotableResponse,
          weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&DeviceCapabilityManagerImpl::OnErrorResponse,
                 weak_ptr_factory_.GetWeakPtr()));
}

void DeviceCapabilityManagerImpl::OnToggleEasyUnlockResponse(
    const ToggleEasyUnlockResponse& response) {
  current_cryptauth_client_.reset();
  current_request_->set_capability_callback.Run();
  current_request_.reset();
  ProcessRequestQueue();
}

void DeviceCapabilityManagerImpl::OnFindEligibleUnlockDevicesResponse(
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

void DeviceCapabilityManagerImpl::OnIsDeviceUnlockPromotableResponse(
    const FindEligibleForPromotionResponse& response) {
  current_cryptauth_client_.reset();
  current_request_->is_device_promotable_callback.Run(
      response.may_show_promo());
  current_request_.reset();
  ProcessRequestQueue();
}

void DeviceCapabilityManagerImpl::OnErrorResponse(const std::string& response) {
  current_cryptauth_client_.reset();
  current_request_->error_callback.Run(response);
  current_request_.reset();
  ProcessRequestQueue();
}

void DeviceCapabilityManagerImpl::ProcessRequestQueue() {
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