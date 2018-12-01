// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/host_verifier_impl.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "chromeos/components/proximity_auth/logging/logging.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace chromeos {

namespace multidevice_setup {

namespace {

// Software features which, when enabled, represent a verified host.
constexpr const cryptauth::SoftwareFeature kPotentialHostFeatures[] = {
    cryptauth::SoftwareFeature::EASY_UNLOCK_HOST,
    cryptauth::SoftwareFeature::MAGIC_TETHER_HOST,
    cryptauth::SoftwareFeature::SMS_CONNECT_HOST};

// Name of the preference containing the time (in milliseconds since Unix
// epoch) at which a verification attempt should be retried. If the preference
// value is kTimestampNotSet, no retry is scheduled.
const char kRetryTimestampPrefName[] =
    "multidevice_setup.current_retry_timestamp_ms";

// Value set for the kRetryTimestampPrefName preference when no retry attempt is
// underway (i.e., verification is complete or there is no current host).
const int64_t kTimestampNotSet = 0;

// Name of the preference containing the time delta (in ms) between the
// timestamp present in the kRetryTimestampPrefName preference and the attempt
// before that one. If the value of kRetryTimestampPrefName is kTimestampNotSet,
// the value at this preference is meaningless.
const char kLastUsedTimeDeltaMsPrefName[] =
    "multidevice_setup.last_used_time_delta_ms";

// Delta to set for the first retry.
constexpr const base::TimeDelta kFirstRetryDelta =
    base::TimeDelta::FromMinutes(10);

// The multiplier for increasing the backoff timer between retries.
const double kExponentialBackoffMultiplier = 1.5;

}  // namespace

// static
HostVerifierImpl::Factory* HostVerifierImpl::Factory::test_factory_ = nullptr;

// static
HostVerifierImpl::Factory* HostVerifierImpl::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<Factory> factory;
  return factory.get();
}

// static
void HostVerifierImpl::Factory::SetFactoryForTesting(Factory* test_factory) {
  test_factory_ = test_factory;
}

HostVerifierImpl::Factory::~Factory() = default;

std::unique_ptr<HostVerifier> HostVerifierImpl::Factory::BuildInstance(
    HostBackendDelegate* host_backend_delegate,
    device_sync::DeviceSyncClient* device_sync_client,
    PrefService* pref_service,
    base::Clock* clock,
    std::unique_ptr<base::OneShotTimer> timer) {
  return base::WrapUnique(new HostVerifierImpl(host_backend_delegate,
                                               device_sync_client, pref_service,
                                               clock, std::move(timer)));
}

// static
void HostVerifierImpl::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterInt64Pref(kRetryTimestampPrefName, kTimestampNotSet);
  registry->RegisterInt64Pref(kLastUsedTimeDeltaMsPrefName, 0);
}

HostVerifierImpl::HostVerifierImpl(
    HostBackendDelegate* host_backend_delegate,
    device_sync::DeviceSyncClient* device_sync_client,
    PrefService* pref_service,
    base::Clock* clock,
    std::unique_ptr<base::OneShotTimer> timer)
    : host_backend_delegate_(host_backend_delegate),
      device_sync_client_(device_sync_client),
      pref_service_(pref_service),
      clock_(clock),
      timer_(std::move(timer)) {
  host_backend_delegate_->AddObserver(this);
  device_sync_client_->AddObserver(this);

  UpdateRetryState();
}

HostVerifierImpl::~HostVerifierImpl() {
  host_backend_delegate_->RemoveObserver(this);
  device_sync_client_->RemoveObserver(this);
}

bool HostVerifierImpl::IsHostVerified() {
  base::Optional<multidevice::RemoteDeviceRef> current_host =
      host_backend_delegate_->GetMultiDeviceHostFromBackend();
  if (!current_host)
    return false;

  // If a host exists on the back-end but there is a pending request to remove
  // that host, the device pending removal is no longer considered verified.
  if (host_backend_delegate_->HasPendingHostRequest() &&
      !host_backend_delegate_->GetPendingHostRequest()) {
    return false;
  }

  // If one or more potential host sofware features is enabled, the host is
  // considered verified.
  for (const auto& software_feature : kPotentialHostFeatures) {
    if (current_host->GetSoftwareFeatureState(software_feature) ==
        multidevice::SoftwareFeatureState::kEnabled) {
      return true;
    }
  }

  return false;
}

void HostVerifierImpl::PerformAttemptVerificationNow() {
  AttemptHostVerification();
}

void HostVerifierImpl::OnHostChangedOnBackend() {
  UpdateRetryState();
}

void HostVerifierImpl::OnNewDevicesSynced() {
  UpdateRetryState();
}

void HostVerifierImpl::UpdateRetryState() {
  // If there is no host, verification is not applicable.
  if (!host_backend_delegate_->GetMultiDeviceHostFromBackend()) {
    StopTimerAndClearPrefs();
    return;
  }

  // If there is a host and it is verified, verification is no longer necessary.
  if (IsHostVerified()) {
    bool was_timer_running = timer_->IsRunning();
    StopTimerAndClearPrefs();
    if (was_timer_running)
      NotifyHostVerified();
    return;
  }

  // If |timer_| is running, an ongoing retry attempt is in progress.
  if (timer_->IsRunning())
    return;

  int64_t timestamp_from_prefs =
      pref_service_->GetInt64(kRetryTimestampPrefName);

  // If no retry timer was set, set the timer to the initial value and attempt
  // to verify now.
  if (timestamp_from_prefs == kTimestampNotSet) {
    AttemptVerificationWithInitialTimeout();
    return;
  }

  base::Time retry_time_from_prefs =
      base::Time::FromJavaTime(timestamp_from_prefs);

  // If a timeout value was set but has not yet occurred, start the timer.
  if (clock_->Now() < retry_time_from_prefs) {
    StartTimer(retry_time_from_prefs);
    return;
  }

  AttemptVerificationAfterInitialTimeout(retry_time_from_prefs);
}

void HostVerifierImpl::StopTimerAndClearPrefs() {
  timer_->Stop();
  pref_service_->SetInt64(kRetryTimestampPrefName, kTimestampNotSet);
  pref_service_->SetInt64(kLastUsedTimeDeltaMsPrefName, 0);
}

void HostVerifierImpl::AttemptVerificationWithInitialTimeout() {
  base::Time retry_time = clock_->Now() + kFirstRetryDelta;

  pref_service_->SetInt64(kRetryTimestampPrefName, retry_time.ToJavaTime());
  pref_service_->SetInt64(kLastUsedTimeDeltaMsPrefName,
                          kFirstRetryDelta.InMilliseconds());

  StartTimer(retry_time);
  AttemptHostVerification();
}

void HostVerifierImpl::AttemptVerificationAfterInitialTimeout(
    const base::Time& retry_time_from_prefs) {
  int64_t time_delta_ms = pref_service_->GetInt64(kLastUsedTimeDeltaMsPrefName);
  DCHECK(time_delta_ms > 0);

  base::Time retry_time = retry_time_from_prefs;
  while (clock_->Now() >= retry_time) {
    time_delta_ms *= kExponentialBackoffMultiplier;
    retry_time += base::TimeDelta::FromMilliseconds(time_delta_ms);
  }

  pref_service_->SetInt64(kRetryTimestampPrefName, retry_time.ToJavaTime());
  pref_service_->SetInt64(kLastUsedTimeDeltaMsPrefName, time_delta_ms);

  StartTimer(retry_time);
  AttemptHostVerification();
}

void HostVerifierImpl::StartTimer(const base::Time& time_to_fire) {
  base::Time now = clock_->Now();
  DCHECK(now < time_to_fire);

  timer_->Start(
      FROM_HERE, time_to_fire - now /* delay */,
      base::Bind(&HostVerifierImpl::UpdateRetryState, base::Unretained(this)));
}

void HostVerifierImpl::AttemptHostVerification() {
  base::Optional<multidevice::RemoteDeviceRef> current_host =
      host_backend_delegate_->GetMultiDeviceHostFromBackend();
  if (!current_host) {
    PA_LOG(WARNING) << "HostVerifierImpl::AttemptHostVerification(): Cannot "
                    << "attempt verification because there is no active host.";
    return;
  }

  PA_LOG(VERBOSE) << "HostVerifierImpl::AttemptHostVerification(): Attempting "
                  << "host verification now.";
  device_sync_client_->FindEligibleDevices(
      cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST, base::DoNothing());
}

}  // namespace multidevice_setup

}  // namespace chromeos
