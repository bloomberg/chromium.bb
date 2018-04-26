// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/quick_unlock/pin_storage_cryptohome.h"

namespace chromeos {
namespace quick_unlock {

PinStorageCryptohome::PinStorageCryptohome() = default;

PinStorageCryptohome::~PinStorageCryptohome() = default;

void PinStorageCryptohome::IsPinSetInCryptohome(const AccountId& account_id,
                                                BoolCallback callback) const {
  NOTREACHED();
}

void PinStorageCryptohome::SetPin(const UserContext& user_context,
                                  const std::string& pin,
                                  BoolCallback did_set) {
  NOTREACHED();
}

void PinStorageCryptohome::RemovePin(const UserContext& user_context,
                                     BoolCallback did_remove) {
  NOTREACHED();
}

void PinStorageCryptohome::TryAuthenticate(const AccountId& account_id,
                                           const std::string& key,
                                           const Key::KeyType& key_type,
                                           BoolCallback result) {
  NOTREACHED();
}

}  // namespace quick_unlock
}  // namespace chromeos
