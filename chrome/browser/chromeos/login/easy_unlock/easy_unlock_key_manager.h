// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_EASY_UNLOCK_EASY_UNLOCK_KEY_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_EASY_UNLOCK_EASY_UNLOCK_KEY_MANAGER_H_

#include <deque>
#include <map>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/stl_util.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_get_keys_operation.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_refresh_keys_operation.h"
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
  typedef EasyUnlockRefreshKeysOperation::RefreshKeysCallback
      RefreshKeysCallback;
  typedef EasyUnlockGetKeysOperation::GetKeysCallback GetDeviceDataListCallback;

  EasyUnlockKeyManager();
  ~EasyUnlockKeyManager();

  // Nukes existing Easy unlock keys and creates new ones for the given
  // |remote_devices| and the given |user_context|. |user_context| must have
  // secret to allow keys to be created.
  void RefreshKeys(const UserContext& user_context,
                   const base::ListValue& remote_devices,
                   const RefreshKeysCallback& callback);

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
  // Runs the next operation if there is one. We first run all the operations in
  // the |write_operation_queue_| and then run all the operations in the
  // |read_operation_queue_|.
  void RunNextOperation();

  // Called when the TPM key is ready to be used for creating Easy Unlock key
  // challenges.
  void RefreshKeysWithTpmKeyPresent(const UserContext& user_context,
                                    base::ListValue* remote_devices,
                                    const RefreshKeysCallback& callback);

  // Returns true if there are pending operations.
  bool HasPendingOperations() const;

  // Callback invoked after refresh keys operation.
  void OnKeysRefreshed(const RefreshKeysCallback& callback,
                       bool create_success);

  // Callback invoked after get keys op.
  void OnKeysFetched(const GetDeviceDataListCallback& callback,
                     bool fetch_success,
                     const EasyUnlockDeviceKeyDataList& fetched_data);

  // Queued operations are stored as raw pointers, as scoped_ptrs may not behave
  // nicely with std::deque.
  using WriteOperationQueue = std::deque<EasyUnlockRefreshKeysOperation*>;
  using ReadOperationQueue = std::deque<EasyUnlockGetKeysOperation*>;
  WriteOperationQueue write_operation_queue_;
  ReadOperationQueue read_operation_queue_;

  // Scopes the raw operation pointers to the lifetime of this object.
  STLElementDeleter<WriteOperationQueue> write_queue_deleter_;
  STLElementDeleter<ReadOperationQueue> read_queue_deleter_;

  // Stores the current operation in progress. At most one of these variables
  // can be non-null at any time.
  scoped_ptr<EasyUnlockRefreshKeysOperation> pending_write_operation_;
  scoped_ptr<EasyUnlockGetKeysOperation> pending_read_operation_;

  base::WeakPtrFactory<EasyUnlockKeyManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockKeyManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_EASY_UNLOCK_EASY_UNLOCK_KEY_MANAGER_H_
