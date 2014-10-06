// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_key_manager.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"

namespace chromeos {

namespace {

const char kKeyBluetoothAddress[] = "bluetoothAddress";
const char kKeyPermitRecord[] = "permitRecord";
const char kKeyPermitId[] = "permitRecord.id";
const char kKeyPermitPermitId[] = "permitRecord.permitId";
const char kKeyPermitData[] = "permitRecord.data";
const char kKeyPermitType[] = "permitRecord.type";
const char kKeyPsk[] = "psk";

const char kKeyLabelPrefix[] = "easy-unlock-";

const char kPermitPermitIdFormat[] = "permit://google.com/easyunlock/v1/%s";
const char kPermitTypeLicence[] = "licence";

}  // namespace

EasyUnlockKeyManager::EasyUnlockKeyManager()
    : operation_id_(0),
      weak_ptr_factory_(this) {
}

EasyUnlockKeyManager::~EasyUnlockKeyManager() {
  STLDeleteContainerPairSecondPointers(get_keys_ops_.begin(),
                                       get_keys_ops_.end());
}

void EasyUnlockKeyManager::RefreshKeys(const UserContext& user_context,
                                       const base::ListValue& remote_devices,
                                       const RefreshKeysCallback& callback) {
  // Must have the secret.
  DCHECK(!user_context.GetKey()->GetSecret().empty());

  EasyUnlockDeviceKeyDataList devices;
  if (!RemoteDeviceListToDeviceDataList(remote_devices, &devices))
    devices.clear();

  // Only one pending request.
  DCHECK(!HasPendingOperations());
  create_keys_op_.reset(new EasyUnlockCreateKeysOperation(
      user_context,
      devices,
      base::Bind(&EasyUnlockKeyManager::OnKeysCreated,
                 weak_ptr_factory_.GetWeakPtr(),
                 devices.size(),
                 callback)));
  create_keys_op_->Start();
}

void EasyUnlockKeyManager::RemoveKeys(const UserContext& user_context,
                                      size_t start_index,
                                      const RemoveKeysCallback& callback) {
  // Must have the secret.
  DCHECK(!user_context.GetKey()->GetSecret().empty());
  // Only one pending request.
  DCHECK(!HasPendingOperations());

  remove_keys_op_.reset(
      new EasyUnlockRemoveKeysOperation(
          user_context,
          start_index,
          base::Bind(&EasyUnlockKeyManager::OnKeysRemoved,
                     weak_ptr_factory_.GetWeakPtr(),
                     callback)));
  remove_keys_op_->Start();
}

void EasyUnlockKeyManager::GetDeviceDataList(
    const UserContext& user_context,
    const GetDeviceDataListCallback& callback) {
  // Defer the get operation if there is pending write operations.
  if (create_keys_op_ || remove_keys_op_) {
    pending_ops_.push_back(base::Bind(&EasyUnlockKeyManager::GetDeviceDataList,
                                      weak_ptr_factory_.GetWeakPtr(),
                                      user_context,
                                      callback));
    return;
  }

  const int op_id = GetNextOperationId();
  scoped_ptr<EasyUnlockGetKeysOperation> op(new EasyUnlockGetKeysOperation(
      user_context,
      base::Bind(&EasyUnlockKeyManager::OnKeysFetched,
                 weak_ptr_factory_.GetWeakPtr(),
                 op_id,
                 callback)));
  op->Start();
  get_keys_ops_[op_id] = op.release();
}

// static
void EasyUnlockKeyManager::DeviceDataToRemoteDeviceDictionary(
    const std::string& user_id,
    const EasyUnlockDeviceKeyData& data,
    base::DictionaryValue* dict) {
  dict->SetString(kKeyBluetoothAddress, data.bluetooth_address);
  dict->SetString(kKeyPsk, data.psk);
  scoped_ptr<base::DictionaryValue> permit_record(new base::DictionaryValue);
  dict->Set(kKeyPermitRecord, permit_record.release());
  dict->SetString(kKeyPermitId, data.public_key);
  dict->SetString(kKeyPermitData, data.public_key);
  dict->SetString(kKeyPermitType, kPermitTypeLicence);
  dict->SetString(kKeyPermitPermitId,
                  base::StringPrintf(kPermitPermitIdFormat,
                                     user_id.c_str()));
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

  data->bluetooth_address.swap(bluetooth_address);
  data->public_key.swap(public_key);
  data->psk.swap(psk);
  return true;
}

// static
void EasyUnlockKeyManager::DeviceDataListToRemoteDeviceList(
    const std::string& user_id,
    const EasyUnlockDeviceKeyDataList& data_list,
    base::ListValue* device_list) {
  device_list->Clear();
  for (size_t i = 0; i < data_list.size(); ++i) {
    scoped_ptr<base::DictionaryValue> device_dict(new base::DictionaryValue);
    DeviceDataToRemoteDeviceDictionary(
        user_id, data_list[i], device_dict.get());
    device_list->Append(device_dict.release());
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
    if (!(*it)->GetAsDictionary(&dict) || !dict)
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

bool EasyUnlockKeyManager::HasPendingOperations() const {
  return create_keys_op_ || remove_keys_op_ || !get_keys_ops_.empty();
}

int EasyUnlockKeyManager::GetNextOperationId() {
  return ++operation_id_;
}

void EasyUnlockKeyManager::RunNextPendingOp() {
  if (pending_ops_.empty())
    return;

  pending_ops_.front().Run();
  pending_ops_.pop_front();
}

void EasyUnlockKeyManager::OnKeysCreated(
    size_t remove_start_index,
    const RefreshKeysCallback& callback,
    bool create_success) {
  scoped_ptr<EasyUnlockCreateKeysOperation> op = create_keys_op_.Pass();
  if (!callback.is_null())
    callback.Run(create_success);

  // Remove extra existing keys.
  RemoveKeys(op->user_context(), remove_start_index, RemoveKeysCallback());
}

void EasyUnlockKeyManager::OnKeysRemoved(const RemoveKeysCallback& callback,
                                         bool remove_success) {
  scoped_ptr<EasyUnlockRemoveKeysOperation> op = remove_keys_op_.Pass();
  if (!callback.is_null())
    callback.Run(remove_success);

  if (!HasPendingOperations())
    RunNextPendingOp();
}

void EasyUnlockKeyManager::OnKeysFetched(
    int op_id,
    const GetDeviceDataListCallback& callback,
    bool fetch_success,
    const EasyUnlockDeviceKeyDataList& fetched_data) {
  std::map<int, EasyUnlockGetKeysOperation*>::iterator it =
      get_keys_ops_.find(op_id);
  scoped_ptr<EasyUnlockGetKeysOperation> op;
  if (it != get_keys_ops_.end()) {
    op.reset(it->second);
    get_keys_ops_.erase(it);
  } else {
    NOTREACHED();
  }

  if (!callback.is_null())
    callback.Run(fetch_success, fetched_data);

  if (!HasPendingOperations())
    RunNextPendingOp();
}

}  // namespace chromeos
