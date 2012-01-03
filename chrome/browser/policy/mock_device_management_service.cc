// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/mock_device_management_service.h"

namespace em = enterprise_management;

namespace policy {

class MockDeviceManagementRequestJob : public DeviceManagementRequestJob {
 public:
  MockDeviceManagementRequestJob(
      JobType type,
      MockDeviceManagementService* service,
      DeviceManagementStatus status,
      const enterprise_management::DeviceManagementResponse& response)
      : DeviceManagementRequestJob(type),
        service_(service),
        status_(status),
        response_(response) {}

 protected:
  virtual void Run() OVERRIDE {
    service_->StartJob(this);
    callback_.Run(status_, response_);
  }

 private:
  MockDeviceManagementService* service_;
  DeviceManagementStatus status_;
  enterprise_management::DeviceManagementResponse response_;

  DISALLOW_COPY_AND_ASSIGN(MockDeviceManagementRequestJob);
};

ACTION_P3(CreateMockDeviceManagementRequestJob, service, status, response) {
  return new MockDeviceManagementRequestJob(arg0, service, status, response);
}

MockDeviceManagementService::MockDeviceManagementService()
    : DeviceManagementService("") {}

MockDeviceManagementService::~MockDeviceManagementService() {}

testing::Action<MockDeviceManagementService::CreateJobFunction>
    MockDeviceManagementService::SucceedJob(
        const enterprise_management::DeviceManagementResponse& response) {
  return CreateMockDeviceManagementRequestJob(this, DM_STATUS_SUCCESS,
                                              response);
}

testing::Action<MockDeviceManagementService::CreateJobFunction>
    MockDeviceManagementService::FailJob(DeviceManagementStatus status) {
  const enterprise_management::DeviceManagementResponse dummy_response;
  return CreateMockDeviceManagementRequestJob(this, status, dummy_response);
}

}  // namespace policy
