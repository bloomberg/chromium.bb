// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/persistent_enrollment_scheduler.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/optional.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/services/device_sync/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace chromeos {

namespace device_sync {

namespace {

// The default period between successful enrollments in days. Superseded by the
// ClientDirective's checkin_delay_millis sent by CryptAuth in SyncKeysResponse.
constexpr base::TimeDelta kDefaultRefreshPeriod = base::TimeDelta::FromDays(30);

// The default period, in hours, between enrollment attempts if the previous
// enrollment attempt failed. Superseded by the ClientDirective's
// retry_period_millis sent by CryptAuth in SyncKeysResponse.
constexpr base::TimeDelta kDefaultRetryPeriod = base::TimeDelta::FromHours(12);

// The default number of immediate retries after a failed enrollment attempt.
// Superseded by the ClientDirective's retry_attempts sent by CryptAuth in the
// SyncKeysResponse.
const int kDefaultMaxImmediateRetries = 3;

bool IsClientDirectiveValid(
    const cryptauthv2::ClientDirective& client_directive) {
  return client_directive.checkin_delay_millis() > 0 &&
         client_directive.retry_period_millis() > 0 &&
         client_directive.retry_attempts() >= 0;
}

// Fills a ClientDirective with our chosen default parameters. This
// ClientDirective is used until a ClientDirective is received from CryptAuth in
// the SyncKeysResponse.
cryptauthv2::ClientDirective CreateDefaultClientDirective() {
  cryptauthv2::ClientDirective client_directive;
  client_directive.set_checkin_delay_millis(
      kDefaultRefreshPeriod.InMilliseconds());
  client_directive.set_retry_period_millis(
      kDefaultRetryPeriod.InMilliseconds());
  client_directive.set_retry_attempts(kDefaultMaxImmediateRetries);

  return client_directive;
}

// Decodes and parses the base64-encoded serialized ClientDirective string.
base::Optional<cryptauthv2::ClientDirective> ClientDirectiveFromPrefString(
    const std::string& encoded_serialized_client_directive) {
  if (encoded_serialized_client_directive.empty())
    return base::nullopt;

  std::string decoded_serialized_client_directive;
  if (!base::Base64Decode(encoded_serialized_client_directive,
                          &decoded_serialized_client_directive)) {
    PA_LOG(ERROR) << "Error decoding ClientDirective pref string";
    return base::nullopt;
  }

  cryptauthv2::ClientDirective client_directive;
  if (!client_directive.ParseFromString(decoded_serialized_client_directive)) {
    PA_LOG(ERROR) << "Error parsing ClientDirective from pref string";
    return base::nullopt;
  }

  return client_directive;
}

// Serializes and base64 encodes the input ClientDirective.
std::string ClientDirectiveToPrefString(
    const cryptauthv2::ClientDirective& client_directive) {
  std::string encoded_serialized_client_directive;
  base::Base64Encode(client_directive.SerializeAsString(),
                     &encoded_serialized_client_directive);

  return encoded_serialized_client_directive;
}

cryptauthv2::ClientDirective BuildClientDirective(PrefService* pref_service) {
  DCHECK(pref_service);

  base::Optional<cryptauthv2::ClientDirective> client_directive_from_pref =
      ClientDirectiveFromPrefString(pref_service->GetString(
          prefs::kCryptAuthEnrollmentSchedulerClientDirective));
  if (client_directive_from_pref)
    return *client_directive_from_pref;

  return CreateDefaultClientDirective();
}

}  // namespace

// static
PersistentEnrollmentScheduler::Factory*
    PersistentEnrollmentScheduler::Factory::test_factory_ = nullptr;

// static
PersistentEnrollmentScheduler::Factory*
PersistentEnrollmentScheduler::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<PersistentEnrollmentScheduler::Factory> factory;
  return factory.get();
}

// static
void PersistentEnrollmentScheduler::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

PersistentEnrollmentScheduler::Factory::~Factory() = default;

std::unique_ptr<CryptAuthEnrollmentScheduler>
PersistentEnrollmentScheduler::Factory::BuildInstance(
    Delegate* delegate,
    PrefService* pref_service,
    base::Clock* clock,
    std::unique_ptr<base::OneShotTimer> timer,
    scoped_refptr<base::TaskRunner> task_runner) {
  return base::WrapUnique(new PersistentEnrollmentScheduler(
      delegate, pref_service, clock, std::move(timer), task_runner));
}

// static
void PersistentEnrollmentScheduler::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterStringPref(
      prefs::kCryptAuthEnrollmentSchedulerClientDirective, std::string());
  registry->RegisterTimePref(
      prefs::kCryptAuthEnrollmentSchedulerLastEnrollmentAttemptTime,
      base::Time());
  registry->RegisterTimePref(
      prefs::kCryptAuthEnrollmentSchedulerLastSuccessfulEnrollmentTime,
      base::Time());
  registry->RegisterUint64Pref(
      prefs::kCryptAuthEnrollmentSchedulerNumConsecutiveFailures, 0);
}

PersistentEnrollmentScheduler::PersistentEnrollmentScheduler(
    Delegate* delegate,
    PrefService* pref_service,
    base::Clock* clock,
    std::unique_ptr<base::OneShotTimer> timer,
    scoped_refptr<base::TaskRunner> task_runner)
    : CryptAuthEnrollmentScheduler(delegate),
      pref_service_(pref_service),
      clock_(clock),
      timer_(std::move(timer)),
      client_directive_(BuildClientDirective(pref_service)),
      weak_ptr_factory_(this) {
  DCHECK(pref_service);
  DCHECK(clock);
  DCHECK(IsClientDirectiveValid(client_directive_));

  // If we are recovering from a failure, set the failure count back to 1 in the
  // hopes that the restart solved the issue. This will allow for immediate
  // retries again if allowed by the ClientDirective.
  if (GetNumConsecutiveFailures() > 1) {
    pref_service_->SetUint64(
        prefs::kCryptAuthEnrollmentSchedulerNumConsecutiveFailures, 1);
  }

  // Schedule the next enrollment as part of a new task. This ensures that the
  // delegate can complete its initialization before handling an enrollment
  // request.
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&PersistentEnrollmentScheduler::ScheduleNextEnrollment,
                     weak_ptr_factory_.GetWeakPtr()));
}

PersistentEnrollmentScheduler::~PersistentEnrollmentScheduler() = default;

void PersistentEnrollmentScheduler::RequestEnrollmentNow() {
  timer_->Stop();
  NotifyEnrollmentRequested(GetPolicyReference());
}

void PersistentEnrollmentScheduler::HandleEnrollmentResult(
    const CryptAuthEnrollmentResult& enrollment_result) {
  DCHECK(!timer_->IsRunning());

  base::Time now = clock_->Now();

  if (enrollment_result.IsSuccess()) {
    pref_service_->SetUint64(
        prefs::kCryptAuthEnrollmentSchedulerNumConsecutiveFailures, 0);

    pref_service_->SetTime(
        prefs::kCryptAuthEnrollmentSchedulerLastSuccessfulEnrollmentTime, now);
  } else {
    pref_service_->SetUint64(
        prefs::kCryptAuthEnrollmentSchedulerNumConsecutiveFailures,
        GetNumConsecutiveFailures() + 1);
  }

  pref_service_->SetTime(
      prefs::kCryptAuthEnrollmentSchedulerLastEnrollmentAttemptTime, now);

  if (enrollment_result.client_directive() &&
      IsClientDirectiveValid(*enrollment_result.client_directive())) {
    client_directive_ = *enrollment_result.client_directive();

    pref_service_->SetString(
        prefs::kCryptAuthEnrollmentSchedulerClientDirective,
        ClientDirectiveToPrefString(client_directive_));
  }

  ScheduleNextEnrollment();
}

base::Optional<base::Time>
PersistentEnrollmentScheduler::GetLastSuccessfulEnrollmentTime() const {
  base::Time time = pref_service_->GetTime(
      prefs::kCryptAuthEnrollmentSchedulerLastSuccessfulEnrollmentTime);
  if (time.is_null())
    return base::nullopt;

  return time;
}

base::TimeDelta PersistentEnrollmentScheduler::GetRefreshPeriod() const {
  return base::TimeDelta::FromMilliseconds(
      client_directive_.checkin_delay_millis());
}

base::TimeDelta PersistentEnrollmentScheduler::GetTimeToNextEnrollmentRequest()
    const {
  if (IsWaitingForEnrollmentResult())
    return base::TimeDelta::FromMilliseconds(0);

  return timer_->GetCurrentDelay();
}

bool PersistentEnrollmentScheduler::IsWaitingForEnrollmentResult() const {
  return !timer_->IsRunning();
}

size_t PersistentEnrollmentScheduler::GetNumConsecutiveFailures() const {
  return pref_service_->GetUint64(
      prefs::kCryptAuthEnrollmentSchedulerNumConsecutiveFailures);
}

base::TimeDelta
PersistentEnrollmentScheduler::CalculateTimeBetweenEnrollmentRequests() const {
  size_t num_consecutive_failures = GetNumConsecutiveFailures();
  if (num_consecutive_failures == 0)
    return GetRefreshPeriod();

  if (num_consecutive_failures <= (size_t)client_directive_.retry_attempts())
    return base::TimeDelta::FromMilliseconds(0);

  return base::TimeDelta::FromMilliseconds(
      client_directive_.retry_period_millis());
}

void PersistentEnrollmentScheduler::ScheduleNextEnrollment() {
  DCHECK(!timer_->IsRunning());

  base::Time last_attempt_time = pref_service_->GetTime(
      prefs::kCryptAuthEnrollmentSchedulerLastEnrollmentAttemptTime);

  base::TimeDelta time_until_next_request =
      base::TimeDelta::FromMilliseconds(0);
  if (!last_attempt_time.is_null()) {
    time_until_next_request =
        std::max(base::TimeDelta::FromMilliseconds(0),
                 CalculateTimeBetweenEnrollmentRequests() -
                     (clock_->Now() - last_attempt_time));
  }

  timer_->Start(
      FROM_HERE, time_until_next_request,
      base::BindOnce(&PersistentEnrollmentScheduler::NotifyEnrollmentRequested,
                     base::Unretained(this), GetPolicyReference()));
}

base::Optional<cryptauthv2::PolicyReference>
PersistentEnrollmentScheduler::GetPolicyReference() const {
  if (client_directive_.has_policy_reference())
    return client_directive_.policy_reference();

  return base::nullopt;
}

}  // namespace device_sync

}  // namespace chromeos
