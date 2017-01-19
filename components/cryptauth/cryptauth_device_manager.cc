// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cryptauth/cryptauth_device_manager.h"

#include <stddef.h>
#include <stdexcept>
#include <utility>

#include "base/base64url.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "components/cryptauth/cryptauth_client.h"
#include "components/cryptauth/pref_names.h"
#include "components/cryptauth/sync_scheduler_impl.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/proximity_auth/logging/logging.h"

namespace cryptauth {

namespace {

// The normal period between successful syncs, in hours.
const int kRefreshPeriodHours = 24;

// A more aggressive period between sync attempts to recover when the last
// sync attempt fails, in minutes. This is a base time that increases for each
// subsequent failure.
const int kDeviceSyncBaseRecoveryPeriodMinutes = 10;

// The bound on the amount to jitter the period between syncs.
const double kDeviceSyncMaxJitterRatio = 0.2;

// Keys for ExternalDeviceInfo dictionaries that are stored in the user's prefs.
const char kExternalDeviceKeyPublicKey[] = "public_key";
const char kExternalDeviceKeyDeviceName[] = "device_name";
const char kExternalDeviceKeyBluetoothAddress[] = "bluetooth_address";
const char kExternalDeviceKeyUnlockKey[] = "unlock_key";
const char kExternalDeviceKeyUnlockable[] = "unlockable";
const char kExternalDeviceKeyLastUpdateTimeMillis[] = "last_update_time_millis";
const char kExternalDeviceKeyMobileHotspotSupported[] =
    "mobile_hotspot_supported";
const char kExternalDeviceKeyDeviceType[] = "device_type";
const char kExternalDeviceKeyBeaconSeeds[] = "beacon_seeds";
const char kExternalDeviceKeyBeaconSeedData[] = "beacon_seed_data";
const char kExternalDeviceKeyBeaconSeedStartMs[] = "beacon_seed_start_ms";
const char kExternalDeviceKeyBeaconSeedEndMs[] = "beacon_seed_end_ms";

// Converts BeaconSeed protos to a list value that can be stored in user prefs.
std::unique_ptr<base::ListValue> BeaconSeedsToListValue(
    const google::protobuf::RepeatedPtrField<BeaconSeed>& seeds) {
  std::unique_ptr<base::ListValue> list(new base::ListValue());

  for (int i = 0; i < seeds.size(); i++) {
    BeaconSeed seed = seeds.Get(i);

    if (!seed.has_data()
        || !seed.has_start_time_millis()
        || !seed.has_end_time_millis()) {
      PA_LOG(WARNING) << "Unable to serialize BeaconSeed due to missing data; "
          << "skipping.";
      continue;
    }

    std::unique_ptr<base::DictionaryValue> beacon_seed_value(
        new base::DictionaryValue());

    // Note that the |BeaconSeed|s' data is stored in Base64Url encoding because
    // dictionary values must be valid UTF8 strings.
    std::string seed_data_b64;
    base::Base64UrlEncode(seed.data(),
                          base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                          &seed_data_b64);
    beacon_seed_value->SetString(kExternalDeviceKeyBeaconSeedData,
                                 seed_data_b64);

    // Set the timestamps as string representations of their numeric value
    // since there is no notion of a base::LongValue.
    beacon_seed_value->SetString(
        kExternalDeviceKeyBeaconSeedStartMs,
        std::to_string(seed.start_time_millis()));
    beacon_seed_value->SetString(
        kExternalDeviceKeyBeaconSeedEndMs,
        std::to_string(seed.end_time_millis()));

    list->Append(std::move(beacon_seed_value));
  }

  return list;
}

// Converts an unlock key proto to a dictionary that can be stored in user
// prefs.
std::unique_ptr<base::DictionaryValue> UnlockKeyToDictionary(
    const ExternalDeviceInfo& device) {
  // The device public key is a required value.
  if (!device.has_public_key()) {
    return nullptr;
  }

  std::unique_ptr<base::DictionaryValue> dictionary(
      new base::DictionaryValue());

  // Note that the device public key, name, and Bluetooth addresses are stored
  // in Base64Url form because dictionary values must be valid UTF8 strings.

  std::string public_key_b64;
  base::Base64UrlEncode(device.public_key(),
                        base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &public_key_b64);
  dictionary->SetString(kExternalDeviceKeyPublicKey, public_key_b64);

  if (device.has_friendly_device_name()) {
    std::string device_name_b64;
    base::Base64UrlEncode(device.friendly_device_name(),
                          base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                          &device_name_b64);
    dictionary->SetString(kExternalDeviceKeyDeviceName, device_name_b64);
  }

  if (device.has_bluetooth_address()) {
    std::string bluetooth_address_b64;
    base::Base64UrlEncode(device.bluetooth_address(),
                        base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &bluetooth_address_b64);
    dictionary->SetString(kExternalDeviceKeyBluetoothAddress,
                          bluetooth_address_b64);
  }

  if (device.has_unlock_key()) {
    dictionary->SetBoolean(kExternalDeviceKeyUnlockKey,
                           device.unlock_key());
  }

  if (device.has_unlockable()) {
    dictionary->SetBoolean(kExternalDeviceKeyUnlockable,
                           device.unlockable());
  }

  if (device.has_last_update_time_millis()) {
    dictionary->SetString(kExternalDeviceKeyLastUpdateTimeMillis,
                          std::to_string(device.last_update_time_millis()));
  }

  if (device.has_mobile_hotspot_supported()) {
    dictionary->SetBoolean(kExternalDeviceKeyMobileHotspotSupported,
                           device.mobile_hotspot_supported());
  }

  if (device.has_device_type() && DeviceType_IsValid(device.device_type())) {
    dictionary->SetInteger(kExternalDeviceKeyDeviceType,
                           device.device_type());
  }

  std::unique_ptr<base::ListValue> beacon_seed_list =
      BeaconSeedsToListValue(device.beacon_seeds());
  dictionary->Set(kExternalDeviceKeyBeaconSeeds, std::move(beacon_seed_list));

  return dictionary;
}

void AddBeaconSeedsToExternalDevice(
    const base::ListValue& beacon_seeds,
    ExternalDeviceInfo& external_device) {
  for (size_t i = 0; i < beacon_seeds.GetSize(); i++) {
    const base::DictionaryValue* seed_dictionary = nullptr;
    if (!beacon_seeds.GetDictionary(i, &seed_dictionary)) {
      PA_LOG(WARNING) << "Unable to retrieve BeaconSeed dictionary; "
          << "skipping.";
      continue;
    }

    std::string seed_data_b64, start_time_millis_str, end_time_millis_str;
    if (!seed_dictionary->GetString(kExternalDeviceKeyBeaconSeedData,
                                    &seed_data_b64) ||
        !seed_dictionary->GetString(kExternalDeviceKeyBeaconSeedStartMs,
                                    &start_time_millis_str) ||
        !seed_dictionary->GetString(kExternalDeviceKeyBeaconSeedEndMs,
                                    &end_time_millis_str)) {
      PA_LOG(WARNING) << "Unable to deserialize BeaconSeed due to missing "
          << "data; skipping.";
      continue;
    }

    // Seed data is returned as raw data, not in Base64 encoding.
    std::string seed_data;
    if (!base::Base64UrlDecode(seed_data_b64,
                               base::Base64UrlDecodePolicy::REQUIRE_PADDING,
                               &seed_data)) {
      PA_LOG(WARNING) << "Decoding seed data failed.";
      continue;
    }

    int64_t start_time_millis, end_time_millis;
    if (!base::StringToInt64(start_time_millis_str, &start_time_millis)
        || !base::StringToInt64(end_time_millis_str, &end_time_millis)) {
      PA_LOG(WARNING) << "Unable to convert stored timestamp to int64_t: "
          << start_time_millis_str << " or " << end_time_millis_str;
      continue;
    }

    BeaconSeed* seed = external_device.add_beacon_seeds();
    seed->set_data(seed_data);
    seed->set_start_time_millis(start_time_millis);
    seed->set_end_time_millis(end_time_millis);
  }
}

// Converts an unlock key dictionary stored in user prefs to an
// ExternalDeviceInfo proto. Returns true if the dictionary is valid, and the
// parsed proto is written to |external_device|.
bool DictionaryToUnlockKey(const base::DictionaryValue& dictionary,
                           ExternalDeviceInfo* external_device) {
  std::string public_key_b64;
  if (!dictionary.GetString(kExternalDeviceKeyPublicKey, &public_key_b64)) {
    // The public key is a required field, so if it is absent, there is no
    // valid data to return.
    return false;
  }

  std::string public_key;
  if (!base::Base64UrlDecode(public_key_b64,
                             base::Base64UrlDecodePolicy::REQUIRE_PADDING,
                             &public_key)) {
    // The public key is stored as a Base64Url, so if it cannot be decoded,
    // there is no valid data to return.
    return false;
  }
  external_device->set_public_key(public_key);

  std::string device_name_b64;
  if (dictionary.GetString(kExternalDeviceKeyDeviceName, &device_name_b64)) {
    std::string device_name;
    if (base::Base64UrlDecode(device_name_b64,
                               base::Base64UrlDecodePolicy::REQUIRE_PADDING,
                               &device_name)) {
      external_device->set_friendly_device_name(device_name);
    }
  }

  std::string bluetooth_address_b64;
  if (dictionary.GetString(
      kExternalDeviceKeyBluetoothAddress, &bluetooth_address_b64)) {
    std::string bluetooth_address;
    if (base::Base64UrlDecode(bluetooth_address_b64,
                              base::Base64UrlDecodePolicy::REQUIRE_PADDING,
                              &bluetooth_address)) {
      external_device->set_bluetooth_address(bluetooth_address);
    }
  }

  bool unlock_key;
  if (dictionary.GetBoolean(kExternalDeviceKeyUnlockKey, &unlock_key)) {
    external_device->set_unlock_key(unlock_key);
  }

  bool unlockable;
  if (dictionary.GetBoolean(kExternalDeviceKeyUnlockable, &unlockable)) {
    external_device->set_unlockable(unlockable);
  }

  std::string last_update_time_millis_str;
  if (dictionary.GetString(
      kExternalDeviceKeyLastUpdateTimeMillis, &last_update_time_millis_str)) {
    int64_t last_update_time_millis;
    if (base::StringToInt64(
        last_update_time_millis_str, &last_update_time_millis)) {
      external_device->set_last_update_time_millis(last_update_time_millis);
    } else {
      PA_LOG(WARNING) << "Unable to convert stored update time to int64_t: "
          << last_update_time_millis_str;
    }
  }

  bool mobile_hotspot_supported;
  if (dictionary.GetBoolean(
      kExternalDeviceKeyMobileHotspotSupported, &mobile_hotspot_supported)) {
    external_device->set_mobile_hotspot_supported(mobile_hotspot_supported);
  }

  int device_type;
  if (dictionary.GetInteger(kExternalDeviceKeyDeviceType, &device_type)) {
    if (DeviceType_IsValid(device_type)) {
      external_device->set_device_type(static_cast<DeviceType>(device_type));
    }
  }

  const base::ListValue* beacon_seeds = nullptr;
  dictionary.GetList(kExternalDeviceKeyBeaconSeeds, &beacon_seeds);
  if (beacon_seeds) {
    AddBeaconSeedsToExternalDevice(*beacon_seeds, *external_device);
  }

  return true;
}

}  // namespace

CryptAuthDeviceManager::CryptAuthDeviceManager(
    std::unique_ptr<base::Clock> clock,
    std::unique_ptr<CryptAuthClientFactory> client_factory,
    CryptAuthGCMManager* gcm_manager,
    PrefService* pref_service)
    : clock_(std::move(clock)),
      client_factory_(std::move(client_factory)),
      gcm_manager_(gcm_manager),
      pref_service_(pref_service),
      weak_ptr_factory_(this) {
  UpdateUnlockKeysFromPrefs();
}

// Test-only constructor.
CryptAuthDeviceManager::CryptAuthDeviceManager()
    : clock_(nullptr),
      client_factory_(nullptr),
      gcm_manager_(nullptr),
      pref_service_(nullptr),
      weak_ptr_factory_(this) {}

CryptAuthDeviceManager::~CryptAuthDeviceManager() {
  if (gcm_manager_) {
    gcm_manager_->RemoveObserver(this);
  }
}

// static
void CryptAuthDeviceManager::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDoublePref(prefs::kCryptAuthDeviceSyncLastSyncTimeSeconds,
                               0.0);
  registry->RegisterBooleanPref(
      prefs::kCryptAuthDeviceSyncIsRecoveringFromFailure, false);
  registry->RegisterIntegerPref(prefs::kCryptAuthDeviceSyncReason,
                                INVOCATION_REASON_UNKNOWN);
  registry->RegisterListPref(prefs::kCryptAuthDeviceSyncUnlockKeys);
}

void CryptAuthDeviceManager::Start() {
  gcm_manager_->AddObserver(this);

  base::Time last_successful_sync = GetLastSyncTime();
  base::TimeDelta elapsed_time_since_last_sync =
      clock_->Now() - last_successful_sync;

  bool is_recovering_from_failure =
      pref_service_->GetBoolean(
          prefs::kCryptAuthDeviceSyncIsRecoveringFromFailure) ||
      last_successful_sync.is_null();

  scheduler_ = CreateSyncScheduler();
  scheduler_->Start(elapsed_time_since_last_sync,
                    is_recovering_from_failure
                        ? SyncScheduler::Strategy::AGGRESSIVE_RECOVERY
                        : SyncScheduler::Strategy::PERIODIC_REFRESH);
}

void CryptAuthDeviceManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void CryptAuthDeviceManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void CryptAuthDeviceManager::ForceSyncNow(
    InvocationReason invocation_reason) {
  pref_service_->SetInteger(prefs::kCryptAuthDeviceSyncReason,
                            invocation_reason);
  scheduler_->ForceSync();
}

base::Time CryptAuthDeviceManager::GetLastSyncTime() const {
  return base::Time::FromDoubleT(
      pref_service_->GetDouble(prefs::kCryptAuthDeviceSyncLastSyncTimeSeconds));
}

base::TimeDelta CryptAuthDeviceManager::GetTimeToNextAttempt() const {
  return scheduler_->GetTimeToNextSync();
}

bool CryptAuthDeviceManager::IsSyncInProgress() const {
  return scheduler_->GetSyncState() ==
         SyncScheduler::SyncState::SYNC_IN_PROGRESS;
}

bool CryptAuthDeviceManager::IsRecoveringFromFailure() const {
  return scheduler_->GetStrategy() ==
         SyncScheduler::Strategy::AGGRESSIVE_RECOVERY;
}

std::vector<ExternalDeviceInfo>
CryptAuthDeviceManager::GetSyncedDevices() const {
  return synced_devices_;
}

std::vector<ExternalDeviceInfo> CryptAuthDeviceManager::GetUnlockKeys() const {
  std::vector<ExternalDeviceInfo> unlock_keys;
  for (const auto& device : synced_devices_) {
    if (device.unlock_key()) {
      unlock_keys.push_back(device);
    }
  }
  return unlock_keys;
}

std::vector<ExternalDeviceInfo> CryptAuthDeviceManager::GetTetherHosts() const {
  std::vector<ExternalDeviceInfo> tether_hosts;
  for (const auto& device : synced_devices_) {
    if (device.mobile_hotspot_supported()) {
      tether_hosts.push_back(device);
    }
  }
  return tether_hosts;
}

void CryptAuthDeviceManager::OnGetMyDevicesSuccess(
    const GetMyDevicesResponse& response) {
  // Update the synced devices stored in the user's prefs.
  std::unique_ptr<base::ListValue> devices_as_list(new base::ListValue());
  for (const auto& device : response.devices()) {
    devices_as_list->Append(UnlockKeyToDictionary(device));
  }
  PA_LOG(INFO) << "Devices Synced:\n" << *devices_as_list;

  bool unlock_keys_changed = !devices_as_list->Equals(
      pref_service_->GetList(prefs::kCryptAuthDeviceSyncUnlockKeys));
  {
    ListPrefUpdate update(pref_service_, prefs::kCryptAuthDeviceSyncUnlockKeys);
    update.Get()->Swap(devices_as_list.get());
  }
  UpdateUnlockKeysFromPrefs();

  // Reset metadata used for scheduling syncing.
  pref_service_->SetBoolean(prefs::kCryptAuthDeviceSyncIsRecoveringFromFailure,
                            false);
  pref_service_->SetDouble(prefs::kCryptAuthDeviceSyncLastSyncTimeSeconds,
                           clock_->Now().ToDoubleT());
  pref_service_->SetInteger(prefs::kCryptAuthDeviceSyncReason,
                            INVOCATION_REASON_UNKNOWN);

  sync_request_->OnDidComplete(true);
  cryptauth_client_.reset();
  sync_request_.reset();
  for (auto& observer : observers_) {
    observer.OnSyncFinished(SyncResult::SUCCESS,
                            unlock_keys_changed
                                ? DeviceChangeResult::CHANGED
                                : DeviceChangeResult::UNCHANGED);
  }
}

void CryptAuthDeviceManager::OnGetMyDevicesFailure(const std::string& error) {
  PA_LOG(ERROR) << "GetMyDevices API failed: " << error;
  pref_service_->SetBoolean(prefs::kCryptAuthDeviceSyncIsRecoveringFromFailure,
                            true);
  sync_request_->OnDidComplete(false);
  cryptauth_client_.reset();
  sync_request_.reset();
  for (auto& observer : observers_)
    observer.OnSyncFinished(SyncResult::FAILURE, DeviceChangeResult::UNCHANGED);
}

std::unique_ptr<SyncScheduler> CryptAuthDeviceManager::CreateSyncScheduler() {
  return base::MakeUnique<SyncSchedulerImpl>(
      this, base::TimeDelta::FromHours(kRefreshPeriodHours),
      base::TimeDelta::FromMinutes(kDeviceSyncBaseRecoveryPeriodMinutes),
      kDeviceSyncMaxJitterRatio, "CryptAuth DeviceSync");
}

void CryptAuthDeviceManager::OnResyncMessage() {
  ForceSyncNow(INVOCATION_REASON_SERVER_INITIATED);
}

void CryptAuthDeviceManager::UpdateUnlockKeysFromPrefs() {
  const base::ListValue* unlock_key_list =
      pref_service_->GetList(prefs::kCryptAuthDeviceSyncUnlockKeys);
  synced_devices_.clear();
  for (size_t i = 0; i < unlock_key_list->GetSize(); ++i) {
    const base::DictionaryValue* unlock_key_dictionary;
    if (unlock_key_list->GetDictionary(i, &unlock_key_dictionary)) {
      ExternalDeviceInfo unlock_key;
      if (DictionaryToUnlockKey(*unlock_key_dictionary, &unlock_key)) {
        synced_devices_.push_back(unlock_key);
      } else {
        PA_LOG(ERROR) << "Unable to deserialize unlock key dictionary "
                      << "(index=" << i << "):\n" << *unlock_key_dictionary;
      }
    } else {
      PA_LOG(ERROR) << "Can not get dictionary in list of unlock keys "
                    << "(index=" << i << "):\n" << *unlock_key_list;
    }
  }
}

void CryptAuthDeviceManager::OnSyncRequested(
    std::unique_ptr<SyncScheduler::SyncRequest> sync_request) {
  for (auto& observer : observers_)
    observer.OnSyncStarted();

  sync_request_ = std::move(sync_request);
  cryptauth_client_ = client_factory_->CreateInstance();

  InvocationReason invocation_reason =
      INVOCATION_REASON_UNKNOWN;

  int reason_stored_in_prefs =
      pref_service_->GetInteger(prefs::kCryptAuthDeviceSyncReason);

  // If the sync attempt is not forced, it is acceptable for CryptAuth to return
  // a cached copy of the user's devices, rather taking a database hit for the
  // freshest data.
  bool is_sync_speculative =
      reason_stored_in_prefs != INVOCATION_REASON_UNKNOWN;

  if (InvocationReason_IsValid(reason_stored_in_prefs) &&
      reason_stored_in_prefs != INVOCATION_REASON_UNKNOWN) {
    invocation_reason =
        static_cast<InvocationReason>(reason_stored_in_prefs);
  } else if (GetLastSyncTime().is_null()) {
    invocation_reason = INVOCATION_REASON_INITIALIZATION;
  } else if (IsRecoveringFromFailure()) {
    invocation_reason = INVOCATION_REASON_FAILURE_RECOVERY;
  } else {
    invocation_reason = INVOCATION_REASON_PERIODIC;
  }

  GetMyDevicesRequest request;
  request.set_invocation_reason(invocation_reason);
  request.set_allow_stale_read(is_sync_speculative);
  cryptauth_client_->GetMyDevices(
      request, base::Bind(&CryptAuthDeviceManager::OnGetMyDevicesSuccess,
                          weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&CryptAuthDeviceManager::OnGetMyDevicesFailure,
                 weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace cryptauth
