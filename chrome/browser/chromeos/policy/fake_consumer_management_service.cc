// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/fake_consumer_management_service.h"

namespace policy {

FakeConsumerManagementService::FakeConsumerManagementService()
    : ConsumerManagementService(NULL, NULL),
      status_(STATUS_UNKNOWN),
      stage_(ConsumerManagementStage::None()) {
}

FakeConsumerManagementService::~FakeConsumerManagementService() {
}

void FakeConsumerManagementService::SetStatusAndStage(
    Status status, const ConsumerManagementStage& stage) {
  status_ = status;
  SetStage(stage);
}

ConsumerManagementService::Status
FakeConsumerManagementService::GetStatus() const {
  return status_;
}

ConsumerManagementStage FakeConsumerManagementService::GetStage() const {
  return stage_;
}

void FakeConsumerManagementService::SetStage(
    const ConsumerManagementStage& stage) {
  stage_ = stage;
  NotifyStatusChanged();
}

}  // namespace policy
