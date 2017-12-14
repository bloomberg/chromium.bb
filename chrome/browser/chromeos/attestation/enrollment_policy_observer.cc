// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/attestation/enrollment_policy_observer.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/attestation/attestation_ca_client.h"
#include "chrome/browser/chromeos/attestation/attestation_key_payload.pb.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chromeos/attestation/attestation_flow.h"
#include "chromeos/cryptohome/async_method_caller.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/user_manager/known_user.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "net/cert/pem_tokenizer.h"
#include "net/cert/x509_certificate.h"

namespace {

const int kRetryDelay = 5;  // Seconds.
const int kRetryLimit = 100;

// A dbus callback which handles a string result.
//
// Parameters
//   on_success - Called when status=success and result=true.
//   status - The dbus operation status.
//   result - The result returned by the dbus operation.
//   data - The data returned by the dbus operation.
void DBusStringCallback(
    const base::Callback<void(const std::string&)> on_success,
    const base::Closure& on_failure,
    const base::Location& from_here,
    const chromeos::CryptohomeClient::TpmAttestationDataResult& result) {
  if (!result.success) {
    LOG(ERROR) << "Cryptohome DBus method failed: " << from_here.ToString();
    if (!on_failure.is_null())
      on_failure.Run();
    return;
  }
  on_success.Run(result.data);
}

}  // namespace

namespace chromeos {
namespace attestation {

EnrollmentPolicyObserver::EnrollmentPolicyObserver(
    policy::CloudPolicyClient* policy_client)
    : cros_settings_(CrosSettings::Get()),
      policy_client_(policy_client),
      cryptohome_client_(NULL),
      attestation_flow_(NULL),
      num_retries_(0),
      retry_delay_(kRetryDelay),
      weak_factory_(this) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  attestation_subscription_ = cros_settings_->AddSettingsObserver(
      kDeviceEnrollmentIdNeeded,
      base::Bind(&EnrollmentPolicyObserver::EnrollmentSettingChanged,
                 base::Unretained(this)));
  Start();
}

EnrollmentPolicyObserver::EnrollmentPolicyObserver(
    policy::CloudPolicyClient* policy_client,
    CryptohomeClient* cryptohome_client,
    AttestationFlow* attestation_flow)
    : cros_settings_(CrosSettings::Get()),
      policy_client_(policy_client),
      cryptohome_client_(cryptohome_client),
      attestation_flow_(attestation_flow),
      num_retries_(0),
      retry_delay_(kRetryDelay),
      weak_factory_(this) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  attestation_subscription_ = cros_settings_->AddSettingsObserver(
      kDeviceEnrollmentIdNeeded,
      base::Bind(&EnrollmentPolicyObserver::EnrollmentSettingChanged,
                 base::Unretained(this)));
  Start();
}

EnrollmentPolicyObserver::~EnrollmentPolicyObserver() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void EnrollmentPolicyObserver::EnrollmentSettingChanged() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  num_retries_ = 0;
  Start();
}

void EnrollmentPolicyObserver::Start() {
  // If we identification for enrollment isn't needed, there is nothing to do.
  bool needed = false;
  if (!cros_settings_->GetBoolean(kDeviceEnrollmentIdNeeded, &needed) ||
      !needed)
    return;

  // We expect a registered CloudPolicyClient.
  if (!policy_client_->is_registered()) {
    LOG(ERROR) << "EnrollmentPolicyObserver: Invalid CloudPolicyClient.";
    return;
  }

  if (!cryptohome_client_)
    cryptohome_client_ = DBusThreadManager::Get()->GetCryptohomeClient();

  if (!attestation_flow_) {
    std::unique_ptr<ServerProxy> attestation_ca_client(
        new AttestationCAClient());
    default_attestation_flow_.reset(new AttestationFlow(
        cryptohome::AsyncMethodCaller::GetInstance(), cryptohome_client_,
        std::move(attestation_ca_client)));
    attestation_flow_ = default_attestation_flow_.get();
  }

  GetEnrollmentCertificate();
}

void EnrollmentPolicyObserver::GetEnrollmentCertificate() {
  // We can reuse the dbus callback handler logic.
  attestation_flow_->GetCertificate(
      PROFILE_ENTERPRISE_ENROLLMENT_CERTIFICATE,
      EmptyAccountId(),  // Not used.
      std::string(),     // Not used.
      false,             // Not used.
      base::Bind(
          [](const base::Callback<void(const std::string&)> on_success,
             const base::Closure& on_failure, const base::Location& from_here,
             bool success, const std::string& data) {
            DBusStringCallback(on_success, on_failure, from_here,
                               CryptohomeClient::TpmAttestationDataResult{
                                   success, std::move(data)});
          },
          base::Bind(&EnrollmentPolicyObserver::UploadCertificate,
                     weak_factory_.GetWeakPtr()),
          base::Bind(&EnrollmentPolicyObserver::Reschedule,
                     weak_factory_.GetWeakPtr()),
          FROM_HERE));
}

void EnrollmentPolicyObserver::UploadCertificate(
    const std::string& pem_certificate_chain) {
  policy_client_->UploadEnterpriseEnrollmentCertificate(
      pem_certificate_chain,
      base::Bind(&EnrollmentPolicyObserver::OnUploadComplete,
                 weak_factory_.GetWeakPtr()));
}

void EnrollmentPolicyObserver::OnUploadComplete(bool status) {
  if (!status)
    return;
  VLOG(1) << "Enterprise Enrollment Certificate uploaded to DMServer.";
  cros_settings_->SetBoolean(kDeviceEnrollmentIdNeeded, false);
}

void EnrollmentPolicyObserver::Reschedule() {
  if (++num_retries_ < kRetryLimit) {
    content::BrowserThread::PostDelayedTask(
        content::BrowserThread::UI, FROM_HERE,
        base::BindOnce(&EnrollmentPolicyObserver::Start,
                       weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromSeconds(retry_delay_));
  } else {
    LOG(WARNING) << "EnrollmentPolicyObserver: Retry limit exceeded.";
  }
}

}  // namespace attestation
}  // namespace chromeos
