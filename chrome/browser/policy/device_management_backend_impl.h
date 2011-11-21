// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_DEVICE_MANAGEMENT_BACKEND_IMPL_H_
#define CHROME_BROWSER_POLICY_DEVICE_MANAGEMENT_BACKEND_IMPL_H_
#pragma once

#include <set>
#include <string>

#include "base/basictypes.h"
#include "chrome/browser/policy/device_management_backend.h"

namespace policy {

class DeviceManagementJobBase;
class DeviceManagementService;

// Implements the actual backend interface. It creates device management jobs
// and passes them on to the service for processing.
class DeviceManagementBackendImpl : public DeviceManagementBackend {
 public:
  explicit DeviceManagementBackendImpl(DeviceManagementService* service);
  virtual ~DeviceManagementBackendImpl();

  static std::string GetAgentString();
  static std::string GetPlatformString();

  // Name constants for URL query parameters.
  static const char kParamAgent[];
  static const char kParamAppType[];
  static const char kParamDeviceID[];
  static const char kParamDeviceType[];
  static const char kParamOAuthToken[];
  static const char kParamPlatform[];
  static const char kParamRequest[];
  static const char kParamUserAffiliation[];

  // String constants for the device and app type we report to the server.
  static const char kValueAppType[];
  static const char kValueDeviceType[];
  static const char kValueRequestPolicy[];
  static const char kValueRequestRegister[];
  static const char kValueRequestUnregister[];
  static const char kValueUserAffiliationManaged[];
  static const char kValueUserAffiliationNone[];

 private:
  friend class DeviceManagementJobBase;

  typedef std::set<DeviceManagementJobBase*> JobSet;

  // Called by the DeviceManagementJobBase dtor so we can clean up.
  void JobDone(DeviceManagementJobBase* job);

  // Add a job to the pending job set and register it with the service (if
  // available).
  void AddJob(DeviceManagementJobBase* job);

  // DeviceManagementBackend overrides.
  virtual void ProcessRegisterRequest(
      const std::string& gaia_auth_token,
      const std::string& oauth_token,
      const std::string& device_id,
      const em::DeviceRegisterRequest& request,
      DeviceRegisterResponseDelegate* response_delegate) OVERRIDE;
  virtual void ProcessUnregisterRequest(
      const std::string& device_management_token,
      const std::string& device_id,
      const em::DeviceUnregisterRequest& request,
      DeviceUnregisterResponseDelegate* response_delegate) OVERRIDE;
  virtual void ProcessPolicyRequest(
      const std::string& device_management_token,
      const std::string& device_id,
      CloudPolicyDataStore::UserAffiliation affiliation,
      const em::DevicePolicyRequest& request,
      DevicePolicyResponseDelegate* response_delegate) OVERRIDE;

  // Converts a user affiliation to the appropriate query parameter value.
  static const char* UserAffiliationToString(
      CloudPolicyDataStore::UserAffiliation affiliation);

  // Keeps track of the jobs currently in flight.
  JobSet pending_jobs_;

  DeviceManagementService* service_;

  DISALLOW_COPY_AND_ASSIGN(DeviceManagementBackendImpl);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_DEVICE_MANAGEMENT_BACKEND_IMPL_H_
