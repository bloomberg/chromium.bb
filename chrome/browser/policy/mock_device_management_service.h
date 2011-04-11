// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MOCK_DEVICE_MANAGEMENT_SERVICE_H_
#define CHROME_BROWSER_POLICY_MOCK_DEVICE_MANAGEMENT_SERVICE_H_
#pragma once

#include <string>

#include "chrome/browser/policy/device_management_backend.h"
#include "chrome/browser/policy/device_management_service.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"

namespace policy {

// This proxy class is used so that expectations can be defined for a single
// persistent instance of DMBackend while the DeviceTokenFetcher under test
// merrily creates and destroys proxies.
class ProxyDeviceManagementBackend : public DeviceManagementBackend {
 public:
  explicit ProxyDeviceManagementBackend(DeviceManagementBackend* backend);
  virtual ~ProxyDeviceManagementBackend();

  virtual void ProcessRegisterRequest(
      const std::string& auth_token,
      const std::string& device_id,
      const em::DeviceRegisterRequest& request,
      DeviceRegisterResponseDelegate* delegate);

  virtual void ProcessUnregisterRequest(
      const std::string& device_management_token,
      const std::string& device_id,
      const em::DeviceUnregisterRequest& request,
      DeviceUnregisterResponseDelegate* delegate);

  virtual void ProcessPolicyRequest(
      const std::string& device_management_token,
      const std::string& device_id,
      const em::DevicePolicyRequest& request,
      DevicePolicyResponseDelegate* delegate);

 private:
  DeviceManagementBackend* backend_;  // weak
  DISALLOW_COPY_AND_ASSIGN(ProxyDeviceManagementBackend);
};

class MockDeviceManagementService : public DeviceManagementService {
 public:
  MockDeviceManagementService();
  virtual ~MockDeviceManagementService();

  void set_backend(DeviceManagementBackend* backend) {
    backend_ = backend;
  }

  virtual DeviceManagementBackend* CreateBackend();

 private:
  DeviceManagementBackend* backend_;  // weak
  DISALLOW_COPY_AND_ASSIGN(MockDeviceManagementService);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_MOCK_DEVICE_MANAGEMENT_SERVICE_H_
