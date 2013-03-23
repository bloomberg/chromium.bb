// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_MANAGED_MANAGED_USER_AUTHENTICATOR_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_MANAGED_MANAGED_USER_AUTHENTICATOR_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/cros/cryptohome_library.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

// Authenticates locally managed users against the cryptohome.
//
// Typical flow:
// AuthenticateToMount() calls a Cryptohome to perform offline login,
// AuthenticateToCreate() calls a Cryptohome to create new cryptohome.
class ManagedUserAuthenticator
    : public base::RefCountedThreadSafe<ManagedUserAuthenticator> {
 public:
  enum AuthState {
    CONTINUE,      // State indeterminate; try again when more info available.
    NO_MOUNT,      // No cryptohome exist for user.
    FAILED_MOUNT,  // Failed to mount existing cryptohome - login failed.
    FAILED_TPM,    // Failed to mount/create cryptohome because of TPM error.
    SUCCESS,       // Login succeeded .
  };

  class AuthAttempt {
   public:
    AuthAttempt(const std::string& username,
                const std::string& password,
                const std::string& hashed_password);
    ~AuthAttempt();

    // Copy |cryptohome_code| and |cryptohome_outcome| into this object,
    // so we can have a copy we're sure to own, and can make available
    // on the IO thread.  Must be called from the IO thread.
    void RecordCryptohomeStatus(bool cryptohome_outcome,
                                cryptohome::MountError cryptohome_code);

    // Copy |hash| into this object so we can have a copy we're sure to own
    // and can make available on the IO thread.
    // Must be called from the IO thread.
    void RecordHash(const std::string& hash);

    bool cryptohome_complete();
    bool cryptohome_outcome();
    bool hash_obtained();
    std::string hash();
    cryptohome::MountError cryptohome_code();

    const std::string username;
    const std::string password;
    const std::string hashed_password;

   private:
    bool cryptohome_complete_;
    bool cryptohome_outcome_;
    bool hash_obtained_;
    std::string hash_;

    cryptohome::MountError cryptohome_code_;
    DISALLOW_COPY_AND_ASSIGN(AuthAttempt);
  };

  class StatusConsumer {
   public:
    virtual ~StatusConsumer() {}
    // The current login attempt has ended in failure, with error.
    virtual void OnAuthenticationFailure(AuthState state) = 0;
    // The current login attempt has ended succesfully.
    virtual void OnMountSuccess(const std::string& mount_hash) = 0;
    // The current cryptohome creation attempt has ended succesfully.
    virtual void OnCreationSuccess() = 0;
  };

  explicit ManagedUserAuthenticator(StatusConsumer* consumer);

  void AuthenticateToMount(const std::string& username,
                           const std::string& password);

  void AuthenticateToCreate(const std::string& username,
                            const std::string& password);
  void Resolve();

 private:
  friend class base::RefCountedThreadSafe<ManagedUserAuthenticator>;

  ~ManagedUserAuthenticator();

  AuthState ResolveState();
  AuthState ResolveCryptohomeFailureState();
  AuthState ResolveCryptohomeSuccessState();
  void OnAuthenticationSuccess(const std::string& mount_hash);
  void OnAuthenticationFailure(AuthState state);

  scoped_ptr<AuthAttempt> current_state_;
  StatusConsumer* consumer_;

  DISALLOW_COPY_AND_ASSIGN(ManagedUserAuthenticator);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_MANAGED_MANAGED_USER_AUTHENTICATOR_H_
