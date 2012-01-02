// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/mock_device_management_service.h"

namespace em = enterprise_management;

namespace policy {

ProxyDeviceManagementBackend::ProxyDeviceManagementBackend(
    DeviceManagementBackend* backend)
    : backend_(backend) {
}
ProxyDeviceManagementBackend::~ProxyDeviceManagementBackend() {}

void ProxyDeviceManagementBackend::ProcessRegisterRequest(
    const std::string& gaia_auth_token,
    const std::string& oauth_token,
    const std::string& device_id,
    const em::DeviceRegisterRequest& request,
    DeviceRegisterResponseDelegate* delegate) {
  backend_->ProcessRegisterRequest(gaia_auth_token, oauth_token,
                                   device_id, request, delegate);
}

void ProxyDeviceManagementBackend::ProcessUnregisterRequest(
    const std::string& device_management_token,
    const std::string& device_id,
    const em::DeviceUnregisterRequest& request,
    DeviceUnregisterResponseDelegate* delegate) {
  backend_->ProcessUnregisterRequest(device_management_token, device_id,
                                     request, delegate);
}

void ProxyDeviceManagementBackend::ProcessPolicyRequest(
    const std::string& device_management_token,
    const std::string& device_id,
    CloudPolicyDataStore::UserAffiliation affiliation,
    const em::DevicePolicyRequest& request,
    const em::DeviceStatusReportRequest* device_status,
    DevicePolicyResponseDelegate* delegate) {
  backend_->ProcessPolicyRequest(device_management_token, device_id,
                                 affiliation, request, device_status,
                                 delegate);
}

void ProxyDeviceManagementBackend::ProcessAutoEnrollmentRequest(
    const std::string& device_id,
    const em::DeviceAutoEnrollmentRequest& request,
    DeviceAutoEnrollmentResponseDelegate* delegate) {
  backend_->ProcessAutoEnrollmentRequest(device_id, request, delegate);
}

MockDeviceManagementService::MockDeviceManagementService()
    : DeviceManagementService("") {}

MockDeviceManagementService::~MockDeviceManagementService() {}

}  // namespace policy
