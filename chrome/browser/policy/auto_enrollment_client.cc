// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/auto_enrollment_client.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/policy/device_management_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/guid.h"
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
    if ((1 << i) >= value)
      return i;
  }
  // No other value can be represented in an int64.
  return kMaximumPower + 1;
}

}  // namespace

namespace policy {

AutoEnrollmentClient::AutoEnrollmentClient(const base::Closure& callback,
                                           DeviceManagementService* service,
                                           const std::string& serial_number,
                                           int power_initial,
                                           int power_limit)
    : completion_callback_(callback),
      should_auto_enroll_(false),
      device_id_(guid::GenerateGUID()),
      power_initial_(power_initial),
      power_limit_(power_limit),
      device_management_service_(service) {
  DCHECK_LE(power_initial_, power_limit_);
  if (!serial_number.empty())
    serial_number_hash_ = crypto::SHA256HashString(serial_number);
}

AutoEnrollmentClient::~AutoEnrollmentClient() {}

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
  CommandLine* command_line = CommandLine::ForCurrentProcess();

  // The client won't do anything if |service| is NULL.
  DeviceManagementService* service = NULL;
  if (IsDisabled()) {
    VLOG(1) << "Auto-enrollment is disabled";
  } else {
    std::string url =
        command_line->GetSwitchValueASCII(switches::kDeviceManagementUrl);
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

  return new AutoEnrollmentClient(completion_callback,
                                  service,
                                  BrowserPolicyConnector::GetSerialNumber(),
                                  power_initial,
                                  power_limit);
}

void AutoEnrollmentClient::Start() {
  // Drop the previous job and reset state.
  request_job_.reset();
  should_auto_enroll_ = false;

  if (device_management_service_.get()) {
    if (serial_number_hash_.empty()) {
      LOG(ERROR) << "Failed to get the hash of the serial number, "
                 << "will not attempt to auto-enroll.";
    } else {
      SendRequest(power_initial_);
      // Don't invoke the callback now.
      return;
    }
  }

  // Auto-enrollment can't even start, so we're done.
  completion_callback_.Run();
}

void AutoEnrollmentClient::SendRequest(int power) {
  if (power < 0 || power > power_limit_ || serial_number_hash_.empty()) {
    NOTREACHED();
    completion_callback_.Run();
    return;
  }

  last_power_used_ = power;

  // Only power-of-2 moduli are supported for now. These are computed by taking
  // the lower |power| bits of the hash.
  uint64 remainder = 0;
  for (int i = 0; 8 * i < power; ++i) {
    uint64 byte = serial_number_hash_[31 - i] & 0xff;
    remainder = remainder | (byte << (8 * i));
  }
  remainder = remainder & ((1ULL << power) - 1);

  request_job_.reset(
      device_management_service_->CreateJob(
          DeviceManagementRequestJob::TYPE_AUTO_ENROLLMENT));
  request_job_->SetClientID(device_id_);
  em::DeviceAutoEnrollmentRequest* request =
      request_job_->GetRequest()->mutable_auto_enrollment_request();
  request->set_remainder(remainder);
  request->set_modulus((int64) 1 << power);
  request_job_->Start(base::Bind(&AutoEnrollmentClient::OnRequestCompletion,
                                 base::Unretained(this)));
}

void AutoEnrollmentClient::OnRequestCompletion(
    DeviceManagementStatus status,
    const em::DeviceManagementResponse& response) {
  if (status != DM_STATUS_SUCCESS || !response.has_auto_enrollment_response()) {
    LOG(ERROR) << "Auto enrollment error: " << status;
    completion_callback_.Run();
    return;
  }

  const em::DeviceAutoEnrollmentResponse& enrollment_response =
      response.auto_enrollment_response();
  if (enrollment_response.has_expected_modulus()) {
    // Server is asking us to retry with a different modulus.
    int64 modulus = enrollment_response.expected_modulus();
    int64 last_modulus_used = 1 << last_power_used_;
    int power = NextPowerOf2(modulus);
    if ((1 << power) != modulus) {
      LOG(WARNING) << "Auto enrollment: the server didn't ask for a power-of-2 "
                   << "modulus. Using the closest power-of-2 instead "
                   << "(" << modulus << " vs 2^" << power << ")";
    }
    if (modulus < last_modulus_used) {
      LOG(ERROR) << "Auto enrollment error: the server asked for a smaller "
                 << "modulus than we used.";
    } else if (modulus == last_modulus_used) {
      LOG(ERROR) << "Auto enrollment error: the server asked for the same "
                 << "modulus that was already used.";
    } else if (last_power_used_ != power_initial_) {
      LOG(ERROR) << "Auto enrollment error: already retried with an updated "
                 << "modulus but the server asked for a new one.";
    } else if (power > power_limit_) {
      LOG(ERROR) << "Auto enrollment error: the server asked for a larger "
                 << "modulus than the client accepts (" << power << " vs "
                 << power_limit_ << ").";
    } else {
      // Retry with a larger modulus.
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
  completion_callback_.Run();
}

bool AutoEnrollmentClient::IsSerialInProtobuf(
      const google::protobuf::RepeatedPtrField<std::string>& hashes) {
  for (int i = 0; i < hashes.size(); ++i) {
    if (hashes.Get(i) == serial_number_hash_)
      return true;
  }
  return false;
}

}  // namespace policy
