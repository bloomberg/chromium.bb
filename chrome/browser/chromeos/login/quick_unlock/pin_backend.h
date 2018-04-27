// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_QUICK_UNLOCK_PIN_BACKEND_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_QUICK_UNLOCK_PIN_BACKEND_H_

#include <string>

#include "base/callback.h"
#include "chromeos/login/auth/key.h"

class AccountId;

namespace chromeos {

namespace quick_unlock {

class PinStorageCryptohome;

// Provides high-level access to the user's PIN. The underlying storage can be
// either cryptohome or prefs.
class PinBackend {
 public:
  using BoolCallback = base::OnceCallback<void(bool)>;

  // Fetch the PinBackend instance.
  static PinBackend* GetInstance();

  // Use GetInstance().
  PinBackend();
  ~PinBackend();

  // Check if the given account_id has a PIN registered.
  void IsSet(const AccountId& account_id, BoolCallback result);

  // Set the PIN for the given user.
  void Set(const AccountId& account_id,
           const std::string& auth_token,
           const std::string& pin,
           BoolCallback did_set);

  // Remove the given user's PIN.
  void Remove(const AccountId& account_id,
              const std::string& auth_token,
              BoolCallback did_remove);

  // Is PIN authentication available for the given account? Even if PIN is set,
  // it may not be available for authentication due to some additional
  // restrictions.
  void CanAuthenticate(const AccountId& account_id, BoolCallback result);

  // Try to authenticate.
  void TryAuthenticate(const AccountId& account_id,
                       const std::string& key,
                       const Key::KeyType& key_type,
                       BoolCallback result);

  // Computes the secret for a given |pin| and |salt|.
  std::string ComputeSecret(const std::string& pin,
                            const std::string& salt,
                            Key::KeyType key_type);

  // Resets any cached state for testing purposes.
  static void ResetForTesting();

 private:
  // True if still trying to determine which backend should be used.
  bool resolving_backend_ = true;
  // Determining if the device supports cryptohome-based keys requires an async
  // dbus call to cryptohome. If we receive a request before we know which
  // backend to use, the request will be pushed to this list and invoked once
  // the backend configuration is determined.
  std::vector<base::OnceClosure> on_cryptohome_support_received_;

  // Non-null if we should use the cryptohome backend. If null, the prefs
  // backend should be used.
  std::unique_ptr<PinStorageCryptohome> cryptohome_backend_;

  DISALLOW_COPY_AND_ASSIGN(PinBackend);
};

}  // namespace quick_unlock
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_QUICK_UNLOCK_PIN_BACKEND_H_
