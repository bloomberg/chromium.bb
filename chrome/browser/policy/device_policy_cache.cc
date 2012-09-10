// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/device_policy_cache.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/values.h"
#include "chrome/browser/policy/cloud_policy_data_store.h"
#include "chrome/browser/policy/device_policy_decoder_chromeos.h"
#include "chrome/browser/policy/enterprise_install_attributes.h"
#include "chrome/browser/policy/enterprise_metrics.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/policy/proto/chrome_device_policy.pb.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"
#include "chrome/browser/policy/proto/device_management_local.pb.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace em = enterprise_management;

namespace policy {

DevicePolicyCache::DevicePolicyCache(
    CloudPolicyDataStore* data_store,
    EnterpriseInstallAttributes* install_attributes)
    : data_store_(data_store),
      install_attributes_(install_attributes),
      device_settings_service_(chromeos::DeviceSettingsService::Get()),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)),
      policy_fetch_pending_(false) {
  device_settings_service_->AddObserver(this);
}

DevicePolicyCache::DevicePolicyCache(
    CloudPolicyDataStore* data_store,
    EnterpriseInstallAttributes* install_attributes,
    chromeos::DeviceSettingsService* device_settings_service)
    : data_store_(data_store),
      install_attributes_(install_attributes),
      device_settings_service_(device_settings_service),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)),
      policy_fetch_pending_(false) {
  device_settings_service_->AddObserver(this);
}

DevicePolicyCache::~DevicePolicyCache() {
  device_settings_service_->RemoveObserver(this);
}

void DevicePolicyCache::Load() {
  DeviceSettingsUpdated();
}

bool DevicePolicyCache::SetPolicy(const em::PolicyFetchResponse& policy) {
  DCHECK(IsReady());

  // Make sure we have an enterprise device.
  std::string registration_domain(install_attributes_->GetDomain());
  if (registration_domain.empty()) {
    LOG(WARNING) << "Refusing to accept policy on non-enterprise device.";
    UMA_HISTOGRAM_ENUMERATION(kMetricPolicy,
                              kMetricPolicyFetchNonEnterpriseDevice,
                              kMetricPolicySize);
    InformNotifier(CloudPolicySubsystem::LOCAL_ERROR,
                   CloudPolicySubsystem::POLICY_LOCAL_ERROR);
    return false;
  }

  // Check the user this policy is for against the device-locked name.
  em::PolicyData policy_data;
  if (!policy_data.ParseFromString(policy.policy_data())) {
    LOG(WARNING) << "Invalid policy protobuf";
    UMA_HISTOGRAM_ENUMERATION(kMetricPolicy, kMetricPolicyFetchInvalidPolicy,
                              kMetricPolicySize);
    InformNotifier(CloudPolicySubsystem::LOCAL_ERROR,
                   CloudPolicySubsystem::POLICY_LOCAL_ERROR);
    return false;
  }

  // Existing installations may not have a canonicalized version of the
  // registration domain in install attributes, so lower-case the data here.
  if (registration_domain != gaia::ExtractDomainName(policy_data.username())) {
    LOG(WARNING) << "Refusing policy blob for " << policy_data.username()
                 << " which doesn't match domain " << registration_domain;
    UMA_HISTOGRAM_ENUMERATION(kMetricPolicy, kMetricPolicyFetchUserMismatch,
                              kMetricPolicySize);
    InformNotifier(CloudPolicySubsystem::LOCAL_ERROR,
                   CloudPolicySubsystem::POLICY_LOCAL_ERROR);
    return false;
  }

  set_last_policy_refresh_time(base::Time::NowFromSystemTime());

  // Start a store operation.
  policy_fetch_pending_ = true;
  device_settings_service_->Store(
      scoped_ptr<em::PolicyFetchResponse>(new em::PolicyFetchResponse(policy)),
      base::Bind(&DevicePolicyCache::PolicyStoreOpCompleted,
                 weak_ptr_factory_.GetWeakPtr()));
  return true;
}

void DevicePolicyCache::SetUnmanaged() {
  LOG(WARNING) << "Tried to set DevicePolicyCache to 'unmanaged'!";
  // This is not supported for DevicePolicyCache.
}

void DevicePolicyCache::SetFetchingDone() {
  // Don't send the notification just yet if there is a pending policy
  // store/reload cycle.
  if (!policy_fetch_pending_)
    CloudPolicyCacheBase::SetFetchingDone();
}

void DevicePolicyCache::OwnershipStatusChanged() {}

void DevicePolicyCache::DeviceSettingsUpdated() {
  DCHECK(CalledOnValidThread());
  chromeos::DeviceSettingsService::Status status =
      device_settings_service_->status();
  const em::PolicyData* policy_data = device_settings_service_->policy_data();
  if (status == chromeos::DeviceSettingsService::STORE_SUCCESS &&
      !policy_data) {
    // Initial policy load is still pending.
    return;
  }

  if (!IsReady()) {
    std::string device_token;
    InstallInitialPolicy(status, policy_data, &device_token);
    SetTokenAndFlagReady(device_token);
  } else {  // In other words, IsReady() == true
    if (status != chromeos::DeviceSettingsService::STORE_SUCCESS ||
        !policy_data) {
      if (status == chromeos::DeviceSettingsService::STORE_VALIDATION_ERROR) {
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
    } else {
      em::PolicyFetchResponse policy_response;
      CHECK(policy_data->SerializeToString(
          policy_response.mutable_policy_data()));
      bool ok = SetPolicyInternal(policy_response, NULL, false);
      if (ok) {
        UMA_HISTOGRAM_ENUMERATION(kMetricPolicy, kMetricPolicyFetchOK,
                                  kMetricPolicySize);
      }
    }
  }
}

bool DevicePolicyCache::DecodePolicyData(const em::PolicyData& policy_data,
                                         PolicyMap* policies) {
  em::ChromeDeviceSettingsProto policy;
  if (!policy.ParseFromString(policy_data.policy_value())) {
    LOG(WARNING) << "Failed to parse ChromeDeviceSettingsProto.";
    return false;
  }
  DecodeDevicePolicy(policy, policies, install_attributes_);
  return true;
}

void DevicePolicyCache::PolicyStoreOpCompleted() {
  DCHECK(CalledOnValidThread());
  chromeos::DeviceSettingsService::Status status =
      device_settings_service_->status();
  if (status != chromeos::DeviceSettingsService::STORE_SUCCESS) {
    UMA_HISTOGRAM_ENUMERATION(kMetricPolicy, kMetricPolicyStoreFailed,
                              kMetricPolicySize);
    if (status == chromeos::DeviceSettingsService::STORE_VALIDATION_ERROR) {
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
    CheckFetchingDone();
    return;
  }
  UMA_HISTOGRAM_ENUMERATION(kMetricPolicy, kMetricPolicyStoreSucceeded,
                            kMetricPolicySize);

  CheckFetchingDone();
}

void DevicePolicyCache::InstallInitialPolicy(
    chromeos::DeviceSettingsService::Status status,
    const em::PolicyData* policy_data,
    std::string* device_token) {
  if (status == chromeos::DeviceSettingsService::STORE_NO_POLICY ||
      status == chromeos::DeviceSettingsService::STORE_KEY_UNAVAILABLE) {
    InformNotifier(CloudPolicySubsystem::UNENROLLED,
                   CloudPolicySubsystem::NO_DETAILS);
    return;
  }
  if (!policy_data) {
    LOG(WARNING) << "Failed to parse PolicyData protobuf.";
    UMA_HISTOGRAM_ENUMERATION(kMetricPolicy, kMetricPolicyLoadFailed,
                              kMetricPolicySize);
    InformNotifier(CloudPolicySubsystem::LOCAL_ERROR,
                   CloudPolicySubsystem::POLICY_LOCAL_ERROR);
    return;
  }
  if (!policy_data->has_request_token() ||
      policy_data->request_token().empty()) {
    SetUnmanagedInternal(base::Time::NowFromSystemTime());
    InformNotifier(CloudPolicySubsystem::UNMANAGED,
                   CloudPolicySubsystem::NO_DETAILS);
    // TODO(jkummerow): Reminder: When we want to feed device-wide settings
    // made by a local owner into this cache, we need to call
    // SetPolicyInternal() here.
    return;
  }
  if (!policy_data->has_username() || !policy_data->has_device_id()) {
    UMA_HISTOGRAM_ENUMERATION(kMetricPolicy, kMetricPolicyLoadFailed,
                              kMetricPolicySize);
    InformNotifier(CloudPolicySubsystem::LOCAL_ERROR,
                   CloudPolicySubsystem::POLICY_LOCAL_ERROR);
    return;
  }
  UMA_HISTOGRAM_ENUMERATION(kMetricPolicy, kMetricPolicyLoadSucceeded,
                            kMetricPolicySize);
  data_store_->set_user_name(policy_data->username());
  data_store_->set_device_id(policy_data->device_id());
  *device_token = policy_data->request_token();
  base::Time timestamp;
  em::PolicyFetchResponse policy_response;
  CHECK(policy_data->SerializeToString(policy_response.mutable_policy_data()));
  if (SetPolicyInternal(policy_response, &timestamp, true))
    set_last_policy_refresh_time(timestamp);
}

void DevicePolicyCache::SetTokenAndFlagReady(const std::string& device_token) {
  // We need to call SetDeviceToken unconditionally to indicate the cache has
  // finished loading.
  data_store_->SetDeviceToken(device_token, true);
  SetReady();
}

void DevicePolicyCache::CheckFetchingDone() {
  if (policy_fetch_pending_) {
    CloudPolicyCacheBase::SetFetchingDone();
    policy_fetch_pending_ = false;
  }
}

}  // namespace policy
