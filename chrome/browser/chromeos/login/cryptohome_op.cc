// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/cryptohome_op.h"

#include <string>

#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/cryptohome_library.h"
#include "chrome/browser/chromeos/login/auth_attempt_state.h"
#include "chrome/browser/chromeos/login/auth_attempt_state_resolver.h"

namespace chromeos {

CryptohomeOp::CryptohomeOp(AuthAttemptState* current_attempt,
                           AuthAttemptStateResolver* callback)
    : attempt_(current_attempt),
      resolver_(callback) {
  CHECK(chromeos::CrosLibrary::Get()->EnsureLoaded());
}

CryptohomeOp::~CryptohomeOp() {}

void CryptohomeOp::OnComplete(bool success, int return_code) {
  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(this,
                        &CryptohomeOp::TriggerResolve,
                        success, return_code));
}

void CryptohomeOp::TriggerResolve(bool success, int return_code) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  attempt_->RecordOfflineLoginStatus(success, return_code);
  resolver_->Resolve();
}

MountAttempt::MountAttempt(AuthAttemptState* current_attempt,
                           AuthAttemptStateResolver* callback)
    : CryptohomeOp(current_attempt, callback) {
}

MountAttempt::~MountAttempt() {}

bool MountAttempt::Initiate() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  CryptohomeLibrary* lib = CrosLibrary::Get()->GetCryptohomeLibrary();
  return lib->AsyncMount(attempt_->username, attempt_->ascii_hash, this);
}

MountGuestAttempt::MountGuestAttempt(AuthAttemptState* current_attempt,
                                     AuthAttemptStateResolver* callback)
    : CryptohomeOp(current_attempt, callback) {
}

MountGuestAttempt::~MountGuestAttempt() {}

bool MountGuestAttempt::Initiate() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  CryptohomeLibrary* lib = CrosLibrary::Get()->GetCryptohomeLibrary();
  return lib->AsyncMountForBwsi(this);
}

MigrateAttempt::MigrateAttempt(AuthAttemptState* current_attempt,
                               AuthAttemptStateResolver* callback,
                               bool passing_old_hash,
                               const std::string& hash)
    : CryptohomeOp(current_attempt, callback),
      is_old_hash_(passing_old_hash),
      hash_(hash) {
}

MigrateAttempt::~MigrateAttempt() {}

bool MigrateAttempt::Initiate() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  CryptohomeLibrary* lib = CrosLibrary::Get()->GetCryptohomeLibrary();
  if (is_old_hash_) {
    return lib->AsyncMigrateKey(attempt_->username,
                                hash_,
                                attempt_->ascii_hash,
                                this);
  } else {
    return lib->AsyncMigrateKey(attempt_->username,
                                attempt_->ascii_hash,
                                hash_,
                                this);
  }
}

void MigrateAttempt::TriggerResolve(bool success, int return_code) {
  if (success)
    attempt_->ResetOfflineLoginStatus();
  else
    attempt_->RecordOfflineLoginStatus(success, return_code);
  resolver_->Resolve();
}

RemoveAttempt::RemoveAttempt(AuthAttemptState* current_attempt,
                             AuthAttemptStateResolver* callback)
    : CryptohomeOp(current_attempt, callback) {
}

RemoveAttempt::~RemoveAttempt() {}

bool RemoveAttempt::Initiate() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  CryptohomeLibrary* lib = CrosLibrary::Get()->GetCryptohomeLibrary();
  return lib->AsyncRemove(attempt_->username, this);
}

void RemoveAttempt::TriggerResolve(bool success, int return_code) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  if (success)
    attempt_->ResetOfflineLoginStatus();
  else
    attempt_->RecordOfflineLoginStatus(success, return_code);
  resolver_->Resolve();
}

}  // namespace chromeos
