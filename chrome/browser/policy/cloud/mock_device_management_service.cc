// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/mock_device_management_service.h"

#include "base/string_util.h"

using testing::Action;

namespace em = enterprise_management;

namespace policy {
namespace {

// Common mock request job functionality.
class MockRequestJobBase : public DeviceManagementRequestJob {
 public:
  MockRequestJobBase(JobType type,
                     MockDeviceManagementService* service)
      : DeviceManagementRequestJob(type),
        service_(service) {}
  virtual ~MockRequestJobBase() {}

 protected:
  virtual void Run() OVERRIDE {
    service_->StartJob(ExtractParameter(dm_protocol::kParamRequest),
                       gaia_token_,
                       ExtractParameter(dm_protocol::kParamOAuthToken),
                       dm_token_,
                       ExtractParameter(dm_protocol::kParamUserAffiliation),
                       ExtractParameter(dm_protocol::kParamDeviceID),
                       request_);
  }

 private:
  // Searches for a query parameter and returns the associated value.
  const std::string& ExtractParameter(const std::string& name) const {
    for (ParameterMap::const_iterator entry(query_params_.begin());
         entry != query_params_.end();
         ++entry) {
      if (name == entry->first)
        return entry->second;
    }

    return EmptyString();
  }

  MockDeviceManagementService* service_;

  DISALLOW_COPY_AND_ASSIGN(MockRequestJobBase);
};

// Synchronous mock request job that immediately completes on calling Run().
class SyncRequestJob : public MockRequestJobBase {
 public:
  SyncRequestJob(JobType type,
                 MockDeviceManagementService* service,
                 DeviceManagementStatus status,
                 const em::DeviceManagementResponse& response)
      : MockRequestJobBase(type, service),
        status_(status),
        response_(response) {}
  virtual ~SyncRequestJob() {}

 protected:
  virtual void Run() OVERRIDE {
    MockRequestJobBase::Run();
    callback_.Run(status_, response_);
  }

 private:
  DeviceManagementStatus status_;
  em::DeviceManagementResponse response_;

  DISALLOW_COPY_AND_ASSIGN(SyncRequestJob);
};

// Asynchronous job that allows the test to delay job completion.
class AsyncRequestJob : public MockRequestJobBase,
                        public MockDeviceManagementJob {
 public:
  AsyncRequestJob(JobType type, MockDeviceManagementService* service)
      : MockRequestJobBase(type, service) {}
  virtual ~AsyncRequestJob() {}

 protected:
  virtual void RetryJob() OVERRIDE {
    if (!retry_callback_.is_null())
      retry_callback_.Run(this);
    Run();
  }

  virtual void SendResponse(
      DeviceManagementStatus status,
      const em::DeviceManagementResponse& response) OVERRIDE {
    callback_.Run(status, response);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AsyncRequestJob);
};

}  // namespace

ACTION_P3(CreateSyncMockDeviceManagementJob, service, status, response) {
  return new SyncRequestJob(arg0, service, status, response);
}

ACTION_P2(CreateAsyncMockDeviceManagementJob, service, mock_job) {
  AsyncRequestJob* job = new AsyncRequestJob(arg0, service);
  *mock_job = job;
  return job;
}

MockDeviceManagementJob::~MockDeviceManagementJob() {}

MockDeviceManagementService::MockDeviceManagementService()
    : DeviceManagementService(std::string()) {}

MockDeviceManagementService::~MockDeviceManagementService() {}

Action<MockDeviceManagementService::CreateJobFunction>
    MockDeviceManagementService::SucceedJob(
        const em::DeviceManagementResponse& response) {
  return CreateSyncMockDeviceManagementJob(this, DM_STATUS_SUCCESS, response);
}

Action<MockDeviceManagementService::CreateJobFunction>
    MockDeviceManagementService::FailJob(DeviceManagementStatus status) {
  const em::DeviceManagementResponse dummy_response;
  return CreateSyncMockDeviceManagementJob(this, status, dummy_response);
}

Action<MockDeviceManagementService::CreateJobFunction>
    MockDeviceManagementService::CreateAsyncJob(MockDeviceManagementJob** job) {
  return CreateAsyncMockDeviceManagementJob(this, job);
}

}  // namespace policy
