// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_key_manager.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_tpm_key_manager.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_tpm_key_manager_factory.h"
#include "components/proximity_auth/logging/logging.h"
#include "components/signin/core/account_id/account_id.h"

namespace chromeos {

namespace {

const char kKeyBluetoothAddress[] = "bluetoothAddress";
const char kKeyBluetoothType[] = "bluetoothType";
const char kKeyPermitRecord[] = "permitRecord";
const char kKeyPermitId[] = "permitRecord.id";
const char kKeyPermitPermitId[] = "permitRecord.permitId";
const char kKeyPermitData[] = "permitRecord.data";
const char kKeyPermitType[] = "permitRecord.type";
const char kKeyPsk[] = "psk";
const char kKeySerializedBeaconSeeds[] = "serializedBeaconSeeds";

const char kKeyLabelPrefix[] = "easy-unlock-";

const char kPermitPermitIdFormat[] = "permit://google.com/easyunlock/v1/%s";
const char kPermitTypeLicence[] = "licence";

}  // namespace

EasyUnlockKeyManager::EasyUnlockKeyManager() : weak_ptr_factory_(this) {}

EasyUnlockKeyManager::~EasyUnlockKeyManager() {
}

void EasyUnlockKeyManager::RefreshKeys(const UserContext& user_context,
                                       const base::ListValue& remote_devices,
                                       const RefreshKeysCallback& callback) {
  base::Closure do_refresh_keys = base::Bind(
      &EasyUnlockKeyManager::RefreshKeysWithTpmKeyPresent,
      weak_ptr_factory_.GetWeakPtr(),
      user_context,
      base::Owned(remote_devices.DeepCopy()),
      callback);

  EasyUnlockTpmKeyManager* tpm_key_manager =
      EasyUnlockTpmKeyManagerFactory::GetInstance()->GetForUser(
          user_context.GetAccountId().GetUserEmail());
  if (!tpm_key_manager) {
    PA_LOG(ERROR) << "No TPM key manager.";
    callback.Run(false);
    return;
  }

  // Private TPM key is needed only when adding new keys.
  if (remote_devices.empty() ||
      tpm_key_manager->PrepareTpmKey(false /* check_private_key */,
                                     do_refresh_keys)) {
    do_refresh_keys.Run();
  } else {
    // In case Chrome is supposed to restart to apply user session flags, the
    // Chrome restart will be postponed until Easy Sign-in keys are refreshed.
    // This is to ensure that creating TPM key does not hang if TPM system
    // loading takes too much time. Note that in normal circumstances the
    // chances that TPM slot cannot be loaded should be extremely low.
    // TODO(tbarzic): Add some metrics to measure if the timeout even gets hit.
    tpm_key_manager->StartGetSystemSlotTimeoutMs(2000);
  }
}

void EasyUnlockKeyManager::RefreshKeysWithTpmKeyPresent(
    const UserContext& user_context,
    base::ListValue* remote_devices,
    const RefreshKeysCallback& callback) {
  EasyUnlockTpmKeyManager* tpm_key_manager =
      EasyUnlockTpmKeyManagerFactory::GetInstance()->GetForUser(
          user_context.GetAccountId().GetUserEmail());
  const std::string tpm_public_key =
      tpm_key_manager->GetPublicTpmKey(user_context.GetAccountId());

  EasyUnlockDeviceKeyDataList devices;
  if (!RemoteDeviceListToDeviceDataList(*remote_devices, &devices))
    devices.clear();

  write_operation_queue_.push_back(
      base::MakeUnique<EasyUnlockRefreshKeysOperation>(
          user_context, tpm_public_key, devices,
          base::Bind(&EasyUnlockKeyManager::OnKeysRefreshed,
                     weak_ptr_factory_.GetWeakPtr(), callback)));
  RunNextOperation();
}

void EasyUnlockKeyManager::GetDeviceDataList(
    const UserContext& user_context,
    const GetDeviceDataListCallback& callback) {
  read_operation_queue_.push_back(base::MakeUnique<EasyUnlockGetKeysOperation>(
      user_context, base::Bind(&EasyUnlockKeyManager::OnKeysFetched,
                               weak_ptr_factory_.GetWeakPtr(), callback)));
  RunNextOperation();
}

// static
void EasyUnlockKeyManager::DeviceDataToRemoteDeviceDictionary(
    const AccountId& account_id,
    const EasyUnlockDeviceKeyData& data,
    base::DictionaryValue* dict) {
  dict->SetString(kKeyBluetoothAddress, data.bluetooth_address);
  dict->SetInteger(kKeyBluetoothType, static_cast<int>(data.bluetooth_type));
  dict->SetString(kKeyPsk, data.psk);
  std::unique_ptr<base::DictionaryValue> permit_record(
      new base::DictionaryValue);
  dict->Set(kKeyPermitRecord, std::move(permit_record));
  dict->SetString(kKeyPermitId, data.public_key);
  dict->SetString(kKeyPermitData, data.public_key);
  dict->SetString(kKeyPermitType, kPermitTypeLicence);
  dict->SetString(kKeyPermitPermitId,
                  base::StringPrintf(kPermitPermitIdFormat,
                                     account_id.GetUserEmail().c_str()));
  dict->SetString(kKeySerializedBeaconSeeds, data.serialized_beacon_seeds);
}

// static
bool EasyUnlockKeyManager::RemoteDeviceDictionaryToDeviceData(
    const base::DictionaryValue& dict,
    EasyUnlockDeviceKeyData* data) {
  std::string bluetooth_address;
  std::string public_key;
  std::string psk;

  if (!dict.GetString(kKeyBluetoothAddress, &bluetooth_address) ||
      !dict.GetString(kKeyPermitId, &public_key) ||
      !dict.GetString(kKeyPsk, &psk)) {
    return false;
  }

  // TODO(tengs): Move this conditional up once we can be certain that the
  // dictionary will contain the Bluetooth type key.
  int bluetooth_type_as_int;
  if (dict.GetInteger(kKeyBluetoothType, &bluetooth_type_as_int)) {
    if (bluetooth_type_as_int >= EasyUnlockDeviceKeyData::NUM_BLUETOOTH_TYPES) {
      PA_LOG(ERROR) << "Invalid Bluetooth type: " << bluetooth_type_as_int;
    } else {
      data->bluetooth_type =
          static_cast<EasyUnlockDeviceKeyData::BluetoothType>(
              bluetooth_type_as_int);
    }
  }

  std::string serialized_beacon_seeds;
  if (dict.GetString(kKeySerializedBeaconSeeds, &serialized_beacon_seeds)) {
    data->serialized_beacon_seeds = serialized_beacon_seeds;
  } else {
    PA_LOG(ERROR) << "Failed to parse key data: "
                  << "expected serialized_beacon_seeds.";
  }

  data->bluetooth_address.swap(bluetooth_address);
  data->public_key.swap(public_key);
  data->psk.swap(psk);
  return true;
}

// static
void EasyUnlockKeyManager::DeviceDataListToRemoteDeviceList(
    const AccountId& account_id,
    const EasyUnlockDeviceKeyDataList& data_list,
    base::ListValue* device_list) {
  device_list->Clear();
  for (size_t i = 0; i < data_list.size(); ++i) {
    std::unique_ptr<base::DictionaryValue> device_dict(
        new base::DictionaryValue);
    DeviceDataToRemoteDeviceDictionary(account_id, data_list[i],
                                       device_dict.get());
    device_list->Append(std::move(device_dict));
  }
}

// static
bool EasyUnlockKeyManager::RemoteDeviceListToDeviceDataList(
    const base::ListValue& device_list,
    EasyUnlockDeviceKeyDataList* data_list) {
  EasyUnlockDeviceKeyDataList parsed_devices;
  for (base::ListValue::const_iterator it = device_list.begin();
       it != device_list.end();
       ++it) {
    const base::DictionaryValue* dict;
    if (!it->GetAsDictionary(&dict) || !dict)
      return false;

    EasyUnlockDeviceKeyData data;
    if (!RemoteDeviceDictionaryToDeviceData(*dict, &data))
      return false;

    parsed_devices.push_back(data);
  }

  data_list->swap(parsed_devices);
  return true;
}

// static
std::string EasyUnlockKeyManager::GetKeyLabel(size_t key_index) {
  return base::StringPrintf("%s%zu", kKeyLabelPrefix, key_index);
}

void EasyUnlockKeyManager::RunNextOperation() {
  if (pending_write_operation_ || pending_read_operation_)
    return;

  if (!write_operation_queue_.empty()) {
    pending_write_operation_ = std::move(write_operation_queue_.front());
    write_operation_queue_.pop_front();
    pending_write_operation_->Start();
  } else if (!read_operation_queue_.empty()) {
    pending_read_operation_ = std::move(read_operation_queue_.front());
    read_operation_queue_.pop_front();
    pending_read_operation_->Start();
  }
}

void EasyUnlockKeyManager::OnKeysRefreshed(const RefreshKeysCallback& callback,
                                           bool refresh_success) {
  if (!callback.is_null())
    callback.Run(refresh_success);

  DCHECK(pending_write_operation_);
  pending_write_operation_.reset();
  RunNextOperation();
}

void EasyUnlockKeyManager::OnKeysFetched(
    const GetDeviceDataListCallback& callback,
    bool fetch_success,
    const EasyUnlockDeviceKeyDataList& fetched_data) {
  if (!callback.is_null())
    callback.Run(fetch_success, fetched_data);

  DCHECK(pending_read_operation_);
  pending_read_operation_.reset();
  RunNextOperation();
}

}  // namespace chromeos
