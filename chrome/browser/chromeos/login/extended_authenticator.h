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

  typedef base::Callback<void(const std::string& hash)> HashSuccessCallback;

  class AuthStatusConsumer {
   public:
    virtual ~AuthStatusConsumer() {}
    // The current login attempt has ended in failure, with error.
    virtual void OnAuthenticationFailure(AuthState state) = 0;
  };

  explicit ExtendedAuthenticator(AuthStatusConsumer* consumer);
  explicit ExtendedAuthenticator(LoginStatusConsumer* consumer);

  // Updates consumer of the class.
  void SetConsumer(LoginStatusConsumer* consumer);

  // This call will attempt to mount home dir for user, key (and key label)
  // specified in |context|. If |context.need_password_hashing| is true, the key
  // will be hashed with password salt before passing it to cryptohome. This
  // call assumes that homedir already exist for user, otherwise call will
  // result in error. On success username hash (used as mount point) will be
  // passed to |success_callback|.
  void AuthenticateToMount(const UserContext& context,
                           const HashSuccessCallback& success_callback);

  // This call will attempt to authenticate |user| with key (and key label)
  // specified in |context|. No actions are taken upon authentication.
  void AuthenticateToCheck(const UserContext& context,
                           const base::Closure& success_callback);

  // This call will create and mount home dir for |user_id| with supplied
  // |keys| if home dir is missing. If homedir already exist, the mount attempt
  // will be performed using first key for |auth|.
  // Note, that all keys in |keys| should be already hashed with system salt if
  // it is necessary, this method does not alter them.
  void CreateMount(const std::string& user_id,
                   const std::vector<cryptohome::KeyDefinition>& keys,
                   const HashSuccessCallback& success_callback);

  // Hashes |password| with system salt. Result will be passed to
  // |success_callback|.
  void HashPasswordWithSalt(const std::string& password,
                            const HashSuccessCallback& success_callback);

  // Attempts to add new |key| for user identified/authorized by |auth|.
  // If if key with same label already exist, behavior depends on
  // |replace_existing| flag. If flag is set, old key will be replaced. If it
  // is not set, attempt will lead to error.
  // It is prohibited to use same key label both in |auth| and |key|.
  void AddKey(const UserContext& context,
              const cryptohome::KeyDefinition& key,
              bool replace_existing,
              const base::Closure& success_callback);

 private:
  friend class base::RefCountedThreadSafe<ExtendedAuthenticator>;

  ~ExtendedAuthenticator();

  typedef base::Callback<void(const std::string& system_salt)>
      PendingHashCallback;
  typedef base::Callback<void(const UserContext& context)> ContextCallback;

  // Callback for system salt getter.
  void OnSaltObtained(const std::string& system_salt);

  // Updates UserContext (salts given key with system salt) if necessary.
  void UpdateContextToMount(const UserContext& context,
                            const std::string& hashed_password);
  void UpdateContextAndCheckKey(const UserContext& context,
                                const std::string& hashed_password);

  // Performs actual operation with fully configured |context|.
  void DoAuthenticateToMount(const HashSuccessCallback& success_callback,
                             const UserContext& context);
  void DoAuthenticateToCheck(const base::Closure& success_callback,
                             const UserContext& context);
  void DoAddKey(const cryptohome::KeyDefinition& key,
                bool replace_existing,
                const base::Closure& success_callback,
                const UserContext& context);

  // Inner operation callbacks.
  void OnMountComplete(const std::string& time_marker,
                       const UserContext& context,
                       const HashSuccessCallback& success_callback,
                       bool success,
                       cryptohome::MountError return_code,
                       const std::string& mount_hash);
  void OnCheckKeyComplete(const std::string& time_marker,
                          const UserContext& context,
                          const base::Closure& success_callback,
                          bool success,
                          cryptohome::MountError return_code);
  void OnAddKeyComplete(const std::string& time_marker,
                        const UserContext& user_context,
                        const base::Closure& success_callback,
                        bool success,
                        cryptohome::MountError return_code);

  // Inner implementation for hashing |password| with system salt. Will queue
  // requests if |system_salt| is not known yet.
  // Invokes |callback| with result.
  void DoHashWithSalt(const std::string& password,
                      const HashSuccessCallback& callback,
                      const std::string& system_salt);

  // Transforms |user_context| so that it can be used by DoNNN methods.
  // Currently it consists of hashing password with system salt if needed.
  void TransformContext(const UserContext& user_context,
                        const ContextCallback& callback);

  // Callback from previous method.
  void DidTransformContext(const UserContext& user_context,
                           const ContextCallback& callback,
                           const std::string& hashed_password);

  bool salt_obtained_;
  std::string system_salt_;
  std::vector<PendingHashCallback> hashing_queue_;

  AuthStatusConsumer* consumer_;
  LoginStatusConsumer* old_consumer_;

  DISALLOW_COPY_AND_ASSIGN(ExtendedAuthenticator);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_EXTENDED_AUTHENTICATOR_H_
