// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MOCK_DEVICE_MANAGEMENT_BACKEND_H_
#define CHROME_BROWSER_POLICY_MOCK_DEVICE_MANAGEMENT_BACKEND_H_
#pragma once

#include <map>
#include <string>

#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/policy/device_management_backend.h"
#include "chrome/browser/policy/proto/device_management_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace em = enterprise_management;

// Useful for unit testing when a server-based backend isn't
// available. Simulates both successful and failed requests to the device
// management server.
class MockDeviceManagementBackend : public DeviceManagementBackend {
 public:
  MockDeviceManagementBackend();
  virtual ~MockDeviceManagementBackend();

  // DeviceManagementBackend method overrides:
  MOCK_METHOD4(ProcessRegisterRequest, void(
      const std::string& auth_token,
      const std::string& device_id,
      const em::DeviceRegisterRequest& request,
      DeviceRegisterResponseDelegate* delegate));

  MOCK_METHOD4(ProcessUnregisterRequest, void(
      const std::string& device_management_token,
      const std::string& device_id,
      const em::DeviceUnregisterRequest& request,
      DeviceUnregisterResponseDelegate* delegate));

  MOCK_METHOD4(ProcessPolicyRequest, void(
      const std::string& device_management_token,
      const std::string& device_id,
      const em::DevicePolicyRequest& request,
      DevicePolicyResponseDelegate* delegate));

  MOCK_METHOD4(ProcessCloudPolicyRequest, void(
      const std::string& device_management_token,
      const std::string& device_id,
      const em::CloudPolicyRequest& request,
      DevicePolicyResponseDelegate* delegate));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDeviceManagementBackend);
};

ACTION(MockDeviceManagementBackendSucceedRegister) {
  em::DeviceRegisterResponse response;
  std::string token("FAKE_DEVICE_TOKEN_");
  static int next_token_suffix;
  token += next_token_suffix++;
  response.set_device_management_token(token);
  arg3->HandleRegisterResponse(response);
}

ACTION_P2(MockDeviceManagementBackendSucceedBooleanPolicy, name, value) {
  em::DevicePolicyResponse response;
  em::DevicePolicySetting* setting = response.add_setting();
  setting->set_policy_key(kChromeDevicePolicySettingKey);
  setting->set_watermark("fresh");
  em::GenericSetting* policy_value = setting->mutable_policy_value();
  em::GenericNamedValue* named_value = policy_value->add_named_value();
  named_value->set_name(name);
  named_value->mutable_value()->set_value_type(
      em::GenericValue::VALUE_TYPE_BOOL);
  named_value->mutable_value()->set_bool_value(value);
  arg3->HandlePolicyResponse(response);
}

ACTION(MockDeviceManagementBackendSucceedSpdyCloudPolicy) {
  em::SignedCloudPolicyResponse signed_response;
  em::CloudPolicySettings* settings = signed_response.mutable_settings();
  em::DisableSpdyProto* spdy_proto = settings->mutable_disablespdy();
  spdy_proto->set_disablespdy(true);
  spdy_proto->mutable_policy_options()->set_mode(em::PolicyOptions::MANDATORY);
  signed_response.set_timestamp(base::Time::NowFromSystemTime().ToTimeT());
  std::string serialized_signed_response;
  EXPECT_TRUE(signed_response.SerializeToString(&serialized_signed_response));
  em::CloudPolicyResponse response;
  response.set_signed_response(serialized_signed_response);
  // TODO(jkummerow): Set proper certificate_chain and signature (when
  // implementing support for signature verification).
  response.set_signature("TODO");
  response.add_certificate_chain("TODO");
  arg3->HandleCloudPolicyResponse(response);
}

ACTION_P(MockDeviceManagementBackendFailRegister, error) {
  arg3->OnError(error);
}

ACTION_P(MockDeviceManagementBackendFailPolicy, error) {
  arg3->OnError(error);
}

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_MOCK_DEVICE_MANAGEMENT_BACKEND_H_
