// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/quick_unlock/pin_storage.h"

#include "base/base64.h"
#include "base/strings/string_util.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/common/pref_names.h"
#include "chromeos/login/auth/key.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "crypto/random.h"

namespace chromeos {
namespace quick_unlock {

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

}  // namespace

// static
void PinStorage::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kQuickUnlockPinSalt, "");
  registry->RegisterStringPref(prefs::kQuickUnlockPinSecret, "");
}

PinStorage::PinStorage(PrefService* pref_service)
    : pref_service_(pref_service) {}

PinStorage::~PinStorage() {}

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

  return IsPinEnabled(pref_service_) && IsPinSet() && !exceeded_unlock_attempts;
}

bool PinStorage::TryAuthenticatePin(const std::string& pin) {
  if (!IsPinAuthenticationAvailable())
    return false;

  AddUnlockAttempt();
  return ComputeSecret(pin, PinSalt()) == PinSecret();
}

}  // namespace quick_unlock
}  // namespace chromeos
