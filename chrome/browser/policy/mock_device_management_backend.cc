// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/mock_device_management_backend.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/stl_util-inl.h"

namespace {

static const char kFakeDeviceManagementToken[] = "FAKE_DEVICE_TOKEN_";
static char next_token_suffix_ = '0';

}  // namespace

namespace policy {

using ::testing::_;
using ::testing::Invoke;

using em::DeviceRegisterRequest;
using em::DeviceUnregisterRequest;
using em::DevicePolicyRequest;
using em::DeviceRegisterResponse;
using em::DeviceUnregisterResponse;
using em::DevicePolicyResponse;

MockDeviceManagementBackend::MockDeviceManagementBackend()
    : DeviceManagementBackend() {
  policy_setting_ = policy_response_.add_setting();
  policy_setting_->set_policy_key("chrome-policy");
  policy_setting_->set_watermark("fresh");
}

MockDeviceManagementBackend::~MockDeviceManagementBackend() {
}

void MockDeviceManagementBackend::AllShouldSucceed() {
  ON_CALL(*this, ProcessRegisterRequest(_, _, _, _)).
      WillByDefault(Invoke(
          this,
          &MockDeviceManagementBackend::SimulateSuccessfulRegisterRequest));
  ON_CALL(*this, ProcessPolicyRequest(_, _, _)).
      WillByDefault(Invoke(
          this,
          &MockDeviceManagementBackend::SimulateSuccessfulPolicyRequest));
}

void MockDeviceManagementBackend::AllShouldFail() {
  ON_CALL(*this, ProcessRegisterRequest(_, _, _, _)).
      WillByDefault(Invoke(
          this,
          &MockDeviceManagementBackend::SimulateFailedRegisterRequest));
  ON_CALL(*this, ProcessPolicyRequest(_, _, _)).
      WillByDefault(Invoke(
          this,
          &MockDeviceManagementBackend::SimulateFailedPolicyRequest));
}

void MockDeviceManagementBackend::UnmanagedDevice() {
  ON_CALL(*this, ProcessRegisterRequest(_, _, _, _)).
      WillByDefault(Invoke(
          this,
          &MockDeviceManagementBackend::SimulateUnmanagedRegisterRequest));
}

void MockDeviceManagementBackend::AddBooleanPolicy(const char* policy_name,
                                                   bool value) {
  em::GenericSetting* policy_value = policy_setting_->mutable_policy_value();
  em::GenericNamedValue* named_value = policy_value->add_named_value();
  named_value->set_name(policy_name);
  named_value->mutable_value()->set_value_type(
      em::GenericValue::VALUE_TYPE_BOOL);
  named_value->mutable_value()->set_bool_value(value);
}

void MockDeviceManagementBackend::SimulateSuccessfulRegisterRequest(
    const std::string& auth_token,
    const std::string& device_id,
    const em::DeviceRegisterRequest& request,
    DeviceRegisterResponseDelegate* delegate) {
  DeviceRegisterResponse response;
  std::string token(kFakeDeviceManagementToken);
  token += next_token_suffix_++;
  response.set_device_management_token(token);
  delegate->HandleRegisterResponse(response);
}

void MockDeviceManagementBackend::SimulateSuccessfulPolicyRequest(
    const std::string& device_management_token,
    const em::DevicePolicyRequest& request,
    DevicePolicyResponseDelegate* delegate) {
  delegate->HandlePolicyResponse(policy_response_);
}

void MockDeviceManagementBackend::SimulateFailedRegisterRequest(
    const std::string& auth_token,
    const std::string& device_id,
    const em::DeviceRegisterRequest& request,
    DeviceRegisterResponseDelegate* delegate) {
  delegate->OnError(kErrorRequestFailed);
}

void MockDeviceManagementBackend::SimulateFailedPolicyRequest(
    const std::string& device_management_token,
    const em::DevicePolicyRequest& request,
    DevicePolicyResponseDelegate* delegate) {
  delegate->OnError(kErrorRequestFailed);
}

void MockDeviceManagementBackend::SimulateUnmanagedRegisterRequest(
    const std::string& auth_token,
    const std::string& device_id,
    const em::DeviceRegisterRequest& request,
    DeviceRegisterResponseDelegate* delegate) {
  delegate->OnError(kErrorServiceManagementNotSupported);
}

}  // namespace
