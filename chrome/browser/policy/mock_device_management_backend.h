// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MOCK_DEVICE_MANAGEMENT_BACKEND_H_
#define CHROME_BROWSER_POLICY_MOCK_DEVICE_MANAGEMENT_BACKEND_H_
#pragma once

#include <string>

#include "base/time.h"
#include "chrome/browser/policy/device_management_backend.h"
#include "chrome/browser/policy/proto/cloud_policy.pb.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"
#include "chrome/browser/policy/proto/device_management_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

// Useful for unit testing when a server-based backend isn't
// available. Simulates both successful and failed requests to the device
// management server.
class MockDeviceManagementBackend : public DeviceManagementBackend {
 public:
  MockDeviceManagementBackend();
  virtual ~MockDeviceManagementBackend();

  // DeviceManagementBackend method overrides:
  MOCK_METHOD5(ProcessRegisterRequest, void(
      const std::string& gaia_auth_token,
      const std::string& oauth_token,
      const std::string& device_id,
      const enterprise_management::DeviceRegisterRequest& request,
      DeviceRegisterResponseDelegate* delegate));

  MOCK_METHOD4(ProcessUnregisterRequest, void(
      const std::string& device_management_token,
      const std::string& device_id,
      const enterprise_management::DeviceUnregisterRequest& request,
      DeviceUnregisterResponseDelegate* delegate));

  MOCK_METHOD6(ProcessPolicyRequest, void(
      const std::string& device_management_token,
      const std::string& device_id,
      CloudPolicyDataStore::UserAffiliation affiliation,
      const enterprise_management::DevicePolicyRequest& request,
      const enterprise_management::DeviceStatusReportRequest* device_status,
      DevicePolicyResponseDelegate* delegate));

  MOCK_METHOD3(ProcessAutoEnrollmentRequest, void(
      const std::string& device_id,
      const enterprise_management::DeviceAutoEnrollmentRequest& request,
      DeviceAutoEnrollmentResponseDelegate* delegate));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDeviceManagementBackend);
};

ACTION(MockDeviceManagementBackendSucceedRegister) {
  enterprise_management::DeviceRegisterResponse response;
  std::string token("FAKE_DEVICE_TOKEN_");
  static int next_token_suffix;
  token += next_token_suffix++;
  response.set_device_management_token(token);
  arg4->HandleRegisterResponse(response);
}

ACTION(MockDeviceManagementBackendSucceedSpdyCloudPolicy) {
  enterprise_management::PolicyData signed_response;
  enterprise_management::CloudPolicySettings settings;
  enterprise_management::DisableSpdyProto* spdy_proto =
      settings.mutable_disablespdy();
  spdy_proto->set_disablespdy(true);
  spdy_proto->mutable_policy_options()->set_mode(
      enterprise_management::PolicyOptions::MANDATORY);
  EXPECT_TRUE(
      settings.SerializeToString(signed_response.mutable_policy_value()));
  base::TimeDelta timestamp =
      base::Time::NowFromSystemTime() - base::Time::UnixEpoch();
  signed_response.set_timestamp(timestamp.InMilliseconds());
  std::string serialized_signed_response;
  EXPECT_TRUE(signed_response.SerializeToString(&serialized_signed_response));
  enterprise_management::DevicePolicyResponse response;
  enterprise_management::PolicyFetchResponse* fetch_response =
      response.add_response();
  fetch_response->set_policy_data(serialized_signed_response);
  // TODO(jkummerow): Set proper new_public_key and signature (when
  // implementing support for signature verification).
  fetch_response->set_policy_data_signature("TODO");
  fetch_response->set_new_public_key("TODO");
  arg5->HandlePolicyResponse(response);
}

ACTION_P(MockDeviceManagementBackendFailRegister, error) {
  arg4->OnError(error);
}

ACTION_P(MockDeviceManagementBackendFailPolicy, error) {
  arg5->OnError(error);
}

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_MOCK_DEVICE_MANAGEMENT_BACKEND_H_
