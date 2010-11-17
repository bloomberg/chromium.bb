// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MOCK_DEVICE_MANAGEMENT_BACKEND_H_
#define CHROME_BROWSER_POLICY_MOCK_DEVICE_MANAGEMENT_BACKEND_H_
#pragma once

#include <map>
#include <string>

#include "base/values.h"
#include "chrome/browser/policy/device_management_backend.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace policy {

namespace em = enterprise_management;

// Useful for unit testing when a server-based backend isn't
// available. Simulates both successful and failed requests to the device
// management server.
class MockDeviceManagementBackend
    : public DeviceManagementBackend {
 public:
  MockDeviceManagementBackend();
  virtual ~MockDeviceManagementBackend();

  // DeviceManagementBackend method overrides:
  MOCK_METHOD4(ProcessRegisterRequest, void(
      const std::string& auth_token,
      const std::string& device_id,
      const em::DeviceRegisterRequest& request,
      DeviceRegisterResponseDelegate* delegate));

  MOCK_METHOD3(ProcessUnregisterRequest, void(
      const std::string& device_management_token,
      const em::DeviceUnregisterRequest& request,
      DeviceUnregisterResponseDelegate* delegate));

  MOCK_METHOD3(ProcessPolicyRequest, void(
      const std::string& device_management_token,
      const em::DevicePolicyRequest& request,
      DevicePolicyResponseDelegate* delegate));

  void AllShouldSucceed();
  void AllShouldFail();
  void UnmanagedDevice();

  void SimulateSuccessfulRegisterRequest(
      const std::string& auth_token,
      const std::string& device_id,
      const em::DeviceRegisterRequest& request,
      DeviceRegisterResponseDelegate* delegate);

  void SimulateSuccessfulPolicyRequest(
      const std::string& device_management_token,
      const em::DevicePolicyRequest& request,
      DevicePolicyResponseDelegate* delegate);

  void SimulateFailedRegisterRequest(
      const std::string& auth_token,
      const std::string& device_id,
      const em::DeviceRegisterRequest& request,
      DeviceRegisterResponseDelegate* delegate);

  void SimulateFailedPolicyRequest(
      const std::string& device_management_token,
      const em::DevicePolicyRequest& request,
      DevicePolicyResponseDelegate* delegate);

  void SimulateUnmanagedRegisterRequest(
      const std::string& auth_token,
      const std::string& device_id,
      const em::DeviceRegisterRequest& request,
      DeviceRegisterResponseDelegate* delegate);

  void AddBooleanPolicy(const char* policy_name, bool value);

 private:
  em::DevicePolicyResponse policy_response_;
  em::DevicePolicySetting* policy_setting_;

  DISALLOW_COPY_AND_ASSIGN(MockDeviceManagementBackend);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_MOCK_DEVICE_MANAGEMENT_BACKEND_H_
