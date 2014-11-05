// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/enrollment_status_chromeos.h"

#include "net/http/http_status_code.h"

namespace policy {

// static
EnrollmentStatus EnrollmentStatus::ForStatus(Status status) {
  return EnrollmentStatus(status, DM_STATUS_SUCCESS, net::HTTP_OK,
                          CloudPolicyStore::STATUS_OK,
                          CloudPolicyValidatorBase::VALIDATION_OK,
                          EnterpriseInstallAttributes::LOCK_SUCCESS);
}

// static
EnrollmentStatus EnrollmentStatus::ForRegistrationError(
    DeviceManagementStatus client_status) {
  return EnrollmentStatus(STATUS_REGISTRATION_FAILED, client_status,
                          net::HTTP_OK, CloudPolicyStore::STATUS_OK,
                          CloudPolicyValidatorBase::VALIDATION_OK,
                          EnterpriseInstallAttributes::LOCK_SUCCESS);
}

// static
EnrollmentStatus EnrollmentStatus::ForRobotAuthFetchError(
    DeviceManagementStatus client_status) {
  return EnrollmentStatus(STATUS_ROBOT_AUTH_FETCH_FAILED, client_status,
                          net::HTTP_OK, CloudPolicyStore::STATUS_OK,
                          CloudPolicyValidatorBase::VALIDATION_OK,
                          EnterpriseInstallAttributes::LOCK_SUCCESS);
}

// static
EnrollmentStatus EnrollmentStatus::ForRobotRefreshFetchError(int http_status) {
  return EnrollmentStatus(STATUS_ROBOT_REFRESH_FETCH_FAILED, DM_STATUS_SUCCESS,
                          http_status, CloudPolicyStore::STATUS_OK,
                          CloudPolicyValidatorBase::VALIDATION_OK,
                          EnterpriseInstallAttributes::LOCK_SUCCESS);
}

// static
EnrollmentStatus EnrollmentStatus::ForFetchError(
    DeviceManagementStatus client_status) {
  return EnrollmentStatus(STATUS_POLICY_FETCH_FAILED, client_status,
                          net::HTTP_OK, CloudPolicyStore::STATUS_OK,
                          CloudPolicyValidatorBase::VALIDATION_OK,
                          EnterpriseInstallAttributes::LOCK_SUCCESS);
}

// static
EnrollmentStatus EnrollmentStatus::ForValidationError(
    CloudPolicyValidatorBase::Status validation_status) {
  return EnrollmentStatus(STATUS_VALIDATION_FAILED, DM_STATUS_SUCCESS,
                          net::HTTP_OK, CloudPolicyStore::STATUS_OK,
                          validation_status,
                          EnterpriseInstallAttributes::LOCK_SUCCESS);
}

// static
EnrollmentStatus EnrollmentStatus::ForStoreError(
    CloudPolicyStore::Status store_error,
    CloudPolicyValidatorBase::Status validation_status) {
  return EnrollmentStatus(STATUS_STORE_ERROR, DM_STATUS_SUCCESS,
                          net::HTTP_OK, store_error, validation_status,
                          EnterpriseInstallAttributes::LOCK_SUCCESS);
}

// static
EnrollmentStatus EnrollmentStatus::ForLockError(
    EnterpriseInstallAttributes::LockResult lock_status) {
  return EnrollmentStatus(STATUS_LOCK_ERROR, DM_STATUS_SUCCESS,
                          net::HTTP_OK, CloudPolicyStore::STATUS_OK,
                          CloudPolicyValidatorBase::VALIDATION_OK,
                          lock_status);
}

EnrollmentStatus::EnrollmentStatus(
    EnrollmentStatus::Status status,
    DeviceManagementStatus client_status,
    int http_status,
    CloudPolicyStore::Status store_status,
    CloudPolicyValidatorBase::Status validation_status,
    EnterpriseInstallAttributes::LockResult lock_status)
    : status_(status),
      client_status_(client_status),
      http_status_(http_status),
      store_status_(store_status),
      validation_status_(validation_status),
      lock_status_(lock_status) {}

}  // namespace policy
