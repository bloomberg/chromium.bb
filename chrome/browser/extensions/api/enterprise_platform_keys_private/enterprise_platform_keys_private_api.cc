// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_platform_keys_private/enterprise_platform_keys_private_api.h"

#include <string>

#include "base/base64.h"
#include "base/callback.h"
#include "base/message_loop.h"
#include "base/prefs/pref_service.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/enterprise_install_attributes.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/cros_settings_names.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/common/extensions/api/enterprise_platform_keys_private.h"
#include "chrome/common/pref_names.h"
#include "chromeos/cryptohome/async_method_caller.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace extensions {

namespace api_epkp = api::enterprise_platform_keys_private;

// Base class

EPKPChallengeKeyBase::EPKPChallengeKeyBase()
    : cryptohome_client_(
          chromeos::DBusThreadManager::Get()->GetCryptohomeClient()),
      async_caller_(cryptohome::AsyncMethodCaller::GetInstance()),
      install_attributes_(g_browser_process->browser_policy_connector()->
                          GetInstallAttributes()) {
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

  MessageLoop::current()->PostTask(FROM_HERE, base::Bind(callback, value));
}

bool EPKPChallengeKeyBase::IsEnterpriseDevice() const {
  return install_attributes_->IsEnterpriseDevice();
}

std::string EPKPChallengeKeyBase::GetEnterpriseDomain() const {
  return install_attributes_->GetDomain();
}

std::string EPKPChallengeKeyBase::GetDeviceId() const {
  return install_attributes_->GetDeviceId();
}

// Implementation of ChallengeMachineKey()

const char EPKPChallengeMachineKey::kKeyName[] = "attest-ent-machine";

EPKPChallengeMachineKey::~EPKPChallengeMachineKey() {
}

bool EPKPChallengeMachineKey::RunImpl() {
  scoped_ptr<api_epkp::ChallengeMachineKey::Params>
      params(api_epkp::ChallengeMachineKey::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::string challenge;
  if (!base::Base64Decode(params->challenge, &challenge)) {
    SetError("Challenge is not base64 encoded.");
    SendResponse(false);
    return false;
  }

  // Check if the device is enterprise enrolled.
  if (!IsEnterpriseDevice()) {
    SetError("The device is not enterprise enrolled.");
    SendResponse(false);
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
    SetError("Remote attestation is not enabled for your device.");
    SendResponse(false);
    return;
  }

  // Everything is checked. Sign the challenge.
  async_caller_->TpmAttestationSignEnterpriseChallenge(
      chromeos::attestation::KEY_DEVICE,
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
    SetError("Challenge failed.");
    SendResponse(false);
    return;
  }

  std::string encoded_response;
  if (!base::Base64Encode(response, &encoded_response)) {
    SetError("Response cannot be encoded in base64.");
    SendResponse(false);
    return;
  }

  results_ = api_epkp::ChallengeMachineKey::Results::Create(encoded_response);
  SendResponse(true);
}

// Implementation of ChallengeUserKey()

const char EPKPChallengeUserKey::kKeyName[] = "attest-ent-user";

EPKPChallengeUserKey::~EPKPChallengeUserKey() {
}

void EPKPChallengeUserKey::RegisterUserPrefs(PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kAttestationEnabled, false,
                                PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterListPref(prefs::kAttestationExtensionWhitelist,
                             PrefRegistrySyncable::UNSYNCABLE_PREF);
}

bool EPKPChallengeUserKey::RunImpl() {
  scoped_ptr<api_epkp::ChallengeUserKey::Params> params(
      api_epkp::ChallengeUserKey::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::string challenge;
  if (!base::Base64Decode(params->challenge, &challenge)) {
    SetError("Challenge is not base64 encoded.");
    SendResponse(false);
    return false;
  }

  // Check if RA is enabled in the user policy.
  if (!IsRemoteAttestationEnabledForUser()) {
    SetError("Remote attestation is not enabled for your account.");
    SendResponse(false);
    return false;
  }

  // Check if the extension is whitelisted in the user policy.
  if (!IsExtensionWhitelisted()) {
    SetError("The extension does not have permission to call this function.");
    SendResponse(false);
    return false;
  }

  std::string user_domain = GetUserDomain();

  if (IsEnterpriseDevice()) {
    // Check if the user domain is the same as the enrolled enterprise domain.
    std::string enterprise_domain = GetEnterpriseDomain();
    if (user_domain != enterprise_domain) {
      SetError("User domain " + user_domain + " and Enterprise domain " +
               enterprise_domain + " don't match");
      SendResponse(false);
      return false;
    }

    // Check if RA is enabled in the device policy.
    GetDeviceAttestationEnabled(
        base::Bind(&EPKPChallengeUserKey::GetDeviceAttestationEnabledCallback,
                   this, challenge, params->register_key, user_domain));
  } else {
    // If this is a personal device, we should explicitly ask the user before
    // we use the key.
    AskForUserConsent(
        base::Bind(&EPKPChallengeUserKey::UserConsentCallback, this,
                   challenge, params->register_key, user_domain));
  }

  return true;
}

void EPKPChallengeUserKey::GetDeviceAttestationEnabledCallback(
    const std::string& challenge,
    bool register_key,
    const std::string& domain,
    bool enabled) {
  if (!enabled) {
    SetError("Remote attestation is not enabled for your device.");
    SendResponse(false);
    return;
  }

  // If remote attestation is enabled at the device level, we don't need to
  // ask for user consent.
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&EPKPChallengeUserKey::UserConsentCallback, this,
                 challenge, register_key, domain, true));
}

void EPKPChallengeUserKey::UserConsentCallback(const std::string& challenge,
                                               bool register_key,
                                               const std::string& domain,
                                               bool action) {
  if (!action) {
      SetError("User rejects the action.");
      SendResponse(false);
      return;
  }

  // Everything is checked. Sign the challenge.
  async_caller_->TpmAttestationSignEnterpriseChallenge(
      chromeos::attestation::KEY_USER,
      kKeyName,
      domain,
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
    SetError("Challenge failed.");
    SendResponse(false);
    return;
  }

  if (register_key) {
    async_caller_->TpmAttestationRegisterKey(
        chromeos::attestation::KEY_USER,
        kKeyName,
        base::Bind(&EPKPChallengeUserKey::RegisterKeyCallback, this, response));
  } else {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&EPKPChallengeUserKey::RegisterKeyCallback, this,
                   response, true, cryptohome::MOUNT_ERROR_NONE));
  }
}

void EPKPChallengeUserKey::RegisterKeyCallback(
    const std::string& response,
    bool success,
    cryptohome::MountError return_code) {
  if (!success || return_code != cryptohome::MOUNT_ERROR_NONE) {
    SetError("Key registration failed.");
    SendResponse(false);
    return;
  }

  std::string encoded_response;
  if (!base::Base64Encode(response, &encoded_response)) {
    SetError("Response cannot be encoded in base64.");
    SendResponse(false);
    return;
  }

  results_ = api_epkp::ChallengeUserKey::Results::Create(encoded_response);
  SendResponse(true);
}

void EPKPChallengeUserKey::AskForUserConsent(
    const base::Callback<void(bool)>& callback) {
  // TODO(davidyu): right now we just simply reject the request before we have
  // a way to ask for user consent.
  MessageLoop::current()->PostTask(FROM_HERE, base::Bind(callback, false));
}

bool EPKPChallengeUserKey::IsExtensionWhitelisted() const {
  const base::ListValue* list =
      profile()->GetPrefs()->GetList(prefs::kAttestationExtensionWhitelist);
  StringValue value(extension_->id());
  return list->Find(value) != list->end();
}

bool EPKPChallengeUserKey::IsRemoteAttestationEnabledForUser() const {
  return profile()->GetPrefs()->GetBoolean(prefs::kAttestationEnabled);
}

std::string EPKPChallengeUserKey::GetUserDomain() const {
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile());
  if (!signin_manager)
    return std::string();

  return gaia::ExtractDomainName(
      gaia::CanonicalizeEmail(signin_manager->GetAuthenticatedUsername()));
}

}  // namespace extensions
