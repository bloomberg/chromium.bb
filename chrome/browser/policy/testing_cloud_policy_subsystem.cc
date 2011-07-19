// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/testing_cloud_policy_subsystem.h"

#include "chrome/browser/policy/cloud_policy_controller.h"
#include "chrome/browser/policy/device_token_fetcher.h"
#include "chrome/browser/policy/logging_work_scheduler.h"

namespace policy {

TestingCloudPolicySubsystem::TestingCloudPolicySubsystem(
    CloudPolicyDataStore* data_store,
    CloudPolicyCacheBase* policy_cache,
    const std::string& device_management_url,
    EventLogger* logger)
        : CloudPolicySubsystem(),
          logger_(logger) {
  Initialize(data_store, policy_cache, device_management_url);
}

void TestingCloudPolicySubsystem::CreateDeviceTokenFetcher() {
  device_token_fetcher_.reset(
      new DeviceTokenFetcher(device_management_service_.get(),
                             cloud_policy_cache_.get(),
                             data_store_,
                             notifier_.get(),
                             new LoggingWorkScheduler(logger_)));
}

void TestingCloudPolicySubsystem::CreateCloudPolicyController() {
  cloud_policy_controller_.reset(
      new CloudPolicyController(device_management_service_.get(),
                                cloud_policy_cache_.get(),
                                device_token_fetcher_.get(),
                                data_store_,
                                notifier_.get(),
                                new LoggingWorkScheduler(logger_)));
}

}  // namespace policy
