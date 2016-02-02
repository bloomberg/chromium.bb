// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_platform_keys_private/enterprise_platform_keys_private_api.h"

#include <string>
#include <utility>

#include "base/base64.h"
#include "base/callback.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/attestation/attestation_ca_client.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/enterprise_install_attributes.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/common/extensions/api/enterprise_platform_keys_private.h"
#include "chrome/common/pref_names.h"
#include "chromeos/attestation/attestation_constants.h"
#include "chromeos/attestation/attestation_flow.h"
#include "chromeos/cryptohome/async_method_caller.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace extensions {

namespace api_epkp = api::enterprise_platform_keys_private;

// Base class

const char EPKPChallengeKeyBase::kChallengeBadBase64Error[] =
    "Challenge is not base64 encoded.";
const char EPKPChallengeKeyBase::kDevicePolicyDisabledError[] =
    "Remote attestation is not enabled for your device.";
const char EPKPChallengeKeyBase::kExtensionNotWhitelistedError[] =
    "The extension does not have permission to call this function.";
const char EPKPChallengeKeyBase::kResponseBadBase64Error[] =
    "Response cannot be encoded in base64.";
const char EPKPChallengeKeyBase::kSignChallengeFailedError[] =
    "Failed to sign the challenge.";
const char EPKPChallengeKeyBase::kUserNotManaged[] =
    "The user account is not enterprise managed.";

EPKPChallengeKeyBase::PrepareKeyContext::PrepareKeyContext(
    chromeos::attestation::AttestationKeyType key_type,
    const std::string& user_id,
    const std::string& key_name,
    chromeos::attestation::AttestationCertificateProfile certificate_profile,
    bool require_user_consent,
    const base::Callback<void(PrepareKeyResult)>& callback)
    : key_type(key_type),
      user_id(user_id),
      key_name(key_name),
      certificate_profile(certificate_profile),
      require_user_consent(require_user_consent),
      callback(callback) {
}

EPKPChallengeKeyBase::PrepareKeyContext::~PrepareKeyContext() {
}

EPKPChallengeKeyBase::EPKPChallengeKeyBase()
    : cryptohome_client_(
          chromeos::DBusThreadManager::Get()->GetCryptohomeClient()),
      async_caller_(cryptohome::AsyncMethodCaller::GetInstance()),
      install_attributes_(g_browser_process->platform_part()
                              ->browser_policy_connector_chromeos()
                              ->GetInstallAttributes()) {
  scoped_ptr<chromeos::attestation::ServerProxy> ca_client(
      new chromeos::attestation::AttestationCAClient());
  default_attestation_flow_.reset(new chromeos::attestation::AttestationFlow(
      async_caller_, cryptohome_client_, std::move(ca_client)));
  attestation_flow_ = default_attestation_flow_.get();
}

EPKPChallengeKeyBase::EPKPChallengeKeyBase(
    chromeos::CryptohomeClient* cryptohome_client,
    cryptohome::AsyncMethodCaller* async_caller,
    chromeos::attestation::AttestationFlow* attestation_flow,
    policy::EnterpriseInstallAttributes* install_attributes) :
    cryptohome_client_(cryptohome_client),
    async_caller_(async_caller),
    attestation_flow_(attestation_flow),
    install_attributes_(install_attributes) {
}

EPKPChallengeKeyBase::~EPKPChallengeKeyBase() {
}

void EPKPChallengeKeyBase::GetDeviceAttestationEnabled(
    const base::Callback<void(bool)>& callback) const {
  chromeos::CrosSettings* settings = chromeos::CrosSettings::Get();
  chromeos::CrosSettingsProvider::TrustedStatus status =
      settings->PrepareTrustedValues(
          base::Bind(&EPKPChallengeKeyBase::GetDeviceAttestationEnabled, this,
                     callback));

  bool value = false;
  switch (status) {
    case chromeos::CrosSettingsProvider::TRUSTED:
      if (!settings->GetBoolean(chromeos::kDeviceAttestationEnabled, &value))
        value = false;
      break;
    case chromeos::CrosSettingsProvider::TEMPORARILY_UNTRUSTED:
      // Do nothing. This function will be called again when the values are
      // ready.
      return;
    case chromeos::CrosSettingsProvider::PERMANENTLY_UNTRUSTED:
      // If the value cannot be trusted, we assume that the device attestation
      // is false to be on the safe side.
      break;
  }

  callback.Run(value);
}

bool EPKPChallengeKeyBase::IsEnterpriseDevice() const {
  return install_attributes_->IsEnterpriseDevice();
}

bool EPKPChallengeKeyBase::IsExtensionWhitelisted() const {
  const base::ListValue* list =
      GetProfile()->GetPrefs()->GetList(prefs::kAttestationExtensionWhitelist);
  base::StringValue value(extension_->id());
  return list->Find(value) != list->end();
}

bool EPKPChallengeKeyBase::IsUserManaged() const {
  std::string email = GetUserEmail();

  if (email.empty()) {
    return false;
  }

  const user_manager::User* const user =
      user_manager::UserManager::Get()->FindUser(
          AccountId::FromUserEmail(email));

  if (user) {
    return user->is_affiliated();
  }

  return false;
}

std::string EPKPChallengeKeyBase::GetEnterpriseDomain() const {
  return install_attributes_->GetDomain();
}

std::string EPKPChallengeKeyBase::GetUserEmail() const {
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(GetProfile());
  if (!signin_manager)
    return std::string();

  return gaia::CanonicalizeEmail(
      signin_manager->GetAuthenticatedAccountInfo().email);
}

std::string EPKPChallengeKeyBase::GetDeviceId() const {
  return install_attributes_->GetDeviceId();
}

void EPKPChallengeKeyBase::PrepareKey(
    chromeos::attestation::AttestationKeyType key_type,
    const std::string& user_id,
    const std::string& key_name,
    chromeos::attestation::AttestationCertificateProfile certificate_profile,
    bool require_user_consent,
    const base::Callback<void(PrepareKeyResult)>& callback) {
  const PrepareKeyContext context = PrepareKeyContext(key_type,
                                                      user_id,
                                                      key_name,
                                                      certificate_profile,
                                                      require_user_consent,
                                                      callback);
  cryptohome_client_->TpmAttestationIsPrepared(base::Bind(
      &EPKPChallengeKeyBase::IsAttestationPreparedCallback, this, context));
}

void EPKPChallengeKeyBase::IsAttestationPreparedCallback(
    const PrepareKeyContext& context,
    chromeos::DBusMethodCallStatus status,
    bool result) {
  if (status == chromeos::DBUS_METHOD_CALL_FAILURE) {
    context.callback.Run(PREPARE_KEY_DBUS_ERROR);
    return;
  }
  if (!result) {
    context.callback.Run(PREPARE_KEY_RESET_REQUIRED);
    return;
  }
  // Attestation is available, see if the key we need already exists.
  cryptohome_client_->TpmAttestationDoesKeyExist(
      context.key_type, context.user_id, context.key_name, base::Bind(
          &EPKPChallengeKeyBase::DoesKeyExistCallback, this, context));
}

void EPKPChallengeKeyBase::DoesKeyExistCallback(
    const PrepareKeyContext& context,
    chromeos::DBusMethodCallStatus status,
    bool result) {
  if (status == chromeos::DBUS_METHOD_CALL_FAILURE) {
    context.callback.Run(PREPARE_KEY_DBUS_ERROR);
    return;
  }

  if (result) {
    // The key exists. Do nothing more.
    context.callback.Run(PREPARE_KEY_OK);
  } else {
    // The key does not exist. Create a new key and have it signed by PCA.
    if (context.require_user_consent) {
      // We should ask the user explicitly before sending any private
      // information to PCA.
      AskForUserConsent(
          base::Bind(&EPKPChallengeKeyBase::AskForUserConsentCallback, this,
                     context));
    } else {
      // User consent is not required. Skip to the next step.
      AskForUserConsentCallback(context, true);
    }
  }
}

void EPKPChallengeKeyBase::AskForUserConsent(
    const base::Callback<void(bool)>& callback) const {
  // TODO(davidyu): right now we just simply reject the request before we have
  // a way to ask for user consent.
  callback.Run(false);
}

void EPKPChallengeKeyBase::AskForUserConsentCallback(
    const PrepareKeyContext& context,
    bool result) {
  if (!result) {
    // The user rejects the request.
    context.callback.Run(PREPARE_KEY_USER_REJECTED);
    return;
  }

  // Generate a new key and have it signed by PCA.
  attestation_flow_->GetCertificate(
      context.certificate_profile,
      context.user_id,
      std::string(),  // Not used.
      true,  // Force a new key to be generated.
      base::Bind(&EPKPChallengeKeyBase::GetCertificateCallback, this,
                 context.callback));
}

void EPKPChallengeKeyBase::GetCertificateCallback(
    const base::Callback<void(PrepareKeyResult)>& callback,
    bool success,
    const std::string& pem_certificate_chain) {
  if (!success) {
    callback.Run(PREPARE_KEY_GET_CERTIFICATE_FAILED);
    return;
  }

  callback.Run(PREPARE_KEY_OK);
}

// Implementation of ChallengeMachineKey()

const char EPKPChallengeMachineKey::kGetCertificateFailedError[] =
    "Failed to get Enterprise machine certificate. Error code = %d";
const char EPKPChallengeMachineKey::kNonEnterpriseDeviceError[] =
    "The device is not enterprise enrolled.";

const char EPKPChallengeMachineKey::kKeyName[] = "attest-ent-machine";

EPKPChallengeMachineKey::EPKPChallengeMachineKey() : EPKPChallengeKeyBase() {
}

EPKPChallengeMachineKey::EPKPChallengeMachineKey(
    chromeos::CryptohomeClient* cryptohome_client,
    cryptohome::AsyncMethodCaller* async_caller,
    chromeos::attestation::AttestationFlow* attestation_flow,
    policy::EnterpriseInstallAttributes* install_attributes) :
    EPKPChallengeKeyBase(cryptohome_client,
                         async_caller,
                         attestation_flow,
                         install_attributes) {
}

EPKPChallengeMachineKey::~EPKPChallengeMachineKey() {
}

bool EPKPChallengeMachineKey::RunAsync() {
  scoped_ptr<api_epkp::ChallengeMachineKey::Params>
      params(api_epkp::ChallengeMachineKey::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::string challenge;
  if (!base::Base64Decode(params->challenge, &challenge)) {
    SetError(kChallengeBadBase64Error);
    return false;
  }

  // Check if the device is enterprise enrolled.
  if (!IsEnterpriseDevice()) {
    SetError(kNonEnterpriseDeviceError);
    return false;
  }

  // Check if the extension is whitelisted in the user policy.
  if (!IsExtensionWhitelisted()) {
    SetError(kExtensionNotWhitelistedError);
    return false;
  }

  // Check if the user domain is the same as the enrolled enterprise domain.
  if (!IsUserManaged()) {
    SetError(kUserNotManaged);
    return false;
  }

  // Check if RA is enabled in the device policy.
  GetDeviceAttestationEnabled(
      base::Bind(&EPKPChallengeMachineKey::GetDeviceAttestationEnabledCallback,
                 this, challenge));

  return true;
}

void EPKPChallengeMachineKey::GetDeviceAttestationEnabledCallback(
    const std::string& challenge, bool enabled) {
  if (!enabled) {
    SetError(kDevicePolicyDisabledError);
    SendResponse(false);
    return;
  }

  PrepareKey(chromeos::attestation::KEY_DEVICE,
             std::string(),  // Not used.
             kKeyName,
             chromeos::attestation::PROFILE_ENTERPRISE_MACHINE_CERTIFICATE,
             false,  // user consent is not required.
             base::Bind(&EPKPChallengeMachineKey::PrepareKeyCallback, this,
                        challenge));
}

void EPKPChallengeMachineKey::PrepareKeyCallback(
    const std::string& challenge, PrepareKeyResult result) {
  if (result != PREPARE_KEY_OK) {
    SetError(base::StringPrintf(kGetCertificateFailedError, result));
    SendResponse(false);
    return;
  }

  // Everything is checked. Sign the challenge.
  async_caller_->TpmAttestationSignEnterpriseChallenge(
      chromeos::attestation::KEY_DEVICE,
      std::string(),  // Not used.
      kKeyName,
      GetEnterpriseDomain(),
      GetDeviceId(),
      chromeos::attestation::CHALLENGE_OPTION_NONE,
      challenge,
      base::Bind(&EPKPChallengeMachineKey::SignChallengeCallback, this));
}

void EPKPChallengeMachineKey::SignChallengeCallback(
    bool success, const std::string& response) {
  if (!success) {
    SetError(kSignChallengeFailedError);
    SendResponse(false);
    return;
  }

  std::string encoded_response;
  base::Base64Encode(response, &encoded_response);

  results_ = api_epkp::ChallengeMachineKey::Results::Create(encoded_response);
  SendResponse(true);
}

// Implementation of ChallengeUserKey()

const char EPKPChallengeUserKey::kGetCertificateFailedError[] =
    "Failed to get Enterprise user certificate. Error code = %d";
const char EPKPChallengeUserKey::kKeyRegistrationFailedError[] =
    "Key registration failed.";
const char EPKPChallengeUserKey::kUserPolicyDisabledError[] =
    "Remote attestation is not enabled for your account.";

const char EPKPChallengeUserKey::kKeyName[] = "attest-ent-user";

EPKPChallengeUserKey::EPKPChallengeUserKey() : EPKPChallengeKeyBase() {
}

EPKPChallengeUserKey::EPKPChallengeUserKey(
    chromeos::CryptohomeClient* cryptohome_client,
    cryptohome::AsyncMethodCaller* async_caller,
    chromeos::attestation::AttestationFlow* attestation_flow,
    policy::EnterpriseInstallAttributes* install_attributes) :
    EPKPChallengeKeyBase(cryptohome_client,
                         async_caller,
                         attestation_flow,
                         install_attributes) {
}

EPKPChallengeUserKey::~EPKPChallengeUserKey() {
}

void EPKPChallengeUserKey::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kAttestationEnabled, false);
  registry->RegisterListPref(prefs::kAttestationExtensionWhitelist);
}

bool EPKPChallengeUserKey::RunAsync() {
  scoped_ptr<api_epkp::ChallengeUserKey::Params> params(
      api_epkp::ChallengeUserKey::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::string challenge;
  if (!base::Base64Decode(params->challenge, &challenge)) {
    SetError(kChallengeBadBase64Error);
    return false;
  }

  // Check if RA is enabled in the user policy.
  if (!IsRemoteAttestationEnabledForUser()) {
    SetError(kUserPolicyDisabledError);
    return false;
  }

  // Check if the extension is whitelisted in the user policy.
  if (!IsExtensionWhitelisted()) {
    SetError(kExtensionNotWhitelistedError);
    return false;
  }

  if (IsEnterpriseDevice()) {
    // Check if the user domain is the same as the enrolled enterprise domain.
    if (!IsUserManaged()) {
      SetError(kUserNotManaged);
      return false;
    }

    // Check if RA is enabled in the device policy.
    GetDeviceAttestationEnabled(
        base::Bind(&EPKPChallengeUserKey::GetDeviceAttestationEnabledCallback,
                   this,
                   challenge,
                   params->register_key,
                   false));  // user consent is not required.
  } else {
    // For personal devices, we don't need to check if RA is enabled in the
    // device, but we need to ask for user consent if the key does not exist.
    GetDeviceAttestationEnabledCallback(
        challenge,
        params->register_key,
        true,  // user consent is required.
        true);  // attestation is enabled.
  }

  return true;
}

void EPKPChallengeUserKey::GetDeviceAttestationEnabledCallback(
    const std::string& challenge,
    bool register_key,
    bool require_user_consent,
    bool enabled) {
  if (!enabled) {
    SetError(kDevicePolicyDisabledError);
    SendResponse(false);
    return;
  }

  PrepareKey(chromeos::attestation::KEY_USER,
             GetUserEmail(),
             kKeyName,
             chromeos::attestation::PROFILE_ENTERPRISE_USER_CERTIFICATE,
             require_user_consent,
             base::Bind(&EPKPChallengeUserKey::PrepareKeyCallback, this,
                        challenge, register_key));
}

void EPKPChallengeUserKey::PrepareKeyCallback(const std::string& challenge,
                                              bool register_key,
                                              PrepareKeyResult result) {
  if (result != PREPARE_KEY_OK) {
    SetError(base::StringPrintf(kGetCertificateFailedError, result));
    SendResponse(false);
    return;
  }

  // Everything is checked. Sign the challenge.
  async_caller_->TpmAttestationSignEnterpriseChallenge(
      chromeos::attestation::KEY_USER,
      GetUserEmail(),
      kKeyName,
      GetUserEmail(),
      GetDeviceId(),
      register_key ?
          chromeos::attestation::CHALLENGE_INCLUDE_SIGNED_PUBLIC_KEY :
          chromeos::attestation::CHALLENGE_OPTION_NONE,
      challenge,
      base::Bind(&EPKPChallengeUserKey::SignChallengeCallback, this,
                 register_key));
}

void EPKPChallengeUserKey::SignChallengeCallback(bool register_key,
                                                 bool success,
                                                 const std::string& response) {
  if (!success) {
    SetError(kSignChallengeFailedError);
    SendResponse(false);
    return;
  }

  if (register_key) {
    async_caller_->TpmAttestationRegisterKey(
        chromeos::attestation::KEY_USER,
        GetUserEmail(),
        kKeyName,
        base::Bind(&EPKPChallengeUserKey::RegisterKeyCallback, this, response));
  } else {
    RegisterKeyCallback(response, true, cryptohome::MOUNT_ERROR_NONE);
  }
}

void EPKPChallengeUserKey::RegisterKeyCallback(
    const std::string& response,
    bool success,
    cryptohome::MountError return_code) {
  if (!success || return_code != cryptohome::MOUNT_ERROR_NONE) {
    SetError(kKeyRegistrationFailedError);
    SendResponse(false);
    return;
  }

  std::string encoded_response;
  base::Base64Encode(response, &encoded_response);

  results_ = api_epkp::ChallengeUserKey::Results::Create(encoded_response);
  SendResponse(true);
}

bool EPKPChallengeUserKey::IsRemoteAttestationEnabledForUser() const {
  return GetProfile()->GetPrefs()->GetBoolean(prefs::kAttestationEnabled);
}

}  // namespace extensions
