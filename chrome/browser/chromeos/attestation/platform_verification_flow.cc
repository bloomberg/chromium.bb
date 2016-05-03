// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/attestation/platform_verification_flow.h"

#include <utility>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/attestation/attestation_ca_client.h"
#include "chrome/browser/chromeos/attestation/attestation_signed_data.pb.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/permissions/permission_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/attestation/attestation_flow.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/cryptohome/async_method_caller.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/permissions/permission_status.mojom.h"
#include "components/user_manager/user.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "net/cert/pem_tokenizer.h"
#include "net/cert/x509_certificate.h"

namespace {

using chromeos::attestation::PlatformVerificationFlow;

const int kTimeoutInSeconds = 8;
const char kAttestationResultHistogram[] =
    "ChromeOS.PlatformVerification.Result";
const char kAttestationAvailableHistogram[] =
    "ChromeOS.PlatformVerification.Available";
const char kAttestationExpiryHistogram[] =
    "ChromeOS.PlatformVerification.ExpiryStatus";
const int kOpportunisticRenewalThresholdInDays = 30;

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

// A helper to call a ChallengeCallback with an error result.
void ReportError(
    const PlatformVerificationFlow::ChallengeCallback& callback,
    chromeos::attestation::PlatformVerificationFlow::Result error) {
  UMA_HISTOGRAM_ENUMERATION(kAttestationResultHistogram, error,
                            PlatformVerificationFlow::RESULT_MAX);
  callback.Run(error, std::string(), std::string(), std::string());
}

// A helper to report expiry status to UMA.
void ReportExpiryStatus(PlatformVerificationFlow::ExpiryStatus status) {
  UMA_HISTOGRAM_ENUMERATION(kAttestationExpiryHistogram, status,
                            PlatformVerificationFlow::EXPIRY_STATUS_MAX);
}

}  // namespace

namespace chromeos {
namespace attestation {

// A default implementation of the Delegate interface.
class DefaultDelegate : public PlatformVerificationFlow::Delegate {
 public:
  DefaultDelegate() {}
  ~DefaultDelegate() override {}

  const GURL& GetURL(content::WebContents* web_contents) override {
    const GURL& url = web_contents->GetLastCommittedURL();
    if (!url.is_valid())
      return web_contents->GetVisibleURL();
    return url;
  }

  const user_manager::User* GetUser(
      content::WebContents* web_contents) override {
    return ProfileHelper::Get()->GetUserByProfile(
        Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  }

  bool IsPermittedByUser(content::WebContents* web_contents) override {
    // TODO(xhwang): Using delegate_->GetURL() here is not right. The platform
    // verification may be requested by a frame from a different origin. This
    // will be solved when http://crbug.com/454847 is fixed.
    const GURL& requesting_origin = GetURL(web_contents).GetOrigin();

    GURL embedding_origin = web_contents->GetLastCommittedURL().GetOrigin();
    permissions::mojom::PermissionStatus status =
        PermissionManager::Get(
            Profile::FromBrowserContext(web_contents->GetBrowserContext()))
            ->GetPermissionStatus(
                content::PermissionType::PROTECTED_MEDIA_IDENTIFIER,
                requesting_origin, embedding_origin);

    return status == permissions::mojom::PermissionStatus::GRANTED;
  }

  bool IsInSupportedMode(content::WebContents* web_contents) override {
    Profile* profile =
        Profile::FromBrowserContext(web_contents->GetBrowserContext());
    if (profile->IsOffTheRecord() || profile->IsGuestSession())
      return false;

    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    return !command_line->HasSwitch(chromeos::switches::kSystemDevMode) ||
           command_line->HasSwitch(chromeos::switches::kAllowRAInDevMode);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DefaultDelegate);
};

PlatformVerificationFlow::ChallengeContext::ChallengeContext(
    content::WebContents* web_contents,
    const std::string& service_id,
    const std::string& challenge,
    const ChallengeCallback& callback)
    : web_contents(web_contents),
      service_id(service_id),
      challenge(challenge),
      callback(callback) {}

PlatformVerificationFlow::ChallengeContext::ChallengeContext(
    const ChallengeContext& other) = default;

PlatformVerificationFlow::ChallengeContext::~ChallengeContext() {}

PlatformVerificationFlow::PlatformVerificationFlow()
    : attestation_flow_(NULL),
      async_caller_(cryptohome::AsyncMethodCaller::GetInstance()),
      cryptohome_client_(DBusThreadManager::Get()->GetCryptohomeClient()),
      delegate_(NULL),
      timeout_delay_(base::TimeDelta::FromSeconds(kTimeoutInSeconds)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::unique_ptr<ServerProxy> attestation_ca_client(new AttestationCAClient());
  default_attestation_flow_.reset(new AttestationFlow(
      async_caller_, cryptohome_client_, std::move(attestation_ca_client)));
  attestation_flow_ = default_attestation_flow_.get();
  default_delegate_.reset(new DefaultDelegate());
  delegate_ = default_delegate_.get();
}

PlatformVerificationFlow::PlatformVerificationFlow(
    AttestationFlow* attestation_flow,
    cryptohome::AsyncMethodCaller* async_caller,
    CryptohomeClient* cryptohome_client,
    Delegate* delegate)
    : attestation_flow_(attestation_flow),
      async_caller_(async_caller),
      cryptohome_client_(cryptohome_client),
      delegate_(delegate),
      timeout_delay_(base::TimeDelta::FromSeconds(kTimeoutInSeconds)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!delegate_) {
    default_delegate_.reset(new DefaultDelegate());
    delegate_ = default_delegate_.get();
  }
}

PlatformVerificationFlow::~PlatformVerificationFlow() {
}

void PlatformVerificationFlow::ChallengePlatformKey(
    content::WebContents* web_contents,
    const std::string& service_id,
    const std::string& challenge,
    const ChallengeCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!delegate_->GetURL(web_contents).is_valid()) {
    LOG(WARNING) << "PlatformVerificationFlow: Invalid URL.";
    ReportError(callback, INTERNAL_ERROR);
    return;
  }

  // Note: The following two checks are also checked in GetPermissionStatus.
  // Checking them here explicitly to report the correct error type.

  if (!IsAttestationAllowedByPolicy()) {
    VLOG(1) << "Platform verification not allowed by device policy.";
    ReportError(callback, POLICY_REJECTED);
    return;
  }

  // TODO(xhwang): Change to DCHECK when prefixed EME support is removed.
  // See http://crbug.com/249976
  if (!delegate_->IsInSupportedMode(web_contents)) {
    VLOG(1) << "Platform verification denied because it's not supported in the "
            << "current mode.";
    ReportError(callback, PLATFORM_NOT_VERIFIED);
    return;
  }

  if (!delegate_->IsPermittedByUser(web_contents)) {
    VLOG(1) << "Platform verification not permitted by user.";
    ReportError(callback, USER_REJECTED);
    return;
  }

  ChallengeContext context(web_contents, service_id, challenge, callback);
  // Check if the device has been prepared to use attestation.
  BoolDBusMethodCallback dbus_callback =
      base::Bind(&DBusCallback,
                 base::Bind(&PlatformVerificationFlow::OnAttestationPrepared,
                            this, context),
                 base::Bind(&ReportError, callback, INTERNAL_ERROR));
  cryptohome_client_->TpmAttestationIsPrepared(dbus_callback);
}

void PlatformVerificationFlow::OnAttestationPrepared(
    const ChallengeContext& context,
    bool attestation_prepared) {
  UMA_HISTOGRAM_BOOLEAN(kAttestationAvailableHistogram, attestation_prepared);

  if (!attestation_prepared) {
    // This device is not currently able to use attestation features.
    ReportError(context.callback, PLATFORM_NOT_VERIFIED);
    return;
  }

  // Permission allowed. Now proceed to get certificate.
  const user_manager::User* user = delegate_->GetUser(context.web_contents);
  if (!user) {
    ReportError(context.callback, INTERNAL_ERROR);
    LOG(ERROR) << "Profile does not map to a valid user.";
    return;
  }

  GetCertificate(context, user->GetAccountId(),
                 false /* Don't force a new key */);
}

void PlatformVerificationFlow::GetCertificate(const ChallengeContext& context,
                                              const AccountId& account_id,
                                              bool force_new_key) {
  std::unique_ptr<base::Timer> timer(new base::Timer(false,    // Don't retain.
                                                     false));  // Don't repeat.
  base::Closure timeout_callback = base::Bind(
      &PlatformVerificationFlow::OnCertificateTimeout,
      this,
      context);
  timer->Start(FROM_HERE, timeout_delay_, timeout_callback);

  AttestationFlow::CertificateCallback certificate_callback =
      base::Bind(&PlatformVerificationFlow::OnCertificateReady, this, context,
                 account_id, base::Passed(&timer));
  attestation_flow_->GetCertificate(PROFILE_CONTENT_PROTECTION_CERTIFICATE,
                                    account_id, context.service_id,
                                    force_new_key, certificate_callback);
}

void PlatformVerificationFlow::OnCertificateReady(
    const ChallengeContext& context,
    const AccountId& account_id,
    std::unique_ptr<base::Timer> timer,
    bool operation_success,
    const std::string& certificate_chain) {
  // Log failure before checking the timer so all failures are logged, even if
  // they took too long.
  if (!operation_success) {
    LOG(WARNING) << "PlatformVerificationFlow: Failed to certify platform.";
  }
  if (!timer->IsRunning()) {
    LOG(WARNING) << "PlatformVerificationFlow: Certificate ready but call has "
                 << "already timed out.";
    return;
  }
  timer->Stop();
  if (!operation_success) {
    ReportError(context.callback, PLATFORM_NOT_VERIFIED);
    return;
  }
  ExpiryStatus expiry_status = CheckExpiry(certificate_chain);
  ReportExpiryStatus(expiry_status);
  if (expiry_status == EXPIRY_STATUS_EXPIRED) {
    GetCertificate(context, account_id, true /* Force a new key */);
    return;
  }
  bool is_expiring_soon = (expiry_status == EXPIRY_STATUS_EXPIRING_SOON);
  cryptohome::AsyncMethodCaller::DataCallback cryptohome_callback =
      base::Bind(&PlatformVerificationFlow::OnChallengeReady, this, context,
                 account_id, certificate_chain, is_expiring_soon);
  std::string key_name = kContentProtectionKeyPrefix;
  key_name += context.service_id;
  async_caller_->TpmAttestationSignSimpleChallenge(
      KEY_USER, cryptohome::Identification(account_id), key_name,
      context.challenge, cryptohome_callback);
}

void PlatformVerificationFlow::OnCertificateTimeout(
    const ChallengeContext& context) {
  LOG(WARNING) << "PlatformVerificationFlow: Timing out.";
  ReportError(context.callback, TIMEOUT);
}

void PlatformVerificationFlow::OnChallengeReady(
    const ChallengeContext& context,
    const AccountId& account_id,
    const std::string& certificate_chain,
    bool is_expiring_soon,
    bool operation_success,
    const std::string& response_data) {
  if (!operation_success) {
    LOG(ERROR) << "PlatformVerificationFlow: Failed to sign challenge.";
    ReportError(context.callback, INTERNAL_ERROR);
    return;
  }
  SignedData signed_data_pb;
  if (response_data.empty() || !signed_data_pb.ParseFromString(response_data)) {
    LOG(ERROR) << "PlatformVerificationFlow: Failed to parse response data.";
    ReportError(context.callback, INTERNAL_ERROR);
    return;
  }
  VLOG(1) << "Platform verification successful.";
  UMA_HISTOGRAM_ENUMERATION(kAttestationResultHistogram, SUCCESS, RESULT_MAX);
  context.callback.Run(SUCCESS, signed_data_pb.data(),
                       signed_data_pb.signature(), certificate_chain);
  if (is_expiring_soon && renewals_in_progress_.count(certificate_chain) == 0) {
    renewals_in_progress_.insert(certificate_chain);
    // Fire off a certificate request so next time we'll have a new one.
    AttestationFlow::CertificateCallback renew_callback =
        base::Bind(&PlatformVerificationFlow::RenewCertificateCallback, this,
                   certificate_chain);
    attestation_flow_->GetCertificate(PROFILE_CONTENT_PROTECTION_CERTIFICATE,
                                      account_id, context.service_id,
                                      true,  // force_new_key
                                      renew_callback);
  }
}

bool PlatformVerificationFlow::IsAttestationAllowedByPolicy() {
  // Check the device policy for the feature.
  bool enabled_for_device = false;
  if (!CrosSettings::Get()->GetBoolean(kAttestationForContentProtectionEnabled,
                                       &enabled_for_device)) {
    LOG(ERROR) << "Failed to get device setting.";
    return false;
  }
  if (!enabled_for_device) {
    VLOG(1) << "Platform verification denied because Verified Access is "
            << "disabled for the device.";
    return false;
  }

  return true;
}

PlatformVerificationFlow::ExpiryStatus PlatformVerificationFlow::CheckExpiry(
    const std::string& certificate_chain) {
  bool is_expiring_soon = false;
  bool invalid_certificate_found = false;
  int num_certificates = 0;
  net::PEMTokenizer pem_tokenizer(certificate_chain, {"CERTIFICATE"});
  while (pem_tokenizer.GetNext()) {
    ++num_certificates;
    scoped_refptr<net::X509Certificate> x509 =
        net::X509Certificate::CreateFromBytes(pem_tokenizer.data().data(),
                                              pem_tokenizer.data().length());
    if (!x509.get() || x509->valid_expiry().is_null()) {
      // This logic intentionally fails open. In theory this should not happen
      // but in practice parsing X.509 can be brittle and there are a lot of
      // factors including which underlying module is parsing the certificate,
      // whether that module performs more checks than just ASN.1/DER format,
      // and the server module that generated the certificate(s). Renewal is
      // expensive so we only renew certificates with good evidence that they
      // have expired or will soon expire; if we don't know, we don't renew.
      LOG(WARNING) << "Failed to parse certificate, cannot check expiry.";
      invalid_certificate_found = true;
      continue;
    }
    if (base::Time::Now() > x509->valid_expiry()) {
      return EXPIRY_STATUS_EXPIRED;
    }
    base::TimeDelta threshold =
        base::TimeDelta::FromDays(kOpportunisticRenewalThresholdInDays);
    if (x509->valid_expiry() - base::Time::Now() < threshold) {
      is_expiring_soon = true;
    }
  }
  if (is_expiring_soon) {
    return EXPIRY_STATUS_EXPIRING_SOON;
  }
  if (invalid_certificate_found) {
    return EXPIRY_STATUS_INVALID_X509;
  }
  if (num_certificates == 0) {
    LOG(WARNING) << "Failed to parse certificate chain, cannot check expiry.";
    return EXPIRY_STATUS_INVALID_PEM_CHAIN;
  }
  return EXPIRY_STATUS_OK;
}

void PlatformVerificationFlow::RenewCertificateCallback(
    const std::string& old_certificate_chain,
    bool operation_success,
    const std::string& certificate_chain) {
  renewals_in_progress_.erase(old_certificate_chain);
  if (!operation_success) {
    LOG(WARNING) << "PlatformVerificationFlow: Failed to renew platform "
                    "certificate.";
    return;
  }
  VLOG(1) << "Certificate successfully renewed.";
}

}  // namespace attestation
}  // namespace chromeos
