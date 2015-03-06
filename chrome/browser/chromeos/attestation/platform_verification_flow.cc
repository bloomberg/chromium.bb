// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/attestation/platform_verification_flow.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/attestation/attestation_ca_client.h"
#include "chrome/browser/chromeos/attestation/attestation_signed_data.pb.h"
#include "chrome/browser/chromeos/attestation/platform_verification_dialog.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/attestation/attestation_flow.h"
#include "chromeos/cryptohome/async_method_caller.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/user_manager/user.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "net/cert/x509_certificate.h"

namespace {

const char kDefaultHttpsPort[] = "443";
const int kTimeoutInSeconds = 8;
const char kAttestationResultHistogram[] =
    "ChromeOS.PlatformVerification.Result";
const char kAttestationAvailableHistogram[] =
    "ChromeOS.PlatformVerification.Available";
const int kAttestationResultHistogramMax = 10;

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
    const chromeos::attestation::PlatformVerificationFlow::ChallengeCallback&
        callback,
    chromeos::attestation::PlatformVerificationFlow::Result error) {
  UMA_HISTOGRAM_ENUMERATION(kAttestationResultHistogram, error,
                            kAttestationResultHistogramMax);
  callback.Run(error, std::string(), std::string(), std::string());
}
}  // namespace

namespace chromeos {
namespace attestation {

// A default implementation of the Delegate interface.
class DefaultDelegate : public PlatformVerificationFlow::Delegate {
 public:
  DefaultDelegate() {}
  ~DefaultDelegate() override {}

  void ShowConsentPrompt(
      content::WebContents* web_contents,
      const GURL& requesting_origin,
      const PlatformVerificationFlow::Delegate::ConsentCallback& callback)
      override {
    PlatformVerificationDialog::ShowDialog(web_contents, requesting_origin,
                                           callback);
  }

  PrefService* GetPrefs(content::WebContents* web_contents) override {
    return user_prefs::UserPrefs::Get(web_contents->GetBrowserContext());
  }

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

  HostContentSettingsMap* GetContentSettings(
      content::WebContents* web_contents) override {
    return Profile::FromBrowserContext(web_contents->GetBrowserContext())->
        GetHostContentSettingsMap();
  }

  bool IsGuestOrIncognito(content::WebContents* web_contents) override {
    Profile* profile =
        Profile::FromBrowserContext(web_contents->GetBrowserContext());
    return (profile->IsOffTheRecord() || profile->IsGuestSession());
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

PlatformVerificationFlow::ChallengeContext::~ChallengeContext() {}

PlatformVerificationFlow::PlatformVerificationFlow()
    : attestation_flow_(NULL),
      async_caller_(cryptohome::AsyncMethodCaller::GetInstance()),
      cryptohome_client_(DBusThreadManager::Get()->GetCryptohomeClient()),
      delegate_(NULL),
      timeout_delay_(base::TimeDelta::FromSeconds(kTimeoutInSeconds)) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  scoped_ptr<ServerProxy> attestation_ca_client(new AttestationCAClient());
  default_attestation_flow_.reset(new AttestationFlow(
      async_caller_,
      cryptohome_client_,
      attestation_ca_client.Pass()));
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
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
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
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (!delegate_->GetURL(web_contents).is_valid()) {
    LOG(WARNING) << "PlatformVerificationFlow: Invalid URL.";
    ReportError(callback, INTERNAL_ERROR);
    return;
  }
  if (!IsAttestationEnabled(web_contents)) {
    ReportError(callback, POLICY_REJECTED);
    return;
  }
  // A platform key must be bound to a user.  They are not allowed in incognito
  // or guest mode.
  if (delegate_->IsGuestOrIncognito(web_contents)) {
    VLOG(1) << "Platform verification denied because the current session is "
            << "guest or incognito.";
    ReportError(callback, PLATFORM_NOT_VERIFIED);
    return;
  }
  ChallengeContext context(web_contents, service_id, challenge, callback);
  // Check if the device has been prepared to use attestation.
  BoolDBusMethodCallback dbus_callback = base::Bind(
      &DBusCallback,
      base::Bind(&PlatformVerificationFlow::CheckEnrollment, this, context),
      base::Bind(&ReportError, callback, INTERNAL_ERROR));
  cryptohome_client_->TpmAttestationIsPrepared(dbus_callback);
}

void PlatformVerificationFlow::CheckEnrollment(const ChallengeContext& context,
                                               bool attestation_prepared) {
  UMA_HISTOGRAM_BOOLEAN(kAttestationAvailableHistogram, attestation_prepared);
  if (!attestation_prepared) {
    // This device is not currently able to use attestation features.
    ReportError(context.callback, PLATFORM_NOT_VERIFIED);
    return;
  }
  BoolDBusMethodCallback dbus_callback = base::Bind(
      &DBusCallback,
      base::Bind(&PlatformVerificationFlow::CheckConsent, this, context),
      base::Bind(&ReportError, context.callback, INTERNAL_ERROR));
  cryptohome_client_->TpmAttestationIsEnrolled(dbus_callback);
}

void PlatformVerificationFlow::CheckConsent(const ChallengeContext& context,
                                            bool /* attestation_enrolled */) {
  content::WebContents* web_contents = context.web_contents;

  bool enabled_for_origin = false;
  bool found =
      GetOriginPref(delegate_->GetContentSettings(web_contents),
                    delegate_->GetURL(web_contents), &enabled_for_origin);
  if (found && !enabled_for_origin) {
    VLOG(1) << "Platform verification denied because the origin has been "
            << "blocked by the user.";
    ReportError(context.callback, USER_REJECTED);
    return;
  }

  PrefService* pref_service = delegate_->GetPrefs(web_contents);
  if (!pref_service) {
    LOG(ERROR) << "Failed to get user prefs.";
    ReportError(context.callback, INTERNAL_ERROR);
    return;
  }

  // Consent required if user has never given consent for this origin, or if
  // user has never given consent to attestation for content protection on this
  // device.
  bool consent_required =
      !found || !pref_service->GetBoolean(prefs::kRAConsentGranted);

  Delegate::ConsentCallback consent_callback = base::Bind(
      &PlatformVerificationFlow::OnConsentResponse,
      this,
      context,
      consent_required);
  if (consent_required) {
    // TODO(xhwang): Using delegate_->GetURL() here is not right. The consent
    // may be requested by a frame from a different origin. This will be solved
    // when http://crbug.com/454847 is fixed.
    delegate_->ShowConsentPrompt(
        context.web_contents,
        delegate_->GetURL(context.web_contents).GetOrigin(), consent_callback);
  } else {
    consent_callback.Run(CONSENT_RESPONSE_NONE);
  }
}

void PlatformVerificationFlow::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* prefs) {
  prefs->RegisterBooleanPref(prefs::kRAConsentGranted,
                             false,  // Default value.
                             user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

void PlatformVerificationFlow::OnConsentResponse(
    const ChallengeContext& context,
    bool consent_required,
    ConsentResponse consent_response) {
  if (consent_required) {
    if (consent_response == CONSENT_RESPONSE_NONE) {
      // No user response - do not proceed and do not modify any settings.
      LOG(WARNING) << "PlatformVerificationFlow: No response from user.";
      ReportError(context.callback, USER_REJECTED);
      return;
    }
    if (!UpdateSettings(context.web_contents, consent_response)) {
      ReportError(context.callback, INTERNAL_ERROR);
      return;
    }
    if (consent_response == CONSENT_RESPONSE_DENY) {
      content::RecordAction(
          base::UserMetricsAction("PlatformVerificationRejected"));
      VLOG(1) << "Platform verification denied by user.";
      ReportError(context.callback, USER_REJECTED);
      return;
    } else if (consent_response == CONSENT_RESPONSE_ALLOW) {
      content::RecordAction(
          base::UserMetricsAction("PlatformVerificationAccepted"));
      VLOG(1) << "Platform verification accepted by user.";
    }
  }

  // At this point all user interaction is complete and we can proceed with the
  // certificate request.
  const user_manager::User* user = delegate_->GetUser(context.web_contents);
  if (!user) {
    ReportError(context.callback, INTERNAL_ERROR);
    LOG(ERROR) << "Profile does not map to a valid user.";
    return;
  }

  GetCertificate(context, user->email(), false /* Don't force a new key */);
}

void PlatformVerificationFlow::GetCertificate(const ChallengeContext& context,
                                              const std::string& user_id,
                                              bool force_new_key) {
  scoped_ptr<base::Timer> timer(new base::Timer(false,    // Don't retain.
                                                false));  // Don't repeat.
  base::Closure timeout_callback = base::Bind(
      &PlatformVerificationFlow::OnCertificateTimeout,
      this,
      context);
  timer->Start(FROM_HERE, timeout_delay_, timeout_callback);

  AttestationFlow::CertificateCallback certificate_callback = base::Bind(
      &PlatformVerificationFlow::OnCertificateReady,
      this,
      context,
      user_id,
      base::Passed(&timer));
  attestation_flow_->GetCertificate(
      PROFILE_CONTENT_PROTECTION_CERTIFICATE,
      user_id,
      context.service_id,
      force_new_key,
      certificate_callback);
}

void PlatformVerificationFlow::OnCertificateReady(
    const ChallengeContext& context,
    const std::string& user_id,
    scoped_ptr<base::Timer> timer,
    bool operation_success,
    const std::string& certificate) {
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
  if (IsExpired(certificate)) {
    GetCertificate(context, user_id, true /* Force a new key */);
    return;
  }
  cryptohome::AsyncMethodCaller::DataCallback cryptohome_callback = base::Bind(
      &PlatformVerificationFlow::OnChallengeReady,
      this,
      context,
      certificate);
  std::string key_name = kContentProtectionKeyPrefix;
  key_name += context.service_id;
  async_caller_->TpmAttestationSignSimpleChallenge(KEY_USER,
                                                   user_id,
                                                   key_name,
                                                   context.challenge,
                                                   cryptohome_callback);
}

void PlatformVerificationFlow::OnCertificateTimeout(
    const ChallengeContext& context) {
  LOG(WARNING) << "PlatformVerificationFlow: Timing out.";
  ReportError(context.callback, TIMEOUT);
}

void PlatformVerificationFlow::OnChallengeReady(
    const ChallengeContext& context,
    const std::string& certificate,
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
  UMA_HISTOGRAM_ENUMERATION(kAttestationResultHistogram, SUCCESS,
                            kAttestationResultHistogramMax);
  context.callback.Run(SUCCESS,
                       signed_data_pb.data(),
                       signed_data_pb.signature(),
                       certificate);
}

bool PlatformVerificationFlow::IsAttestationEnabled(
    content::WebContents* web_contents) {
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

  // Check the user preference for the feature.
  PrefService* pref_service = delegate_->GetPrefs(web_contents);
  if (!pref_service) {
    LOG(ERROR) << "Failed to get user prefs.";
    return false;
  }
  if (!pref_service->GetBoolean(prefs::kEnableDRM)) {
    VLOG(1) << "Platform verification denied because content protection "
            << "identifiers have been disabled by the user.";
    return false;
  }

  // Check the user preference for this origin.
  bool enabled_for_origin = false;
  bool found =
      GetOriginPref(delegate_->GetContentSettings(web_contents),
                    delegate_->GetURL(web_contents), &enabled_for_origin);
  if (found && !enabled_for_origin) {
    VLOG(1) << "Platform verification denied because the origin has been "
            << "blocked by the user.";
    return false;
  }
  return true;
}

bool PlatformVerificationFlow::UpdateSettings(
    content::WebContents* web_contents,
    ConsentResponse consent_response) {
  PrefService* pref_service = delegate_->GetPrefs(web_contents);
  if (!pref_service) {
    LOG(ERROR) << "Failed to get user prefs.";
    return false;
  }

  if (consent_response == CONSENT_RESPONSE_ALLOW) {
    pref_service->SetBoolean(prefs::kRAConsentGranted, true);
  }

  RecordOriginConsent(delegate_->GetContentSettings(web_contents),
                      delegate_->GetURL(web_contents),
                      (consent_response == CONSENT_RESPONSE_ALLOW));
  return true;
}

bool PlatformVerificationFlow::GetOriginPref(
    HostContentSettingsMap* content_settings,
    const GURL& url,
    bool* pref_value) {
  CHECK(content_settings);
  CHECK(url.is_valid());
  ContentSetting setting = content_settings->GetContentSetting(
      url,
      url,
      CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER,
      std::string());
  if (setting != CONTENT_SETTING_ALLOW && setting != CONTENT_SETTING_BLOCK)
    return false;
  if (pref_value)
    *pref_value = (setting == CONTENT_SETTING_ALLOW);
  return true;
}

void PlatformVerificationFlow::RecordOriginConsent(
    HostContentSettingsMap* content_settings,
    const GURL& url,
    bool allow_origin) {
  CHECK(content_settings);
  CHECK(url.is_valid());
  // Build a pattern to represent scheme and host.
  scoped_ptr<ContentSettingsPattern::BuilderInterface> builder(
      ContentSettingsPattern::CreateBuilder(false));
  builder->WithScheme(url.scheme())
         ->WithDomainWildcard()
         ->WithHost(url.host())
         ->WithPathWildcard();
  if (!url.port().empty())
    builder->WithPort(url.port());
  else if (url.SchemeIs(url::kHttpsScheme))
    builder->WithPort(kDefaultHttpsPort);
  else if (url.SchemeIs(url::kHttpScheme))
    builder->WithPortWildcard();
  ContentSettingsPattern pattern = builder->Build();
  if (pattern.IsValid()) {
    ContentSetting setting =
        allow_origin ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK;
    content_settings->SetContentSetting(
        pattern,
        pattern,
        CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER,
        std::string(),
        setting);
  } else {
    LOG(WARNING) << "Not recording action: invalid URL pattern";
  }
}

bool PlatformVerificationFlow::IsExpired(const std::string& certificate) {
  scoped_refptr<net::X509Certificate> x509(
      net::X509Certificate::CreateFromBytes(certificate.data(),
                                            certificate.length()));
  if (!x509.get() || x509->valid_expiry().is_null()) {
    LOG(WARNING) << "Failed to parse certificate, cannot check expiry.";
    return false;
  }
  return (base::Time::Now() > x509->valid_expiry());
}

}  // namespace attestation
}  // namespace chromeos
