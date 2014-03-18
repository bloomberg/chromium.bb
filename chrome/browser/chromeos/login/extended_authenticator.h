// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_EXTENDED_AUTHENTICATOR_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_EXTENDED_AUTHENTICATOR_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

class LoginStatusConsumer;

// Interaction with cryptohome : mounting home dirs, create new home dirs,
// udpate passwords.
//
// Typical flow:
// AuthenticateToMount() calls a Cryptohome to perform offline login,
// AuthenticateToCreate() calls a Cryptohome to create new cryptohome.
class ExtendedAuthenticator
    : public base::RefCountedThreadSafe<ExtendedAuthenticator> {
 public:
  enum AuthState {
    SUCCESS,       // Login succeeded.
    NO_MOUNT,      // No cryptohome exist for user.
    FAILED_MOUNT,  // Failed to mount existing cryptohome - login failed.
    FAILED_TPM,    // Failed to mount/create cryptohome because of TPM error.
  };

  class AuthStatusConsumer {
   public:
    virtual ~AuthStatusConsumer() {}
    // The current login attempt has ended in failure, with error.
    virtual void OnAuthenticationFailure(AuthState state) = 0;
    // The current login attempt has ended succesfully.
    virtual void OnMountSuccess(const std::string& mount_hash) = 0;
    virtual void OnPasswordHashingSuccess(int id,
                                          const std::string& password_hash) = 0;
  };

  explicit ExtendedAuthenticator(AuthStatusConsumer* consumer);
  explicit ExtendedAuthenticator(LoginStatusConsumer* consumer);

  // Updates consumer of the class.
  void SetConsumer(LoginStatusConsumer* consumer);

  // This call will attempt to mount home dir for user, key (and key label)
  // specified in |context|. If |context.need_password_hashing| is true, the key
  // will be hashed with password salt before passing it to cryptohome. This
  // call assumes that homedir already exist for user, otherwise call will
  // result in error.
  void AuthenticateToMount(const UserContext& context);

  // This call will create and mount home dir for |user_id| with supplied
  // |keys| if home dir is missing. If homedir already exist, the mount attempt
  // will be performed using first key for |auth|.
  // Note, that all keys in |keys| should be already hashed with system salt if
  // it is necessary, this method does not alter them.
  void CreateMount(const std::string& user_id,
                   const std::vector<cryptohome::KeyDefinition>& keys);

  // Hashes |password| with system salt. Result will be passed to
  // consumer's OnPasswordHashingSuccess method. |id| can be used to
  // identify result if several passwords are to be hashed.
  void HashPasswordWithSalt(int id, const std::string& password);

 private:
  friend class base::RefCountedThreadSafe<ExtendedAuthenticator>;

  ~ExtendedAuthenticator();

  typedef base::Callback<void(const std::string& result)> HashingCallback;

  void OnSaltObtained(const std::string& system_salt);
  void UpdateContextToMount(const UserContext& context,
                            const std::string& hashed_password);
  void DoAuthenticateToMount(const UserContext& context);
  void OnMountComplete(const std::string& time_marker,
                       const UserContext& context,
                       bool success,
                       cryptohome::MountError return_code,
                       const std::string& mount_hash);

  void DidHashPasswordWithSalt(int id, const std::string& hash);
  void DoHashWithSalt(const std::string& password,
                      const HashingCallback& callback,
                      const std::string& system_salt);

  bool salt_obtained_;
  std::string system_salt_;
  std::vector<HashingCallback> hashing_queue_;

  AuthStatusConsumer* consumer_;
  LoginStatusConsumer* old_consumer_;

  DISALLOW_COPY_AND_ASSIGN(ExtendedAuthenticator);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_EXTENDED_AUTHENTICATOR_H_
