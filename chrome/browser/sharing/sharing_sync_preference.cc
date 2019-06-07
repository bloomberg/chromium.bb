// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_sync_preference.h"

#include "base/base64.h"
#include "base/value_conversions.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync_preferences/pref_service_syncable.h"

namespace {

const char kVapidKey[] = "sharing.vapid_key";
const char kVapidECPrivateKey[] = "vapid_private_key";
const char kVapidCreationTimestamp[] = "vapid_creation_timestamp";

const char kSyncedDevices[] = "sharing.synced_devices";
const char kDeviceFcmToken[] = "device_fcm_token";
const char kDeviceP256dh[] = "device_p256dh";
const char kDeviceAuthSecret[] = "device_auth_secret";
const char kDeviceCapabilities[] = "device_capabilities";
}  // namespace

SharingSyncPreference::Device::Device(const std::string& fcm_token,
                                      const std::string& p256dh,
                                      const std::string& auth_secret,
                                      const int capabilities)
    : fcm_token(fcm_token),
      p256dh(p256dh),
      auth_secret(auth_secret),
      capabilities(capabilities) {}

SharingSyncPreference::Device::Device(Device&& other) = default;

SharingSyncPreference::Device::~Device() = default;

SharingSyncPreference::SharingSyncPreference(PrefService* prefs)
    : prefs_(prefs) {}

SharingSyncPreference::~SharingSyncPreference() = default;

// static
void SharingSyncPreference::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(
      kSyncedDevices, user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterDictionaryPref(
      kVapidKey, user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

base::Optional<std::vector<uint8_t>> SharingSyncPreference::GetVapidKey()
    const {
  const base::DictionaryValue* vapid_key = prefs_->GetDictionary(kVapidKey);
  std::string base64_private_key, private_key;
  if (!vapid_key->GetString(kVapidECPrivateKey, &base64_private_key))
    return base::nullopt;

  if (base::Base64Decode(base64_private_key, &private_key)) {
    return std::vector<uint8_t>(private_key.begin(), private_key.end());
  } else {
    LOG(ERROR) << "Could not decode stored vapid keys.";
    return base::nullopt;
  }
}

void SharingSyncPreference::SetVapidKey(
    const std::vector<uint8_t>& vapid_key) const {
  base::Time creation_timestamp = base::Time::Now();
  std::string base64_vapid_key;
  base::Base64Encode(std::string(vapid_key.begin(), vapid_key.end()),
                     &base64_vapid_key);

  DictionaryPrefUpdate update(prefs_, kVapidKey);
  update->SetString(kVapidECPrivateKey, base64_vapid_key);
  update->SetString(kVapidCreationTimestamp,
                    base::CreateTimeValue(creation_timestamp).GetString());
}

std::map<std::string, SharingSyncPreference::Device>
SharingSyncPreference::GetSyncedDevices() const {
  std::map<std::string, Device> synced_devices;
  const base::DictionaryValue* devices_preferences =
      prefs_->GetDictionary(kSyncedDevices);
  for (const auto& it : devices_preferences->DictItems()) {
    base::Optional<Device> device = ValueToDevice(it.second);
    if (device)
      synced_devices.emplace(it.first, std::move(*device));
  }
  return synced_devices;
}

void SharingSyncPreference::SetSyncDevice(const std::string& guid,
                                          const Device& device) {
  DictionaryPrefUpdate update(prefs_, kSyncedDevices);
  update->SetKey(guid, DeviceToValue(device));
}

void SharingSyncPreference::RemoveDevice(const std::string& guid) {
  DictionaryPrefUpdate update(prefs_, kSyncedDevices);
  update->RemoveKey(guid);
}

base::Value SharingSyncPreference::DeviceToValue(const Device& device) const {
  std::string base64_p256dh, base64_auth_secret;
  base::Base64Encode(device.p256dh, &base64_p256dh);
  base::Base64Encode(device.auth_secret, &base64_auth_secret);

  base::Value result(base::Value::Type::DICTIONARY);
  result.SetStringKey(kDeviceFcmToken, device.fcm_token);
  result.SetStringKey(kDeviceP256dh, base64_p256dh);
  result.SetStringKey(kDeviceAuthSecret, base64_auth_secret);
  result.SetIntKey(kDeviceCapabilities, device.capabilities);
  return result;
}

base::Optional<SharingSyncPreference::Device>
SharingSyncPreference::ValueToDevice(const base::Value& value) const {
  const std::string* fcm_token = value.FindStringKey(kDeviceFcmToken);
  const std::string* base64_p256dh = value.FindStringKey(kDeviceP256dh);
  const std::string* base64_auth_secret =
      value.FindStringKey(kDeviceAuthSecret);
  const base::Optional<int> capabilities =
      value.FindIntKey(kDeviceCapabilities);

  std::string p256dh, auth_secret;
  if (!fcm_token || !base64_p256dh || !base64_auth_secret || !capabilities ||
      !base::Base64Decode(*base64_p256dh, &p256dh) ||
      !base::Base64Decode(*base64_auth_secret, &auth_secret)) {
    LOG(ERROR) << "Could not convert synced value to device object.";
    return base::nullopt;
  } else {
    return Device(*fcm_token, p256dh, auth_secret, *capabilities);
  }
}
