// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_SCHEDULER_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_SCHEDULER_H_

#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "chromeos/services/device_sync/cryptauth_enrollment_result.h"
#include "chromeos/services/device_sync/proto/cryptauth_common.pb.h"

namespace chromeos {

namespace device_sync {

// Schedules periodic enrollments, alerting the delegate when an enrollment
// attempt is due via Delegate::OnEnrollmentRequested(). The periodic scheduling
// begins on StartEnrollmentScheduling(). The client can bypass the periodic
// schedule and trigger an enrollment request via RequestEnrollment(). When an
// enrollment attempt has completed, successfully or not, the client should
// invoke HandleEnrollmentResult() so the scheduler can process the enrollment
// attempt outcome.
class CryptAuthScheduler {
 public:
  class EnrollmentDelegate {
   public:
    EnrollmentDelegate() = default;
    virtual ~EnrollmentDelegate() = default;

    // Called to alert the delegate that an enrollment attempt has been
    // requested by the scheduler.
    //   |client_metadata|: Contains the retry count, invocation reason, and
    //       possible session ID of the enrollment request.
    //   |client_directive_policy_reference|: Identifies the CryptAuth policy
    //       associated with the ClientDirective parameters used to schedule
    //       this enrollment attempt. If no ClientDirective was used by the
    //       scheduler, base::nullopt is passed.
    virtual void OnEnrollmentRequested(
        const cryptauthv2::ClientMetadata& client_metadata,
        const base::Optional<cryptauthv2::PolicyReference>&
            client_directive_policy_reference) = 0;
  };

  virtual ~CryptAuthScheduler();

  // Note: This should only be called once.
  void StartEnrollmentScheduling(
      const base::WeakPtr<EnrollmentDelegate>& enrollment_delegate);

  bool HasEnrollmentSchedulingStarted();

  // Requests an enrollment with the desired |invocation_reason| and, if
  // relevant, the |session_id| of the GCM message that requested enrollment.
  virtual void RequestEnrollment(
      const cryptauthv2::ClientMetadata::InvocationReason& invocation_reason,
      const base::Optional<std::string>& session_id) = 0;

  // Processes the result of the previous enrollment attempt.
  virtual void HandleEnrollmentResult(
      const CryptAuthEnrollmentResult& enrollment_result) = 0;

  // Returns the time of the last known successful enrollment. If no successful
  // enrollment has occurred, base::nullopt is returned.
  virtual base::Optional<base::Time> GetLastSuccessfulEnrollmentTime()
      const = 0;

  // Returns the scheduler's time period between a successful enrollment and its
  // next enrollment request. Note that this period may not be strictly adhered
  // to due to RequestEnrollment() calls or the system being offline, for
  // instance.
  virtual base::TimeDelta GetRefreshPeriod() const = 0;

  // Returns the time until the next scheduled enrollment request.
  virtual base::TimeDelta GetTimeToNextEnrollmentRequest() const = 0;

  // Return true if an enrollment has been requested but the enrollment result
  // has not been processed yet.
  virtual bool IsWaitingForEnrollmentResult() const = 0;

  // The number of times the current enrollment request has failed. Once the
  // enrollment request succeeds or a fresh request is made--for example, via a
  // forced enrollment--this counter is reset.
  virtual size_t GetNumConsecutiveEnrollmentFailures() const = 0;

 protected:
  CryptAuthScheduler();

  virtual void OnEnrollmentSchedulingStarted();

  // Alerts the delegate that an enrollment has been requested.
  void NotifyEnrollmentRequested(
      const cryptauthv2::ClientMetadata& client_metadata,
      const base::Optional<cryptauthv2::PolicyReference>&
          client_directive_policy_reference) const;

 private:
  base::WeakPtr<EnrollmentDelegate> enrollment_delegate_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(CryptAuthScheduler);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_SCHEDULER_H_
