// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/easy_unlock_private/easy_unlock_private_api.h"

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/easy_unlock_private/easy_unlock_private_bluetooth_util.h"
#include "chrome/browser/extensions/api/easy_unlock_private/easy_unlock_private_crypto_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/easy_unlock_screenlock_state_handler.h"
#include "chrome/browser/signin/easy_unlock_service.h"
#include "chrome/common/extensions/api/easy_unlock_private.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/chromeos_utils.h"
#endif

namespace extensions {
namespace api {

namespace {

static base::LazyInstance<BrowserContextKeyedAPIFactory<EasyUnlockPrivateAPI> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

// Utility method for getting the API's crypto delegate.
EasyUnlockPrivateCryptoDelegate* GetCryptoDelegate(
    content::BrowserContext* context) {
  return BrowserContextKeyedAPIFactory<EasyUnlockPrivateAPI>::Get(context)
             ->crypto_delegate();
}

EasyUnlockScreenlockStateHandler* GetScreenlockStateHandler(
    content::BrowserContext* context) {
  return EasyUnlockService::Get(Profile::FromBrowserContext(context))
      ->GetScreenlockStateHandler();
}

EasyUnlockScreenlockStateHandler::State ToScreenlockStateHandlerState(
    easy_unlock_private::State state) {
  switch (state) {
    case easy_unlock_private::STATE_NO_BLUETOOTH:
      return EasyUnlockScreenlockStateHandler::STATE_NO_BLUETOOTH;
    case easy_unlock_private::STATE_BLUETOOTH_CONNECTING:
      return EasyUnlockScreenlockStateHandler::STATE_BLUETOOTH_CONNECTING;
    case easy_unlock_private::STATE_NO_PHONE:
      return EasyUnlockScreenlockStateHandler::STATE_NO_PHONE;
    case easy_unlock_private::STATE_PHONE_NOT_AUTHENTICATED:
      return EasyUnlockScreenlockStateHandler::STATE_PHONE_NOT_AUTHENTICATED;
    case easy_unlock_private::STATE_PHONE_LOCKED:
      return EasyUnlockScreenlockStateHandler::STATE_PHONE_LOCKED;
    case easy_unlock_private::STATE_PHONE_UNLOCKABLE:
      return EasyUnlockScreenlockStateHandler::STATE_PHONE_UNLOCKABLE;
    case easy_unlock_private::STATE_PHONE_NOT_NEARBY:
      return EasyUnlockScreenlockStateHandler::STATE_PHONE_NOT_NEARBY;
    case easy_unlock_private::STATE_AUTHENTICATED:
      return EasyUnlockScreenlockStateHandler::STATE_AUTHENTICATED;
    default:
      return EasyUnlockScreenlockStateHandler::STATE_INACTIVE;
  }
}

}  // namespace

// static
BrowserContextKeyedAPIFactory<EasyUnlockPrivateAPI>*
    EasyUnlockPrivateAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

EasyUnlockPrivateAPI::EasyUnlockPrivateAPI(content::BrowserContext* context)
    : crypto_delegate_(EasyUnlockPrivateCryptoDelegate::Create()) {
}

EasyUnlockPrivateAPI::~EasyUnlockPrivateAPI() {}

EasyUnlockPrivateGetStringsFunction::EasyUnlockPrivateGetStringsFunction() {
}
EasyUnlockPrivateGetStringsFunction::~EasyUnlockPrivateGetStringsFunction() {
}

bool EasyUnlockPrivateGetStringsFunction::RunSync() {
  scoped_ptr<base::DictionaryValue> strings(new base::DictionaryValue);

#if defined(OS_CHROMEOS)
  const base::string16 device_type = chromeos::GetChromeDeviceType();
#else
  // TODO(isherman): Set an appropriate device name for non-ChromeOS devices.
  const base::string16 device_type = base::ASCIIToUTF16("Chromeschnozzle");
#endif  // defined(OS_CHROMEOS)

  // Common strings.
  strings->SetString(
      "learnMoreLinkTitle",
      l10n_util::GetStringUTF16(IDS_EASY_UNLOCK_LEARN_MORE_LINK_TITLE));

  // Setup notification strings.
  strings->SetString(
      "setupNotificationTitle",
      l10n_util::GetStringFUTF16(IDS_EASY_UNLOCK_SETUP_NOTIFICATION_TITLE,
                                 device_type));
  strings->SetString(
      "setupNotificationMessage",
      l10n_util::GetStringFUTF16(IDS_EASY_UNLOCK_SETUP_NOTIFICATION_MESSAGE,
                                 device_type));
  strings->SetString(
      "setupNotificationButtonTitle",
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_SETUP_NOTIFICATION_BUTTON_TITLE));

  // Success notification strings.
  strings->SetString(
      "successNotificationTitle",
      l10n_util::GetStringUTF16(IDS_EASY_UNLOCK_SUCCESS_NOTIFICATION_TITLE));
  strings->SetString(
      "successNotificationMessage",
      l10n_util::GetStringFUTF16(IDS_EASY_UNLOCK_SUCCESS_NOTIFICATION_MESSAGE,
                                 device_type));

  // Chromebook added to Easy Unlock notification strings.
  strings->SetString(
      "chromebookAddedNotificationTitle",
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_CHROMEBOOK_ADDED_NOTIFICATION_TITLE));
  strings->SetString(
     "chromebookAddedNotificationMessage",
     l10n_util::GetStringFUTF16(
          IDS_EASY_UNLOCK_CHROMEBOOK_ADDED_NOTIFICATION_MESSAGE,
          device_type));

  // Setup dialog strings.
  // Step 1: Intro.
  strings->SetString(
      "setupIntroHeaderTitle",
      l10n_util::GetStringFUTF16(
          IDS_EASY_UNLOCK_SETUP_INTRO_HEADER_TITLE, device_type));
  strings->SetString(
      "setupIntroHeaderText",
      l10n_util::GetStringFUTF16(
          IDS_EASY_UNLOCK_SETUP_INTRO_HEADER_TEXT, device_type));
  strings->SetString(
      "setupIntroHeaderFootnote",
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_SETUP_INTRO_HEADER_FOOTNOTE));
  strings->SetString(
      "setupIntroFindPhoneButtonLabel",
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_SETUP_INTRO_FIND_PHONE_BUTTON_LABEL));
  strings->SetString(
      "setupIntroFindingPhoneButtonLabel",
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_SETUP_INTRO_FINDING_PHONE_BUTTON_LABEL));
  strings->SetString(
      "setupIntroRetryFindPhoneButtonLabel",
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_SETUP_INTRO_RETRY_FIND_PHONE_BUTTON_LABEL));
  strings->SetString(
      "setupIntroHowIsThisSecureLinkText",
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_SETUP_INTRO_HOW_IS_THIS_SECURE_LINK_TEXT));
  // Step 2: Found a viable phone.
  strings->SetString(
      "setupFoundPhoneHeaderTitle",
      l10n_util::GetStringFUTF16(
          IDS_EASY_UNLOCK_SETUP_FOUND_PHONE_HEADER_TITLE, device_type));
  strings->SetString(
      "setupFoundPhoneHeaderText",
      l10n_util::GetStringFUTF16(
          IDS_EASY_UNLOCK_SETUP_FOUND_PHONE_HEADER_TEXT, device_type));
  strings->SetString(
      "setupFoundPhoneUseThisPhoneButtonLabel",
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_SETUP_FOUND_PHONE_USE_THIS_PHONE_BUTTON_LABEL));
  strings->SetString(
      "setupPairingPhoneFailedButtonLabel",
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_SETUP_PAIRING_PHONE_FAILED_BUTTON_LABEL));
  // Step 3: Setup completed successfully.
  strings->SetString(
      "setupCompleteHeaderTitle",
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_SETUP_COMPLETE_HEADER_TITLE));
  strings->SetString(
      "setupCompleteHeaderText",
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_SETUP_COMPLETE_HEADER_TEXT));
  strings->SetString(
      "setupCompleteTryItOutButtonLabel",
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_SETUP_COMPLETE_TRY_IT_OUT_BUTTON_LABEL));
  strings->SetString(
      "setupCompleteSettingsLinkText",
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_SETUP_COMPLETE_SETTINGS_LINK_TEXT));

  // Error strings.
  strings->SetString(
      "setupErrorBluetoothUnavailable",
      l10n_util::GetStringFUTF16(
          IDS_EASY_UNLOCK_SETUP_ERROR_BLUETOOTH_UNAVAILBLE, device_type));
  strings->SetString(
      "setupErrorOffline",
      l10n_util::GetStringFUTF16(
          IDS_EASY_UNLOCK_SETUP_ERROR_OFFLINE, device_type));
  strings->SetString(
      "setupErrorFindingPhone",
      l10n_util::GetStringUTF16(IDS_EASY_UNLOCK_SETUP_ERROR_FINDING_PHONE));
  strings->SetString(
      "setupErrorBluetoothConnectionFailed",
      l10n_util::GetStringFUTF16(
          IDS_EASY_UNLOCK_SETUP_ERROR_BLUETOOTH_CONNECTION_FAILED,
          device_type));
  strings->SetString(
      "setupErrorConnectionToPhoneTimeout",
       l10n_util::GetStringFUTF16(
           IDS_EASY_UNLOCK_SETUP_ERROR_CONNECT_TO_PHONE_TIMEOUT,
           device_type));
  strings->SetString(
      "setupErrorSyncPhoneState",
       l10n_util::GetStringUTF16(
           IDS_EASY_UNLOCK_SETUP_ERROR_SYNC_PHONE_STATE_FAILED));
  strings->SetString(
      "setupErrorConnectingToPhone",
      l10n_util::GetStringFUTF16(
          IDS_EASY_UNLOCK_SETUP_ERROR_CONNECTING_TO_PHONE, device_type));

  // TODO(isherman): Remove these strings once the app has been updated.
  strings->SetString(
      "notificationTitle",
      l10n_util::GetStringFUTF16(IDS_EASY_UNLOCK_SETUP_NOTIFICATION_TITLE,
                                 device_type));
  strings->SetString(
      "notificationMessage",
      l10n_util::GetStringFUTF16(IDS_EASY_UNLOCK_SETUP_NOTIFICATION_MESSAGE,
                                 device_type));
  strings->SetString(
      "notificationButtonTitle",
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_SETUP_NOTIFICATION_BUTTON_TITLE));

  SetResult(strings.release());
  return true;
}

EasyUnlockPrivatePerformECDHKeyAgreementFunction::
EasyUnlockPrivatePerformECDHKeyAgreementFunction() {}

EasyUnlockPrivatePerformECDHKeyAgreementFunction::
~EasyUnlockPrivatePerformECDHKeyAgreementFunction() {}

bool EasyUnlockPrivatePerformECDHKeyAgreementFunction::RunAsync() {
  scoped_ptr<easy_unlock_private::PerformECDHKeyAgreement::Params> params =
      easy_unlock_private::PerformECDHKeyAgreement::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  GetCryptoDelegate(browser_context())->PerformECDHKeyAgreement(
      params->private_key,
      params->public_key,
      base::Bind(&EasyUnlockPrivatePerformECDHKeyAgreementFunction::OnData,
                 this));
  return true;
}

void EasyUnlockPrivatePerformECDHKeyAgreementFunction::OnData(
    const std::string& secret_key) {
  // TODO(tbarzic): Improve error handling.
  if (!secret_key.empty()) {
    results_ = easy_unlock_private::PerformECDHKeyAgreement::Results::Create(
        secret_key);
  }
  SendResponse(true);
}

EasyUnlockPrivateGenerateEcP256KeyPairFunction::
EasyUnlockPrivateGenerateEcP256KeyPairFunction() {}

EasyUnlockPrivateGenerateEcP256KeyPairFunction::
~EasyUnlockPrivateGenerateEcP256KeyPairFunction() {}

bool EasyUnlockPrivateGenerateEcP256KeyPairFunction::RunAsync() {
  GetCryptoDelegate(browser_context())->GenerateEcP256KeyPair(
      base::Bind(&EasyUnlockPrivateGenerateEcP256KeyPairFunction::OnData,
                 this));
  return true;
}

void EasyUnlockPrivateGenerateEcP256KeyPairFunction::OnData(
    const std::string& private_key,
    const std::string& public_key) {
  // TODO(tbarzic): Improve error handling.
  if (!public_key.empty() && !private_key.empty()) {
    results_ = easy_unlock_private::GenerateEcP256KeyPair::Results::Create(
        public_key, private_key);
  }
  SendResponse(true);
}

EasyUnlockPrivateCreateSecureMessageFunction::
EasyUnlockPrivateCreateSecureMessageFunction() {}

EasyUnlockPrivateCreateSecureMessageFunction::
~EasyUnlockPrivateCreateSecureMessageFunction() {}

bool EasyUnlockPrivateCreateSecureMessageFunction::RunAsync() {
  scoped_ptr<easy_unlock_private::CreateSecureMessage::Params> params =
      easy_unlock_private::CreateSecureMessage::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  GetCryptoDelegate(browser_context())->CreateSecureMessage(
      params->payload,
      params->key,
      params->options.associated_data ?
          *params->options.associated_data : std::string(),
      params->options.public_metadata ?
          *params->options.public_metadata : std::string(),
      params->options.verification_key_id ?
          *params->options.verification_key_id : std::string(),
      params->options.encrypt_type,
      params->options.sign_type,
      base::Bind(&EasyUnlockPrivateCreateSecureMessageFunction::OnData,
                 this));
  return true;
}

void EasyUnlockPrivateCreateSecureMessageFunction::OnData(
    const std::string& message) {
  // TODO(tbarzic): Improve error handling.
  if (!message.empty()) {
    results_ = easy_unlock_private::CreateSecureMessage::Results::Create(
        message);
  }
  SendResponse(true);
}

EasyUnlockPrivateUnwrapSecureMessageFunction::
EasyUnlockPrivateUnwrapSecureMessageFunction() {}

EasyUnlockPrivateUnwrapSecureMessageFunction::
~EasyUnlockPrivateUnwrapSecureMessageFunction() {}

bool EasyUnlockPrivateUnwrapSecureMessageFunction::RunAsync() {
  scoped_ptr<easy_unlock_private::UnwrapSecureMessage::Params> params =
      easy_unlock_private::UnwrapSecureMessage::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  GetCryptoDelegate(browser_context())->UnwrapSecureMessage(
      params->secure_message,
      params->key,
      params->options.associated_data ?
          *params->options.associated_data : std::string(),
      params->options.encrypt_type,
      params->options.sign_type,
      base::Bind(&EasyUnlockPrivateUnwrapSecureMessageFunction::OnData,
                 this));
  return true;
}

void EasyUnlockPrivateUnwrapSecureMessageFunction::OnData(
    const std::string& data) {
  // TODO(tbarzic): Improve error handling.
  if (!data.empty())
    results_ = easy_unlock_private::UnwrapSecureMessage::Results::Create(data);
  SendResponse(true);
}

EasyUnlockPrivateSeekBluetoothDeviceByAddressFunction::
    EasyUnlockPrivateSeekBluetoothDeviceByAddressFunction() {}

EasyUnlockPrivateSeekBluetoothDeviceByAddressFunction::
    ~EasyUnlockPrivateSeekBluetoothDeviceByAddressFunction() {}

bool EasyUnlockPrivateSeekBluetoothDeviceByAddressFunction::RunAsync() {
  scoped_ptr<easy_unlock_private::SeekBluetoothDeviceByAddress::Params> params(
      easy_unlock_private::SeekBluetoothDeviceByAddress::Params::Create(
          *args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  easy_unlock::SeekBluetoothDeviceByAddress(
      params->device_address,
      base::Bind(
          &EasyUnlockPrivateSeekBluetoothDeviceByAddressFunction::
              OnSeekCompleted,
          this));
  return true;
}

void EasyUnlockPrivateSeekBluetoothDeviceByAddressFunction::OnSeekCompleted(
    const easy_unlock::SeekDeviceResult& seek_result) {
  if (seek_result.success) {
    SendResponse(true);
  } else {
    SetError(seek_result.error_message);
    SendResponse(false);
  }
}

EasyUnlockPrivateUpdateScreenlockStateFunction::
    EasyUnlockPrivateUpdateScreenlockStateFunction() {}

EasyUnlockPrivateUpdateScreenlockStateFunction::
    ~EasyUnlockPrivateUpdateScreenlockStateFunction() {}

bool EasyUnlockPrivateUpdateScreenlockStateFunction::RunSync() {
  scoped_ptr<easy_unlock_private::UpdateScreenlockState::Params> params(
      easy_unlock_private::UpdateScreenlockState::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  EasyUnlockScreenlockStateHandler* screenlock_state_handler =
      GetScreenlockStateHandler(browser_context());
  if (screenlock_state_handler) {
    screenlock_state_handler->ChangeState(
        ToScreenlockStateHandlerState(params->state));
    return true;
  }

  SetError("Not allowed");
  return false;
}

EasyUnlockPrivateSetPermitAccessFunction::
    EasyUnlockPrivateSetPermitAccessFunction() {
}

EasyUnlockPrivateSetPermitAccessFunction::
    ~EasyUnlockPrivateSetPermitAccessFunction() {
}

bool EasyUnlockPrivateSetPermitAccessFunction::RunSync() {
  scoped_ptr<easy_unlock_private::SetPermitAccess::Params> params(
      easy_unlock_private::SetPermitAccess::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  Profile* profile = Profile::FromBrowserContext(browser_context());
  EasyUnlockService::Get(profile)
      ->SetPermitAccess(*params->permit_access.ToValue());

  return true;
}

EasyUnlockPrivateGetPermitAccessFunction::
    EasyUnlockPrivateGetPermitAccessFunction() {
}

EasyUnlockPrivateGetPermitAccessFunction::
    ~EasyUnlockPrivateGetPermitAccessFunction() {
}

bool EasyUnlockPrivateGetPermitAccessFunction::RunSync() {
  Profile* profile = Profile::FromBrowserContext(browser_context());
  const base::DictionaryValue* permit_value =
      EasyUnlockService::Get(profile)->GetPermitAccess();
  if (permit_value) {
    scoped_ptr<easy_unlock_private::PermitRecord> permit =
        easy_unlock_private::PermitRecord::FromValue(*permit_value);
    results_ = easy_unlock_private::GetPermitAccess::Results::Create(*permit);
  }

  return true;
}

EasyUnlockPrivateClearPermitAccessFunction::
    EasyUnlockPrivateClearPermitAccessFunction() {
}

EasyUnlockPrivateClearPermitAccessFunction::
    ~EasyUnlockPrivateClearPermitAccessFunction() {
}

bool EasyUnlockPrivateClearPermitAccessFunction::RunSync() {
  Profile* profile = Profile::FromBrowserContext(browser_context());
  EasyUnlockService::Get(profile)->ClearPermitAccess();
  return true;
}

EasyUnlockPrivateSetRemoteDevicesFunction::
    EasyUnlockPrivateSetRemoteDevicesFunction() {
}

EasyUnlockPrivateSetRemoteDevicesFunction::
    ~EasyUnlockPrivateSetRemoteDevicesFunction() {
}

bool EasyUnlockPrivateSetRemoteDevicesFunction::RunSync() {
  scoped_ptr<easy_unlock_private::SetRemoteDevices::Params> params(
      easy_unlock_private::SetRemoteDevices::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (params->devices.empty()) {
    EasyUnlockService::Get(profile)->ClearRemoteDevices();
  } else {
    base::ListValue devices;
    for (size_t i = 0; i < params->devices.size(); ++i) {
      devices.Append(params->devices[i]->ToValue().release());
    }
    EasyUnlockService::Get(profile)->SetRemoteDevices(devices);
  }

  return true;
}

EasyUnlockPrivateGetRemoteDevicesFunction::
    EasyUnlockPrivateGetRemoteDevicesFunction() {
}

EasyUnlockPrivateGetRemoteDevicesFunction::
    ~EasyUnlockPrivateGetRemoteDevicesFunction() {
}

bool EasyUnlockPrivateGetRemoteDevicesFunction::RunSync() {
  Profile* profile = Profile::FromBrowserContext(browser_context());
  const base::ListValue* devices =
      EasyUnlockService::Get(profile)->GetRemoteDevices();
  SetResult(devices ? devices->DeepCopy() : new base::ListValue());
  return true;
}

}  // namespace api
}  // namespace extensions
