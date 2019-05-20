// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_SCHEDULER_IMPL_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_SCHEDULER_IMPL_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/time/default_clock.h"
#include "base/timer/timer.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state_handler_observer.h"
#include "chromeos/services/device_sync/cryptauth_scheduler.h"
#include "chromeos/services/device_sync/proto/cryptauth_common.pb.h"
#include "chromeos/services/device_sync/proto/cryptauth_directive.pb.h"

class PrefRegistrySimple;
class PrefService;

namespace chromeos {

class NetworkStateHandler;

namespace device_sync {

// CryptAuthScheduler implementation which stores scheduling metadata
// persistently so that the enrollment schedule is saved across device reboots.
//
// Enrollment scheduling will not start until StartEnrollmentScheduling() is
// called. Enrollment requests made before scheduling has started, while another
// enrollment request is in process, or while offline will be cached and
// rescheduled as soon as possible.
//
// Enrollment requests can come from four sources:
//   1) If an enrollment has never successfully completed, an initialization
//      request will be made immediately.
//   2) Periodic enrollment requests are made at time intervals provided by the
//      server in the ClientDirective proto.
//   3) Enrollment requests can be made using the scheduler's public function
//      RequestEnrollment().
//   4) Failed enrollment attempts will be retried at time intervals provided by
//      the server in the ClientDirective proto.
class CryptAuthSchedulerImpl : public CryptAuthScheduler,
                               public NetworkStateHandlerObserver {
 public:
  class Factory {
   public:
    static Factory* Get();
    static void SetFactoryForTesting(Factory* test_factory);
    virtual ~Factory();
    virtual std::unique_ptr<CryptAuthScheduler> BuildInstance(
        PrefService* pref_service,
        NetworkStateHandler* network_state_handler =
            NetworkHandler::Get()->network_state_handler(),
        base::Clock* clock = base::DefaultClock::GetInstance(),
        std::unique_ptr<base::OneShotTimer> enrollment_timer =
            std::make_unique<base::OneShotTimer>());

   private:
    static Factory* test_factory_;
  };

  // Registers the prefs used by this class to the given |registry|.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  ~CryptAuthSchedulerImpl() override;

 private:
  CryptAuthSchedulerImpl(PrefService* pref_service,
                         NetworkStateHandler* network_state_handler,
                         base::Clock* clock,
                         std::unique_ptr<base::OneShotTimer> enrollment_timer);

  // CryptAuthScheduler:
  void OnEnrollmentSchedulingStarted() override;
  void RequestEnrollment(
      const cryptauthv2::ClientMetadata::InvocationReason& invocation_reason,
      const base::Optional<std::string>& session_id) override;
  void HandleEnrollmentResult(
      const CryptAuthEnrollmentResult& enrollment_result) override;
  base::Optional<base::Time> GetLastSuccessfulEnrollmentTime() const override;
  base::TimeDelta GetRefreshPeriod() const override;
  base::TimeDelta GetTimeToNextEnrollmentRequest() const override;
  bool IsWaitingForEnrollmentResult() const override;
  size_t GetNumConsecutiveEnrollmentFailures() const override;

  // NetworkStateHandlerObserver:
  void DefaultNetworkChanged(const NetworkState* network) override;
  void OnShuttingDown() override;

  bool DoesMachineHaveNetworkConnectivity();

  // Starts a new timer that will fire when an enrollment is ready to be
  // attempted.
  void ScheduleNextEnrollment();

  void OnEnrollmentTimerFired();

  base::Optional<cryptauthv2::ClientMetadata> pending_enrollment_request_;

  // Only non-null while an enrollment attempt is in progress, in other words,
  // between NotifyEnrollmentRequested() and HandleEnrollmentResult().
  base::Optional<cryptauthv2::ClientMetadata> current_enrollment_request_;

  PrefService* pref_service_ = nullptr;
  NetworkStateHandler* network_state_handler_ = nullptr;
  base::Clock* clock_ = nullptr;
  std::unique_ptr<base::OneShotTimer> enrollment_timer_ = nullptr;
  cryptauthv2::ClientDirective client_directive_;

  DISALLOW_COPY_AND_ASSIGN(CryptAuthSchedulerImpl);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_SCHEDULER_IMPL_H_
