// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_CRYPTOHOME_OP_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_CRYPTOHOME_OP_H_
#pragma once

#include <string>

#include "base/ref_counted.h"
#include "chrome/browser/chromeos/cros/cryptohome_library.h"
#include "chrome/browser/chrome_thread.h"

namespace chromeos {
class AuthAttemptState;
class AuthAttemptStateResolver;

class CryptohomeOp
    : public base::RefCountedThreadSafe<CryptohomeOp>,
      public CryptohomeLibrary::Delegate {
 public:
  CryptohomeOp(AuthAttemptState* current_attempt,
               AuthAttemptStateResolver* callback);

  virtual ~CryptohomeOp();

  virtual bool Initiate() = 0;

  // Implementation of CryptohomeLibrary::Delegate.
  virtual void OnComplete(bool success, int return_code);

 protected:
  virtual void TriggerResolve(bool offline_outcome, int offline_code);

  AuthAttemptState* const attempt_;
  AuthAttemptStateResolver* const resolver_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CryptohomeOp);
};

class MountAttempt : public CryptohomeOp {
 public:
  MountAttempt(AuthAttemptState* current_attempt,
               AuthAttemptStateResolver* callback);
  virtual ~MountAttempt();

  bool Initiate();

 private:
  DISALLOW_COPY_AND_ASSIGN(MountAttempt);
};

class MountGuestAttempt : public CryptohomeOp {
 public:
  MountGuestAttempt(AuthAttemptState* current_attempt,
                    AuthAttemptStateResolver* callback);
  virtual ~MountGuestAttempt();

  bool Initiate();

 private:
  DISALLOW_COPY_AND_ASSIGN(MountGuestAttempt);
};

class MigrateAttempt : public CryptohomeOp {
 public:
  MigrateAttempt(AuthAttemptState* current_attempt,
                 AuthAttemptStateResolver* callback,
                 bool passing_old_hash,
                 const std::string& hash);
  virtual ~MigrateAttempt();

  bool Initiate();

 private:
  void TriggerResolve(bool offline_outcome, int offline_code);

  const bool is_old_hash_;
  const std::string hash_;

  DISALLOW_COPY_AND_ASSIGN(MigrateAttempt);
};

class RemoveAttempt : public CryptohomeOp {
 public:
  RemoveAttempt(AuthAttemptState* current_attempt,
                AuthAttemptStateResolver* callback);
  virtual ~RemoveAttempt();

  bool Initiate();

 private:
  void TriggerResolve(bool offline_outcome, int offline_code);

  DISALLOW_COPY_AND_ASSIGN(RemoveAttempt);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_CRYPTOHOME_OP_H_
