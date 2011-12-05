// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_DEVICE_MANAGEMENT_BACKEND_MOCK_H_
#define CHROME_BROWSER_POLICY_DEVICE_MANAGEMENT_BACKEND_MOCK_H_
#pragma once

#include "chrome/browser/policy/device_management_backend.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace policy {

// Mock classes for the various DeviceManagementBackend delegates that allow to
// capture callbacks using gmock.
class DeviceRegisterResponseDelegateMock
    : public DeviceManagementBackend::DeviceRegisterResponseDelegate {
 public:
  DeviceRegisterResponseDelegateMock();
  virtual ~DeviceRegisterResponseDelegateMock();

  MOCK_METHOD1(HandleRegisterResponse, void(const em::DeviceRegisterResponse&));
  MOCK_METHOD1(OnError, void(DeviceManagementBackend::ErrorCode error));
};

class DeviceUnregisterResponseDelegateMock
    : public DeviceManagementBackend::DeviceUnregisterResponseDelegate {
 public:
  DeviceUnregisterResponseDelegateMock();
  virtual ~DeviceUnregisterResponseDelegateMock();

  MOCK_METHOD1(HandleUnregisterResponse,
               void(const em::DeviceUnregisterResponse&));
  MOCK_METHOD1(OnError, void(DeviceManagementBackend::ErrorCode error));
};

class DevicePolicyResponseDelegateMock
    : public DeviceManagementBackend::DevicePolicyResponseDelegate {
 public:
  DevicePolicyResponseDelegateMock();
  virtual ~DevicePolicyResponseDelegateMock();

  MOCK_METHOD1(HandlePolicyResponse, void(const em::DevicePolicyResponse&));
  MOCK_METHOD1(OnError, void(DeviceManagementBackend::ErrorCode error));
};

class DeviceAutoEnrollmentResponseDelegateMock
    : public DeviceManagementBackend::DeviceAutoEnrollmentResponseDelegate {
 public:
  DeviceAutoEnrollmentResponseDelegateMock();
  virtual ~DeviceAutoEnrollmentResponseDelegateMock();

  MOCK_METHOD1(HandleAutoEnrollmentResponse,
               void(const em::DeviceAutoEnrollmentResponse&));
  MOCK_METHOD1(OnError, void(DeviceManagementBackend::ErrorCode error));
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_DEVICE_MANAGEMENT_BACKEND_MOCK_H_
