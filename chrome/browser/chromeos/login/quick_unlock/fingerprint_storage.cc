// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/quick_unlock/fingerprint_storage.h"

#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace chromeos {
namespace quick_unlock {

FingerprintStorage::FingerprintStorage(PrefService* pref_service)
    : pref_service_(pref_service) {}

FingerprintStorage::~FingerprintStorage() {}

bool FingerprintStorage::IsFingerprintAuthenticationAvailable() const {
  const bool exceeded_unlock_attempts =
      unlock_attempt_count() >= kMaximumUnlockAttempts;

  return IsFingerprintEnabled() && HasEnrollment() && !exceeded_unlock_attempts;
}

bool FingerprintStorage::HasEnrollment() const {
  return has_enrollments_;
}

void FingerprintStorage::AddUnlockAttempt() {
  ++unlock_attempt_count_;
}

void FingerprintStorage::ResetUnlockAttemptCount() {
  unlock_attempt_count_ = 0;
}

}  // namespace quick_unlock
}  // namespace chromeos
