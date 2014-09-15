// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_EASY_UNLOCK_EASY_UNLOCK_GET_KEYS_OPERATION_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_EASY_UNLOCK_EASY_UNLOCK_GET_KEYS_OPERATION_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_types.h"
#include "chromeos/cryptohome/homedir_methods.h"
#include "chromeos/login/auth/user_context.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

class EasyUnlockGetKeysOperation {
 public:
  typedef base::Callback<void(bool success,
                              const EasyUnlockDeviceKeyDataList& data_list)>
      GetKeysCallback;
  EasyUnlockGetKeysOperation(const UserContext& user_context,
                             const GetKeysCallback& callback);
  ~EasyUnlockGetKeysOperation();

  void Start();

 private:
  void GetKeyData();
  void OnGetKeyData(
      bool success,
      cryptohome::MountError return_code,
      const std::vector<cryptohome::KeyDefinition>& key_definitions);

  UserContext user_context_;
  GetKeysCallback callback_;

  size_t key_index_;
  EasyUnlockDeviceKeyDataList devices_;

  base::WeakPtrFactory<EasyUnlockGetKeysOperation> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockGetKeysOperation);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_EASY_UNLOCK_EASY_UNLOCK_GET_KEYS_OPERATION_H_
