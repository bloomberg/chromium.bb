// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_NETWORK_AWARE_ENROLLMENT_SCHEDULER_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_NETWORK_AWARE_ENROLLMENT_SCHEDULER_H_

#include <memory>

#include "base/macros.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state_handler_observer.h"
#include "chromeos/services/device_sync/cryptauth_enrollment_scheduler.h"

class PrefService;

namespace chromeos {

class NetworkStateHandler;

namespace device_sync {

// CryptAuthEnrollmentScheduler implementation which ensures that enrollment is
// only requested while online. This class owns and serves as a delegate to a
// network-unaware scheduler which requests enrollment without checking for
// network connectivity. When this class receives a request from the network-
// unaware scheduler, it:
//   *Requests enrollment immediately if there is network connectivity, or
//   *Caches the request until network connectivity has been attained, then
//    requests enrollment.
class NetworkAwareEnrollmentScheduler
    : public CryptAuthEnrollmentScheduler,
      public CryptAuthEnrollmentScheduler::Delegate,
      public NetworkStateHandlerObserver {
 public:
  class Factory {
   public:
    static Factory* Get();
    static void SetFactoryForTesting(Factory* test_factory);
    virtual ~Factory();
    virtual std::unique_ptr<CryptAuthEnrollmentScheduler> BuildInstance(
        CryptAuthEnrollmentScheduler::Delegate* delegate,
        PrefService* pref_service,
        NetworkStateHandler* network_state_handler =
            NetworkHandler::Get()->network_state_handler());

   private:
    static Factory* test_factory_;
  };

  ~NetworkAwareEnrollmentScheduler() override;

 private:
  NetworkAwareEnrollmentScheduler(
      CryptAuthEnrollmentScheduler::Delegate* delegate,
      NetworkStateHandler* network_state_handler);

  // CryptAuthEnrollmentScheduler:
  void RequestEnrollmentNow() override;
  void HandleEnrollmentResult(
      const CryptAuthEnrollmentResult& enrollment_result) override;
  base::Optional<base::Time> GetLastSuccessfulEnrollmentTime() const override;
  base::TimeDelta GetRefreshPeriod() const override;
  base::TimeDelta GetTimeToNextEnrollmentRequest() const override;
  bool IsWaitingForEnrollmentResult() const override;
  size_t GetNumConsecutiveFailures() const override;

  // CryptAuthEnrollmentScheduler::Delegate:
  void OnEnrollmentRequested(const base::Optional<cryptauthv2::PolicyReference>&
                                 client_directive_policy_reference) override;

  // NetworkStateHandlerObserver:
  void DefaultNetworkChanged(const NetworkState* network) override;
  void OnShuttingDown() override;

  bool DoesMachineHaveNetworkConnectivity();

  NetworkStateHandler* network_state_handler_;
  std::unique_ptr<CryptAuthEnrollmentScheduler> network_unaware_scheduler_;

  // The pending enrollment request, if it exists. If OnEnrollmentRequested() is
  // invoked while offline, this class stores the optional PolicyReference
  // parameter for that function to this instance field; then, when online
  // connectivity has been restored, this class notifies *its* observer,
  // forwarding this instance field. If this class does not have a pending
  // enrollment, this field is null.
  base::Optional<base::Optional<cryptauthv2::PolicyReference>>
      pending_client_directive_policy_reference_;

  DISALLOW_COPY_AND_ASSIGN(NetworkAwareEnrollmentScheduler);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_NETWORK_AWARE_ENROLLMENT_SCHEDULER_H_
