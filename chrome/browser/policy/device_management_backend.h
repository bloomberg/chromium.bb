// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_DEVICE_MANAGEMENT_BACKEND_H_
#define CHROME_BROWSER_POLICY_DEVICE_MANAGEMENT_BACKEND_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/threading/non_thread_safe.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"

namespace policy {

namespace em = enterprise_management;

// Interface for clients that need to converse with the device management
// server, which provides services to register Chrome installations and CrOS
// devices for the purpose of fetching centrally-administered policy from the
// cloud.
class DeviceManagementBackend : base::NonThreadSafe {
 public:
  enum ErrorCode {
    // Request payload invalid.
    kErrorRequestInvalid,
    // The HTTP request failed.
    kErrorRequestFailed,
    // The HTTP request returned a non-success code.
    kErrorHttpStatus,
    // Response could not be decoded.
    kErrorResponseDecoding,
    // Service error: Management not supported.
    kErrorServiceManagementNotSupported,
    // Service error: Device not found.
    kErrorServiceDeviceNotFound,
    // Service error: Device token invalid.
    kErrorServiceManagementTokenInvalid,
    // Service error: Activation pending.
    kErrorServiceActivationPending,
    // Service error: Policy not found.
    kErrorServicePolicyNotFound,
  };

  class DeviceRegisterResponseDelegate {
   public:
    virtual ~DeviceRegisterResponseDelegate() {}
    virtual void HandleRegisterResponse(
        const em::DeviceRegisterResponse& response) = 0;
    virtual void OnError(ErrorCode code) = 0;

   protected:
    DeviceRegisterResponseDelegate() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(DeviceRegisterResponseDelegate);
  };

  class DeviceUnregisterResponseDelegate {
   public:
    virtual ~DeviceUnregisterResponseDelegate() {}
    virtual void HandleUnregisterResponse(
        const em::DeviceUnregisterResponse& response) = 0;
    virtual void OnError(ErrorCode code) = 0;

   protected:
    DeviceUnregisterResponseDelegate() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(DeviceUnregisterResponseDelegate);
  };

  class DevicePolicyResponseDelegate {
   public:
    virtual ~DevicePolicyResponseDelegate() {}

    virtual void HandlePolicyResponse(
        const em::DevicePolicyResponse& response) = 0;
    virtual void OnError(ErrorCode code) = 0;

   protected:
    DevicePolicyResponseDelegate() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(DevicePolicyResponseDelegate);
  };

  virtual ~DeviceManagementBackend() {}

  virtual void ProcessRegisterRequest(
      const std::string& auth_token,
      const std::string& device_id,
      const em::DeviceRegisterRequest& request,
      DeviceRegisterResponseDelegate* delegate) = 0;

  virtual void ProcessUnregisterRequest(
      const std::string& device_management_token,
      const std::string& device_id,
      const em::DeviceUnregisterRequest& request,
      DeviceUnregisterResponseDelegate* delegate) = 0;

  virtual void ProcessPolicyRequest(
      const std::string& device_management_token,
      const std::string& device_id,
      const em::DevicePolicyRequest& request,
      DevicePolicyResponseDelegate* delegate) = 0;

 protected:
  DeviceManagementBackend() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceManagementBackend);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_DEVICE_MANAGEMENT_BACKEND_H_
