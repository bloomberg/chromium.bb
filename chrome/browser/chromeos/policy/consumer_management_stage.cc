// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/consumer_management_stage.h"

#include "base/logging.h"

namespace policy {

// static
ConsumerManagementStage ConsumerManagementStage::None() {
  return ConsumerManagementStage(NONE);
}

// static
ConsumerManagementStage ConsumerManagementStage::EnrollmentRequested() {
  return ConsumerManagementStage(ENROLLMENT_REQUESTED);
}

// static
ConsumerManagementStage ConsumerManagementStage::EnrollmentOwnerStored() {
  return ConsumerManagementStage(ENROLLMENT_OWNER_STORED);
}

// static
ConsumerManagementStage ConsumerManagementStage::EnrollmentSuccess() {
  return ConsumerManagementStage(ENROLLMENT_SUCCESS);
}

// static
ConsumerManagementStage ConsumerManagementStage::EnrollmentCanceled() {
  return ConsumerManagementStage(ENROLLMENT_CANCELED);
}

// static
ConsumerManagementStage ConsumerManagementStage::EnrollmentBootLockboxFailed() {
  return ConsumerManagementStage(ENROLLMENT_BOOT_LOCKBOX_FAILED);
}

// static
ConsumerManagementStage ConsumerManagementStage::EnrollmentGetTokenFailed() {
  return ConsumerManagementStage(ENROLLMENT_GET_TOKEN_FAILED);
}

// static
ConsumerManagementStage ConsumerManagementStage::EnrollmentDMServerFailed() {
  return ConsumerManagementStage(ENROLLMENT_DM_SERVER_FAILED);
}

// static
ConsumerManagementStage ConsumerManagementStage::UnenrollmentRequested() {
  return ConsumerManagementStage(UNENROLLMENT_REQUESTED);
}

// static
ConsumerManagementStage ConsumerManagementStage::UnenrollmentSuccess() {
  return ConsumerManagementStage(UNENROLLMENT_SUCCESS);
}

// static
ConsumerManagementStage ConsumerManagementStage::UnenrollmentDMServerFailed() {
  return ConsumerManagementStage(UNENROLLMENT_DM_SERVER_FAILED);
}

// static
ConsumerManagementStage
ConsumerManagementStage::UnenrollmentUpdateDeviceSettingsFailed() {
  return ConsumerManagementStage(UNENROLLMENT_UPDATE_DEVICE_SETTINGS_FAILED);
}

bool ConsumerManagementStage::operator==(
    const ConsumerManagementStage& other) const {
  return value_ == other.value_;
}

bool ConsumerManagementStage::HasPendingNotification() const {
  return HasEnrollmentSucceeded() ||
      HasEnrollmentFailed() ||
      HasUnenrollmentSucceeded() ||
      HasUnenrollmentFailed();
}

bool ConsumerManagementStage::IsEnrolling() const {
  return value_ == ENROLLMENT_REQUESTED || value_ == ENROLLMENT_OWNER_STORED;
}

bool ConsumerManagementStage::IsEnrollmentRequested() const {
  return value_ == ENROLLMENT_REQUESTED;
}

bool ConsumerManagementStage::HasEnrollmentSucceeded() const {
  return value_ == ENROLLMENT_SUCCESS;
}

bool ConsumerManagementStage::HasEnrollmentFailed() const {
  return value_ == ENROLLMENT_CANCELED ||
      value_ == ENROLLMENT_BOOT_LOCKBOX_FAILED ||
      value_ == ENROLLMENT_GET_TOKEN_FAILED ||
      value_ == ENROLLMENT_DM_SERVER_FAILED;
}

bool ConsumerManagementStage::IsUnenrolling() const {
  return value_ == UNENROLLMENT_REQUESTED;
}

bool ConsumerManagementStage::HasUnenrollmentSucceeded() const {
  return value_ == UNENROLLMENT_SUCCESS;
}

bool ConsumerManagementStage::HasUnenrollmentFailed() const {
  return value_ == UNENROLLMENT_DM_SERVER_FAILED ||
      value_ == UNENROLLMENT_UPDATE_DEVICE_SETTINGS_FAILED;
}

// static
ConsumerManagementStage ConsumerManagementStage::FromInternalValue(
    int int_value) {
  ConsumerManagementStage::Value value = ConsumerManagementStage::NONE;
  if (int_value >= ConsumerManagementStage::NONE &&
      int_value < ConsumerManagementStage::LAST) {
    value = static_cast<ConsumerManagementStage::Value>(int_value);
  } else {
    LOG(ERROR) << "Unknown stage: " << int_value;
  }

  return ConsumerManagementStage(value);
}

int ConsumerManagementStage::ToInternalValue() const {
  return static_cast<int>(value_);
}

ConsumerManagementStage::ConsumerManagementStage(Value value) : value_(value) {
}

}  // namespace policy
