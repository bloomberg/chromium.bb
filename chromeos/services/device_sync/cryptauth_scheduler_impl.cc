// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/cryptauth_scheduler_impl.h"

#include <algorithm>
#include <utility>

#include "base/base64.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/services/device_sync/pref_names.h"
#include "chromeos/services/device_sync/value_string_encoding.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace chromeos {

namespace device_sync {

namespace {

constexpr base::TimeDelta kZeroTimeDelta = base::TimeDelta::FromSeconds(0);

// The default period between successful enrollments in days. Superseded by the
// ClientDirective's checkin_delay_millis sent by CryptAuth in SyncKeysResponse.
constexpr base::TimeDelta kDefaultRefreshPeriod = base::TimeDelta::FromDays(30);

// The default period, in hours, between enrollment attempts if the previous
// enrollment attempt failed. Superseded by the ClientDirective's
// retry_period_millis sent by CryptAuth in SyncKeysResponse.
constexpr base::TimeDelta kDefaultRetryPeriod = base::TimeDelta::FromHours(12);

// The time to wait before an "immediate" retry attempt after a failed
// enrollment attempt. Note: SyncKeys requests are throttled by CryptAuth if
// more than one is sent within a five-minute window.
constexpr base::TimeDelta kImmediateRetryDelay =
    base::TimeDelta::FromMinutes(5);

// The default number of "immediate" retries after a failed enrollment attempt.
// Superseded by the ClientDirective's retry_attempts sent by CryptAuth in the
// SyncKeysResponse.
const int kDefaultMaxImmediateRetries = 3;

const char kNoClientDirective[] = "[No ClientDirective]";

const char kNoClientMetadata[] = "[No ClientMetadata]";

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

cryptauthv2::ClientDirective BuildClientDirective(PrefService* pref_service) {
  DCHECK(pref_service);
  const base::Value* encoded_client_directive =
      pref_service->Get(prefs::kCryptAuthSchedulerClientDirective);
  if (encoded_client_directive->GetString() == kNoClientDirective)
    return CreateDefaultClientDirective();

  base::Optional<cryptauthv2::ClientDirective> client_directive_from_pref =
      util::DecodeProtoMessageFromValueString<cryptauthv2::ClientDirective>(
          encoded_client_directive);

  return client_directive_from_pref.value_or(CreateDefaultClientDirective());
}

cryptauthv2::ClientMetadata BuildClientMetadata(
    size_t retry_count,
    const cryptauthv2::ClientMetadata::InvocationReason& invocation_reason,
    const base::Optional<std::string>& session_id) {
  cryptauthv2::ClientMetadata client_metadata;
  client_metadata.set_retry_count(retry_count);
  client_metadata.set_invocation_reason(invocation_reason);
  if (session_id)
    client_metadata.set_session_id(*session_id);

  return client_metadata;
}

}  // namespace

// static
CryptAuthSchedulerImpl::Factory*
    CryptAuthSchedulerImpl::Factory::test_factory_ = nullptr;

// static
CryptAuthSchedulerImpl::Factory* CryptAuthSchedulerImpl::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<CryptAuthSchedulerImpl::Factory> factory;
  return factory.get();
}

// static
void CryptAuthSchedulerImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

CryptAuthSchedulerImpl::Factory::~Factory() = default;

std::unique_ptr<CryptAuthScheduler>
CryptAuthSchedulerImpl::Factory::BuildInstance(
    PrefService* pref_service,
    NetworkStateHandler* network_state_handler,
    base::Clock* clock,
    std::unique_ptr<base::OneShotTimer> enrollment_timer) {
  return base::WrapUnique(new CryptAuthSchedulerImpl(
      pref_service, network_state_handler, clock, std::move(enrollment_timer)));
}

// static
void CryptAuthSchedulerImpl::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kCryptAuthSchedulerClientDirective,
                               kNoClientDirective);
  registry->RegisterStringPref(
      prefs::kCryptAuthSchedulerNextEnrollmentRequestClientMetadata,
      kNoClientMetadata);
  registry->RegisterTimePref(
      prefs::kCryptAuthSchedulerLastEnrollmentAttemptTime, base::Time());
  registry->RegisterTimePref(
      prefs::kCryptAuthSchedulerLastSuccessfulEnrollmentTime, base::Time());
}

CryptAuthSchedulerImpl::CryptAuthSchedulerImpl(
    PrefService* pref_service,
    NetworkStateHandler* network_state_handler,
    base::Clock* clock,
    std::unique_ptr<base::OneShotTimer> enrollment_timer)
    : pref_service_(pref_service),
      network_state_handler_(network_state_handler),
      clock_(clock),
      enrollment_timer_(std::move(enrollment_timer)),
      client_directive_(BuildClientDirective(pref_service)) {
  DCHECK(pref_service_);
  DCHECK(network_state_handler_);
  DCHECK(clock_);
  DCHECK(IsClientDirectiveValid(client_directive_));

  // Queue up the most recently scheduled enrollment request if applicable.
  const base::Value* client_metadata_from_pref = pref_service_->Get(
      prefs::kCryptAuthSchedulerNextEnrollmentRequestClientMetadata);
  if (client_metadata_from_pref->GetString() != kNoClientMetadata) {
    pending_enrollment_request_ =
        util::DecodeProtoMessageFromValueString<cryptauthv2::ClientMetadata>(
            client_metadata_from_pref);
  }

  // If we are recovering from a failure, reset the failure count to 1 in the
  // hopes that the restart solved the issue. This will allow for immediate
  // retries again if allowed by the ClientDirective.
  if (pending_enrollment_request_ &&
      pending_enrollment_request_->retry_count() > 0) {
    pending_enrollment_request_->set_retry_count(1);
  }
}

CryptAuthSchedulerImpl::~CryptAuthSchedulerImpl() {
  if (network_state_handler_)
    network_state_handler_->RemoveObserver(this, FROM_HERE);
}

void CryptAuthSchedulerImpl::OnEnrollmentSchedulingStarted() {
  network_state_handler_->AddObserver(this, FROM_HERE);

  ScheduleNextEnrollment();
}

void CryptAuthSchedulerImpl::RequestEnrollment(
    const cryptauthv2::ClientMetadata::InvocationReason& invocation_reason,
    const base::Optional<std::string>& session_id) {
  enrollment_timer_->Stop();

  pending_enrollment_request_ =
      BuildClientMetadata(0 /* retry_count */, invocation_reason, session_id);

  ScheduleNextEnrollment();
}

void CryptAuthSchedulerImpl::HandleEnrollmentResult(
    const CryptAuthEnrollmentResult& enrollment_result) {
  DCHECK(current_enrollment_request_);
  DCHECK(!enrollment_timer_->IsRunning());

  base::Time now = clock_->Now();

  pref_service_->SetTime(prefs::kCryptAuthSchedulerLastEnrollmentAttemptTime,
                         now);

  // If unsuccessful and a more immediate request isn't pending, queue up the
  // failure recovery attempt.
  if (enrollment_result.IsSuccess()) {
    pref_service_->SetTime(
        prefs::kCryptAuthSchedulerLastSuccessfulEnrollmentTime, now);
  } else if (!pending_enrollment_request_) {
    current_enrollment_request_->set_retry_count(
        current_enrollment_request_->retry_count() + 1);
    pending_enrollment_request_ = current_enrollment_request_;
  }

  if (enrollment_result.client_directive() &&
      IsClientDirectiveValid(*enrollment_result.client_directive())) {
    client_directive_ = *enrollment_result.client_directive();

    pref_service_->Set(
        prefs::kCryptAuthSchedulerClientDirective,
        util::EncodeProtoMessageAsValueString(&client_directive_));
  }

  current_enrollment_request_.reset();

  ScheduleNextEnrollment();
}

base::Optional<base::Time>
CryptAuthSchedulerImpl::GetLastSuccessfulEnrollmentTime() const {
  base::Time time = pref_service_->GetTime(
      prefs::kCryptAuthSchedulerLastSuccessfulEnrollmentTime);
  if (time.is_null())
    return base::nullopt;

  return time;
}

base::TimeDelta CryptAuthSchedulerImpl::GetRefreshPeriod() const {
  return base::TimeDelta::FromMilliseconds(
      client_directive_.checkin_delay_millis());
}

base::TimeDelta CryptAuthSchedulerImpl::GetTimeToNextEnrollmentRequest() const {
  // Enrollment already requested.
  if (IsWaitingForEnrollmentResult())
    return kZeroTimeDelta;

  if (!pending_enrollment_request_)
    return base::TimeDelta::Max();

  // Attempt the pending enrollment request immediately unless it is periodic.
  if (pending_enrollment_request_->retry_count() == 0) {
    if (pending_enrollment_request_->invocation_reason() !=
        cryptauthv2::ClientMetadata::PERIODIC) {
      return kZeroTimeDelta;
    }

    base::Optional<base::Time> last_successful_enrollment_time =
        GetLastSuccessfulEnrollmentTime();
    if (!last_successful_enrollment_time)
      return kZeroTimeDelta;

    base::TimeDelta time_since_last_success =
        clock_->Now() - *last_successful_enrollment_time;
    return std::max(kZeroTimeDelta,
                    GetRefreshPeriod() - time_since_last_success);
  }

  base::TimeDelta time_since_last_attempt =
      clock_->Now() - pref_service_->GetTime(
                          prefs::kCryptAuthSchedulerLastEnrollmentAttemptTime);

  // Recover from failure using immediate retry.
  if (pending_enrollment_request_->retry_count() <=
      client_directive_.retry_attempts()) {
    return std::max(kZeroTimeDelta,
                    kImmediateRetryDelay - time_since_last_attempt);
  }

  // Recover from failure after expending allotted immediate retries.
  return std::max(kZeroTimeDelta, base::TimeDelta::FromMilliseconds(
                                      client_directive_.retry_period_millis()) -
                                      time_since_last_attempt);
}

bool CryptAuthSchedulerImpl::IsWaitingForEnrollmentResult() const {
  return current_enrollment_request_.has_value();
}

size_t CryptAuthSchedulerImpl::GetNumConsecutiveEnrollmentFailures() const {
  if (current_enrollment_request_)
    return current_enrollment_request_->retry_count();

  if (pending_enrollment_request_)
    return pending_enrollment_request_->retry_count();

  return 0;
}

void CryptAuthSchedulerImpl::DefaultNetworkChanged(
    const NetworkState* network) {
  // The updated default network may not be online.
  if (!DoesMachineHaveNetworkConnectivity())
    return;

  // Now that the device has connectivity, reschedule enrollment.
  ScheduleNextEnrollment();
}

void CryptAuthSchedulerImpl::OnShuttingDown() {
  DCHECK(network_state_handler_);
  network_state_handler_->RemoveObserver(this, FROM_HERE);
  network_state_handler_ = nullptr;
}

bool CryptAuthSchedulerImpl::DoesMachineHaveNetworkConnectivity() {
  if (!network_state_handler_)
    return false;

  // TODO(khorimoto): IsConnectedState() can still return true if connected to
  // a captive portal; use the "online" boolean once we fetch data via the
  // networking Mojo API. See https://crbug.com/862420.
  const NetworkState* default_network =
      network_state_handler_->DefaultNetwork();
  return default_network && default_network->IsConnectedState();
}

void CryptAuthSchedulerImpl::ScheduleNextEnrollment() {
  // Wait for the current enrollment attempt to finish before determining the
  // next enrollment request in case we need to recover from a failure.
  if (IsWaitingForEnrollmentResult())
    return;

  // If no enrollment request has been explicitly made, schedule the standard
  // periodic enrollment attempt.
  if (!pending_enrollment_request_) {
    pending_enrollment_request_ =
        BuildClientMetadata(0 /* retry_count */,
                            GetLastSuccessfulEnrollmentTime()
                                ? cryptauthv2::ClientMetadata::PERIODIC
                                : cryptauthv2::ClientMetadata::INITIALIZATION,
                            base::nullopt /* session_id */);
  }

  DCHECK(pending_enrollment_request_);
  pref_service_->Set(
      prefs::kCryptAuthSchedulerNextEnrollmentRequestClientMetadata,
      util::EncodeProtoMessageAsValueString(
          &pending_enrollment_request_.value()));

  if (!HasEnrollmentSchedulingStarted())
    return;

  enrollment_timer_->Start(
      FROM_HERE, GetTimeToNextEnrollmentRequest(),
      base::BindOnce(&CryptAuthSchedulerImpl::OnEnrollmentTimerFired,
                     base::Unretained(this)));
}

void CryptAuthSchedulerImpl::OnEnrollmentTimerFired() {
  DCHECK(!current_enrollment_request_);
  DCHECK(pending_enrollment_request_);

  if (!DoesMachineHaveNetworkConnectivity()) {
    PA_LOG(INFO) << "Enrollment triggered while the device is offline. Waiting "
                 << "for online connectivity before making request.";
    return;
  }

  current_enrollment_request_ = pending_enrollment_request_;
  pending_enrollment_request_.reset();

  base::Optional<cryptauthv2::PolicyReference> policy_reference = base::nullopt;
  if (client_directive_.has_policy_reference())
    policy_reference = client_directive_.policy_reference();

  NotifyEnrollmentRequested(*current_enrollment_request_, policy_reference);
}

}  // namespace device_sync

}  // namespace chromeos
