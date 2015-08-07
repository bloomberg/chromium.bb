// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/webui/proximity_auth_webui_handler.h"

#include <algorithm>

#include "base/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/prefs/pref_service.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/default_clock.h"
#include "base/values.h"
#include "components/proximity_auth/bluetooth_connection.h"
#include "components/proximity_auth/bluetooth_util.h"
#include "components/proximity_auth/client_impl.h"
#include "components/proximity_auth/cryptauth/base64url.h"
#include "components/proximity_auth/cryptauth/cryptauth_enrollment_manager.h"
#include "components/proximity_auth/cryptauth/cryptauth_gcm_manager_impl.h"
#include "components/proximity_auth/cryptauth/proto/cryptauth_api.pb.h"
#include "components/proximity_auth/cryptauth/secure_message_delegate.h"
#include "components/proximity_auth/device_to_device_authenticator.h"
#include "components/proximity_auth/logging/logging.h"
#include "components/proximity_auth/remote_status_update.h"
#include "components/proximity_auth/secure_context.h"
#include "components/proximity_auth/webui/cryptauth_enroller_factory_impl.h"
#include "components/proximity_auth/webui/proximity_auth_ui_delegate.h"
#include "components/proximity_auth/webui/reachable_phone_flow.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui.h"
#include "device/bluetooth/bluetooth_uuid.h"

namespace proximity_auth {

namespace {

// The UUID of the Smart Lock Bluetooth service.
const char kBluetoothServiceUUID[] = "704EE561-3782-405A-A14B-2D47A2DDCDDF";

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
const char kExternalDeviceBluetoothAddress[] = "bluetoothAddress";
const char kExternalDeviceUnlockKey[] = "unlockKey";
const char kExternalDeviceConnectionStatus[] = "connectionStatus";
const char kExternalDeviceRemoteState[] = "remoteState";

// The possible values of the |kExternalDeviceConnectionStatus| field.
const char kExternalDeviceConnected[] = "connected";
const char kExternalDeviceDisconnected[] = "disconnected";
const char kExternalDeviceConnecting[] = "connecting";

// Keys in the JSON representation of an IneligibleDevice proto.
const char kIneligibleDeviceReasons[] = "ineligibilityReasons";

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
    : delegate_(delegate),
      web_contents_initialized_(false),
      weak_ptr_factory_(this) {
  cryptauth_client_factory_ = delegate_->CreateCryptAuthClientFactory();
}

ProximityAuthWebUIHandler::~ProximityAuthWebUIHandler() {
  LogBuffer::GetInstance()->RemoveObserver(this);
}

void ProximityAuthWebUIHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "clearLogBuffer", base::Bind(&ProximityAuthWebUIHandler::ClearLogBuffer,
                                   base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "getLogMessages", base::Bind(&ProximityAuthWebUIHandler::GetLogMessages,
                                   base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "toggleUnlockKey", base::Bind(&ProximityAuthWebUIHandler::ToggleUnlockKey,
                                    base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "findEligibleUnlockDevices",
      base::Bind(&ProximityAuthWebUIHandler::FindEligibleUnlockDevices,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "findReachableDevices",
      base::Bind(&ProximityAuthWebUIHandler::FindReachableDevices,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "getLocalState", base::Bind(&ProximityAuthWebUIHandler::GetLocalState,
                                  base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "forceEnrollment", base::Bind(&ProximityAuthWebUIHandler::ForceEnrollment,
                                    base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "forceDeviceSync", base::Bind(&ProximityAuthWebUIHandler::ForceDeviceSync,
                                    base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "toggleConnection",
      base::Bind(&ProximityAuthWebUIHandler::ToggleConnection,
                 base::Unretained(this)));

  InitGCMManager();
  InitEnrollmentManager();
  InitDeviceManager();

  // Note: We add the observer for the logs after initializing the managers
  // because when this function is called, the WebUI's underlying WebContents
  // has not been initialized, so calling any JavaScript function will crash
  // Chrome.
  LogBuffer::GetInstance()->AddObserver(this);
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
      "LocalStateInterface.onEnrollmentStateChanged",
      *GetEnrollmentStateDictionary());
}

void ProximityAuthWebUIHandler::OnEnrollmentFinished(bool success) {
  scoped_ptr<base::DictionaryValue> enrollment_state =
      GetEnrollmentStateDictionary();
  PA_LOG(INFO) << "Enrollment attempt completed with success=" << success
               << ":\n" << *enrollment_state;
  web_ui()->CallJavascriptFunction(
      "LocalStateInterface.onEnrollmentStateChanged", *enrollment_state);
}

void ProximityAuthWebUIHandler::OnSyncStarted() {
  web_ui()->CallJavascriptFunction(
      "LocalStateInterface.onDeviceSyncStateChanged",
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
      "LocalStateInterface.onDeviceSyncStateChanged", *device_sync_state);

  if (device_change_result ==
      CryptAuthDeviceManager::DeviceChangeResult::CHANGED) {
    scoped_ptr<base::ListValue> unlock_keys = GetUnlockKeysList();
    PA_LOG(INFO) << "New unlock keys obtained after device sync:\n"
                 << *unlock_keys;
    web_ui()->CallJavascriptFunction("LocalStateInterface.onUnlockKeysChanged",
                                     *unlock_keys);
  }
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

void ProximityAuthWebUIHandler::ToggleUnlockKey(const base::ListValue* args) {
  std::string public_key_b64, public_key;
  bool make_unlock_key;
  if (args->GetSize() != 2 || !args->GetString(0, &public_key_b64) ||
      !args->GetBoolean(1, &make_unlock_key) ||
      !Base64UrlDecode(public_key_b64, &public_key)) {
    PA_LOG(ERROR) << "Invalid arguments to toggleUnlockKey";
    return;
  }

  cryptauth::ToggleEasyUnlockRequest request;
  request.set_enable(make_unlock_key);
  request.set_public_key(public_key);
  *(request.mutable_device_classifier()) = delegate_->GetDeviceClassifier();

  PA_LOG(INFO) << "Toggling unlock key:\n"
               << "    public_key: " << public_key_b64 << "\n"
               << "    make_unlock_key: " << make_unlock_key;
  cryptauth_client_ = cryptauth_client_factory_->CreateInstance();
  cryptauth_client_->ToggleEasyUnlock(
      request, base::Bind(&ProximityAuthWebUIHandler::OnEasyUnlockToggled,
                          weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&ProximityAuthWebUIHandler::OnCryptAuthClientError,
                 weak_ptr_factory_.GetWeakPtr()));
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

void ProximityAuthWebUIHandler::FindReachableDevices(
    const base::ListValue* args) {
  if (reachable_phone_flow_) {
    PA_LOG(INFO) << "Waiting for existing ReachablePhoneFlow to finish.";
    return;
  }

  reachable_phone_flow_.reset(
      new ReachablePhoneFlow(cryptauth_client_factory_.get()));
  reachable_phone_flow_->Run(
      base::Bind(&ProximityAuthWebUIHandler::OnReachablePhonesFound,
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

void ProximityAuthWebUIHandler::ToggleConnection(const base::ListValue* args) {
  std::string b64_public_key;
  std::string public_key;
  if (!device_manager_ || !args->GetSize() ||
      !args->GetString(0, &b64_public_key) ||
      !Base64UrlDecode(b64_public_key, &public_key)) {
    return;
  }

  Connection* connection = GetConnection();
  for (const auto& unlock_key : device_manager_->unlock_keys()) {
    if (unlock_key.public_key() == public_key) {
      // Check if there is an existing connection to disconnect from first.
      if (connection && connection->IsConnected() &&
          connection->remote_device().public_key == public_key) {
        PA_LOG(INFO) << "Disconnecting from "
                     << unlock_key.friendly_device_name() << "["
                     << unlock_key.bluetooth_address() << "]";
        connection->Disconnect();
        return;
      }

      // Derive the PSK before connecting to the device.
      PA_LOG(INFO) << "Connecting to " << unlock_key.friendly_device_name()
                   << "[" << unlock_key.bluetooth_address() << "]";
      secure_message_delegate_ = delegate_->CreateSecureMessageDelegate();
      secure_message_delegate_->DeriveKey(
          user_private_key_, unlock_key.public_key(),
          base::Bind(&ProximityAuthWebUIHandler::OnPSKDerived,
                     weak_ptr_factory_.GetWeakPtr(), unlock_key));

      return;
    }
  }

  PA_LOG(ERROR) << "Unlock key (" << b64_public_key << ") not found";
}

void ProximityAuthWebUIHandler::InitGCMManager() {
  gcm_manager_.reset(new CryptAuthGCMManagerImpl(delegate_->GetGCMDriver(),
                                                 delegate_->GetPrefService()));
  gcm_manager_->StartListening();
}

void ProximityAuthWebUIHandler::InitEnrollmentManager() {
#if defined(OS_CHROMEOS)
  // TODO(tengs): We initialize a CryptAuthEnrollmentManager here for
  // development and testing purposes until it is ready to be moved into Chrome.
  // The public/private key pair has been generated and serialized in a previous
  // session.
  Base64UrlDecode(
      "CAESRgohAD1lP_wgQ8XqVVwz4aK_89SqdvAQG5L_NZH5zXxwg5UbEiEAZFMlgCZ9h8OlyE4"
      "QYKY5oiOBu0FmLSKeTAXEq2jnVJI=",
      &user_public_key_);

  Base64UrlDecode(
      "MIIBeQIBADCCAQMGByqGSM49AgEwgfcCAQEwLAYHKoZIzj0BAQIhAP____8AAAABAAAAAAA"
      "AAAAAAAAA________________MFsEIP____8AAAABAAAAAAAAAAAAAAAA______________"
      "_8BCBaxjXYqjqT57PrvVV2mIa8ZR0GsMxTsPY7zjw-J9JgSwMVAMSdNgiG5wSTamZ44ROdJ"
      "reBn36QBEEEaxfR8uEsQkf4vOblY6RA8ncDfYEt6zOg9KE5RdiYwpZP40Li_hp_m47n60p8"
      "D54WK84zV2sxXs7LtkBoN79R9QIhAP____8AAAAA__________-85vqtpxeehPO5ysL8YyV"
      "RAgEBBG0wawIBAQQgKZ4Dsm5xe4p5U2XPGxjrG376ZWWIa9E6r0y1BdjIntyhRANCAAQ9ZT"
      "_8IEPF6lVcM-Giv_PUqnbwEBuS_zWR-c18cIOVG2RTJYAmfYfDpchOEGCmOaIjgbtBZi0in"
      "kwFxKto51SS",
      &user_private_key_);

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
      user_public_key_, user_private_key_, device_info, gcm_manager_.get(),
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

void ProximityAuthWebUIHandler::OnEasyUnlockToggled(
    const cryptauth::ToggleEasyUnlockResponse& response) {
  web_ui()->CallJavascriptFunction("CryptAuthInterface.onUnlockKeyToggled");
  // TODO(tengs): Update the local state to reflect the toggle.
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

void ProximityAuthWebUIHandler::OnReachablePhonesFound(
    const std::vector<cryptauth::ExternalDeviceInfo>& reachable_phones) {
  reachable_phone_flow_.reset();
  base::ListValue device_list;
  for (const auto& external_device : reachable_phones) {
    device_list.Append(ExternalDeviceInfoToDictionary(external_device));
  }
  web_ui()->CallJavascriptFunction("CryptAuthInterface.onGotReachableDevices",
                                   device_list);
}

void ProximityAuthWebUIHandler::GetLocalState(const base::ListValue* args) {
  scoped_ptr<base::DictionaryValue> enrollment_state =
      GetEnrollmentStateDictionary();
  scoped_ptr<base::DictionaryValue> device_sync_state =
      GetDeviceSyncStateDictionary();
  scoped_ptr<base::ListValue> unlock_keys = GetUnlockKeysList();

  PA_LOG(INFO) << "==== Got Local State ====\n"
               << "Enrollment State: \n" << *enrollment_state
               << "Device Sync State: \n" << *device_sync_state
               << "Unlock Keys: \n" << *unlock_keys;
  web_ui()->CallJavascriptFunction("LocalStateInterface.onGotLocalState",
                                   *enrollment_state, *device_sync_state,
                                   *unlock_keys);
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

scoped_ptr<base::ListValue> ProximityAuthWebUIHandler::GetUnlockKeysList() {
  scoped_ptr<base::ListValue> unlock_keys(new base::ListValue());
  if (!device_manager_)
    return unlock_keys;

  for (const auto& unlock_key : device_manager_->unlock_keys()) {
    unlock_keys->Append(ExternalDeviceInfoToDictionary(unlock_key));
  }

  return unlock_keys;
}

Connection* ProximityAuthWebUIHandler::GetConnection() {
  Connection* connection = bluetooth_connection_.get();
  if (client_) {
    DCHECK(!connection);
    connection = client_->connection();
  }
  return connection;
}

void ProximityAuthWebUIHandler::OnPSKDerived(
    const cryptauth::ExternalDeviceInfo& unlock_key,
    const std::string& persistent_symmetric_key) {
  if (persistent_symmetric_key.empty()) {
    PA_LOG(ERROR) << "Failed to derive PSK.";
    return;
  }

  RemoteDevice remote_device(
      unlock_key.friendly_device_name(), unlock_key.public_key(),
      unlock_key.bluetooth_address(), persistent_symmetric_key);

  bluetooth_connection_.reset(new BluetoothConnection(
      remote_device, device::BluetoothUUID(kBluetoothServiceUUID)));
  bluetooth_connection_->AddObserver(this);

  // This SeekDeviceByAddress operation is needed to connect to a device if
  // it is not already known to the local device.
  bluetooth_util::SeekDeviceByAddress(
      remote_device.bluetooth_address,
      base::Bind(&ProximityAuthWebUIHandler::OnSeekedDeviceByAddress,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&ProximityAuthWebUIHandler::OnSeekedDeviceByAddressError,
                 weak_ptr_factory_.GetWeakPtr()),
      content::BrowserThread::GetBlockingPool()
          ->GetTaskRunnerWithShutdownBehavior(
              base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN)
          .get());
}

void ProximityAuthWebUIHandler::OnSeekedDeviceByAddress() {
  PA_LOG(INFO) << "Found Bluetooth device: "
               << bluetooth_connection_->remote_device().bluetooth_address;
  bluetooth_connection_->Connect();
}

void ProximityAuthWebUIHandler::OnSeekedDeviceByAddressError(
    const std::string& error_message) {
  PA_LOG(WARNING) << "Failed to seek device by address: "
                  << bluetooth_connection_->remote_device().bluetooth_address;
}

void ProximityAuthWebUIHandler::OnAuthenticationResult(
    Authenticator::Result result,
    scoped_ptr<SecureContext> secure_context) {
  secure_context_ = secure_context.Pass();

  // Create the ClientImpl asynchronously. |client_| registers itself as an
  // observer of |bluetooth_connection_|, so creating it synchronously would
  // trigger |OnSendComplete()| as an observer call for |client_|.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&ProximityAuthWebUIHandler::CreateStatusUpdateClient,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ProximityAuthWebUIHandler::CreateStatusUpdateClient() {
  client_.reset(
      new ClientImpl(bluetooth_connection_.Pass(), secure_context_.Pass()));
  client_->AddObserver(this);
}

scoped_ptr<base::DictionaryValue>
ProximityAuthWebUIHandler::ExternalDeviceInfoToDictionary(
    const cryptauth::ExternalDeviceInfo& device_info) {
  std::string base64_public_key;
  Base64UrlEncode(device_info.public_key(), &base64_public_key);

  // Set the fields in the ExternalDeviceInfo proto.
  scoped_ptr<base::DictionaryValue> dictionary(new base::DictionaryValue());
  dictionary->SetString(kExternalDevicePublicKey, base64_public_key);
  dictionary->SetString(kExternalDeviceFriendlyName,
                        device_info.friendly_device_name());
  dictionary->SetString(kExternalDeviceBluetoothAddress,
                        device_info.bluetooth_address());
  dictionary->SetBoolean(kExternalDeviceUnlockKey, device_info.unlock_key());
  dictionary->SetString(kExternalDeviceConnectionStatus,
                        kExternalDeviceDisconnected);

  if (!device_manager_)
    return dictionary;

  // If |device_info| is a known unlock key, then combine the proto data with
  // the corresponding local device data (e.g. connection status and remote
  // status updates).
  std::string public_key = device_info.public_key();
  auto iterator = std::find_if(
      device_manager_->unlock_keys().begin(),
      device_manager_->unlock_keys().end(),
      [&public_key](const cryptauth::ExternalDeviceInfo& unlock_key) {
        return unlock_key.public_key() == public_key;
      });

  if (iterator == device_manager_->unlock_keys().end())
    return dictionary;

  // Fill in the current Bluetooth connection status.
  Connection* connection = GetConnection();
  if (!connection ||
      connection->remote_device().public_key != device_info.public_key())
    return dictionary;

  std::string connection_status = kExternalDeviceDisconnected;
  if (connection->IsConnected()) {
    connection_status = kExternalDeviceConnected;
  } else if (connection->status() == Connection::IN_PROGRESS) {
    connection_status = kExternalDeviceConnecting;
  }
  dictionary->SetString(kExternalDeviceConnectionStatus, connection_status);

  // Fill the remote status dictionary.
  if (last_remote_status_update_) {
    scoped_ptr<base::DictionaryValue> status_dictionary(
        new base::DictionaryValue());
    status_dictionary->SetInteger("userPresent",
                                  last_remote_status_update_->user_presence);
    status_dictionary->SetInteger(
        "secureScreenLock",
        last_remote_status_update_->secure_screen_lock_state);
    status_dictionary->SetInteger(
        "trustAgent", last_remote_status_update_->trust_agent_state);
    dictionary->Set(kExternalDeviceRemoteState, status_dictionary.Pass());
  }

  return dictionary;
}

scoped_ptr<base::DictionaryValue>
ProximityAuthWebUIHandler::IneligibleDeviceToDictionary(
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

void ProximityAuthWebUIHandler::OnConnectionStatusChanged(
    Connection* connection,
    Connection::Status old_status,
    Connection::Status new_status) {
  PA_LOG(INFO) << "Connection status changed from " << old_status << " to "
               << new_status;
  if (new_status == Connection::CONNECTED) {
    authenticator_.reset(new DeviceToDeviceAuthenticator(
        connection, delegate_->GetAccountId(),
        delegate_->CreateSecureMessageDelegate()));
    authenticator_->Authenticate(
        base::Bind(&ProximityAuthWebUIHandler::OnAuthenticationResult,
                   weak_ptr_factory_.GetWeakPtr()));
  } else if (new_status == Connection::DISCONNECTED) {
    last_remote_status_update_.reset();
  }

  scoped_ptr<base::ListValue> unlock_keys = GetUnlockKeysList();
  web_ui()->CallJavascriptFunction("LocalStateInterface.onUnlockKeysChanged",
                                   *unlock_keys);
}

void ProximityAuthWebUIHandler::OnMessageReceived(const Connection& connection,
                                                  const WireMessage& message) {
  std::string address = connection.remote_device().bluetooth_address;
  PA_LOG(INFO) << "Message received from " << address;
}

void ProximityAuthWebUIHandler::OnRemoteStatusUpdate(
    const RemoteStatusUpdate& status_update) {
  PA_LOG(INFO) << "Remote status update:"
               << "\n  user_presence: "
               << static_cast<int>(status_update.user_presence)
               << "\n  secure_screen_lock_state: "
               << static_cast<int>(status_update.secure_screen_lock_state)
               << "\n  trust_agent_state: "
               << static_cast<int>(status_update.trust_agent_state);

  last_remote_status_update_.reset(new RemoteStatusUpdate(status_update));
  scoped_ptr<base::ListValue> unlock_keys = GetUnlockKeysList();
  web_ui()->CallJavascriptFunction("LocalStateInterface.onUnlockKeysChanged",
                                   *unlock_keys);
}

}  // namespace proximity_auth
