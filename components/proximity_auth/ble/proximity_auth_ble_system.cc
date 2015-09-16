// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/ble/proximity_auth_ble_system.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/strings/utf_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/default_tick_clock.h"
#include "components/proximity_auth/ble/bluetooth_low_energy_connection.h"
#include "components/proximity_auth/ble/bluetooth_low_energy_connection_finder.h"
#include "components/proximity_auth/ble/bluetooth_low_energy_device_whitelist.h"
#include "components/proximity_auth/ble/fake_wire_message.h"
#include "components/proximity_auth/bluetooth_throttler_impl.h"
#include "components/proximity_auth/connection.h"
#include "components/proximity_auth/cryptauth/base64url.h"
#include "components/proximity_auth/cryptauth/cryptauth_device_manager.h"
#include "components/proximity_auth/cryptauth/proto/cryptauth_api.pb.h"
#include "components/proximity_auth/logging/logging.h"
#include "components/proximity_auth/proximity_auth_client.h"
#include "components/proximity_auth/remote_device.h"
#include "components/proximity_auth/wire_message.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_gatt_connection.h"

namespace proximity_auth {

namespace {

// The UUID of the Bluetooth Low Energy service.
const char kSmartLockServiceUUID[] = "b3b7e28e-a000-3e17-bd86-6e97b9e28c11";

// Polling interval in seconds.
const int kPollingIntervalSeconds = 5;

// String received when the remote device's screen is unlocked.
const char kScreenUnlocked[] = "Screen Unlocked";

// String received when the remote device's screen is locked.
const char kScreenLocked[] = "Screen Locked";

// String send to poll the remote device screen state.
const char kPollScreenState[] = "PollScreenState";

// String prefix received with the public key.
const char kPublicKeyMessagePrefix[] = "PublicKey:";

// BluetoothLowEnergyConnection parameter, number of attempts to send a write
// request before failing.
const int kMaxNumberOfTries = 2;

// The time, in seconds, to show a spinner for the user pod immediately after
// the screen is locked.
const int kSpinnerTimeSeconds = 15;

// Text shown on the user pod when unlock is allowed.
const char kUserPodUnlockText[] = "Click your photo";

// Text of tooltip shown on when hovering over the user pod icon when unlock is
// not allowed.
const char kUserPodIconLockedTooltip[] = "Unable to find an unlocked phone.";

}  // namespace

ProximityAuthBleSystem::ProximityAuthBleSystem(
    ScreenlockBridge* screenlock_bridge,
    ProximityAuthClient* proximity_auth_client,
    PrefService* pref_service)
    : screenlock_bridge_(screenlock_bridge),
      proximity_auth_client_(proximity_auth_client),
      device_whitelist_(new BluetoothLowEnergyDeviceWhitelist(pref_service)),
      bluetooth_throttler_(new BluetoothThrottlerImpl(
          make_scoped_ptr(new base::DefaultTickClock()))),
      device_authenticated_(false),
      is_polling_screen_state_(false),
      weak_ptr_factory_(this) {
  PA_LOG(INFO) << "Starting Proximity Auth over Bluetooth Low Energy.";
  screenlock_bridge_->AddObserver(this);
}

ProximityAuthBleSystem::~ProximityAuthBleSystem() {
  PA_LOG(INFO) << "Stopping Proximity over Bluetooth Low Energy.";
  screenlock_bridge_->RemoveObserver(this);
  if (connection_)
    connection_->RemoveObserver(this);
}

void ProximityAuthBleSystem::RegisterPrefs(PrefRegistrySimple* registry) {
  BluetoothLowEnergyDeviceWhitelist::RegisterPrefs(registry);
}

void ProximityAuthBleSystem::RemoveStaleWhitelistedDevices() {
  PA_LOG(INFO) << "Removing stale whitelist devices.";
  std::vector<std::string> b64_public_keys = device_whitelist_->GetPublicKeys();
  PA_LOG(INFO) << "There were " << b64_public_keys.size()
               << " whitelisted devices.";

  std::vector<cryptauth::ExternalDeviceInfo> unlock_keys =
      proximity_auth_client_->GetCryptAuthDeviceManager()->unlock_keys();

  for (const auto& b64_public_key : b64_public_keys) {
    std::string public_key;
    if (!Base64UrlDecode(b64_public_key, &public_key))
      continue;

    bool public_key_registered = false;
    for (const auto& unlock_key : unlock_keys) {
      public_key_registered |= unlock_key.public_key() == public_key;
    }

    if (!public_key_registered) {
      PA_LOG(INFO) << "Removing device: " << b64_public_key;
      device_whitelist_->RemoveDeviceWithPublicKey(b64_public_key);
    }
  }
  b64_public_keys = device_whitelist_->GetPublicKeys();
  PA_LOG(INFO) << "There are " << b64_public_keys.size()
               << " whitelisted devices.";
}

void ProximityAuthBleSystem::OnScreenDidLock(
    ScreenlockBridge::LockHandler::ScreenType screen_type) {
  PA_LOG(INFO) << "OnScreenDidLock: " << screen_type;
  if (!IsAnyUnlockKeyBLE()) {
    PA_LOG(INFO) << "No BLE device registered as unlock key";
    return;
  }

  switch (screen_type) {
    case ScreenlockBridge::LockHandler::SIGNIN_SCREEN:
      connection_finder_.reset();
      break;
    case ScreenlockBridge::LockHandler::LOCK_SCREEN:
      DCHECK(!connection_finder_);
      connection_finder_.reset(CreateConnectionFinder());
      connection_finder_->Find(
          base::Bind(&ProximityAuthBleSystem::OnConnectionFound,
                     weak_ptr_factory_.GetWeakPtr()));
      break;
    case ScreenlockBridge::LockHandler::OTHER_SCREEN:
      connection_finder_.reset();
      break;
  }

  // Reset the screen lock UI state to the default state.
  is_remote_screen_locked_ = true;
  screenlock_ui_state_ = ScreenlockUIState::NO_SCREENLOCK;
  last_focused_user_ = screenlock_bridge_->focused_user_id();
  spinner_timer_.Start(FROM_HERE,
                       base::TimeDelta::FromSeconds(kSpinnerTimeSeconds), this,
                       &ProximityAuthBleSystem::OnSpinnerTimerFired);
  UpdateLockScreenUI();
}

ConnectionFinder* ProximityAuthBleSystem::CreateConnectionFinder() {
  return new BluetoothLowEnergyConnectionFinder(
      RemoteDevice(), kSmartLockServiceUUID,
      BluetoothLowEnergyConnectionFinder::FIND_ANY_DEVICE,
      device_whitelist_.get(), bluetooth_throttler_.get(), kMaxNumberOfTries);
}

void ProximityAuthBleSystem::OnScreenDidUnlock(
    ScreenlockBridge::LockHandler::ScreenType screen_type) {
  PA_LOG(INFO) << "OnScreenDidUnlock: " << screen_type;

  if (connection_) {
    // Note: it's important to remove the observer before calling
    // |Disconnect()|, otherwise |OnConnectedStatusChanged()| will be called
    // from |connection_| and a new instance for |connection_finder_| will be
    // created.
    connection_->RemoveObserver(this);
    connection_->Disconnect();
    device_authenticated_ = false;
  }

  connection_.reset();
  connection_finder_.reset();
}

void ProximityAuthBleSystem::OnFocusedUserChanged(const std::string& user_id) {
  PA_LOG(INFO) << "OnFocusedUserChanged: " << user_id;
  // TODO(tengs): We assume that the last focused user is the one with Smart
  // Lock enabled. This may not be the case for multiprofile scenarios.
  last_focused_user_ = user_id;
  UpdateLockScreenUI();
}

void ProximityAuthBleSystem::OnMessageReceived(const Connection& connection,
                                               const WireMessage& message) {
  PA_LOG(INFO) << "Message with " << message.payload().size()
               << " bytes received.";

  // The first message should contain a public key registered with
  // CryptAuthDeviceManager to authenticate the device.
  if (!device_authenticated_) {
    std::string out_public_key;
    if (HasUnlockKey(message.payload(), &out_public_key)) {
      PA_LOG(INFO) << "Device authenticated. Adding "
                   << connection_->remote_device().bluetooth_address << ", "
                   << out_public_key << " to whitelist.";
      device_whitelist_->AddOrUpdateDevice(
          connection_->remote_device().bluetooth_address, out_public_key);
      device_authenticated_ = true;

      // Only start polling the screen state if the device is authenticated.
      if (!is_polling_screen_state_) {
        is_polling_screen_state_ = true;
        StartPollingScreenState();
      }

    } else {
      PA_LOG(INFO) << "Key not found. Authentication failed.";
      connection_->Disconnect();
    }
    return;
  }

  if (message.payload() == kScreenUnlocked) {
    is_remote_screen_locked_ = false;
    spinner_timer_.Stop();
    UpdateLockScreenUI();
  } else if (message.payload() == kScreenLocked) {
    is_remote_screen_locked_ = true;
    UpdateLockScreenUI();
  }
}

void ProximityAuthBleSystem::OnAuthAttempted(const std::string& user_id) {
  if (user_id != last_focused_user_) {
    PA_LOG(ERROR) << "Unexpected user: " << last_focused_user_
                  << " != " << user_id;
    return;
  }

  // Ignore this auth attempt if there is no BLE unlock key.
  if (!IsAnyUnlockKeyBLE())
    return;

  // Accept the auth attempt and authorize the screen unlock if the remote
  // device is connected and its screen is unlocked.
  bool accept_auth_attempt = connection_ && connection_->IsConnected() &&
                             device_authenticated_ && !is_remote_screen_locked_;
  proximity_auth_client_->FinalizeUnlock(accept_auth_attempt);
}

void ProximityAuthBleSystem::OnConnectionFound(
    scoped_ptr<Connection> connection) {
  PA_LOG(INFO) << "Connection found.";
  DCHECK(connection);

  connection_ = connection.Pass();
  connection_->AddObserver(this);
}

void ProximityAuthBleSystem::OnConnectionStatusChanged(
    Connection* connection,
    Connection::Status old_status,
    Connection::Status new_status) {
  PA_LOG(INFO) << "OnConnectionStatusChanged: " << old_status << " -> "
               << new_status;
  if (old_status == Connection::CONNECTED &&
      new_status == Connection::DISCONNECTED) {
    device_authenticated_ = false;
    is_remote_screen_locked_ = true;
    StopPollingScreenState();
    UpdateLockScreenUI();

    // Note: it's not necessary to destroy the |connection_| here, as it's
    // already in a DISCONNECTED state. Moreover, destroying it here can cause
    // memory corruption, since the instance |connection_| still accesses some
    // internal data members after |OnConnectionStatusChanged()| finishes.
    connection_->RemoveObserver(this);

    connection_finder_.reset(CreateConnectionFinder());
    connection_finder_->Find(
        base::Bind(&ProximityAuthBleSystem::OnConnectionFound,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void ProximityAuthBleSystem::StartPollingScreenState() {
  PA_LOG(INFO) << "Polling screen state.";
  if (is_polling_screen_state_) {
    if (!connection_ || !connection_->IsConnected()) {
      PA_LOG(INFO) << "Polling stopped.";
      is_polling_screen_state_ = false;
      return;
    }
    // Sends message requesting screen state.
    connection_->SendMessage(
        make_scoped_ptr(new WireMessage(kPollScreenState)));

    // Schedules the next message in |kPollingIntervalSeconds| s.
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, base::Bind(&ProximityAuthBleSystem::StartPollingScreenState,
                              weak_ptr_factory_.GetWeakPtr()),
        base::TimeDelta::FromSeconds(kPollingIntervalSeconds));

    PA_LOG(INFO) << "Next poll iteration posted.";
  }
}

void ProximityAuthBleSystem::StopPollingScreenState() {
  is_polling_screen_state_ = false;
}

bool ProximityAuthBleSystem::IsAnyUnlockKeyBLE() {
  CryptAuthDeviceManager* device_manager =
      proximity_auth_client_->GetCryptAuthDeviceManager();
  if (!device_manager)
    return false;

  for (const auto& unlock_key : device_manager->unlock_keys()) {
    // TODO(tengs): We currently assume that any device registered as an unlock
    // key, but does not have a Bluetooth address, is a BLE device.
    if (unlock_key.bluetooth_address().empty())
      return true;
  }

  return false;
}

bool ProximityAuthBleSystem::HasUnlockKey(const std::string& message,
                                          std::string* out_public_key) {
  std::string message_prefix(kPublicKeyMessagePrefix);
  if (message.substr(0, message_prefix.size()) != message_prefix)
    return false;
  std::string public_key = message.substr(message_prefix.size());
  if (out_public_key)
    (*out_public_key) = public_key;

  std::vector<cryptauth::ExternalDeviceInfo> unlock_keys =
      proximity_auth_client_->GetCryptAuthDeviceManager()->unlock_keys();

  std::string decoded_public_key;
  if (!Base64UrlDecode(public_key, &decoded_public_key))
    return false;

  bool unlock_key_registered = false;
  for (const auto& unlock_key : unlock_keys) {
    unlock_key_registered |= unlock_key.public_key() == decoded_public_key;
  }

  RemoveStaleWhitelistedDevices();
  return unlock_key_registered ||
         device_whitelist_->HasDeviceWithPublicKey(public_key);
}

void ProximityAuthBleSystem::OnSpinnerTimerFired() {
  UpdateLockScreenUI();
}

void ProximityAuthBleSystem::UpdateLockScreenUI() {
  ScreenlockUIState screenlock_ui_state = ScreenlockUIState::NO_SCREENLOCK;

  // TODO(tengs): We assume that the last focused user is the one with Smart
  // Lock enabled. This may not be the case for multiprofile scenarios.
  if (last_focused_user_.empty())
    return;

  // Check that the lock screen exists.
  ScreenlockBridge::LockHandler* lock_handler =
      screenlock_bridge_->lock_handler();
  if (!lock_handler) {
    PA_LOG(WARNING) << "No LockHandler";
    return;
  }

  // Check the current authentication state of the phone.
  if (connection_ && connection_->IsConnected()) {
    if (!device_authenticated_ || is_remote_screen_locked_)
      screenlock_ui_state = ScreenlockUIState::UNAUTHENTICATED;
    else
      screenlock_ui_state = ScreenlockUIState::AUTHENTICATED;
  } else if (spinner_timer_.IsRunning()) {
    screenlock_ui_state = ScreenlockUIState::SPINNER;
  } else {
    screenlock_ui_state = ScreenlockUIState::UNAUTHENTICATED;
  }

  if (screenlock_ui_state == screenlock_ui_state_)
    return;
  screenlock_ui_state_ = screenlock_ui_state;

  // Customize the user pod for the current UI state.
  PA_LOG(INFO) << "Screenlock UI state changed: "
               << static_cast<int>(screenlock_ui_state_);
  ScreenlockBridge::UserPodCustomIconOptions icon_options;
  ScreenlockBridge::LockHandler::AuthType auth_type =
      ScreenlockBridge::LockHandler::OFFLINE_PASSWORD;
  base::string16 auth_value;

  switch (screenlock_ui_state_) {
    case ScreenlockUIState::SPINNER:
      icon_options.SetIcon(ScreenlockBridge::USER_POD_CUSTOM_ICON_SPINNER);
      icon_options.SetTooltip(base::UTF8ToUTF16(kUserPodIconLockedTooltip),
                              false);
      break;
    case ScreenlockUIState::UNAUTHENTICATED:
      icon_options.SetIcon(ScreenlockBridge::USER_POD_CUSTOM_ICON_LOCKED);
      icon_options.SetTooltip(base::UTF8ToUTF16(kUserPodIconLockedTooltip),
                              false);
      break;
    case ScreenlockUIState::AUTHENTICATED:
      auth_value = base::UTF8ToUTF16(kUserPodUnlockText);
      icon_options.SetIcon(ScreenlockBridge::USER_POD_CUSTOM_ICON_UNLOCKED);
      icon_options.SetTooltip(base::UTF8ToUTF16(kUserPodUnlockText), false);
      auth_type = ScreenlockBridge::LockHandler::USER_CLICK;
      break;
    default:
      break;
  }

  lock_handler->ShowUserPodCustomIcon(last_focused_user_, icon_options);
  lock_handler->SetAuthType(last_focused_user_, auth_type, auth_value);
}

}  // namespace proximity_auth
