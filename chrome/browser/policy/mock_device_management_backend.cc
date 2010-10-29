// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chrome/browser/policy/mock_device_management_backend.h"

namespace {

static const char kFakeDeviceManagementToken[] = "FAKE_DEVICE_TOKEN_";
static char next_token_suffix_ = '0';

}  // namespace

namespace policy {

using enterprise_management::DeviceRegisterRequest;
using enterprise_management::DeviceUnregisterRequest;
using enterprise_management::DevicePolicyRequest;
using enterprise_management::DeviceRegisterResponse;
using enterprise_management::DeviceUnregisterResponse;
using enterprise_management::DevicePolicyResponse;

MockDeviceManagementBackend::MockDeviceManagementBackend()
    : DeviceManagementBackend(),
      failure_(false) {
}

void MockDeviceManagementBackend::ProcessRegisterRequest(
    const std::string& auth_token,
    const std::string& device_id,
    const DeviceRegisterRequest& request,
    DeviceRegisterResponseDelegate* delegate) {
  if (failure_) {
    delegate->OnError(kErrorRequestInvalid);
  } else {
    DeviceRegisterResponse response;
    std::string token(kFakeDeviceManagementToken);
    token += next_token_suffix_++;
    response.set_device_management_token(token);
    delegate->HandleRegisterResponse(response);
  }
}

void MockDeviceManagementBackend::ProcessUnregisterRequest(
    const std::string& device_management_token,
    const DeviceUnregisterRequest& request,
    DeviceUnregisterResponseDelegate* delegate) {
  // TODO(danno): need a mock implementation for the backend here.
  NOTREACHED();
}

void MockDeviceManagementBackend::ProcessPolicyRequest(
    const std::string& device_management_token,
    const DevicePolicyRequest& request,
    DevicePolicyResponseDelegate* delegate) {
  // TODO(danno): need a mock implementation for the backend here.
  NOTREACHED();
}

}  // namespace
