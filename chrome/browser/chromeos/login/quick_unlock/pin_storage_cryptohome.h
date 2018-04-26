// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_QUICK_UNLOCK_PIN_STORAGE_CRYPTOHOME_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_QUICK_UNLOCK_PIN_STORAGE_CRYPTOHOME_H_

#include <string>

#include "base/callback.h"
#include "chromeos/login/auth/user_context.h"

class AccountId;

namespace chromeos {

class UserContext;

namespace quick_unlock {

class PinStorageCryptohome {
 public:
  using BoolCallback = base::OnceCallback<void(bool)>;

  PinStorageCryptohome();
  ~PinStorageCryptohome();

  void IsPinSetInCryptohome(const AccountId& account_id,
                            BoolCallback result) const;
  void SetPin(const UserContext& user_context,
              const std::string& pin,
              BoolCallback did_set);
  void RemovePin(const UserContext& user_context, BoolCallback did_remove);
  void TryAuthenticate(const AccountId& account_id,
                       const std::string& key,
                       const Key::KeyType& key_type,
                       BoolCallback result);

 private:
  DISALLOW_COPY_AND_ASSIGN(PinStorageCryptohome);
};

}  // namespace quick_unlock
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_QUICK_UNLOCK_PIN_STORAGE_CRYPTOHOME_H_
