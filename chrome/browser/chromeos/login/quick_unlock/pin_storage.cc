// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/quick_unlock/pin_storage.h"

#include "base/base64.h"
#include "base/strings/string_util.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/common/pref_names.h"
#include "chromeos/login/auth/key.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "crypto/random.h"

namespace chromeos {

namespace {

const int kSaltByteSize = 16;

// Returns a new salt of length |kSaltByteSize|.
std::string CreateSalt() {
  // The salt needs to be base64 encoded because the pref service requires a
  // UTF8 string.
  std::string salt;
  crypto::RandBytes(base::WriteInto(&salt, kSaltByteSize + 1), kSaltByteSize);
  base::Base64Encode(salt, &salt);
  DCHECK(!salt.empty());
  return salt;
}

// Computes the hash for |pin| and |salt|.
std::string ComputeSecret(const std::string& pin, const std::string& salt) {
  Key key(pin);
  key.Transform(Key::KEY_TYPE_SALTED_PBKDF2_AES256_1234, salt);
  return key.GetSecret();
}

base::TimeDelta QuickUnlockPasswordConfirmationFrequencyToTimeDelta(
    QuickUnlockPasswordConfirmationFrequency frequency) {
  switch (frequency) {
    case QuickUnlockPasswordConfirmationFrequency::SIX_HOURS:
      return base::TimeDelta::FromHours(6);
    case QuickUnlockPasswordConfirmationFrequency::TWELVE_HOURS:
      return base::TimeDelta::FromHours(12);
    case QuickUnlockPasswordConfirmationFrequency::DAY:
      return base::TimeDelta::FromDays(1);
    case QuickUnlockPasswordConfirmationFrequency::WEEK:
      return base::TimeDelta::FromDays(7);
  }
  NOTREACHED();
  return base::TimeDelta();
}

}  // namespace

// static
void PinStorage::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(prefs::kQuickUnlockPinSalt, "");
  registry->RegisterStringPref(prefs::kQuickUnlockPinSecret, "");
}

PinStorage::PinStorage(PrefService* pref_service)
    : pref_service_(pref_service) {}

PinStorage::~PinStorage() {}

void PinStorage::MarkStrongAuth() {
  last_strong_auth_ = base::Time::Now();
  ResetUnlockAttemptCount();
}

bool PinStorage::HasStrongAuth() const {
  if (last_strong_auth_.is_null())
    return false;

  QuickUnlockPasswordConfirmationFrequency strong_auth_interval =
      static_cast<QuickUnlockPasswordConfirmationFrequency>(
          pref_service_->GetInteger(prefs::kQuickUnlockTimeout));
  base::TimeDelta strong_auth_timeout =
      QuickUnlockPasswordConfirmationFrequencyToTimeDelta(strong_auth_interval);

  return TimeSinceLastStrongAuth() < strong_auth_timeout;
}

base::TimeDelta PinStorage::TimeSinceLastStrongAuth() const {
  DCHECK(!last_strong_auth_.is_null());
  return base::Time::Now() - last_strong_auth_;
}

void PinStorage::AddUnlockAttempt() {
  ++unlock_attempt_count_;
}

void PinStorage::ResetUnlockAttemptCount() {
  unlock_attempt_count_ = 0;
}

bool PinStorage::IsPinSet() const {
  return !PinSalt().empty() && !PinSecret().empty();
}

void PinStorage::SetPin(const std::string& pin) {
  const std::string salt = CreateSalt();
  const std::string secret = ComputeSecret(pin, salt);

  pref_service_->SetString(prefs::kQuickUnlockPinSalt, salt);
  pref_service_->SetString(prefs::kQuickUnlockPinSecret, secret);
}

void PinStorage::RemovePin() {
  pref_service_->SetString(prefs::kQuickUnlockPinSalt, "");
  pref_service_->SetString(prefs::kQuickUnlockPinSecret, "");
}

std::string PinStorage::PinSalt() const {
  return pref_service_->GetString(prefs::kQuickUnlockPinSalt);
}

std::string PinStorage::PinSecret() const {
  return pref_service_->GetString(prefs::kQuickUnlockPinSecret);
}

bool PinStorage::IsPinAuthenticationAvailable() const {
  const bool exceeded_unlock_attempts =
      unlock_attempt_count() >= kMaximumUnlockAttempts;

  return IsPinUnlockEnabled(pref_service_) && IsPinSet() && HasStrongAuth() &&
         !exceeded_unlock_attempts;
}

bool PinStorage::TryAuthenticatePin(const std::string& pin) {
  if (!IsPinAuthenticationAvailable())
    return false;

  AddUnlockAttempt();
  return ComputeSecret(pin, PinSalt()) == PinSecret();
}

}  // namespace chromeos
