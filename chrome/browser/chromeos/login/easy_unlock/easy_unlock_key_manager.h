// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_EASY_UNLOCK_EASY_UNLOCK_KEY_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_EASY_UNLOCK_EASY_UNLOCK_KEY_MANAGER_H_

#include <map>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_create_keys_operation.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_get_keys_operation.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_remove_keys_operation.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_types.h"

namespace base {
class DictionaryValue;
class ListValue;
}

namespace chromeos {

class UserContext;

// A class to manage Easy unlock cryptohome keys.
class EasyUnlockKeyManager {
 public:
  typedef EasyUnlockCreateKeysOperation::CreateKeysCallback RefreshKeysCallback;
  typedef EasyUnlockRemoveKeysOperation::RemoveKeysCallback RemoveKeysCallback;
  typedef EasyUnlockGetKeysOperation::GetKeysCallback GetDeviceDataListCallback;

  EasyUnlockKeyManager();
  ~EasyUnlockKeyManager();

  // Nukes existing Easy unlock keys and creates new ones for the given
  // |remote_devices| and the given |user_context|. |user_context| must have
  // secret to allow keys to be created.
  void RefreshKeys(const UserContext& user_context,
                   const base::ListValue& remote_devices,
                   const RefreshKeysCallback& callback);

  // Remove Easy unlock keys starting at the given index for the given
  // |user_context|.
  void RemoveKeys(const UserContext& user_context,
                  size_t start_index,
                  const RemoveKeysCallback& callback);

  // Retrieves the remote device data from cryptohome keys for the given
  // |user_context|.
  void GetDeviceDataList(const UserContext& user_context,
                         const GetDeviceDataListCallback& callback);

  // Helpers to convert between DeviceData and remote device dictionary.
  // DeviceDataToRemoteDeviceDictionary fills the remote device dictionary and
  // always succeeds. RemoteDeviceDictionaryToDeviceData returns false if the
  // conversion fails (missing required propery). Note that
  // EasyUnlockDeviceKeyData contains a sub set of the remote device dictionary.
  static void DeviceDataToRemoteDeviceDictionary(
      const std::string& user_id,
      const EasyUnlockDeviceKeyData& data,
      base::DictionaryValue* dict);
  static bool RemoteDeviceDictionaryToDeviceData(
      const base::DictionaryValue& dict,
      EasyUnlockDeviceKeyData* data);

  // Helpers to convert between EasyUnlockDeviceKeyDataList and remote devices
  // ListValue.
  static void DeviceDataListToRemoteDeviceList(
      const std::string& user_id,
      const EasyUnlockDeviceKeyDataList& data_list,
      base::ListValue* device_list);
  static bool RemoteDeviceListToDeviceDataList(
      const base::ListValue& device_list,
      EasyUnlockDeviceKeyDataList* data_list);

  // Gets key label for the given key index.
  static std::string GetKeyLabel(size_t key_index);

 private:
  // Returns true if there are pending operations.
  bool HasPendingOperations() const;

  // Returns the next operations id. Currently only used for get keys ops.
  int GetNextOperationId();

  // Callback invoked after create keys op.
  void OnKeysCreated(size_t remove_start_index,
                     const RefreshKeysCallback& callback,
                     bool create_success);

  // Callback invoked after remove keys op.
  void OnKeysRemoved(const RemoveKeysCallback& callback, bool remove_success);

  // Callback invoked after get keys op.
  void OnKeysFetched(int op_id,
                     const GetDeviceDataListCallback& callback,
                     bool fetch_success,
                     const EasyUnlockDeviceKeyDataList& fetched_data);

  int operation_id_;

  scoped_ptr<EasyUnlockCreateKeysOperation> create_keys_op_;
  scoped_ptr<EasyUnlockRemoveKeysOperation> remove_keys_op_;
  std::map<int, EasyUnlockGetKeysOperation*> get_keys_ops_;

  base::WeakPtrFactory<EasyUnlockKeyManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockKeyManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_EASY_UNLOCK_EASY_UNLOCK_KEY_MANAGER_H_
