// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MOCK_DEVICE_MANAGEMENT_BACKEND_H_
#define CHROME_BROWSER_POLICY_MOCK_DEVICE_MANAGEMENT_BACKEND_H_
#pragma once

#include <string>

#include "chrome/browser/policy/device_management_backend.h"

namespace policy {

namespace em = enterprise_management;

// Useful for unit testing when a server-based backend isn't
// available. Simulates both successful and failed requests to the device
// management server.
class MockDeviceManagementBackend
    : public DeviceManagementBackend {
 public:
  MockDeviceManagementBackend();
  virtual ~MockDeviceManagementBackend() {}

  void SetFailure(bool failure) { failure_ = failure; }

  // DeviceManagementBackend method overrides:
  virtual void ProcessRegisterRequest(
      const std::string& auth_token,
      const std::string& device_id,
      const em::DeviceRegisterRequest& request,
      DeviceRegisterResponseDelegate* delegate);

  virtual void ProcessUnregisterRequest(
      const std::string& device_management_token,
      const em::DeviceUnregisterRequest& request,
      DeviceUnregisterResponseDelegate* delegate);

  virtual void ProcessPolicyRequest(
      const std::string& device_management_token,
      const em::DevicePolicyRequest& request,
      DevicePolicyResponseDelegate* delegate);

 private:
  bool failure_;
  DISALLOW_COPY_AND_ASSIGN(MockDeviceManagementBackend);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_MOCK_DEVICE_MANAGEMENT_BACKEND_H_
