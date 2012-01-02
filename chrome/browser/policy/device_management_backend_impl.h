// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_DEVICE_MANAGEMENT_BACKEND_IMPL_H_
#define CHROME_BROWSER_POLICY_DEVICE_MANAGEMENT_BACKEND_IMPL_H_
#pragma once

#include <set>
#include <string>

#include "base/basictypes.h"
#include "chrome/browser/policy/cloud_policy_constants.h"
#include "chrome/browser/policy/device_management_backend.h"

namespace policy {

class DeviceManagementRequestJob;
class DeviceManagementService;

// Implements the actual backend interface. It creates device management jobs
// and passes them on to the service for processing.
class DeviceManagementBackendImpl : public DeviceManagementBackend {
 public:
  explicit DeviceManagementBackendImpl(DeviceManagementService* service);
  virtual ~DeviceManagementBackendImpl();

 private:
  typedef std::set<DeviceManagementRequestJob*> JobSet;

  // DeviceManagementBackend overrides.
  virtual void ProcessRegisterRequest(
      const std::string& gaia_auth_token,
      const std::string& oauth_token,
      const std::string& device_id,
      const enterprise_management::DeviceRegisterRequest& request,
      DeviceRegisterResponseDelegate* response_delegate) OVERRIDE;
  virtual void ProcessUnregisterRequest(
      const std::string& device_management_token,
      const std::string& device_id,
      const enterprise_management::DeviceUnregisterRequest& request,
      DeviceUnregisterResponseDelegate* response_delegate) OVERRIDE;
  virtual void ProcessPolicyRequest(
      const std::string& device_management_token,
      const std::string& device_id,
      CloudPolicyDataStore::UserAffiliation affiliation,
      const enterprise_management::DevicePolicyRequest& request,
      const enterprise_management::DeviceStatusReportRequest* device_status,
      DevicePolicyResponseDelegate* response_delegate) OVERRIDE;
  virtual void ProcessAutoEnrollmentRequest(
      const std::string& device_id,
      const enterprise_management::DeviceAutoEnrollmentRequest& request,
      DeviceAutoEnrollmentResponseDelegate* delegate) OVERRIDE;

  // Helpers for mapping new-style callbacks to the old delegate style.
  void OnRegistrationDone(
      DeviceManagementRequestJob* job,
      DeviceRegisterResponseDelegate* delegate,
      DeviceManagementStatus status,
      const enterprise_management::DeviceManagementResponse& response);
  void OnUnregistrationDone(
      DeviceManagementRequestJob* job,
      DeviceUnregisterResponseDelegate* delegate,
      DeviceManagementStatus status,
      const enterprise_management::DeviceManagementResponse& response);
  void OnPolicyFetchDone(
      DeviceManagementRequestJob* job,
      DevicePolicyResponseDelegate* delegate,
      DeviceManagementStatus status,
      const enterprise_management::DeviceManagementResponse& response);
  void OnAutoEnrollmentDone(
      DeviceManagementRequestJob* job,
      DeviceAutoEnrollmentResponseDelegate* delegate,
      DeviceManagementStatus status,
      const enterprise_management::DeviceManagementResponse& response);

  // Keeps track of the jobs currently in flight.
  JobSet pending_jobs_;

  DeviceManagementService* service_;

  DISALLOW_COPY_AND_ASSIGN(DeviceManagementBackendImpl);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_DEVICE_MANAGEMENT_BACKEND_IMPL_H_
