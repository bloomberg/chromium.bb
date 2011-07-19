// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_CRYPTOHOME_OP_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_CRYPTOHOME_OP_H_
#pragma once

#include <string>

#include "base/memory/ref_counted.h"
#include "chrome/browser/chromeos/cros/cryptohome_library.h"

namespace chromeos {
class AuthAttemptState;
class AuthAttemptStateResolver;

class CryptohomeOp
    : public base::RefCountedThreadSafe<CryptohomeOp>,
      public CryptohomeLibrary::Delegate {
 public:
  static CryptohomeOp* CreateMountAttempt(AuthAttemptState* current_attempt,
                                          AuthAttemptStateResolver* callback,
                                          bool create_if_missing);

  static CryptohomeOp* CreateMountGuestAttempt(
      AuthAttemptState* current_attempt,
      AuthAttemptStateResolver* callback);

  static CryptohomeOp* CreateMigrateAttempt(AuthAttemptState* current_attempt,
                                            AuthAttemptStateResolver* callback,
                                            bool passing_old_hash,
                                            const std::string& hash);

  static CryptohomeOp* CreateRemoveAttempt(AuthAttemptState* current_attempt,
                                           AuthAttemptStateResolver* callback);

  static CryptohomeOp* CreateCheckKeyAttempt(
      AuthAttemptState* current_attempt,
      AuthAttemptStateResolver* callback);

  virtual bool Initiate() = 0;

  // Implementation of CryptohomeLibrary::Delegate.
  virtual void OnComplete(bool success, int return_code);

 protected:
  CryptohomeOp(AuthAttemptState* current_attempt,
               AuthAttemptStateResolver* callback);

  virtual ~CryptohomeOp();

  virtual void TriggerResolve(bool offline_outcome, int offline_code);

  AuthAttemptState* const attempt_;
  AuthAttemptStateResolver* const resolver_;

 private:
  friend class base::RefCountedThreadSafe<CryptohomeOp>;
  DISALLOW_COPY_AND_ASSIGN(CryptohomeOp);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_CRYPTOHOME_OP_H_
