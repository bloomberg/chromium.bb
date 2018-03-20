// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_storage.h"

#include <memory>

#include "base/time/time.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace chromeos {
namespace quick_unlock {

const int QuickUnlockStorage::kTokenExpirationSeconds = 5 * 60;

QuickUnlockStorage::QuickUnlockStorage(PrefService* pref_service)
    : pref_service_(pref_service) {
  fingerprint_storage_ = std::make_unique<FingerprintStorage>(pref_service);
  pin_storage_ = std::make_unique<PinStorage>(pref_service);
}

QuickUnlockStorage::~QuickUnlockStorage() {}

void QuickUnlockStorage::MarkStrongAuth() {
  last_strong_auth_ = base::TimeTicks::Now();
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
  return base::TimeTicks::Now() - last_strong_auth_;
}

bool QuickUnlockStorage::IsFingerprintAuthenticationAvailable() const {
  return HasStrongAuth() &&
         fingerprint_storage_->IsFingerprintAuthenticationAvailable();
}

bool QuickUnlockStorage::IsPinAuthenticationAvailable() const {
  return HasStrongAuth() && pin_storage_->IsPinAuthenticationAvailable();
}

bool QuickUnlockStorage::TryAuthenticatePin(const std::string& pin,
                                            Key::KeyType key_type) {
  return HasStrongAuth() && pin_storage()->TryAuthenticatePin(pin, key_type);
}

std::string QuickUnlockStorage::CreateAuthToken() {
  auth_token_ = base::UnguessableToken::Create();
  auth_token_issue_time_ = base::TimeTicks::Now();
  return auth_token_.ToString();
}

bool QuickUnlockStorage::GetAuthTokenExpired() {
  return base::TimeTicks::Now() >=
         auth_token_issue_time_ +
             base::TimeDelta::FromSeconds(kTokenExpirationSeconds);
}

std::string QuickUnlockStorage::GetAuthToken() {
  if (GetAuthTokenExpired()) {
    auth_token_ = base::UnguessableToken();
    return std::string();
  }
  return auth_token_.is_empty() ? std::string() : auth_token_.ToString();
}

void QuickUnlockStorage::Shutdown() {
  fingerprint_storage_.reset();
  pin_storage_.reset();
}

}  // namespace quick_unlock
}  // namespace chromeos
