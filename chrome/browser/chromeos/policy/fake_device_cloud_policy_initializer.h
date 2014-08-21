// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_FAKE_DEVICE_CLOUD_POLICY_INITIALIZER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_FAKE_DEVICE_CLOUD_POLICY_INITIALIZER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_initializer.h"
#include "chrome/browser/chromeos/policy/enrollment_status_chromeos.h"
#include "policy/proto/device_management_backend.pb.h"

namespace policy {

class DeviceManagementService;

class FakeDeviceCloudPolicyInitializer : public DeviceCloudPolicyInitializer {
 public:
  FakeDeviceCloudPolicyInitializer();

  virtual void Init() OVERRIDE;
  virtual void Shutdown() OVERRIDE;

  virtual void StartEnrollment(
      enterprise_management::PolicyData::ManagementMode management_mode,
      DeviceManagementService* device_management_service,
      const std::string& auth_token,
      bool is_auto_enrollment,
      const AllowedDeviceModes& allowed_modes,
      const EnrollmentCallback& enrollment_callback) OVERRIDE;

  bool was_start_enrollment_called() {
    return was_start_enrollment_called_;
  }

  void set_enrollment_status(EnrollmentStatus status) {
    enrollment_status_ = status;
  }

 private:
  bool was_start_enrollment_called_;
  EnrollmentStatus enrollment_status_;

  DISALLOW_COPY_AND_ASSIGN(FakeDeviceCloudPolicyInitializer);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_FAKE_DEVICE_CLOUD_POLICY_INITIALIZER_H_
