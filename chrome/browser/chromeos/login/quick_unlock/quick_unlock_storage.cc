// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_storage.h"

#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace chromeos {
namespace quick_unlock {

QuickUnlockStorage::QuickUnlockStorage(PrefService* pref_service)
    : pref_service_(pref_service) {
  fingerprint_storage_ = base::MakeUnique<FingerprintStorage>(pref_service);
  pin_storage_ = base::MakeUnique<PinStorage>(pref_service);
}

QuickUnlockStorage::~QuickUnlockStorage() {}

void QuickUnlockStorage::MarkStrongAuth() {
  last_strong_auth_ = base::Time::Now();
  fingerprint_storage()->ResetUnlockAttemptCount();
  pin_storage()->ResetUnlockAttemptCount();
}

bool QuickUnlockStorage::HasStrongAuth() const {
  if (last_strong_auth_.is_null())
    return false;

  // PIN and fingerprint share the same timeout policy.
  PasswordConfirmationFrequency strong_auth_interval =
      static_cast<PasswordConfirmationFrequency>(
          pref_service_->GetInteger(prefs::kQuickUnlockTimeout));
  base::TimeDelta strong_auth_timeout =
      PasswordConfirmationFrequencyToTimeDelta(strong_auth_interval);

  return TimeSinceLastStrongAuth() < strong_auth_timeout;
}

base::TimeDelta QuickUnlockStorage::TimeSinceLastStrongAuth() const {
  DCHECK(!last_strong_auth_.is_null());
  return base::Time::Now() - last_strong_auth_;
}

bool QuickUnlockStorage::IsFingerprintAuthenticationAvailable() const {
  return HasStrongAuth() &&
         fingerprint_storage_->IsFingerprintAuthenticationAvailable();
}

bool QuickUnlockStorage::IsPinAuthenticationAvailable() const {
  return HasStrongAuth() && pin_storage_->IsPinAuthenticationAvailable();
}

bool QuickUnlockStorage::TryAuthenticatePin(const std::string& pin) {
  return HasStrongAuth() && pin_storage()->TryAuthenticatePin(pin);
}

FingerprintStorage* QuickUnlockStorage::fingerprint_storage() {
  return fingerprint_storage_.get();
}

PinStorage* QuickUnlockStorage::pin_storage() {
  return pin_storage_.get();
}

void QuickUnlockStorage::Shutdown() {
  fingerprint_storage_.reset();
  pin_storage_.reset();
}

}  // namespace quick_unlock
}  // namespace chromeos
