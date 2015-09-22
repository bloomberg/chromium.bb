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
#include "base/time/default_tick_clock.h"
#include "base/values.h"
#include "components/proximity_auth/ble/bluetooth_low_energy_connection_finder.h"
#include "components/proximity_auth/ble/bluetooth_low_energy_device_whitelist.h"
#include "components/proximity_auth/ble/pref_names.h"
#include "components/proximity_auth/bluetooth_connection_finder.h"
#include "components/proximity_auth/bluetooth_throttler_impl.h"
#include "components/proximity_auth/cryptauth/base64url.h"
#include "components/proximity_auth/cryptauth/cryptauth_enrollment_manager.h"
#include "components/proximity_auth/cryptauth/proto/cryptauth_api.pb.h"
#include "components/proximity_auth/cryptauth/secure_message_delegate.h"
#include "components/proximity_auth/device_to_device_authenticator.h"
#include "components/proximity_auth/logging/logging.h"
#include "components/proximity_auth/messenger_impl.h"
#include "components/proximity_auth/remote_status_update.h"
#include "components/proximity_auth/secure_context.h"
#include "components/proximity_auth/webui/reachable_phone_flow.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui.h"
#include "device/bluetooth/bluetooth_uuid.h"

namespace proximity_auth {

namespace {

// The UUID of the Smart Lock classic Bluetooth service.
const char kClassicBluetoothServiceUUID[] =
    "704EE561-3782-405A-A14B-2D47A2DDCDDF";

// The UUID of the Bluetooth Low Energy service.
const char kBLESmartLockServiceUUID[] = "b3b7e28e-a000-3e17-bd86-6e97b9e28c11";

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
    ProximityAuthClient* proximity_auth_client)
    : proximity_auth_client_(proximity_auth_client),
      web_contents_initialized_(false),
      weak_ptr_factory_(this) {
  cryptauth_client_factory_ =
      proximity_auth_client_->CreateCryptAuthClientFactory();
}

ProximityAuthWebUIHandler::~ProximityAuthWebUIHandler() {
  LogBuffer::GetInstance()->RemoveObserver(this);
}

void ProximityAuthWebUIHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "onWebContentsInitialized",
      base::Bind(&ProximityAuthWebUIHandler::OnWebContentsInitialized,
                 base::Unretained(this)));

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

void ProximityAuthWebUIHandler::OnWebContentsInitialized(
    const base::ListValue* args) {
  if (!web_contents_initialized_) {
    CryptAuthEnrollmentManager* enrollment_manager =
        proximity_auth_client_->GetCryptAuthEnrollmentManager();
    if (enrollment_manager)
      enrollment_manager->AddObserver(this);

    CryptAuthDeviceManager* device_manager =
        proximity_auth_client_->GetCryptAuthDeviceManager();
    if (device_manager)
      device_manager->AddObserver(this);

    LogBuffer::GetInstance()->AddObserver(this);

    web_contents_initialized_ = true;
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
  *(request.mutable_device_classifier()) =
      proximity_auth_client_->GetDeviceClassifier();

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
  *(request.mutable_device_classifier()) =
      proximity_auth_client_->GetDeviceClassifier();
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
  CryptAuthEnrollmentManager* enrollment_manager =
      proximity_auth_client_->GetCryptAuthEnrollmentManager();
  if (enrollment_manager) {
    enrollment_manager->ForceEnrollmentNow(cryptauth::INVOCATION_REASON_MANUAL);
  }
}

void ProximityAuthWebUIHandler::ForceDeviceSync(const base::ListValue* args) {
  CryptAuthDeviceManager* device_manager =
      proximity_auth_client_->GetCryptAuthDeviceManager();
  if (device_manager)
    device_manager->ForceSyncNow(cryptauth::INVOCATION_REASON_MANUAL);
}

void ProximityAuthWebUIHandler::ToggleConnection(const base::ListValue* args) {
  CryptAuthEnrollmentManager* enrollment_manager =
      proximity_auth_client_->GetCryptAuthEnrollmentManager();
  CryptAuthDeviceManager* device_manager =
      proximity_auth_client_->GetCryptAuthDeviceManager();
  if (!enrollment_manager || !device_manager)
    return;

  std::string b64_public_key;
  std::string public_key;
  if (!enrollment_manager || !device_manager || !args->GetSize() ||
      !args->GetString(0, &b64_public_key) ||
      !Base64UrlDecode(b64_public_key, &public_key)) {
    return;
  }

  Connection* connection = GetConnection();
  for (const auto& unlock_key : device_manager->unlock_keys()) {
    if (unlock_key.public_key() == public_key) {
      // Check if there is an existing connection to disconnect from first.
      if (connection && connection->IsConnected() &&
          selected_remote_device_.public_key == public_key) {
        PA_LOG(INFO) << "Disconnecting from "
                     << unlock_key.friendly_device_name() << "["
                     << unlock_key.bluetooth_address() << "]";
        connection->Disconnect();
        return;
      }

      // Derive the PSK before connecting to the device.
      PA_LOG(INFO) << "Deriving PSK before connecting to "
                   << unlock_key.friendly_device_name();
      secure_message_delegate_ =
          proximity_auth_client_->CreateSecureMessageDelegate();
      secure_message_delegate_->DeriveKey(
          enrollment_manager->GetUserPrivateKey(), unlock_key.public_key(),
          base::Bind(&ProximityAuthWebUIHandler::OnPSKDerived,
                     weak_ptr_factory_.GetWeakPtr(), unlock_key));

      return;
    }
  }

  PA_LOG(ERROR) << "Unlock key (" << b64_public_key << ") not found";
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
  CryptAuthEnrollmentManager* enrollment_manager =
      proximity_auth_client_->GetCryptAuthEnrollmentManager();
  if (!enrollment_manager)
    return make_scoped_ptr(new base::DictionaryValue());

  return CreateSyncStateDictionary(
      enrollment_manager->GetLastEnrollmentTime().ToJsTime(),
      enrollment_manager->GetTimeToNextAttempt().InMillisecondsF(),
      enrollment_manager->IsRecoveringFromFailure(),
      enrollment_manager->IsEnrollmentInProgress());
}

scoped_ptr<base::DictionaryValue>
ProximityAuthWebUIHandler::GetDeviceSyncStateDictionary() {
  CryptAuthDeviceManager* device_manager =
      proximity_auth_client_->GetCryptAuthDeviceManager();
  if (!device_manager)
    return make_scoped_ptr(new base::DictionaryValue());

  return CreateSyncStateDictionary(
      device_manager->GetLastSyncTime().ToJsTime(),
      device_manager->GetTimeToNextAttempt().InMillisecondsF(),
      device_manager->IsRecoveringFromFailure(),
      device_manager->IsSyncInProgress());
}

scoped_ptr<base::ListValue> ProximityAuthWebUIHandler::GetUnlockKeysList() {
  scoped_ptr<base::ListValue> unlock_keys(new base::ListValue());
  CryptAuthDeviceManager* device_manager =
      proximity_auth_client_->GetCryptAuthDeviceManager();
  if (!device_manager)
    return unlock_keys;

  for (const auto& unlock_key : device_manager->unlock_keys()) {
    unlock_keys->Append(ExternalDeviceInfoToDictionary(unlock_key));
  }

  return unlock_keys;
}

Connection* ProximityAuthWebUIHandler::GetConnection() {
  Connection* connection = connection_.get();
  if (messenger_) {
    DCHECK(!connection);
    connection = messenger_->connection();
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

  selected_remote_device_ =
      RemoteDevice(unlock_key.friendly_device_name(), unlock_key.public_key(),
                   unlock_key.bluetooth_address(), persistent_symmetric_key);

  // TODO(tengs): We distinguish whether the unlock key uses classic Bluetooth
  // or BLE based on the presence of the |bluetooth_address| field. However, we
  // should ideally have a separate field specifying the protocol.
  if (selected_remote_device_.bluetooth_address.empty())
    FindBluetoothLowEnergyConnection(selected_remote_device_);
  else
    FindBluetoothClassicConnection(selected_remote_device_);
}

void ProximityAuthWebUIHandler::FindBluetoothClassicConnection(
    const RemoteDevice& remote_device) {
  PA_LOG(INFO) << "Finding classic Bluetooth device " << remote_device.name
               << " [" << remote_device.bluetooth_address << "].";

  // TODO(tengs): Set a timeout to stop the connection finder eventually.
  connection_finder_.reset(new BluetoothConnectionFinder(
      remote_device, device::BluetoothUUID(kClassicBluetoothServiceUUID),
      base::TimeDelta::FromSeconds(3)));
  connection_finder_->Find(
      base::Bind(&ProximityAuthWebUIHandler::OnConnectionFound,
                 weak_ptr_factory_.GetWeakPtr()));

  web_ui()->CallJavascriptFunction("LocalStateInterface.onUnlockKeysChanged",
                                   *GetUnlockKeysList());
}

void ProximityAuthWebUIHandler::FindBluetoothLowEnergyConnection(
    const RemoteDevice& remote_device) {
  PrefService* pref_service = proximity_auth_client_->GetPrefService();
  if (!pref_service->FindPreference(
          prefs::kBluetoothLowEnergyDeviceWhitelist)) {
    PA_LOG(ERROR) << "Please enable the BLE experiment in chrome://flags.";
    return;
  }

  PA_LOG(INFO) << "Finding Bluetooth Low Energy device " << remote_device.name;
  if (!bluetooth_throttler_) {
    bluetooth_throttler_.reset(new BluetoothThrottlerImpl(
        make_scoped_ptr(new base::DefaultTickClock())));
  }

  ble_device_whitelist_.reset(new BluetoothLowEnergyDeviceWhitelist(
      proximity_auth_client_->GetPrefService()));

  // TODO(tengs): Set a timeout to stop the connection finder eventually.
  connection_finder_.reset(new BluetoothLowEnergyConnectionFinder(
      remote_device, kBLESmartLockServiceUUID,
      BluetoothLowEnergyConnectionFinder::FIND_ANY_DEVICE,
      ble_device_whitelist_.get(), bluetooth_throttler_.get(), 3));
  connection_finder_->Find(
      base::Bind(&ProximityAuthWebUIHandler::OnConnectionFound,
                 weak_ptr_factory_.GetWeakPtr()));

  web_ui()->CallJavascriptFunction("LocalStateInterface.onUnlockKeysChanged",
                                   *GetUnlockKeysList());
}

void ProximityAuthWebUIHandler::OnAuthenticationResult(
    Authenticator::Result result,
    scoped_ptr<SecureContext> secure_context) {
  secure_context_ = secure_context.Pass();

  // Create the MessengerImpl asynchronously. |messenger_| registers itself as
  // an observer of |connection_|, so creating it synchronously would trigger
  // |OnSendComplete()| as an observer call for |messenger_|.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&ProximityAuthWebUIHandler::CreateStatusUpdateMessenger,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ProximityAuthWebUIHandler::OnConnectionFound(
    scoped_ptr<Connection> connection) {
  DCHECK(connection->IsConnected());
  connection_ = connection.Pass();
  connection_->AddObserver(this);

  web_ui()->CallJavascriptFunction("LocalStateInterface.onUnlockKeysChanged",
                                   *GetUnlockKeysList());

  // TODO(tengs): Create an authenticator for BLE connections.
  if (selected_remote_device_.bluetooth_address.empty())
    return;

  authenticator_.reset(new DeviceToDeviceAuthenticator(
      connection_.get(), proximity_auth_client_->GetAccountId(),
      proximity_auth_client_->CreateSecureMessageDelegate()));
  authenticator_->Authenticate(
      base::Bind(&ProximityAuthWebUIHandler::OnAuthenticationResult,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ProximityAuthWebUIHandler::CreateStatusUpdateMessenger() {
  messenger_.reset(
      new MessengerImpl(connection_.Pass(), secure_context_.Pass()));
  messenger_->AddObserver(this);
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

  CryptAuthDeviceManager* device_manager =
      proximity_auth_client_->GetCryptAuthDeviceManager();
  if (!device_manager)
    return dictionary;

  // If |device_info| is a known unlock key, then combine the proto data with
  // the corresponding local device data (e.g. connection status and remote
  // status updates).
  std::string public_key = device_info.public_key();
  auto iterator = std::find_if(
      device_manager->unlock_keys().begin(),
      device_manager->unlock_keys().end(),
      [&public_key](const cryptauth::ExternalDeviceInfo& unlock_key) {
        return unlock_key.public_key() == public_key;
      });

  if (iterator == device_manager->unlock_keys().end() ||
      selected_remote_device_.public_key != device_info.public_key())
    return dictionary;

  // Fill in the current Bluetooth connection status.
  std::string connection_status = kExternalDeviceDisconnected;
  Connection* connection = GetConnection();
  if (connection && connection->IsConnected()) {
    connection_status = kExternalDeviceConnected;
  } else if (connection_finder_) {
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

  if (new_status == Connection::DISCONNECTED) {
    last_remote_status_update_.reset();
    selected_remote_device_ = RemoteDevice();
    connection_finder_.reset();
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
