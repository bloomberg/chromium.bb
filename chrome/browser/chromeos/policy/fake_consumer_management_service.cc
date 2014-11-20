// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/fake_consumer_management_service.h"

namespace policy {

FakeConsumerManagementService::FakeConsumerManagementService()
    : ConsumerManagementService(NULL, NULL),
      status_(STATUS_UNKNOWN),
      enrollment_stage_(ENROLLMENT_STAGE_NONE) {
}

FakeConsumerManagementService::~FakeConsumerManagementService() {
}

void FakeConsumerManagementService::SetStatusAndEnrollmentStage(
    Status status, EnrollmentStage stage) {
  status_ = status;
  SetEnrollmentStage(stage);
}

ConsumerManagementService::Status
FakeConsumerManagementService::GetStatus() const {
  return status_;
}

ConsumerManagementService::EnrollmentStage
FakeConsumerManagementService::GetEnrollmentStage() const {
  return enrollment_stage_;
}

void FakeConsumerManagementService::SetEnrollmentStage(EnrollmentStage stage) {
  enrollment_stage_ = stage;
  NotifyStatusChanged();
}

}  // namespace policy
