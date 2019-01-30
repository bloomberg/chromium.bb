// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_ENROLLMENT_SCHEDULER_IMPL_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_ENROLLMENT_SCHEDULER_IMPL_H_

#include "chromeos/services/device_sync/cryptauth_enrollment_scheduler.h"

#include <memory>

#include "base/macros.h"
#include "chromeos/services/device_sync/proto/cryptauth_directive.pb.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class Clock;
class OneShotTimer;
}  // namespace base

namespace chromeos {

namespace device_sync {

// Implementation of CryptAuthEnrollmentScheduler.
class CryptAuthEnrollmentSchedulerImpl : public CryptAuthEnrollmentScheduler {
 public:
  class Factory {
   public:
    static Factory* Get();
    static void SetFactoryForTesting(Factory* test_factory);
    virtual std::unique_ptr<CryptAuthEnrollmentScheduler> BuildInstance(
        Delegate* delegate,
        PrefService* pref_service,
        base::Clock* clock,
        std::unique_ptr<base::OneShotTimer> timer);

   private:
    static Factory* test_factory_;
  };

  // Registers the prefs used by this class to the given |registry|.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  ~CryptAuthEnrollmentSchedulerImpl() override;

 private:
  CryptAuthEnrollmentSchedulerImpl(Delegate* delegate,
                                   PrefService* pref_service,
                                   base::Clock* clock,
                                   std::unique_ptr<base::OneShotTimer> timer);

  // CryptAuthEnrollmentScheduler:
  void RequestEnrollmentNow() override;
  void HandleEnrollmentResult(
      const CryptAuthEnrollmentResult& enrollment_result) override;
  base::Optional<base::Time> GetLastSuccessfulEnrollmentTime() const override;
  base::TimeDelta GetRefreshPeriod() const override;
  base::TimeDelta GetTimeToNextEnrollmentRequest() const override;
  bool IsWaitingForEnrollmentResult() const override;
  size_t GetNumConsecutiveFailures() const override;

  // Calculates the time period between the previous enrollment attempt and the
  // next enrollment attempt, taking failures into consideration.
  base::TimeDelta CalculateTimeBetweenEnrollmentRequests() const;

  // Starts a new timer that will fire when an enrollment is ready to be
  // attempted.
  void ScheduleNextEnrollment();

  // Get the ClientDirective's PolicyReference. If one has not been set, returns
  // base::nullopt.
  base::Optional<cryptauthv2::PolicyReference> GetPolicyReference() const;

  PrefService* pref_service_;
  base::Clock* clock_;
  std::unique_ptr<base::OneShotTimer> timer_;
  cryptauthv2::ClientDirective client_directive_;

  DISALLOW_COPY_AND_ASSIGN(CryptAuthEnrollmentSchedulerImpl);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_ENROLLMENT_SCHEDULER_IMPL_H_
