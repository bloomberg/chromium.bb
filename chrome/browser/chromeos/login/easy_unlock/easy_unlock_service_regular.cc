// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_service_regular.h"

#include <stdint.h>

#include <utility>

#include "apps/app_lifetime_monitor_factory.h"
#include "base/base64url.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_string_value_serializer.h"
#include "base/linux_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_clock.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/easy_unlock/chrome_proximity_auth_client.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_key_manager.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_key_names.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_notification_controller.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/multidevice_setup/multidevice_setup_dialog.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/components/proximity_auth/proximity_auth_pref_names.h"
#include "chromeos/components/proximity_auth/proximity_auth_profile_pref_manager.h"
#include "chromeos/components/proximity_auth/proximity_auth_system.h"
#include "chromeos/components/proximity_auth/screenlock_bridge.h"
#include "chromeos/components/proximity_auth/smart_lock_metrics_recorder.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/gcm_driver/gcm_profile_service.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/user_manager/user_manager.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/constants.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace chromeos {

namespace {

// Key name of the remote device list in kEasyUnlockPairing.
const char kKeyDevices[] = "devices";

enum class SmartLockToggleFeature { DISABLE = false, ENABLE = true };

// The result of a SmartLock operation.
enum class SmartLockResult { FAILURE = false, SUCCESS = true };

enum class SmartLockEnabledState {
  ENABLED = 0,
  DISABLED = 1,
  UNSET = 2,
  COUNT
};

void LogSmartLockEnabledState(SmartLockEnabledState state) {
  UMA_HISTOGRAM_ENUMERATION("SmartLock.EnabledState", state,
                            SmartLockEnabledState::COUNT);
}

}  // namespace

EasyUnlockServiceRegular::EasyUnlockServiceRegular(
    Profile* profile,
    secure_channel::SecureChannelClient* secure_channel_client,
    device_sync::DeviceSyncClient* device_sync_client,
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client)
    : EasyUnlockServiceRegular(
          profile,
          secure_channel_client,
          std::make_unique<EasyUnlockNotificationController>(profile),
          device_sync_client,
          multidevice_setup_client) {}

EasyUnlockServiceRegular::EasyUnlockServiceRegular(
    Profile* profile,
    secure_channel::SecureChannelClient* secure_channel_client,
    std::unique_ptr<EasyUnlockNotificationController> notification_controller,
    device_sync::DeviceSyncClient* device_sync_client,
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client)
    : EasyUnlockService(profile, secure_channel_client),
      lock_screen_last_shown_timestamp_(base::TimeTicks::Now()),
      notification_controller_(std::move(notification_controller)),
      device_sync_client_(device_sync_client),
      multidevice_setup_client_(multidevice_setup_client) {}

EasyUnlockServiceRegular::~EasyUnlockServiceRegular() = default;

// TODO(jhawkins): This method with |has_unlock_keys| == true is the only signal
// that SmartLock setup has completed successfully. Make this signal more
// explicit.
void EasyUnlockServiceRegular::LoadRemoteDevices() {
  if (!device_sync_client_->is_ready()) {
    // OnEnrollmentFinished() or OnNewDevicesSynced() will call back on this
    // method once |device_sync_client_| is ready.
    PA_LOG(VERBOSE) << "DeviceSyncClient is not ready yet, delaying "
                       "UseLoadedRemoteDevices().";
    return;
  }

  if (!IsEnabled()) {
    // OnFeatureStatesChanged() will call back on this method when feature state
    // changes.
    PA_LOG(VERBOSE) << "Smart Lock is not enabled by user; aborting.";
    SetProximityAuthDevices(GetAccountId(), multidevice::RemoteDeviceRefList(),
                            base::nullopt /* local_device */);
    return;
  }

  bool has_unlock_keys = !GetUnlockKeys().empty();

  // TODO(jhawkins): The enabled pref should not be tied to whether unlock keys
  // exist; instead, both of these variables should be used to determine
  // IsEnabled().
  pref_manager_->SetIsEasyUnlockEnabled(has_unlock_keys);
  if (has_unlock_keys) {
    // If |has_unlock_keys| is true, then the user must have successfully
    // completed setup. Track that the IsEasyUnlockEnabled pref is actively set
    // by the user, as opposed to passively being set to disabled (the default
    // state).
    pref_manager_->SetEasyUnlockEnabledStateSet();
    LogSmartLockEnabledState(SmartLockEnabledState::ENABLED);
  } else {
    PA_LOG(ERROR) << "Smart Lock is enabled by user, but no unlock key is "
                     "present; aborting.";
    SetProximityAuthDevices(GetAccountId(), multidevice::RemoteDeviceRefList(),
                            base::nullopt /* local_device */);

    if (pref_manager_->IsEasyUnlockEnabledStateSet()) {
      LogSmartLockEnabledState(SmartLockEnabledState::DISABLED);
    } else {
      LogSmartLockEnabledState(SmartLockEnabledState::UNSET);
    }
    return;
  }

  // This code path may be hit by:
  //   1. New devices were synced on the lock screen.
  //   2. The service was initialized while the login screen is still up.
  if (proximity_auth::ScreenlockBridge::Get()->IsLocked()) {
    PA_LOG(VERBOSE) << "Deferring device load until screen is unlocked.";
    deferring_device_load_ = true;
    return;
  }

  UseLoadedRemoteDevices(GetUnlockKeys());
}

void EasyUnlockServiceRegular::UseLoadedRemoteDevices(
    const multidevice::RemoteDeviceRefList& remote_devices) {
  // When EasyUnlock is enabled, only one EasyUnlock host should exist.
  if (remote_devices.size() != 1u) {
    PA_LOG(ERROR) << "There should only be 1 Smart Lock host, but there are: "
                  << remote_devices.size();
    NOTREACHED();
  }

  SetProximityAuthDevices(GetAccountId(), remote_devices,
                          device_sync_client_->GetLocalDeviceMetadata());

  // We need to store a copy of |local_and_remote_devices| in the TPM, so it can
  // be retrieved on the sign-in screen when a user session has not been started
  // yet. This expects a final size of 2 (the one remote device, and the local
  // device).
  // TODO(crbug.com/856380): For historical reasons, the local and remote device
  // are persisted together in a list. This is awkward and hacky; they should
  // be persisted in a dictionary.
  multidevice::RemoteDeviceRefList local_and_remote_devices;
  local_and_remote_devices.push_back(remote_devices[0]);
  local_and_remote_devices.push_back(
      *device_sync_client_->GetLocalDeviceMetadata());

  std::unique_ptr<base::ListValue> device_list(new base::ListValue());
  for (const auto& device : local_and_remote_devices) {
    std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
    std::string b64_public_key, b64_psk;
    base::Base64UrlEncode(device.public_key(),
                          base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                          &b64_public_key);
    base::Base64UrlEncode(device.persistent_symmetric_key(),
                          base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                          &b64_psk);

    dict->SetString(key_names::kKeyPsk, b64_psk);

    // TODO(jhawkins): Remove the bluetoothAddress field from this proto.
    dict->SetString(key_names::kKeyBluetoothAddress, std::string());

    dict->SetString(
        key_names::kKeyPermitPermitId,
        base::StringPrintf(
            key_names::kPermitPermitIdFormat,
            gaia::CanonicalizeEmail(GetAccountId().GetUserEmail()).c_str()));

    dict->SetString(key_names::kKeyPermitId, b64_public_key);
    dict->SetString(key_names::kKeyPermitType, key_names::kPermitTypeLicence);
    dict->SetString(key_names::kKeyPermitData, b64_public_key);

    std::unique_ptr<base::ListValue> beacon_seed_list(new base::ListValue());
    for (const auto& beacon_seed : device.beacon_seeds()) {
      std::string b64_beacon_seed;
      base::Base64UrlEncode(
          multidevice::ToCryptAuthSeed(beacon_seed).SerializeAsString(),
          base::Base64UrlEncodePolicy::INCLUDE_PADDING, &b64_beacon_seed);
      beacon_seed_list->AppendString(b64_beacon_seed);
    }

    std::string serialized_beacon_seeds;
    JSONStringValueSerializer serializer(&serialized_beacon_seeds);
    serializer.Serialize(*beacon_seed_list);
    dict->SetString(key_names::kKeySerializedBeaconSeeds,
                    serialized_beacon_seeds);

    // This differentiates the local device from the remote device.
    bool unlock_key = device.GetSoftwareFeatureState(
                          multidevice::SoftwareFeature::kSmartLockHost) ==
                      multidevice::SoftwareFeatureState::kEnabled;
    dict->SetBoolean(key_names::kKeyUnlockKey, unlock_key);

    PA_LOG(VERBOSE) << "Storing RemoteDevice: { "
                    << "name: " << device.name()
                    << ", unlock_key: " << unlock_key
                    << ", id: " << device.GetTruncatedDeviceIdForLogs()
                    << " }.";
    device_list->Append(std::move(dict));
  }

  if (device_list->GetSize() != 2u) {
    PA_LOG(ERROR) << "There should only be 2 devices persisted, the host and "
                     "the client, but there are: "
                  << device_list->GetSize();
    NOTREACHED();
  }

  SetStoredRemoteDevices(*device_list);
}

void EasyUnlockServiceRegular::SetStoredRemoteDevices(
    const base::ListValue& devices) {
  std::string remote_devices_json;
  JSONStringValueSerializer serializer(&remote_devices_json);
  serializer.Serialize(devices);

  DictionaryPrefUpdate pairing_update(profile()->GetPrefs(),
                                      prefs::kEasyUnlockPairing);
  if (devices.empty())
    pairing_update->RemoveWithoutPathExpansion(kKeyDevices, NULL);
  else
    pairing_update->SetKey(kKeyDevices, devices.Clone());

  CheckCryptohomeKeysAndMaybeHardlock();
}

proximity_auth::ProximityAuthPrefManager*
EasyUnlockServiceRegular::GetProximityAuthPrefManager() {
  return pref_manager_.get();
}

EasyUnlockService::Type EasyUnlockServiceRegular::GetType() const {
  return EasyUnlockService::TYPE_REGULAR;
}

AccountId EasyUnlockServiceRegular::GetAccountId() const {
  const user_manager::User* const primary_user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  DCHECK(primary_user);
  return primary_user->GetAccountId();
}

const base::ListValue* EasyUnlockServiceRegular::GetRemoteDevices() const {
  const base::DictionaryValue* pairing_dict =
      profile()->GetPrefs()->GetDictionary(prefs::kEasyUnlockPairing);
  const base::ListValue* devices = NULL;
  if (pairing_dict && pairing_dict->GetList(kKeyDevices, &devices))
    return devices;
  return NULL;
}

std::string EasyUnlockServiceRegular::GetChallenge() const {
  NOTREACHED();
  return std::string();
}

std::string EasyUnlockServiceRegular::GetWrappedSecret() const {
  NOTREACHED();
  return std::string();
}

void EasyUnlockServiceRegular::RecordEasySignInOutcome(
    const AccountId& account_id,
    bool success) const {
  NOTREACHED();
}

void EasyUnlockServiceRegular::RecordPasswordLoginEvent(
    const AccountId& account_id) const {
  NOTREACHED();
}

void EasyUnlockServiceRegular::InitializeInternal() {
  pref_manager_.reset(new proximity_auth::ProximityAuthProfilePrefManager(
      profile()->GetPrefs(), multidevice_setup_client_));
  pref_manager_->StartSyncingToLocalState(g_browser_process->local_state(),
                                          GetAccountId());

  registrar_.Init(profile()->GetPrefs());
  registrar_.Add(
      proximity_auth::prefs::kProximityAuthIsChromeOSLoginEnabled,
      base::Bind(&EasyUnlockServiceRegular::CheckCryptohomeKeysAndMaybeHardlock,
                 weak_ptr_factory_.GetWeakPtr()));

  // If |device_sync_client_| is not ready yet, wait for it to call back on
  // OnReady().
  if (device_sync_client_->is_ready())
    OnReady();
  device_sync_client_->AddObserver(this);

  OnFeatureStatesChanged(multidevice_setup_client_->GetFeatureStates());
  multidevice_setup_client_->AddObserver(this);

  proximity_auth::ScreenlockBridge::Get()->AddObserver(this);

  LoadRemoteDevices();
}

void EasyUnlockServiceRegular::ShutdownInternal() {
  pref_manager_.reset();
  notification_controller_.reset();

  proximity_auth::ScreenlockBridge::Get()->RemoveObserver(this);

  registrar_.RemoveAll();

  device_sync_client_->RemoveObserver(this);

  multidevice_setup_client_->RemoveObserver(this);

  weak_ptr_factory_.InvalidateWeakPtrs();
}

bool EasyUnlockServiceRegular::IsAllowedInternal() const {
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  if (!user_manager->IsLoggedInAsUserWithGaiaAccount())
    return false;

  // TODO(tengs): Ephemeral accounts generate a new enrollment every time they
  // are added, so disable Smart Lock to reduce enrollments on server. However,
  // ephemeral accounts can be locked, so we should revisit this use case.
  if (user_manager->IsCurrentUserNonCryptohomeDataEphemeral())
    return false;

  if (!ProfileHelper::IsPrimaryProfile(profile()))
    return false;

  if (multidevice_setup_client_->GetFeatureState(
          multidevice_setup::mojom::Feature::kSmartLock) ==
      multidevice_setup::mojom::FeatureState::kProhibitedByPolicy) {
    return false;
  }

  return true;
}

bool EasyUnlockServiceRegular::IsEnabled() const {
  return multidevice_setup_client_->GetFeatureState(
             multidevice_setup::mojom::Feature::kSmartLock) ==
         multidevice_setup::mojom::FeatureState::kEnabledByUser;
}

bool EasyUnlockServiceRegular::IsChromeOSLoginEnabled() const {
  return pref_manager_ && pref_manager_->IsChromeOSLoginEnabled();
}

void EasyUnlockServiceRegular::OnSuspendDoneInternal() {
  lock_screen_last_shown_timestamp_ = base::TimeTicks::Now();
}

void EasyUnlockServiceRegular::OnReady() {
  // If the local device and synced devices are ready for the first time,
  // establish what the unlock keys were before the next sync. This is necessary
  // in order for OnNewDevicesSynced() to determine if new devices were added
  // since the last sync.
  remote_device_unlock_keys_before_sync_ = GetUnlockKeys();
}

void EasyUnlockServiceRegular::OnEnrollmentFinished() {
  // The local device may be ready for the first time, or it may have been
  // updated, so reload devices.
  LoadRemoteDevices();
}

void EasyUnlockServiceRegular::OnNewDevicesSynced() {
  std::set<std::string> public_keys_before_sync;
  for (const auto& remote_device : remote_device_unlock_keys_before_sync_) {
    public_keys_before_sync.insert(remote_device.public_key());
  }

  multidevice::RemoteDeviceRefList remote_device_unlock_keys_after_sync =
      GetUnlockKeys();
  std::set<std::string> public_keys_after_sync;
  for (const auto& remote_device : remote_device_unlock_keys_after_sync) {
    public_keys_after_sync.insert(remote_device.public_key());
  }

  ShowNotificationIfNewDevicePresent(public_keys_before_sync,
                                     public_keys_after_sync);

  LoadRemoteDevices();

  remote_device_unlock_keys_before_sync_ = remote_device_unlock_keys_after_sync;
}

void EasyUnlockServiceRegular::OnFeatureStatesChanged(
    const multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
        feature_states_map) {
  LoadRemoteDevices();
  UpdateAppState();
}

void EasyUnlockServiceRegular::ShowChromebookAddedNotification() {
  // The user may have decided to disable Smart Lock or the whole multidevice
  // suite immediately after completing setup, so ensure that Smart Lock is
  // enabled.
  if (IsEnabled())
    notification_controller_->ShowChromebookAddedNotification();
}

void EasyUnlockServiceRegular::ShowNotificationIfNewDevicePresent(
    const std::set<std::string>& public_keys_before_sync,
    const std::set<std::string>& public_keys_after_sync) {
  if (public_keys_before_sync == public_keys_after_sync)
    return;

  // Show the appropriate notification if an unlock key is first synced or if it
  // changes an existing key.
  // Note: We do not show a notification when EasyUnlock is disabled by sync nor
  // if EasyUnlock was enabled through the setup app.
  if (!public_keys_after_sync.empty()) {
    if (public_keys_before_sync.empty()) {
      multidevice_setup::MultiDeviceSetupDialog* multidevice_setup_dialog =
          multidevice_setup::MultiDeviceSetupDialog::Get();
      if (multidevice_setup_dialog) {
        // Delay showing the "Chromebook added" notification until the
        // MultiDeviceSetupDialog is closed.
        multidevice_setup_dialog->AddOnCloseCallback(base::BindOnce(
            &EasyUnlockServiceRegular::ShowChromebookAddedNotification,
            weak_ptr_factory_.GetWeakPtr()));
        return;
      }

      notification_controller_->ShowChromebookAddedNotification();
    } else {
      shown_pairing_changed_notification_ = true;
      notification_controller_->ShowPairingChangeNotification();
    }
  }
}

void EasyUnlockServiceRegular::OnScreenDidLock(
    proximity_auth::ScreenlockBridge::LockHandler::ScreenType screen_type) {
  set_will_authenticate_using_easy_unlock(false);
  lock_screen_last_shown_timestamp_ = base::TimeTicks::Now();
}

void EasyUnlockServiceRegular::OnScreenDidUnlock(
    proximity_auth::ScreenlockBridge::LockHandler::ScreenType screen_type) {
  // If we tried to load remote devices (e.g. after a sync or the
  // service was initialized) while the screen was locked, we can now
  // load the new remote devices.
  //
  // It's important to go through this code path even if unlocking the
  // login screen. Because when the service is initialized while the
  // user is signing in we need to load the remotes. Otherwise, the
  // first time the user locks the screen the feature won't work.
  if (deferring_device_load_) {
    PA_LOG(VERBOSE) << "Loading deferred devices after screen unlock.";
    deferring_device_load_ = false;
    LoadRemoteDevices();
  }

  // Do not process events for the login screen.
  if (screen_type != proximity_auth::ScreenlockBridge::LockHandler::LOCK_SCREEN)
    return;

  if (shown_pairing_changed_notification_) {
    shown_pairing_changed_notification_ = false;

    if (!GetUnlockKeys().empty()) {
      notification_controller_->ShowPairingChangeAppliedNotification(
          GetUnlockKeys()[0].name());
    }
  }

  // Only record metrics for users who have enabled the feature.
  if (IsEnabled()) {
    EasyUnlockAuthEvent event = will_authenticate_using_easy_unlock()
                                    ? EASY_UNLOCK_SUCCESS
                                    : GetPasswordAuthEvent();
    RecordEasyUnlockScreenUnlockEvent(event);

    if (will_authenticate_using_easy_unlock()) {
      SmartLockMetricsRecorder::RecordSmartLockUnlockAuthMethodChoice(
          SmartLockMetricsRecorder::SmartLockAuthMethodChoice::kSmartLock);
      SmartLockMetricsRecorder::RecordAuthResultUnlockSuccess();
      RecordEasyUnlockScreenUnlockDuration(base::TimeTicks::Now() -
                                           lock_screen_last_shown_timestamp_);
    } else {
      SmartLockMetricsRecorder::RecordAuthMethodChoiceUnlockPasswordState(
          GetSmartUnlockPasswordAuthEvent());
      SmartLockMetricsRecorder::RecordSmartLockUnlockAuthMethodChoice(
          SmartLockMetricsRecorder::SmartLockAuthMethodChoice::kOther);
      OnUserEnteredPassword();
    }
  }

  set_will_authenticate_using_easy_unlock(false);
}

void EasyUnlockServiceRegular::OnFocusedUserChanged(
    const AccountId& account_id) {
  // Nothing to do.
}

multidevice::RemoteDeviceRefList EasyUnlockServiceRegular::GetUnlockKeys() {
  multidevice::RemoteDeviceRefList unlock_keys;
  for (const auto& remote_device : device_sync_client_->GetSyncedDevices()) {
    bool unlock_key = remote_device.GetSoftwareFeatureState(
                          multidevice::SoftwareFeature::kSmartLockHost) ==
                      multidevice::SoftwareFeatureState::kEnabled;
    if (unlock_key)
      unlock_keys.push_back(remote_device);
  }
  return unlock_keys;
}

}  // namespace chromeos
