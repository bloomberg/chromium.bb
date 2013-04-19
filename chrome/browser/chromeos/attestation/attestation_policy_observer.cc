// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/attestation/attestation_policy_observer.h"

#include <string>

#include "base/bind.h"
#include "chrome/browser/chromeos/attestation/attestation_ca_client.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/policy/cloud/cloud_policy_client.h"
#include "chrome/browser/policy/cloud/cloud_policy_manager.h"
#include "chrome/common/chrome_notification_types.h"
#include "chromeos/attestation/attestation_flow.h"
#include "chromeos/cryptohome/async_method_caller.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"

namespace {

// A dbus callback which handles a boolean result.
//
// Parameters
//   on_true - Called when status=success and value=true.
//   on_false - Called when status=success and value=false.
//   status - The dbus operation status.
//   value - The value returned by the dbus operation.
void DBusBoolRedirectCallback(const base::Closure& on_true,
                              const base::Closure& on_false,
                              chromeos::DBusMethodCallStatus status,
                              bool value) {
  if (status != chromeos::DBUS_METHOD_CALL_SUCCESS)
    return;
  const base::Closure& task = value ? on_true : on_false;
  if (!task.is_null())
    task.Run();
}

// A dbus callback which handles a string result.
//
// Parameters
//   on_success - Called when status=success and result=true.
//   status - The dbus operation status.
//   result - The result returned by the dbus operation.
//   data - The data returned by the dbus operation.
void DBusStringCallback(
    const base::Callback<void(const std::string&)> on_success,
    chromeos::DBusMethodCallStatus status,
    bool result,
    const std::string& data) {
  if (status != chromeos::DBUS_METHOD_CALL_SUCCESS || !result)
    return;
  on_success.Run(data);
}

}  // namespace

namespace chromeos {
namespace attestation {

const char AttestationPolicyObserver::kEnterpriseMachineKey[] =
    "attest-ent-machine";

AttestationPolicyObserver::AttestationPolicyObserver(
    policy::CloudPolicyClient* policy_client)
    : cros_settings_(CrosSettings::Get()),
      policy_client_(policy_client),
      cryptohome_client_(NULL),
      attestation_flow_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  cros_settings_->AddSettingsObserver(kDeviceAttestationEnabled, this);
  Start();
}

AttestationPolicyObserver::AttestationPolicyObserver(
    policy::CloudPolicyClient* policy_client,
    CryptohomeClient* cryptohome_client,
    AttestationFlow* attestation_flow)
    : cros_settings_(CrosSettings::Get()),
      policy_client_(policy_client),
      cryptohome_client_(cryptohome_client),
      attestation_flow_(attestation_flow),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  cros_settings_->AddSettingsObserver(kDeviceAttestationEnabled, this);
  Start();
}

AttestationPolicyObserver::~AttestationPolicyObserver() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  cros_settings_->RemoveSettingsObserver(kDeviceAttestationEnabled, this);
}

void AttestationPolicyObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  std::string* path = content::Details<std::string>(details).ptr();
  if (type != chrome::NOTIFICATION_SYSTEM_SETTING_CHANGED ||
      *path != kDeviceAttestationEnabled) {
    LOG(WARNING) << "AttestationPolicyObserver: Unexpected event received.";
    return;
  }
  Start();
}

void AttestationPolicyObserver::Start() {
  // If attestation is not enabled, there is nothing to do.
  bool enabled = false;
  if (!cros_settings_->GetBoolean(kDeviceAttestationEnabled, &enabled) ||
      !enabled)
    return;

  // We expect a registered CloudPolicyClient.
  if (!policy_client_->is_registered()) {
    LOG(ERROR) << "AttestationPolicyObserver: Invalid CloudPolicyClient.";
    return;
  }

  if (!cryptohome_client_)
    cryptohome_client_ = DBusThreadManager::Get()->GetCryptohomeClient();

  if (!attestation_flow_) {
    scoped_ptr<ServerProxy> attestation_ca_client(new AttestationCAClient());
    default_attestation_flow_.reset(new AttestationFlow(
        cryptohome::AsyncMethodCaller::GetInstance(),
        cryptohome_client_,
        attestation_ca_client.Pass()));
    attestation_flow_ = default_attestation_flow_.get();
  }

  // Start a dbus call to check if an Enterprise Machine Key already exists.
  base::Closure on_does_exist =
      base::Bind(&AttestationPolicyObserver::GetExistingCertificate,
                 weak_factory_.GetWeakPtr());
  base::Closure on_does_not_exist =
      base::Bind(&AttestationPolicyObserver::GetNewCertificate,
                 weak_factory_.GetWeakPtr());
  cryptohome_client_->TpmAttestationDoesKeyExist(
      CryptohomeClient::DEVICE_KEY,
      kEnterpriseMachineKey,
      base::Bind(DBusBoolRedirectCallback, on_does_exist, on_does_not_exist));
}

void AttestationPolicyObserver::GetNewCertificate() {
  // We can reuse the dbus callback handler logic.
  attestation_flow_->GetCertificate(
      kEnterpriseMachineKey,
      base::Bind(DBusStringCallback,
                 base::Bind(&AttestationPolicyObserver::UploadCertificate,
                            weak_factory_.GetWeakPtr()),
                 DBUS_METHOD_CALL_SUCCESS));
}

void AttestationPolicyObserver::GetExistingCertificate() {
  cryptohome_client_->TpmAttestationGetCertificate(
      CryptohomeClient::DEVICE_KEY,
      kEnterpriseMachineKey,
      base::Bind(DBusStringCallback,
                 base::Bind(&AttestationPolicyObserver::CheckCertificateExpiry,
                            weak_factory_.GetWeakPtr())));
}

void AttestationPolicyObserver::CheckCertificateExpiry(
    const std::string& certificate) {
  // TODO(dkrahn): Check if the certificate will expire soon, for now assume no.
  CheckIfUploaded(certificate);
}

void AttestationPolicyObserver::UploadCertificate(
    const std::string& certificate) {
  // TODO(dkrahn): Upload the certificate.
}

void AttestationPolicyObserver::CheckIfUploaded(
    const std::string& certificate) {
  // TODO(dkrahn): Check if we've already uploaded the certificate.
}

}  // namespace attestation
}  // namespace chromeos
