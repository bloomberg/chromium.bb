// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_MOCK_DEVICE_MANAGEMENT_SERVICE_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_MOCK_DEVICE_MANAGEMENT_SERVICE_H_

#include <string>

#include "base/basictypes.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "policy/proto/device_management_backend.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace policy {

class MockDeviceManagementJob {
 public:
  virtual ~MockDeviceManagementJob();
  virtual void RetryJob() = 0;
  virtual void SendResponse(
      DeviceManagementStatus status,
      const enterprise_management::DeviceManagementResponse& response) = 0;
};

class MockDeviceManagementServiceConfiguration
    : public DeviceManagementService::Configuration {
 public:
  MockDeviceManagementServiceConfiguration();
  explicit MockDeviceManagementServiceConfiguration(
      const std::string& server_url);
  virtual ~MockDeviceManagementServiceConfiguration();

  virtual std::string GetServerUrl() OVERRIDE;
  virtual std::string GetAgentParameter() OVERRIDE;
  virtual std::string GetPlatformParameter() OVERRIDE;

 private:
  const std::string server_url_;

  DISALLOW_COPY_AND_ASSIGN(MockDeviceManagementServiceConfiguration);
};

class MockDeviceManagementService : public DeviceManagementService {
 public:
  MockDeviceManagementService();
  virtual ~MockDeviceManagementService();

  typedef DeviceManagementRequestJob* CreateJobFunction(
      DeviceManagementRequestJob::JobType, net::URLRequestContextGetter*);

  MOCK_METHOD2(CreateJob, CreateJobFunction);
  MOCK_METHOD7(
      StartJob,
      void(const std::string& request_type,
           const std::string& gaia_token,
           const std::string& oauth_token,
           const std::string& dm_token,
           const std::string& user_affiliation,
           const std::string& client_id,
           const enterprise_management::DeviceManagementRequest& request));

  // Creates a gmock action that will make the job succeed.
  testing::Action<CreateJobFunction> SucceedJob(
      const enterprise_management::DeviceManagementResponse& response);

  // Creates a gmock action which will fail the job with the given error.
  testing::Action<CreateJobFunction> FailJob(DeviceManagementStatus status);

  // Creates a gmock action which will capture the job so the test code can
  // delay job completion.
  testing::Action<CreateJobFunction> CreateAsyncJob(
      MockDeviceManagementJob** job);

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDeviceManagementService);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_MOCK_DEVICE_MANAGEMENT_SERVICE_H_
