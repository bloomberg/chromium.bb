// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/auto_enrollment_client.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/guid.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/policy/cloud/device_management_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "crypto/sha2.h"

namespace em = enterprise_management;

namespace {

// The modulus value is sent in an int64 field in the protobuf, whose maximum
// value is 2^63-1. So 2^64 and 2^63 can't be represented as moduli and the
// max is 2^62 (when the moduli are restricted to powers-of-2).
const int kMaximumPower = 62;

// Returns the int value of the |switch_name| argument, clamped to the [0, 62]
// interval. Returns 0 if the argument doesn't exist or isn't an int value.
int GetSanitizedArg(const std::string& switch_name) {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switch_name))
    return 0;
  std::string value = command_line->GetSwitchValueASCII(switch_name);
  int int_value;
  if (!base::StringToInt(value, &int_value)) {
    LOG(ERROR) << "Switch \"" << switch_name << "\" is not a valid int. "
               << "Defaulting to 0.";
    return 0;
  }
  if (int_value < 0) {
    LOG(ERROR) << "Switch \"" << switch_name << "\" can't be negative. "
               << "Using 0";
    return 0;
  }
  if (int_value > kMaximumPower) {
    LOG(ERROR) << "Switch \"" << switch_name << "\" can't be greater than "
               << kMaximumPower << ". Using " << kMaximumPower;
    return kMaximumPower;
  }
  return int_value;
}

// Returns the power of the next power-of-2 starting at |value|.
int NextPowerOf2(int64 value) {
  for (int i = 0; i <= kMaximumPower; ++i) {
    if ((GG_INT64_C(1) << i) >= value)
      return i;
  }
  // No other value can be represented in an int64.
  return kMaximumPower + 1;
}

}  // namespace

namespace policy {

AutoEnrollmentClient::AutoEnrollmentClient(const base::Closure& callback,
                                           DeviceManagementService* service,
                                           PrefService* local_state,
                                           const std::string& serial_number,
                                           int power_initial,
                                           int power_limit)
    : completion_callback_(callback),
      should_auto_enroll_(false),
      device_id_(base::GenerateGUID()),
      power_initial_(power_initial),
      power_limit_(power_limit),
      requests_sent_(0),
      device_management_service_(service),
      local_state_(local_state) {
  DCHECK_LE(power_initial_, power_limit_);
  if (!serial_number.empty())
    serial_number_hash_ = crypto::SHA256HashString(serial_number);
}

AutoEnrollmentClient::~AutoEnrollmentClient() {}

// static
void AutoEnrollmentClient::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kShouldAutoEnroll, false);
  registry->RegisterIntegerPref(prefs::kAutoEnrollmentPowerLimit, -1);
}

// static
bool AutoEnrollmentClient::IsDisabled() {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  return
      !command_line->HasSwitch(switches::kEnterpriseEnrollmentInitialModulus) &&
      !command_line->HasSwitch(switches::kEnterpriseEnrollmentModulusLimit);
}

// static
AutoEnrollmentClient* AutoEnrollmentClient::Create(
    const base::Closure& completion_callback) {
  // The client won't do anything if |service| is NULL.
  DeviceManagementService* service = NULL;
  if (IsDisabled()) {
    VLOG(1) << "Auto-enrollment is disabled";
  } else {
    std::string url = BrowserPolicyConnector::GetDeviceManagementUrl();
    if (!url.empty()) {
      service = new DeviceManagementService(url);
      service->ScheduleInitialization(0);
    }
  }

  int power_initial = GetSanitizedArg(
      switches::kEnterpriseEnrollmentInitialModulus);
  int power_limit = GetSanitizedArg(
      switches::kEnterpriseEnrollmentModulusLimit);
  if (power_initial > power_limit) {
    LOG(ERROR) << "Initial auto-enrollment modulus is larger than the limit, "
               << "clamping to the limit.";
    power_initial = power_limit;
  }

  return new AutoEnrollmentClient(
      completion_callback,
      service,
      g_browser_process->local_state(),
      DeviceCloudPolicyManagerChromeOS::GetMachineID(),
      power_initial,
      power_limit);
}

// static
void AutoEnrollmentClient::CancelAutoEnrollment() {
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetBoolean(prefs::kShouldAutoEnroll, false);
  local_state->CommitPendingWrite();
}

void AutoEnrollmentClient::Start() {
  // Drop the previous job and reset state.
  request_job_.reset();
  should_auto_enroll_ = false;
  time_start_ = base::Time();  // reset to null.

  if (GetCachedDecision()) {
    VLOG(1) << "AutoEnrollmentClient: using cached decision: "
            << should_auto_enroll_;
  } else if (device_management_service_.get()) {
    if (serial_number_hash_.empty()) {
      LOG(ERROR) << "Failed to get the hash of the serial number, "
                 << "will not attempt to auto-enroll.";
    } else {
      time_start_ = base::Time::Now();
      SendRequest(power_initial_);
      // Don't invoke the callback now.
      return;
    }
  }

  // Auto-enrollment can't even start, so we're done.
  OnProtocolDone();
}

void AutoEnrollmentClient::CancelAndDeleteSoon() {
  if (time_start_.is_null()) {
    // The client isn't running, just delete it.
    delete this;
  } else {
    // Client still running, but our owner isn't interested in the result
    // anymore. Wait until the protocol completes to measure the extra time
    // needed.
    time_extra_start_ = base::Time::Now();
    completion_callback_.Reset();
  }
}

bool AutoEnrollmentClient::GetCachedDecision() {
  const PrefService::Preference* should_enroll_pref =
      local_state_->FindPreference(prefs::kShouldAutoEnroll);
  const PrefService::Preference* previous_limit_pref =
      local_state_->FindPreference(prefs::kAutoEnrollmentPowerLimit);
  bool should_auto_enroll = false;
  int previous_limit = -1;

  if (!should_enroll_pref ||
      should_enroll_pref->IsDefaultValue() ||
      !should_enroll_pref->GetValue()->GetAsBoolean(&should_auto_enroll) ||
      !previous_limit_pref ||
      previous_limit_pref->IsDefaultValue() ||
      !previous_limit_pref->GetValue()->GetAsInteger(&previous_limit) ||
      power_limit_ > previous_limit) {
    return false;
  }

  should_auto_enroll_ = should_auto_enroll;
  return true;
}

void AutoEnrollmentClient::SendRequest(int power) {
  if (power < 0 || power > power_limit_ || serial_number_hash_.empty()) {
    NOTREACHED();
    OnProtocolDone();
    return;
  }

  requests_sent_++;

  // Only power-of-2 moduli are supported for now. These are computed by taking
  // the lower |power| bits of the hash.
  uint64 remainder = 0;
  for (int i = 0; 8 * i < power; ++i) {
    uint64 byte = serial_number_hash_[31 - i] & 0xff;
    remainder = remainder | (byte << (8 * i));
  }
  remainder = remainder & ((GG_UINT64_C(1) << power) - 1);

  request_job_.reset(
      device_management_service_->CreateJob(
          DeviceManagementRequestJob::TYPE_AUTO_ENROLLMENT));
  request_job_->SetClientID(device_id_);
  em::DeviceAutoEnrollmentRequest* request =
      request_job_->GetRequest()->mutable_auto_enrollment_request();
  request->set_remainder(remainder);
  request->set_modulus(GG_INT64_C(1) << power);
  request_job_->Start(base::Bind(&AutoEnrollmentClient::OnRequestCompletion,
                                 base::Unretained(this)));
}

void AutoEnrollmentClient::OnRequestCompletion(
    DeviceManagementStatus status,
    const em::DeviceManagementResponse& response) {
  if (status != DM_STATUS_SUCCESS || !response.has_auto_enrollment_response()) {
    LOG(ERROR) << "Auto enrollment error: " << status;
    OnProtocolDone();
    return;
  }

  const em::DeviceAutoEnrollmentResponse& enrollment_response =
      response.auto_enrollment_response();
  if (enrollment_response.has_expected_modulus()) {
    // Server is asking us to retry with a different modulus.
    int64 modulus = enrollment_response.expected_modulus();
    int power = NextPowerOf2(modulus);
    if ((GG_INT64_C(1) << power) != modulus) {
      LOG(WARNING) << "Auto enrollment: the server didn't ask for a power-of-2 "
                   << "modulus. Using the closest power-of-2 instead "
                   << "(" << modulus << " vs 2^" << power << ")";
    }
    if (requests_sent_ >= 2) {
      LOG(ERROR) << "Auto enrollment error: already retried with an updated "
                 << "modulus but the server asked for a new one again: "
                 << power;
    } else if (power > power_limit_) {
      LOG(ERROR) << "Auto enrollment error: the server asked for a larger "
                 << "modulus than the client accepts (" << power << " vs "
                 << power_limit_ << ").";
    } else {
      // Retry at most once with the modulus that the server requested.
      if (power <= power_initial_) {
        LOG(WARNING) << "Auto enrollment: the server asked to use a modulus ("
                     << power << ") that isn't larger than the first used ("
                     << power_initial_ << "). Retrying anyway.";
      }
      SendRequest(power);
      // Don't invoke the callback yet.
      return;
    }
  } else {
    // Server should have sent down a list of hashes to try.
    should_auto_enroll_ = IsSerialInProtobuf(enrollment_response.hash());
    LOG(INFO) << "Auto enrollment complete, should_auto_enroll = "
              << should_auto_enroll_;
  }

  // Auto-enrollment done.
  OnProtocolDone();
}

bool AutoEnrollmentClient::IsSerialInProtobuf(
      const google::protobuf::RepeatedPtrField<std::string>& hashes) {
  for (int i = 0; i < hashes.size(); ++i) {
    if (hashes.Get(i) == serial_number_hash_)
      return true;
  }
  return false;
}

void AutoEnrollmentClient::OnProtocolDone() {
  static const char* kProtocolTime = "Enterprise.AutoEnrollmentProtocolTime";
  static const char* kExtraTime = "Enterprise.AutoEnrollmentExtraTime";
  // The mininum time can't be 0, must be at least 1.
  static const base::TimeDelta kMin = base::TimeDelta::FromMilliseconds(1);
  static const base::TimeDelta kMax = base::TimeDelta::FromMinutes(5);
  // However, 0 can still be sampled.
  static const base::TimeDelta kZero = base::TimeDelta::FromMilliseconds(0);
  static const int kBuckets = 50;

  base::Time now = base::Time::Now();
  if (!time_start_.is_null()) {
    base::TimeDelta delta = now - time_start_;
    UMA_HISTOGRAM_CUSTOM_TIMES(kProtocolTime, delta, kMin, kMax, kBuckets);
    time_start_ = base::Time();

    // The decision is cached only if the protocol was actually started, which
    // is the case only if |time_start_| was not null.
    local_state_->SetBoolean(prefs::kShouldAutoEnroll, should_auto_enroll_);
    local_state_->SetInteger(prefs::kAutoEnrollmentPowerLimit, power_limit_);
    local_state_->CommitPendingWrite();
  }
  base::TimeDelta delta = kZero;
  if (!time_extra_start_.is_null()) {
    // CancelAndDeleteSoon() was invoked before.
    delta = now - time_extra_start_;
    base::MessageLoopProxy::current()->DeleteSoon(FROM_HERE, this);
    time_extra_start_ = base::Time();
  }
  // This samples |kZero| when there was no need for extra time, so that we can
  // measure the ratio of users that succeeded without needing a delay to the
  // total users going through OOBE.
  UMA_HISTOGRAM_CUSTOM_TIMES(kExtraTime, delta, kMin, kMax, kBuckets);

  if (!completion_callback_.is_null())
    completion_callback_.Run();
}

}  // namespace policy
