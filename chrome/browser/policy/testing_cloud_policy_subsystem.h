// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_TESTING_CLOUD_POLICY_SUBSYSTEM_H_
#define CHROME_BROWSER_POLICY_TESTING_CLOUD_POLICY_SUBSYSTEM_H_
#pragma once

#include "chrome/browser/policy/cloud_policy_subsystem.h"

namespace policy {

class CloudPolicyCacheBase;
class CloudPolicyDataStore;
class EventLogger;

// A CloudPolicySubsystem for testing: it uses EventLogger to issue delayed
// tasks with 0 effective delay, and log the times of their executions.
class TestingCloudPolicySubsystem : public CloudPolicySubsystem {
 public:
  // Takes ownership of |policy_cache|.
  TestingCloudPolicySubsystem(CloudPolicyDataStore* data,
                              CloudPolicyCacheBase* policy_cache,
                              const std::string& device_management_url,
                              EventLogger* logger);

 private:
  virtual void CreateDeviceTokenFetcher() OVERRIDE;
  virtual void CreateCloudPolicyController() OVERRIDE;

  EventLogger* logger_;

  DISALLOW_COPY_AND_ASSIGN(TestingCloudPolicySubsystem);
};

}  // namespace Policy

#endif  // CHROME_BROWSER_POLICY_TESTING_CLOUD_POLICY_SUBSYSTEM_H_
