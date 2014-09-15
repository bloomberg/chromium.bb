// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_remove_keys_operation.h"

#include "base/bind.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_key_manager.h"
#include "chromeos/cryptohome/homedir_methods.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace chromeos {

EasyUnlockRemoveKeysOperation::EasyUnlockRemoveKeysOperation(
    const UserContext& user_context,
    size_t start_index,
    const RemoveKeysCallback& callback)
    : user_context_(user_context),
      callback_(callback),
      key_index_(start_index),
      weak_ptr_factory_(this) {
  // Must have the secret and callback.
  DCHECK(!user_context_.GetKey()->GetSecret().empty());
  DCHECK(!callback_.is_null());
}

EasyUnlockRemoveKeysOperation::~EasyUnlockRemoveKeysOperation() {
}

void EasyUnlockRemoveKeysOperation::Start() {
  // TODO(xiyuan): Use ListKeyEx and delete by label instead of by index.
  RemoveKey();
}

void EasyUnlockRemoveKeysOperation::RemoveKey() {
  std::string canonicalized =
      gaia::CanonicalizeEmail(user_context_.GetUserID());
  cryptohome::Identification id(canonicalized);
  const Key* const auth_key = user_context_.GetKey();
  cryptohome::Authorization auth(auth_key->GetSecret(), auth_key->GetLabel());

  cryptohome::HomedirMethods::GetInstance()->RemoveKeyEx(
      id,
      auth,
      EasyUnlockKeyManager::GetKeyLabel(key_index_),
      base::Bind(&EasyUnlockRemoveKeysOperation::OnKeyRemoved,
                 weak_ptr_factory_.GetWeakPtr()));
}

void EasyUnlockRemoveKeysOperation::OnKeyRemoved(
    bool success,
    cryptohome::MountError return_code) {
  if (success) {
    ++key_index_;
    RemoveKey();
    return;
  }

  // MOUNT_ERROR_KEY_FAILURE is considered as success. Other error codes are
  // treated as failures.
  if (return_code == cryptohome::MOUNT_ERROR_KEY_FAILURE) {
    callback_.Run(true);
  } else {
    LOG(ERROR) << "Easy unlock remove keys operation failed, code="
               << return_code;
    callback_.Run(false);
  }
}

}  // namespace chromeos
