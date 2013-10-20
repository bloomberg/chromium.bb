// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform_verification_flow.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/chromeos/attestation/attestation_ca_client.h"
#include "chrome/browser/chromeos/attestation/attestation_signed_data.pb.h"
#include "chrome/browser/chromeos/attestation/platform_verification_dialog.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/common/pref_names.h"
#include "chromeos/attestation/attestation_flow.h"
#include "chromeos/cryptohome/async_method_caller.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"

namespace {
// A switch which allows consent to be given on the command line.
// TODO(dkrahn): Remove this when UI has been implemented (crbug.com/270908).
const char kAutoApproveSwitch[] =
    "auto-approve-platform-verification-consent-prompts";

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
  callback.Run(error, std::string(), std::string(), std::string());
}
}  // namespace

namespace chromeos {
namespace attestation {

// A default implementation of the Delegate interface.
class DefaultDelegate : public PlatformVerificationFlow::Delegate {
 public:
  DefaultDelegate() {}
  virtual ~DefaultDelegate() {}

  virtual void ShowConsentPrompt(
      PlatformVerificationFlow::ConsentType type,
      content::WebContents* web_contents,
      const PlatformVerificationFlow::Delegate::ConsentCallback& callback)
      OVERRIDE {
    if (CommandLine::ForCurrentProcess()->HasSwitch(kAutoApproveSwitch)) {
      LOG(WARNING) << "PlatformVerificationFlow: Automatic approval enabled.";
      callback.Run(PlatformVerificationFlow::CONSENT_RESPONSE_ALLOW);
    } else {
      PlatformVerificationDialog::ShowDialog(web_contents, callback);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DefaultDelegate);
};

PlatformVerificationFlow::PlatformVerificationFlow()
    : attestation_flow_(NULL),
      async_caller_(cryptohome::AsyncMethodCaller::GetInstance()),
      cryptohome_client_(DBusThreadManager::Get()->GetCryptohomeClient()),
      user_manager_(UserManager::Get()),
      delegate_(NULL),
      testing_prefs_(NULL),
      weak_factory_(this) {
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
    UserManager* user_manager,
    Delegate* delegate)
    : attestation_flow_(attestation_flow),
      async_caller_(async_caller),
      cryptohome_client_(cryptohome_client),
      user_manager_(user_manager),
      delegate_(delegate),
      testing_prefs_(NULL),
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
  if (!IsAttestationEnabled(web_contents)) {
    LOG(INFO) << "PlatformVerificationFlow: Feature disabled.";
    ReportError(callback, POLICY_REJECTED);
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
      base::Bind(&ReportError, callback, INTERNAL_ERROR));
  cryptohome_client_->TpmAttestationIsEnrolled(dbus_callback);
}

void PlatformVerificationFlow::CheckConsent(content::WebContents* web_contents,
                                            const std::string& service_id,
                                            const std::string& challenge,
                                            const ChallengeCallback& callback,
                                            bool attestation_enrolled) {
  ConsentType consent_type = CONSENT_TYPE_NONE;
  if (!attestation_enrolled || IsFirstUse(web_contents)) {
    consent_type = CONSENT_TYPE_ATTESTATION;
  } else if (IsAlwaysAskRequired(web_contents)) {
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

void PlatformVerificationFlow::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* prefs) {
  prefs->RegisterBooleanPref(prefs::kRAConsentFirstTime,
                             false,
                             user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  prefs->RegisterDictionaryPref(
      prefs::kRAConsentDomains,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kRAConsentAlways,
                             false,
                             user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
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
      ReportError(callback, USER_REJECTED);
      return;
    }
    if (!UpdateSettings(web_contents, consent_type, consent_response)) {
      ReportError(callback, INTERNAL_ERROR);
      return;
    }
    if (consent_response == CONSENT_RESPONSE_DENY) {
      LOG(INFO) << "PlatformVerificationFlow: User rejected request.";
      content::RecordAction(
          content::UserMetricsAction("PlatformVerificationRejected"));
      ReportError(callback, USER_REJECTED);
      return;
    } else if (consent_response == CONSENT_RESPONSE_ALLOW) {
      content::RecordAction(
          content::UserMetricsAction("PlatformVerificationAccepted"));
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
    ReportError(callback, PLATFORM_NOT_VERIFIED);
    return;
  }
  cryptohome::AsyncMethodCaller::DataCallback cryptohome_callback = base::Bind(
      &PlatformVerificationFlow::OnChallengeReady,
      weak_factory_.GetWeakPtr(),
      certificate,
      challenge,
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
    const std::string& challenge,
    const ChallengeCallback& callback,
    bool operation_success,
    const std::string& response_data) {
  if (!operation_success) {
    LOG(ERROR) << "PlatformVerificationFlow: Failed to sign challenge.";
    ReportError(callback, INTERNAL_ERROR);
    return;
  }
  SignedData signed_data_pb;
  if (response_data.empty() || !signed_data_pb.ParseFromString(response_data)) {
    LOG(ERROR) << "PlatformVerificationFlow: Failed to parse response data.";
    ReportError(callback, INTERNAL_ERROR);
    return;
  }
  callback.Run(SUCCESS,
               signed_data_pb.data(),
               signed_data_pb.signature(),
               certificate);
  LOG(INFO) << "PlatformVerificationFlow: Platform successfully verified.";
}

PrefService* PlatformVerificationFlow::GetPrefs(
    content::WebContents* web_contents) {
  if (testing_prefs_)
    return testing_prefs_;
  return user_prefs::UserPrefs::Get(web_contents->GetBrowserContext());
}

const GURL& PlatformVerificationFlow::GetURL(
    content::WebContents* web_contents) {
  if (!testing_url_.is_empty())
    return testing_url_;
  return web_contents->GetLastCommittedURL();
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
  if (!enabled_for_device)
    return false;

  // Check the user preference for the feature.
  PrefService* pref_service = GetPrefs(web_contents);
  if (!pref_service) {
    LOG(ERROR) << "Failed to get user prefs.";
    return false;
  }
  if (!pref_service->GetBoolean(prefs::kEnableDRM))
    return false;

  // Check the user preference for this domain.
  bool enabled_for_domain = false;
  bool found = GetDomainPref(web_contents, &enabled_for_domain);
  return (!found || enabled_for_domain);
}

bool PlatformVerificationFlow::IsFirstUse(content::WebContents* web_contents) {
  PrefService* pref_service = GetPrefs(web_contents);
  if (!pref_service) {
    LOG(ERROR) << "Failed to get user prefs.";
    return true;
  }
  return !pref_service->GetBoolean(prefs::kRAConsentFirstTime);
}

bool PlatformVerificationFlow::IsAlwaysAskRequired(
    content::WebContents* web_contents) {
  PrefService* pref_service = GetPrefs(web_contents);
  if (!pref_service) {
    LOG(ERROR) << "Failed to get user prefs.";
    return true;
  }
  if (!pref_service->GetBoolean(prefs::kRAConsentAlways))
    return false;
  // Show the consent UI if the user has not already explicitly allowed or
  // denied for this domain.
  return !GetDomainPref(web_contents, NULL);
}

bool PlatformVerificationFlow::UpdateSettings(
    content::WebContents* web_contents,
    ConsentType consent_type,
    ConsentResponse consent_response) {
  PrefService* pref_service = GetPrefs(web_contents);
  if (!pref_service) {
    LOG(ERROR) << "Failed to get user prefs.";
    return false;
  }
  if (consent_type == CONSENT_TYPE_ATTESTATION) {
    if (consent_response == CONSENT_RESPONSE_DENY) {
      pref_service->SetBoolean(prefs::kEnableDRM, false);
    } else if (consent_response == CONSENT_RESPONSE_ALLOW) {
      pref_service->SetBoolean(prefs::kRAConsentFirstTime, true);
      RecordDomainConsent(web_contents, true);
    } else if (consent_response == CONSENT_RESPONSE_ALWAYS_ASK) {
      pref_service->SetBoolean(prefs::kRAConsentFirstTime, true);
      pref_service->SetBoolean(prefs::kRAConsentAlways, true);
      RecordDomainConsent(web_contents, true);
    }
  } else if (consent_type == CONSENT_TYPE_ALWAYS) {
    bool allowed = (consent_response == CONSENT_RESPONSE_ALLOW ||
                    consent_response == CONSENT_RESPONSE_ALWAYS_ASK);
    RecordDomainConsent(web_contents, allowed);
  }
  return true;
}

bool PlatformVerificationFlow::GetDomainPref(
    content::WebContents* web_contents,
    bool* pref_value) {
  PrefService* pref_service = GetPrefs(web_contents);
  CHECK(pref_service);
  base::DictionaryValue::Iterator iter(
      *pref_service->GetDictionary(prefs::kRAConsentDomains));
  const GURL& url = GetURL(web_contents);
  while (!iter.IsAtEnd()) {
    if (url.DomainIs(iter.key().c_str())) {
      if (pref_value) {
        if (!iter.value().GetAsBoolean(pref_value)) {
          LOG(ERROR) << "Unexpected pref type.";
          *pref_value = false;
        }
      }
      return true;
    }
    iter.Advance();
  }
  return false;
}

void PlatformVerificationFlow::RecordDomainConsent(
    content::WebContents* web_contents,
    bool allow_domain) {
  PrefService* pref_service = GetPrefs(web_contents);
  CHECK(pref_service);
  DictionaryPrefUpdate updater(pref_service, prefs::kRAConsentDomains);
  const GURL& url = GetURL(web_contents);
  updater->SetBoolean(url.host(), allow_domain);
}

}  // namespace attestation
}  // namespace chromeos
