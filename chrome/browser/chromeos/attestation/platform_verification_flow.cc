// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform_verification_flow.h"

#include "base/logging.h"
#include "chrome/browser/chromeos/attestation/attestation_ca_client.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chromeos/attestation/attestation_flow.h"
#include "chromeos/cryptohome/async_method_caller.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

namespace {
// A callback method to handle DBus errors.
void DBusCallback(const base::Callback<void(bool)>& on_success,
                  const base::Closure& on_failure,
                  chromeos::DBusMethodCallStatus call_status,
                  bool result) {
  if (call_status == chromeos::DBUS_METHOD_CALL_SUCCESS) {
    on_success.Run(result);
  } else {
    LOG(ERROR) << "PlatformVerificationFlow: DBus call failed!";
    on_failure.Run();
  }
}
}  // namespace

namespace chromeos {
namespace attestation {

PlatformVerificationFlow::PlatformVerificationFlow(
    AttestationFlow* attestation_flow,
    cryptohome::AsyncMethodCaller* async_caller,
    CryptohomeClient* cryptohome_client,
    UserManager* user_manager,
    Delegate* delegate)
    : attestation_flow_(attestation_flow),
      async_caller_(async_caller),
      cryptohome_client_(cryptohome_client),
      user_manager_(user_manager),
      delegate_(delegate),
      weak_factory_(this) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
}

PlatformVerificationFlow::~PlatformVerificationFlow() {
}

void PlatformVerificationFlow::ChallengePlatformKey(
    content::WebContents* web_contents,
    const std::string& service_id,
    const std::string& challenge,
    const ChallengeCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (delegate_->IsAttestationDisabled()) {
    LOG(INFO) << "PlatformVerificationFlow: Feature disabled.";
    callback.Run(POLICY_REJECTED, std::string(), std::string());
    return;
  }
  BoolDBusMethodCallback dbus_callback = base::Bind(
      &DBusCallback,
      base::Bind(&PlatformVerificationFlow::CheckConsent,
                 weak_factory_.GetWeakPtr(),
                 web_contents,
                 service_id,
                 challenge,
                 callback),
      base::Bind(callback, INTERNAL_ERROR, std::string(), std::string()));
  cryptohome_client_->TpmAttestationIsEnrolled(dbus_callback);
}

void PlatformVerificationFlow::CheckConsent(content::WebContents* web_contents,
                                            const std::string& service_id,
                                            const std::string& challenge,
                                            const ChallengeCallback& callback,
                                            bool attestation_enrolled) {
  ConsentType consent_type = CONSENT_TYPE_NONE;
  if (!attestation_enrolled) {
    consent_type = CONSENT_TYPE_ATTESTATION;
  } else if (delegate_->IsOriginConsentRequired(web_contents)) {
    consent_type = CONSENT_TYPE_ORIGIN;
  } else if (delegate_->IsAlwaysAskRequired(web_contents)) {
    consent_type = CONSENT_TYPE_ALWAYS;
  }
  Delegate::ConsentCallback consent_callback = base::Bind(
      &PlatformVerificationFlow::OnConsentResponse,
      weak_factory_.GetWeakPtr(),
      web_contents,
      service_id,
      challenge,
      callback,
      consent_type);
  if (consent_type == CONSENT_TYPE_NONE) {
    consent_callback.Run(CONSENT_RESPONSE_NONE);
  } else {
    delegate_->ShowConsentPrompt(consent_type,
                                 web_contents,
                                 consent_callback);
  }
}

void PlatformVerificationFlow::OnConsentResponse(
    content::WebContents* web_contents,
    const std::string& service_id,
    const std::string& challenge,
    const ChallengeCallback& callback,
    ConsentType consent_type,
    ConsentResponse consent_response) {
  if (consent_type != CONSENT_TYPE_NONE) {
    if (consent_response == CONSENT_RESPONSE_NONE) {
      // No user response - do not proceed and do not modify any settings.
      LOG(WARNING) << "PlatformVerificationFlow: No response from user.";
      callback.Run(USER_REJECTED, std::string(), std::string());
      return;
    }
    if (!delegate_->UpdateSettings(web_contents,
                                   consent_type,
                                   consent_response)) {
      callback.Run(INTERNAL_ERROR, std::string(), std::string());
      return;
    }
    if (consent_response == CONSENT_RESPONSE_DENY) {
      LOG(INFO) << "PlatformVerificationFlow: User rejected request.";
      callback.Run(USER_REJECTED, std::string(), std::string());
      return;
    }
  }

  // At this point all user interaction is complete and we can proceed with the
  // certificate request.
  AttestationFlow::CertificateCallback certificate_callback = base::Bind(
      &PlatformVerificationFlow::OnCertificateReady,
      weak_factory_.GetWeakPtr(),
      service_id,
      challenge,
      callback);
  attestation_flow_->GetCertificate(
      PROFILE_CONTENT_PROTECTION_CERTIFICATE,
      user_manager_->GetActiveUser()->email(),
      service_id,
      false,  // Don't force a new key.
      certificate_callback);
}

void PlatformVerificationFlow::OnCertificateReady(
    const std::string& service_id,
    const std::string& challenge,
    const ChallengeCallback& callback,
    bool operation_success,
    const std::string& certificate) {
  if (!operation_success) {
    LOG(WARNING) << "PlatformVerificationFlow: Failed to certify platform.";
    callback.Run(PLATFORM_NOT_VERIFIED, std::string(), std::string());
    return;
  }
  cryptohome::AsyncMethodCaller::DataCallback cryptohome_callback = base::Bind(
      &PlatformVerificationFlow::OnChallengeReady,
      weak_factory_.GetWeakPtr(),
      certificate,
      callback);
  std::string key_name = kContentProtectionKeyPrefix;
  key_name += service_id;
  async_caller_->TpmAttestationSignSimpleChallenge(KEY_USER,
                                                   key_name,
                                                   challenge,
                                                   cryptohome_callback);
}

void PlatformVerificationFlow::OnChallengeReady(
    const std::string& certificate,
    const ChallengeCallback& callback,
    bool operation_success,
    const std::string& response_data) {
  if (!operation_success) {
    LOG(ERROR) << "PlatformVerificationFlow: Failed to sign challenge.";
    callback.Run(INTERNAL_ERROR, std::string(), std::string());
    return;
  }
  LOG(INFO) << "PlatformVerificationFlow: Platform successfully verified.";
  callback.Run(SUCCESS, response_data, certificate);
}

}  // namespace attestation
}  // namespace chromeos
