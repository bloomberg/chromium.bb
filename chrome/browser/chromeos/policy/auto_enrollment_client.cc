// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/auto_enrollment_client.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/guid.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/policy/server_backed_device_state.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/system_policy_request_context.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/sha2.h"
#include "net/url_request/url_request_context_getter.h"
#include "policy/proto/device_management_backend.pb.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace em = enterprise_management;

namespace policy {

namespace {

// UMA histogram names.
const char kUMAProtocolTime[] = "Enterprise.AutoEnrollmentProtocolTime";
const char kUMAExtraTime[] = "Enterprise.AutoEnrollmentExtraTime";
const char kUMARequestStatus[] = "Enterprise.AutoEnrollmentRequestStatus";
const char kUMANetworkErrorCode[] =
    "Enterprise.AutoEnrollmentRequestNetworkErrorCode";

// Returns the power of the next power-of-2 starting at |value|.
int NextPowerOf2(int64_t value) {
  for (int i = 0; i <= AutoEnrollmentClient::kMaximumPower; ++i) {
    if ((INT64_C(1) << i) >= value)
      return i;
  }
  // No other value can be represented in an int64_t.
  return AutoEnrollmentClient::kMaximumPower + 1;
}

// Sets or clears a value in a dictionary.
void UpdateDict(base::DictionaryValue* dict,
                const char* pref_path,
                bool set_or_clear,
                base::Value* value) {
  scoped_ptr<base::Value> scoped_value(value);
  if (set_or_clear)
    dict->Set(pref_path, scoped_value.release());
  else
    dict->Remove(pref_path, NULL);
}

// Converts a restore mode enum value from the DM protocol into the
// corresponding prefs string constant.
std::string ConvertRestoreMode(
    em::DeviceStateRetrievalResponse::RestoreMode restore_mode) {
  switch (restore_mode) {
    case em::DeviceStateRetrievalResponse::RESTORE_MODE_NONE:
      return std::string();
    case em::DeviceStateRetrievalResponse::RESTORE_MODE_REENROLLMENT_REQUESTED:
      return kDeviceStateRestoreModeReEnrollmentRequested;
    case em::DeviceStateRetrievalResponse::RESTORE_MODE_REENROLLMENT_ENFORCED:
      return kDeviceStateRestoreModeReEnrollmentEnforced;
    case em::DeviceStateRetrievalResponse::RESTORE_MODE_DISABLED:
      return kDeviceStateRestoreModeDisabled;
  }

  // Return is required to avoid compiler warning.
  NOTREACHED() << "Bad restore mode " << restore_mode;
  return std::string();
}

}  // namespace

AutoEnrollmentClient::AutoEnrollmentClient(
    const ProgressCallback& callback,
    DeviceManagementService* service,
    PrefService* local_state,
    scoped_refptr<net::URLRequestContextGetter> system_request_context,
    const std::string& server_backed_state_key,
    int power_initial,
    int power_limit)
    : progress_callback_(callback),
      state_(AUTO_ENROLLMENT_STATE_IDLE),
      has_server_state_(false),
      device_state_available_(false),
      device_id_(base::GenerateGUID()),
      server_backed_state_key_(server_backed_state_key),
      current_power_(power_initial),
      power_limit_(power_limit),
      modulus_updates_received_(0),
      device_management_service_(service),
      local_state_(local_state) {
  request_context_ = new SystemPolicyRequestContext(
      system_request_context, GetUserAgent());

  DCHECK_LE(current_power_, power_limit_);
  DCHECK(!progress_callback_.is_null());
  CHECK(!server_backed_state_key_.empty());
  server_backed_state_key_hash_ =
      crypto::SHA256HashString(server_backed_state_key_);
}

AutoEnrollmentClient::~AutoEnrollmentClient() {
  net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
}

// static
void AutoEnrollmentClient::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kShouldAutoEnroll, false);
  registry->RegisterIntegerPref(prefs::kAutoEnrollmentPowerLimit, -1);
}

void AutoEnrollmentClient::Start() {
  // (Re-)register the network change observer.
  net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
  net::NetworkChangeNotifier::AddNetworkChangeObserver(this);

  // Drop the previous job and reset state.
  request_job_.reset();
  state_ = AUTO_ENROLLMENT_STATE_PENDING;
  time_start_ = base::Time::Now();
  modulus_updates_received_ = 0;
  has_server_state_ = false;
  device_state_available_ = false;

  NextStep();
}

void AutoEnrollmentClient::Retry() {
  RetryStep();
}

void AutoEnrollmentClient::CancelAndDeleteSoon() {
  if (time_start_.is_null() || !request_job_) {
    // The client isn't running, just delete it.
    delete this;
  } else {
    // Client still running, but our owner isn't interested in the result
    // anymore. Wait until the protocol completes to measure the extra time
    // needed.
    time_extra_start_ = base::Time::Now();
    progress_callback_.Reset();
  }
}

void AutoEnrollmentClient::OnNetworkChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  if (type != net::NetworkChangeNotifier::CONNECTION_NONE &&
      !progress_callback_.is_null()) {
    RetryStep();
  }
}

bool AutoEnrollmentClient::GetCachedDecision() {
  const PrefService::Preference* has_server_state_pref =
      local_state_->FindPreference(prefs::kShouldAutoEnroll);
  const PrefService::Preference* previous_limit_pref =
      local_state_->FindPreference(prefs::kAutoEnrollmentPowerLimit);
  bool has_server_state = false;
  int previous_limit = -1;

  if (!has_server_state_pref ||
      has_server_state_pref->IsDefaultValue() ||
      !has_server_state_pref->GetValue()->GetAsBoolean(&has_server_state) ||
      !previous_limit_pref ||
      previous_limit_pref->IsDefaultValue() ||
      !previous_limit_pref->GetValue()->GetAsInteger(&previous_limit) ||
      power_limit_ > previous_limit) {
    return false;
  }

  has_server_state_ = has_server_state;
  return true;
}

bool AutoEnrollmentClient::RetryStep() {
  // If there is a pending request job, let it finish.
  if (request_job_)
    return true;

  if (GetCachedDecision()) {
    // The bucket download check has completed already. If it came back
    // positive, then device state should be (re-)downloaded.
    if (has_server_state_) {
      if (!device_state_available_) {
        SendDeviceStateRequest();
        return true;
      }
    }
  } else {
    // Start bucket download.
    SendBucketDownloadRequest();
    return true;
  }

  return false;
}

void AutoEnrollmentClient::ReportProgress(AutoEnrollmentState state) {
  state_ = state;
  if (progress_callback_.is_null()) {
    base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
  } else {
    progress_callback_.Run(state_);
  }
}

void AutoEnrollmentClient::NextStep() {
  if (!RetryStep()) {
    // Protocol finished successfully, report result.
    const RestoreMode restore_mode = GetRestoreMode();
    bool trigger_enrollment =
        (restore_mode == RESTORE_MODE_REENROLLMENT_REQUESTED ||
         restore_mode == RESTORE_MODE_REENROLLMENT_ENFORCED);

    ReportProgress(trigger_enrollment ? AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT
                                      : AUTO_ENROLLMENT_STATE_NO_ENROLLMENT);
  }
}

void AutoEnrollmentClient::SendBucketDownloadRequest() {
  // Only power-of-2 moduli are supported for now. These are computed by taking
  // the lower |current_power_| bits of the hash.
  uint64_t remainder = 0;
  for (int i = 0; 8 * i < current_power_; ++i) {
    uint64_t byte = server_backed_state_key_hash_[31 - i] & 0xff;
    remainder = remainder | (byte << (8 * i));
  }
  remainder = remainder & ((UINT64_C(1) << current_power_) - 1);

  ReportProgress(AUTO_ENROLLMENT_STATE_PENDING);

  request_job_.reset(
      device_management_service_->CreateJob(
          DeviceManagementRequestJob::TYPE_AUTO_ENROLLMENT,
          request_context_.get()));
  request_job_->SetClientID(device_id_);
  em::DeviceAutoEnrollmentRequest* request =
      request_job_->GetRequest()->mutable_auto_enrollment_request();
  request->set_remainder(remainder);
  request->set_modulus(INT64_C(1) << current_power_);
  request_job_->Start(
      base::Bind(&AutoEnrollmentClient::HandleRequestCompletion,
                 base::Unretained(this),
                 &AutoEnrollmentClient::OnBucketDownloadRequestCompletion));
}

void AutoEnrollmentClient::SendDeviceStateRequest() {
  ReportProgress(AUTO_ENROLLMENT_STATE_PENDING);

  request_job_.reset(
      device_management_service_->CreateJob(
          DeviceManagementRequestJob::TYPE_DEVICE_STATE_RETRIEVAL,
          request_context_.get()));
  request_job_->SetClientID(device_id_);
  em::DeviceStateRetrievalRequest* request =
      request_job_->GetRequest()->mutable_device_state_retrieval_request();
  request->set_server_backed_state_key(server_backed_state_key_);
  request_job_->Start(
      base::Bind(&AutoEnrollmentClient::HandleRequestCompletion,
                 base::Unretained(this),
                 &AutoEnrollmentClient::OnDeviceStateRequestCompletion));
}

void AutoEnrollmentClient::HandleRequestCompletion(
    RequestCompletionHandler handler,
    DeviceManagementStatus status,
    int net_error,
    const em::DeviceManagementResponse& response) {
  UMA_HISTOGRAM_SPARSE_SLOWLY(kUMARequestStatus, status);
  if (status != DM_STATUS_SUCCESS) {
    LOG(ERROR) << "Auto enrollment error: " << status;
    if (status == DM_STATUS_REQUEST_FAILED)
      UMA_HISTOGRAM_SPARSE_SLOWLY(kUMANetworkErrorCode, -net_error);
    request_job_.reset();

    // Abort if CancelAndDeleteSoon has been called meanwhile.
    if (progress_callback_.is_null()) {
      base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
    } else {
      ReportProgress(status == DM_STATUS_REQUEST_FAILED
                         ? AUTO_ENROLLMENT_STATE_CONNECTION_ERROR
                         : AUTO_ENROLLMENT_STATE_SERVER_ERROR);
    }
    return;
  }

  bool progress = (this->*handler)(status, net_error, response);
  request_job_.reset();
  if (progress)
    NextStep();
  else
    ReportProgress(AUTO_ENROLLMENT_STATE_SERVER_ERROR);
}

bool AutoEnrollmentClient::OnBucketDownloadRequestCompletion(
    DeviceManagementStatus status,
    int net_error,
    const em::DeviceManagementResponse& response) {
  bool progress = false;
  const em::DeviceAutoEnrollmentResponse& enrollment_response =
      response.auto_enrollment_response();
  if (!response.has_auto_enrollment_response()) {
    LOG(ERROR) << "Server failed to provide auto-enrollment response.";
  } else if (enrollment_response.has_expected_modulus()) {
    // Server is asking us to retry with a different modulus.
    modulus_updates_received_++;

    int64_t modulus = enrollment_response.expected_modulus();
    int power = NextPowerOf2(modulus);
    if ((INT64_C(1) << power) != modulus) {
      LOG(ERROR) << "Auto enrollment: the server didn't ask for a power-of-2 "
                 << "modulus. Using the closest power-of-2 instead "
                 << "(" << modulus << " vs 2^" << power << ")";
    }
    if (modulus_updates_received_ >= 2) {
      LOG(ERROR) << "Auto enrollment error: already retried with an updated "
                 << "modulus but the server asked for a new one again: "
                 << power;
    } else if (power > power_limit_) {
      LOG(ERROR) << "Auto enrollment error: the server asked for a larger "
                 << "modulus than the client accepts (" << power << " vs "
                 << power_limit_ << ").";
    } else {
      // Retry at most once with the modulus that the server requested.
      if (power <= current_power_) {
        LOG(WARNING) << "Auto enrollment: the server asked to use a modulus ("
                     << power << ") that isn't larger than the first used ("
                     << current_power_ << "). Retrying anyway.";
      }
      // Remember this value, so that eventual retries start with the correct
      // modulus.
      current_power_ = power;
      return true;
    }
  } else {
    // Server should have sent down a list of hashes to try.
    has_server_state_ = IsIdHashInProtobuf(enrollment_response.hash());
    // Cache the current decision in local_state, so that it is reused in case
    // the device reboots before enrolling.
    local_state_->SetBoolean(prefs::kShouldAutoEnroll, has_server_state_);
    local_state_->SetInteger(prefs::kAutoEnrollmentPowerLimit, power_limit_);
    local_state_->CommitPendingWrite();
    VLOG(1) << "Auto enrollment check complete, has_server_state_ = "
            << has_server_state_;
    progress = true;
  }

  // Bucket download done, update UMA.
  UpdateBucketDownloadTimingHistograms();
  return progress;
}

bool AutoEnrollmentClient::OnDeviceStateRequestCompletion(
    DeviceManagementStatus status,
    int net_error,
    const enterprise_management::DeviceManagementResponse& response) {
  bool progress = false;
  if (!response.has_device_state_retrieval_response()) {
    LOG(ERROR) << "Server failed to provide auto-enrollment response.";
  } else {
    const em::DeviceStateRetrievalResponse& state_response =
        response.device_state_retrieval_response();
    {
      DictionaryPrefUpdate dict(local_state_, prefs::kServerBackedDeviceState);
      UpdateDict(dict.Get(),
                 kDeviceStateManagementDomain,
                 state_response.has_management_domain(),
                 new base::StringValue(state_response.management_domain()));

      std::string restore_mode =
          ConvertRestoreMode(state_response.restore_mode());
      UpdateDict(dict.Get(),
                 kDeviceStateRestoreMode,
                 !restore_mode.empty(),
                 new base::StringValue(restore_mode));

      UpdateDict(dict.Get(),
                 kDeviceStateDisabledMessage,
                 state_response.has_disabled_state(),
                 new base::StringValue(
                     state_response.disabled_state().message()));
    }
    local_state_->CommitPendingWrite();
    device_state_available_ = true;
    progress = true;
  }

  return progress;
}

bool AutoEnrollmentClient::IsIdHashInProtobuf(
      const google::protobuf::RepeatedPtrField<std::string>& hashes) {
  for (int i = 0; i < hashes.size(); ++i) {
    if (hashes.Get(i) == server_backed_state_key_hash_)
      return true;
  }
  return false;
}

void AutoEnrollmentClient::UpdateBucketDownloadTimingHistograms() {
  // The minimum time can't be 0, must be at least 1.
  static const base::TimeDelta kMin = base::TimeDelta::FromMilliseconds(1);
  static const base::TimeDelta kMax = base::TimeDelta::FromMinutes(5);
  // However, 0 can still be sampled.
  static const base::TimeDelta kZero = base::TimeDelta::FromMilliseconds(0);
  static const int kBuckets = 50;

  base::Time now = base::Time::Now();
  if (!time_start_.is_null()) {
    base::TimeDelta delta = now - time_start_;
    UMA_HISTOGRAM_CUSTOM_TIMES(kUMAProtocolTime, delta, kMin, kMax, kBuckets);
  }
  base::TimeDelta delta = kZero;
  if (!time_extra_start_.is_null())
    delta = now - time_extra_start_;
  // This samples |kZero| when there was no need for extra time, so that we can
  // measure the ratio of users that succeeded without needing a delay to the
  // total users going through OOBE.
  UMA_HISTOGRAM_CUSTOM_TIMES(kUMAExtraTime, delta, kMin, kMax, kBuckets);
}

}  // namespace policy
