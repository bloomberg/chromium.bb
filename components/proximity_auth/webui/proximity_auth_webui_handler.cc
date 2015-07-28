// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/webui/proximity_auth_webui_handler.h"

#include "base/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/prefs/pref_service.h"
#include "base/time/default_clock.h"
#include "base/values.h"
#include "components/proximity_auth/cryptauth/base64url.h"
#include "components/proximity_auth/cryptauth/cryptauth_enrollment_manager.h"
#include "components/proximity_auth/cryptauth/cryptauth_gcm_manager_impl.h"
#include "components/proximity_auth/cryptauth/proto/cryptauth_api.pb.h"
#include "components/proximity_auth/logging/logging.h"
#include "components/proximity_auth/webui/cryptauth_enroller_factory_impl.h"
#include "components/proximity_auth/webui/proximity_auth_ui_delegate.h"
#include "content/public/browser/web_ui.h"

namespace proximity_auth {

namespace {

// Keys in the JSON representation of a log message.
const char kLogMessageTextKey[] = "text";
const char kLogMessageTimeKey[] = "time";
const char kLogMessageFileKey[] = "file";
const char kLogMessageLineKey[] = "line";
const char kLogMessageSeverityKey[] = "severity";

// Keys in the JSON representation of a SyncState object for enrollment or
// device sync.
const char kSyncStateLastSuccessTime[] = "lastSuccessTime";
const char kSyncStateNextRefreshTime[] = "nextRefreshTime";
const char kSyncStateRecoveringFromFailure[] = "recoveringFromFailure";
const char kSyncStateOperationInProgress[] = "operationInProgress";

// Converts |log_message| to a raw dictionary value used as a JSON argument to
// JavaScript functions.
scoped_ptr<base::DictionaryValue> LogMessageToDictionary(
    const LogBuffer::LogMessage& log_message) {
  scoped_ptr<base::DictionaryValue> dictionary(new base::DictionaryValue());
  dictionary->SetString(kLogMessageTextKey, log_message.text);
  dictionary->SetString(
      kLogMessageTimeKey,
      base::TimeFormatTimeOfDayWithMilliseconds(log_message.time));
  dictionary->SetString(kLogMessageFileKey, log_message.file);
  dictionary->SetInteger(kLogMessageLineKey, log_message.line);
  dictionary->SetInteger(kLogMessageSeverityKey,
                         static_cast<int>(log_message.severity));
  return dictionary.Pass();
}

// Keys in the JSON representation of an ExternalDeviceInfo proto.
const char kExternalDevicePublicKey[] = "publicKey";
const char kExternalDeviceFriendlyName[] = "friendlyDeviceName";
const char kExternalDeviceUnlockKey[] = "unlockKey";
const char kExternalDeviceConnectionStatus[] = "connectionStatus";

// The possible values of the |kExternalDeviceConnectionStatus| field.
const char kExternalDeviceDisconnected[] = "disconnected";

// Converts an ExternalDeviceInfo proto to a JSON dictionary used in JavaScript.
scoped_ptr<base::DictionaryValue> ExternalDeviceInfoToDictionary(
    const cryptauth::ExternalDeviceInfo& device_info) {
  std::string base64_public_key;
  Base64UrlEncode(device_info.public_key(), &base64_public_key);

  scoped_ptr<base::DictionaryValue> dictionary(new base::DictionaryValue());
  dictionary->SetString(kExternalDevicePublicKey, base64_public_key);
  dictionary->SetString(kExternalDeviceFriendlyName,
                        device_info.friendly_device_name());
  dictionary->SetBoolean(kExternalDeviceUnlockKey, device_info.unlock_key());
  dictionary->SetString(kExternalDeviceConnectionStatus,
                        kExternalDeviceDisconnected);
  return dictionary.Pass();
}

// Keys in the JSON representation of an IneligibleDevice proto.
const char kIneligibleDeviceReasons[] = "ineligibilityReasons";

// Converts an IneligibleDevice proto to a JSON dictionary used in JavaScript.
scoped_ptr<base::DictionaryValue> IneligibleDeviceToDictionary(
    const cryptauth::IneligibleDevice& ineligible_device) {
  scoped_ptr<base::ListValue> ineligibility_reasons(new base::ListValue());
  for (const std::string& reason : ineligible_device.reasons()) {
    ineligibility_reasons->AppendString(reason);
  }

  scoped_ptr<base::DictionaryValue> device_dictionary =
      ExternalDeviceInfoToDictionary(ineligible_device.device());
  device_dictionary->Set(kIneligibleDeviceReasons,
                         ineligibility_reasons.Pass());
  return device_dictionary;
}

// Creates a SyncState JSON object that can be passed to the WebUI.
scoped_ptr<base::DictionaryValue> CreateSyncStateDictionary(
    double last_success_time,
    double next_refresh_time,
    bool is_recovering_from_failure,
    bool is_enrollment_in_progress) {
  scoped_ptr<base::DictionaryValue> sync_state(new base::DictionaryValue());
  sync_state->SetDouble(kSyncStateLastSuccessTime, last_success_time);
  sync_state->SetDouble(kSyncStateNextRefreshTime, next_refresh_time);
  sync_state->SetBoolean(kSyncStateRecoveringFromFailure,
                         is_recovering_from_failure);
  sync_state->SetBoolean(kSyncStateOperationInProgress,
                         is_enrollment_in_progress);
  return sync_state;
}

}  // namespace

ProximityAuthWebUIHandler::ProximityAuthWebUIHandler(
    ProximityAuthUIDelegate* delegate)
    : delegate_(delegate), weak_ptr_factory_(this) {
  cryptauth_client_factory_ = delegate_->CreateCryptAuthClientFactory();
}

ProximityAuthWebUIHandler::~ProximityAuthWebUIHandler() {
  LogBuffer::GetInstance()->RemoveObserver(this);
  if (enrollment_manager_)
    enrollment_manager_->RemoveObserver(this);
}

void ProximityAuthWebUIHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "clearLogBuffer", base::Bind(&ProximityAuthWebUIHandler::ClearLogBuffer,
                                   base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "getLogMessages", base::Bind(&ProximityAuthWebUIHandler::GetLogMessages,
                                   base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "findEligibleUnlockDevices",
      base::Bind(&ProximityAuthWebUIHandler::FindEligibleUnlockDevices,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "getSyncStates", base::Bind(&ProximityAuthWebUIHandler::GetSyncStates,
                                  base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "forceEnrollment", base::Bind(&ProximityAuthWebUIHandler::ForceEnrollment,
                                    base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "forceDeviceSync", base::Bind(&ProximityAuthWebUIHandler::ForceDeviceSync,
                                    base::Unretained(this)));

  LogBuffer::GetInstance()->AddObserver(this);

  InitGCMManager();
  InitEnrollmentManager();
  InitDeviceManager();
}

void ProximityAuthWebUIHandler::OnLogMessageAdded(
    const LogBuffer::LogMessage& log_message) {
  scoped_ptr<base::DictionaryValue> dictionary =
      LogMessageToDictionary(log_message);
  web_ui()->CallJavascriptFunction("LogBufferInterface.onLogMessageAdded",
                                   *dictionary);
}

void ProximityAuthWebUIHandler::OnLogBufferCleared() {
  web_ui()->CallJavascriptFunction("LogBufferInterface.onLogBufferCleared");
}

void ProximityAuthWebUIHandler::OnEnrollmentStarted() {
  web_ui()->CallJavascriptFunction(
      "SyncStateInterface.onEnrollmentStateChanged",
      *GetEnrollmentStateDictionary());
}

void ProximityAuthWebUIHandler::OnEnrollmentFinished(bool success) {
  scoped_ptr<base::DictionaryValue> enrollment_state =
      GetEnrollmentStateDictionary();
  PA_LOG(INFO) << "Enrollment attempt completed with success=" << success
               << ":\n" << *enrollment_state;
  web_ui()->CallJavascriptFunction(
      "SyncStateInterface.onEnrollmentStateChanged", *enrollment_state);
}

void ProximityAuthWebUIHandler::OnSyncStarted() {
  web_ui()->CallJavascriptFunction(
      "SyncStateInterface.onDeviceSyncStateChanged",
      *GetDeviceSyncStateDictionary());
}

void ProximityAuthWebUIHandler::OnSyncFinished(
    CryptAuthDeviceManager::SyncResult sync_result,
    CryptAuthDeviceManager::DeviceChangeResult device_change_result) {
  scoped_ptr<base::DictionaryValue> device_sync_state =
      GetDeviceSyncStateDictionary();
  PA_LOG(INFO) << "Device sync completed with result="
               << static_cast<int>(sync_result) << ":\n" << *device_sync_state;
  web_ui()->CallJavascriptFunction(
      "SyncStateInterface.onDeviceSyncStateChanged", *device_sync_state);
}

void ProximityAuthWebUIHandler::GetLogMessages(const base::ListValue* args) {
  base::ListValue json_logs;
  for (const auto& log : *LogBuffer::GetInstance()->logs()) {
    json_logs.Append(LogMessageToDictionary(log).release());
  }
  web_ui()->CallJavascriptFunction("LogBufferInterface.onGotLogMessages",
                                   json_logs);
}

void ProximityAuthWebUIHandler::ClearLogBuffer(const base::ListValue* args) {
  // The OnLogBufferCleared() observer function will be called after the buffer
  // is cleared.
  LogBuffer::GetInstance()->Clear();
}

void ProximityAuthWebUIHandler::FindEligibleUnlockDevices(
    const base::ListValue* args) {
  cryptauth_client_ = cryptauth_client_factory_->CreateInstance();

  cryptauth::FindEligibleUnlockDevicesRequest request;
  *(request.mutable_device_classifier()) = delegate_->GetDeviceClassifier();
  cryptauth_client_->FindEligibleUnlockDevices(
      request,
      base::Bind(&ProximityAuthWebUIHandler::OnFoundEligibleUnlockDevices,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&ProximityAuthWebUIHandler::OnCryptAuthClientError,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ProximityAuthWebUIHandler::ForceEnrollment(const base::ListValue* args) {
  if (enrollment_manager_) {
    enrollment_manager_->ForceEnrollmentNow(
        cryptauth::INVOCATION_REASON_MANUAL);
  }
}

void ProximityAuthWebUIHandler::ForceDeviceSync(const base::ListValue* args) {
  if (device_manager_)
    device_manager_->ForceSyncNow(cryptauth::INVOCATION_REASON_MANUAL);
}

void ProximityAuthWebUIHandler::InitGCMManager() {
  gcm_manager_.reset(new CryptAuthGCMManagerImpl(delegate_->GetGCMDriver(),
                                                 delegate_->GetPrefService()));
}

void ProximityAuthWebUIHandler::InitEnrollmentManager() {
#if defined(OS_CHROMEOS)
  // TODO(tengs): We initialize a CryptAuthEnrollmentManager here for
  // development and testing purposes until it is ready to be moved into Chrome.
  // The public/private key pair has been generated and serialized in a previous
  // session.
  std::string user_public_key;
  Base64UrlDecode(
      "CAESRgohAD1lP_wgQ8XqVVwz4aK_89SqdvAQG5L_NZH5zXxwg5UbEiEAZFMlgCZ9h8OlyE4"
      "QYKY5oiOBu0FmLSKeTAXEq2jnVJI=",
      &user_public_key);

  std::string user_private_key;
  Base64UrlDecode(
      "MIIBeQIBADCCAQMGByqGSM49AgEwgfcCAQEwLAYHKoZIzj0BAQIhAP____8AAAABAAAAAAA"
      "AAAAAAAAA________________MFsEIP____8AAAABAAAAAAAAAAAAAAAA______________"
      "_8BCBaxjXYqjqT57PrvVV2mIa8ZR0GsMxTsPY7zjw-J9JgSwMVAMSdNgiG5wSTamZ44ROdJ"
      "reBn36QBEEEaxfR8uEsQkf4vOblY6RA8ncDfYEt6zOg9KE5RdiYwpZP40Li_hp_m47n60p8"
      "D54WK84zV2sxXs7LtkBoN79R9QIhAP____8AAAAA__________-85vqtpxeehPO5ysL8YyV"
      "RAgEBBG0wawIBAQQgKZ4Dsm5xe4p5U2XPGxjrG376ZWWIa9E6r0y1BdjIntyhRANCAAQ9ZT"
      "_8IEPF6lVcM-Giv_PUqnbwEBuS_zWR-c18cIOVG2RTJYAmfYfDpchOEGCmOaIjgbtBZi0in"
      "kwFxKto51SS",
      &user_private_key);

  // This serialized DeviceInfo proto was previously captured from a real
  // CryptAuth enrollment, and is replayed here for testing purposes.
  std::string serialized_device_info;
  Base64UrlDecode(
      "IkoIARJGCiEAX_ZjLSq73EVcrarX-7l7No7nSP86GEC322ocSZKqUKwSIQDbEDu9KN7AgLM"
      "v_lzZZNui9zSOgXCeDpLhS2tgrYVXijoEbGlua0IFZW4tVVNKSggBEkYKIQBf9mMtKrvcRV"
      "ytqtf7uXs2judI_zoYQLfbahxJkqpQrBIhANsQO70o3sCAsy_-XNlk26L3NI6BcJ4OkuFLa"
      "2CthVeKam9Nb3ppbGxhLzUuMCAoWDExOyBDck9TIHg4Nl82NCA3MTM0LjAuMCkgQXBwbGVX"
      "ZWJLaXQvNTM3LjM2IChLSFRNTCwgbGlrZSBHZWNrbykgQ2hyb21lLzQ1LjAuMjQyMi4wIFN"
      "hZmFyaS81MzcuMzZwLYoBAzEuMJABAZoBIG1rYWVtaWdob2xlYmNnY2hsa2JhbmttaWhrbm"
      "9qZWFrsAHDPuoBHEJLZEluZWxFZk05VG1adGV3eTRGb19RV1Vicz2AAgKyBqIBQVBBOTFiS"
      "FZDdlJJNGJFSXppMmFXOTBlZ044eHFBYkhWYnJwSVFuMTk3bWltd3RWWTZYN0JEcEI4Szg3"
      "RjRubkJnejdLX1BQV2xkcUtDRVhiZkFiMGwyN1VaQXgtVjBWbEE4WlFwdkhETmpHVlh4RlV"
      "WRDFNY1AzNTgtYTZ3eHRpVG5LQnpMTEVIT1F6Ujdpb0lUMzRtWWY1VmNhbmhPZDh3ugYgs9"
      "7-c7qNUzzLeEqVCDXb_EaJ8wC3iie_Lpid44iuAh3CPo0CCugBCiMIARACGgi5wHHa82avM"
      "ioQ7y8xhiUBs7Um73ZC1vQlzzIBABLAAeCqGnWF7RwtnmdfIQJoEqXoXrH1qLw4yqUAA1TW"
      "M1qxTepJOdDHrh54eiejobW0SKpHqTlZIyiK3ObHAPdfzFum1l640RFdFGZTTTksZFqfD9O"
      "dftoi0pMrApob4gXj8Pv2g22ArX55BiH56TkTIcDcEE3KKnA_2G0INT1y_clZvZfDw1n0WP"
      "0Xdg1PLLCOb46WfDWUhHvUk3GzUce8xyxsjOkiZUNh8yvhFXaP2wJgVKVWInf0inuofo9Za"
      "7p44hIgHgKJIr_4fuVs9Ojf0KcMzxoJTbFUGg58jglUAKFfJBLKPpMBeWEyOS5pQUdTNFZ1"
      "bF9JVWY4YTJDSmJNbXFqaWpYUFYzaVV5dmJXSVRrR3d1bFRaVUs3RGVZczJtT0h5ZkQ1NWR"
      "HRXEtdnJTdVc4VEZ2Z1haa2xhVEZTN0dqM2xCVUktSHd5Z0h6bHZHX2NGLWtzQmw0dXdveG"
      "VPWE1hRlJ3WGJHVUU1Tm9sLS1mdkRIcGVZVnJR",
      &serialized_device_info);
  cryptauth::GcmDeviceInfo device_info;
  device_info.ParseFromString(serialized_device_info);

  enrollment_manager_.reset(new CryptAuthEnrollmentManager(
      make_scoped_ptr(new base::DefaultClock()),
      make_scoped_ptr(new CryptAuthEnrollerFactoryImpl(delegate_)),
      user_public_key, user_private_key, device_info, gcm_manager_.get(),
      delegate_->GetPrefService()));
  enrollment_manager_->AddObserver(this);
  enrollment_manager_->Start();
#endif
}

void ProximityAuthWebUIHandler::InitDeviceManager() {
  // TODO(tengs): We initialize a CryptAuthDeviceManager here for
  // development and testing purposes until it is ready to be moved into Chrome.
  device_manager_.reset(new CryptAuthDeviceManager(
      make_scoped_ptr(new base::DefaultClock()),
      delegate_->CreateCryptAuthClientFactory(), gcm_manager_.get(),
      delegate_->GetPrefService()));
  device_manager_->AddObserver(this);
  device_manager_->Start();
}

void ProximityAuthWebUIHandler::OnCryptAuthClientError(
    const std::string& error_message) {
  PA_LOG(WARNING) << "CryptAuth request failed: " << error_message;
  base::StringValue error_string(error_message);
  web_ui()->CallJavascriptFunction("CryptAuthInterface.onError", error_string);
}

void ProximityAuthWebUIHandler::OnFoundEligibleUnlockDevices(
    const cryptauth::FindEligibleUnlockDevicesResponse& response) {
  base::ListValue eligible_devices;
  for (const auto& external_device : response.eligible_devices()) {
    eligible_devices.Append(ExternalDeviceInfoToDictionary(external_device));
  }

  base::ListValue ineligible_devices;
  for (const auto& ineligible_device : response.ineligible_devices()) {
    ineligible_devices.Append(IneligibleDeviceToDictionary(ineligible_device));
  }

  PA_LOG(INFO) << "Found " << eligible_devices.GetSize()
               << " eligible devices and " << ineligible_devices.GetSize()
               << " ineligible devices.";
  web_ui()->CallJavascriptFunction("CryptAuthInterface.onGotEligibleDevices",
                                   eligible_devices, ineligible_devices);
}

void ProximityAuthWebUIHandler::GetSyncStates(const base::ListValue* args) {
  scoped_ptr<base::DictionaryValue> enrollment_state =
      GetEnrollmentStateDictionary();
  scoped_ptr<base::DictionaryValue> device_sync_state =
      GetDeviceSyncStateDictionary();
  PA_LOG(INFO) << "Enrollment State: \n" << *enrollment_state
               << "Device Sync State: \n" << *device_sync_state;
  web_ui()->CallJavascriptFunction("SyncStateInterface.onGotSyncStates",
                                   *enrollment_state, *device_sync_state);
}

scoped_ptr<base::DictionaryValue>
ProximityAuthWebUIHandler::GetEnrollmentStateDictionary() {
  if (!enrollment_manager_)
    return make_scoped_ptr(new base::DictionaryValue());

  return CreateSyncStateDictionary(
      enrollment_manager_->GetLastEnrollmentTime().ToJsTime(),
      enrollment_manager_->GetTimeToNextAttempt().InMillisecondsF(),
      enrollment_manager_->IsRecoveringFromFailure(),
      enrollment_manager_->IsEnrollmentInProgress());
}

scoped_ptr<base::DictionaryValue>
ProximityAuthWebUIHandler::GetDeviceSyncStateDictionary() {
  if (!device_manager_)
    return make_scoped_ptr(new base::DictionaryValue());

  return CreateSyncStateDictionary(
      device_manager_->GetLastSyncTime().ToJsTime(),
      device_manager_->GetTimeToNextAttempt().InMillisecondsF(),
      device_manager_->IsRecoveringFromFailure(),
      device_manager_->IsSyncInProgress());
}

}  // namespace proximity_auth
