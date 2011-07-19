// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/cryptohome_op.h"

#include <string>

#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/cryptohome_library.h"
#include "chrome/browser/chromeos/login/auth_attempt_state.h"
#include "chrome/browser/chromeos/login/auth_attempt_state_resolver.h"
#include "content/browser/browser_thread.h"

namespace chromeos {

CryptohomeOp::CryptohomeOp(AuthAttemptState* current_attempt,
                           AuthAttemptStateResolver* callback)
    : attempt_(current_attempt),
      resolver_(callback) {
  CHECK(chromeos::CrosLibrary::Get()->EnsureLoaded());
}

CryptohomeOp::~CryptohomeOp() {}

void CryptohomeOp::OnComplete(bool success, int return_code) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(this,
                        &CryptohomeOp::TriggerResolve,
                        success, return_code));
}

void CryptohomeOp::TriggerResolve(bool success, int return_code) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  attempt_->RecordCryptohomeStatus(success, return_code);
  resolver_->Resolve();
}

class MountAttempt : public CryptohomeOp {
 public:
  MountAttempt(AuthAttemptState* current_attempt,
               AuthAttemptStateResolver* callback,
               bool create_if_missing)
      : CryptohomeOp(current_attempt, callback),
        create_if_missing_(create_if_missing) {
  }

  virtual ~MountAttempt() {}

  bool Initiate() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    CryptohomeLibrary* lib = CrosLibrary::Get()->GetCryptohomeLibrary();
    return lib->AsyncMount(attempt_->username,
                           attempt_->ascii_hash,
                           create_if_missing_,
                           this);
  }

 private:
  const bool create_if_missing_;
  DISALLOW_COPY_AND_ASSIGN(MountAttempt);
};

class MountGuestAttempt : public CryptohomeOp {
 public:
  MountGuestAttempt(AuthAttemptState* current_attempt,
                    AuthAttemptStateResolver* callback)
      : CryptohomeOp(current_attempt, callback) {
  }

  virtual ~MountGuestAttempt() {}

  bool Initiate() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    CryptohomeLibrary* lib = CrosLibrary::Get()->GetCryptohomeLibrary();
    return lib->AsyncMountForBwsi(this);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MountGuestAttempt);
};

class MigrateAttempt : public CryptohomeOp {
 public:
  // TODO(cmasone): get rid of passing_old_hash arg, as it's always true.
  MigrateAttempt(AuthAttemptState* current_attempt,
                 AuthAttemptStateResolver* callback,
                 bool passing_old_hash,
                 const std::string& hash)
      : CryptohomeOp(current_attempt, callback),
        is_old_hash_(passing_old_hash),
        hash_(hash) {
  }

  virtual ~MigrateAttempt() {}

  bool Initiate() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
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

 private:
  const bool is_old_hash_;
  const std::string hash_;

  DISALLOW_COPY_AND_ASSIGN(MigrateAttempt);
};

class RemoveAttempt : public CryptohomeOp {
 public:
  RemoveAttempt(AuthAttemptState* current_attempt,
                AuthAttemptStateResolver* callback)
      : CryptohomeOp(current_attempt, callback) {
  }

  virtual ~RemoveAttempt() {}

  bool Initiate() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    CryptohomeLibrary* lib = CrosLibrary::Get()->GetCryptohomeLibrary();
    return lib->AsyncRemove(attempt_->username, this);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(RemoveAttempt);
};

class CheckKeyAttempt : public CryptohomeOp {
 public:
  CheckKeyAttempt(AuthAttemptState* current_attempt,
                  AuthAttemptStateResolver* callback)
      : CryptohomeOp(current_attempt, callback) {
  }

  virtual ~CheckKeyAttempt() {}

  bool Initiate() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    CryptohomeLibrary* lib = CrosLibrary::Get()->GetCryptohomeLibrary();
    return lib->AsyncCheckKey(attempt_->username, attempt_->ascii_hash, this);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CheckKeyAttempt);
};

// static
CryptohomeOp* CryptohomeOp::CreateMountAttempt(
    AuthAttemptState* current_attempt,
    AuthAttemptStateResolver* callback,
    bool create_if_missing) {
  return new MountAttempt(current_attempt, callback, create_if_missing);
}

// static
CryptohomeOp* CryptohomeOp::CreateMountGuestAttempt(
      AuthAttemptState* current_attempt,
      AuthAttemptStateResolver* callback) {
  return new MountGuestAttempt(current_attempt, callback);
}

// static
CryptohomeOp* CryptohomeOp::CreateMigrateAttempt(
    AuthAttemptState* current_attempt,
    AuthAttemptStateResolver* callback,
    bool passing_old_hash,
    const std::string& hash) {
  return new MigrateAttempt(current_attempt, callback, passing_old_hash, hash);
}

// static
CryptohomeOp* CryptohomeOp::CreateRemoveAttempt(
    AuthAttemptState* current_attempt,
    AuthAttemptStateResolver* callback) {
  return new RemoveAttempt(current_attempt, callback);
}

// static
CryptohomeOp* CryptohomeOp::CreateCheckKeyAttempt(
    AuthAttemptState* current_attempt,
    AuthAttemptStateResolver* callback) {

  return new CheckKeyAttempt(current_attempt, callback);
}

}  // namespace chromeos
