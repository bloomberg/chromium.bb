// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/fake_cryptauth_enrollment_scheduler.h"

#include "base/logging.h"

namespace chromeos {

namespace device_sync {

constexpr base::TimeDelta
    FakeCryptAuthEnrollmentScheduler::kDefaultRefreshPeriod;
constexpr base::TimeDelta
    FakeCryptAuthEnrollmentScheduler::kDefaultTimeToNextEnrollmentRequest;

FakeCryptAuthEnrollmentScheduler::FakeCryptAuthEnrollmentScheduler(
    Delegate* delegate)
    : CryptAuthEnrollmentScheduler(delegate) {}

FakeCryptAuthEnrollmentScheduler::~FakeCryptAuthEnrollmentScheduler() = default;

void FakeCryptAuthEnrollmentScheduler::RequestEnrollmentNow() {
  is_waiting_for_enrollment_result_ = true;
  NotifyEnrollmentRequested(client_directive_policy_reference_);
}

void FakeCryptAuthEnrollmentScheduler::HandleEnrollmentResult(
    const CryptAuthEnrollmentResult& enrollment_result) {
  DCHECK(is_waiting_for_enrollment_result_);
  handled_enrollment_results_.push_back(enrollment_result);
  is_waiting_for_enrollment_result_ = false;
}

base::Optional<base::Time>
FakeCryptAuthEnrollmentScheduler::GetLastSuccessfulEnrollmentTime() const {
  return last_successful_enrollment_time_;
}

base::TimeDelta FakeCryptAuthEnrollmentScheduler::GetRefreshPeriod() const {
  return refresh_period_;
}

base::TimeDelta
FakeCryptAuthEnrollmentScheduler::GetTimeToNextEnrollmentRequest() const {
  return time_to_next_enrollment_request_;
}

bool FakeCryptAuthEnrollmentScheduler::IsWaitingForEnrollmentResult() const {
  return is_waiting_for_enrollment_result_;
}

size_t FakeCryptAuthEnrollmentScheduler::GetNumConsecutiveFailures() const {
  return num_consecutive_failures_;
}

FakeCryptAuthEnrollmentSchedulerDelegate::
    FakeCryptAuthEnrollmentSchedulerDelegate() = default;

FakeCryptAuthEnrollmentSchedulerDelegate::
    ~FakeCryptAuthEnrollmentSchedulerDelegate() = default;

void FakeCryptAuthEnrollmentSchedulerDelegate::OnEnrollmentRequested(
    const base::Optional<cryptauthv2::PolicyReference>&
        client_directive_policy_reference) {
  policy_references_from_enrollment_requests_.push_back(
      client_directive_policy_reference);
}

}  // namespace device_sync

}  // namespace chromeos
