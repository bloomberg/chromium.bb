// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_QUICK_UNLOCK_PIN_STORAGE_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_QUICK_UNLOCK_PIN_STORAGE_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"

class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

class PinStorageTestApi;

namespace chromeos {

// TODO(jdufault): Figure out the UX we want on the lock screen when there are
// multiple users. We will be storing either global or per-user unlock state. If
// we end up storing global unlock state, we can pull the unlock attempt and
// strong-auth code out of this class.

class PinStorage : public KeyedService {
 public:
  // TODO(jdufault): Pull these values in from policy. See crbug.com/612271.
  static const int kMaximumUnlockAttempts = 3;
  static const base::TimeDelta kStrongAuthTimeout;

  // Registers profile prefs.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  explicit PinStorage(PrefService* pref_service);
  ~PinStorage() override;

  // Mark in storage that the user has had a strong authentication. This means
  // that they authenticated with their password, for example. PIN unlock will
  // timeout after a delay.
  void MarkStrongAuth();
  // Returns true if the user has been strongly authenticated.
  bool HasStrongAuth() const;
  // Returns the time since the last strong authentication. This should not be
  // called if HasStrongAuth returns false.
  base::TimeDelta TimeSinceLastStrongAuth() const;

  // Add a PIN unlock attempt count.
  void AddUnlockAttempt();
  // Reset the number of unlock attempts to 0.
  void ResetUnlockAttemptCount();
  // Returns the number of unlock attempts.
  int unlock_attempt_count() const { return unlock_attempt_count_; }

  // Returns true if a pin is set.
  bool IsPinSet() const;
  // Sets the pin to the given value; IsPinSet will return true.
  void SetPin(const std::string& pin);
  // Removes the pin; IsPinSet will return false.
  void RemovePin();

  // Is PIN entry currently available?
  bool IsPinAuthenticationAvailable() const;

  // Tries to authenticate the given pin. This will consume an unlock attempt.
  // This always returns false if IsPinAuthenticationAvailable returns false.
  bool TryAuthenticatePin(const std::string& pin);

 private:
  // Return the stored salt/secret. This is fetched directly from pref_service_.
  std::string PinSalt() const;
  std::string PinSecret() const;

  friend class ::PinStorageTestApi;

  PrefService* pref_service_;
  int unlock_attempt_count_ = 0;
  base::Time last_strong_auth_;

  DISALLOW_COPY_AND_ASSIGN(PinStorage);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_QUICK_UNLOCK_PIN_STORAGE_H_
