// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/fake_device_cloud_policy_initializer.h"

#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "chromeos/attestation/mock_attestation_flow.h"
#include "components/policy/core/common/cloud/device_management_service.h"

namespace policy {

FakeDeviceCloudPolicyInitializer::FakeDeviceCloudPolicyInitializer()
    : DeviceCloudPolicyInitializer(
          nullptr,  // local_state
          nullptr,  // enterprise_service
          // background_task_runner
          scoped_refptr<base::SequencedTaskRunner>(nullptr),
          nullptr,  // install_attributes
          nullptr,  // state_keys_broker
          nullptr,  // device_store
          nullptr,  // manager
          nullptr,  // async_caller
          base::MakeUnique<chromeos::attestation::MockAttestationFlow>(),
          nullptr),  // statistics_provider
      was_start_enrollment_called_(false),
      enrollment_status_(
          EnrollmentStatus::ForStatus(EnrollmentStatus::SUCCESS)) {}

void FakeDeviceCloudPolicyInitializer::Init() {
}

void FakeDeviceCloudPolicyInitializer::Shutdown() {
}

void FakeDeviceCloudPolicyInitializer::StartEnrollment(
    DeviceManagementService* device_management_service,
    const EnrollmentConfig& enrollment_config,
    const std::string& auth_token,
    const EnrollmentCallback& enrollment_callback) {
  was_start_enrollment_called_ = true;
  enrollment_callback.Run(enrollment_status_);
}

}  // namespace policy
