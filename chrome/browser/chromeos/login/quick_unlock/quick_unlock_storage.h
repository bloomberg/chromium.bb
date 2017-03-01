// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_QUICK_UNLOCK_QUICK_UNLOCK_STORAGE_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_QUICK_UNLOCK_QUICK_UNLOCK_STORAGE_H_

#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/login/quick_unlock/fingerprint_storage.h"
#include "chrome/browser/chromeos/login/quick_unlock/pin_storage.h"
#include "components/keyed_service/core/keyed_service.h"

class PrefService;

namespace chromeos {

class QuickUnlockStorageTestApi;

namespace quick_unlock {

class QuickUnlockStorage : public KeyedService {
 public:
  explicit QuickUnlockStorage(PrefService* pref_service);
  ~QuickUnlockStorage() override;

  // Mark that the user has had a strong authentication. This means
  // that they authenticated with their password, for example. Quick
  // unlock will timeout after a delay.
  void MarkStrongAuth();

  // Returns true if the user has been strongly authenticated.
  bool HasStrongAuth() const;

  // Returns the time since the last strong authentication. This should not be
  // called if HasStrongAuth returns false.
  base::TimeDelta TimeSinceLastStrongAuth() const;

  // Returns true if fingerprint unlock is currently available.
  bool IsFingerprintAuthenticationAvailable() const;

  // Returns true if PIN unlock is currently available.
  bool IsPinAuthenticationAvailable() const;

  // Tries to authenticate the given pin. This will consume a pin unlock
  // attempt. This always returns false if HasStrongAuth returns false.
  bool TryAuthenticatePin(const std::string& pin);

  FingerprintStorage* fingerprint_storage();

  PinStorage* pin_storage();

 private:
  friend class chromeos::QuickUnlockStorageTestApi;

  // KeyedService:
  void Shutdown() override;

  PrefService* pref_service_;
  base::Time last_strong_auth_;
  std::unique_ptr<FingerprintStorage> fingerprint_storage_;
  std::unique_ptr<PinStorage> pin_storage_;

  DISALLOW_COPY_AND_ASSIGN(QuickUnlockStorage);
};

}  // namespace quick_unlock
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_QUICK_UNLOCK_QUICK_UNLOCK_STORAGE_H_
