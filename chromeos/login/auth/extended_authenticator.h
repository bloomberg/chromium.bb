// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_LOGIN_AUTH_EXTENDED_AUTHENTICATOR_H_
#define CHROMEOS_LOGIN_AUTH_EXTENDED_AUTHENTICATOR_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

class AuthStatusConsumer;
class UserContext;

// Interaction with cryptohomed: mount home dirs, create new home dirs, update
// passwords.
//
// Typical flow:
// AuthenticateToMount() calls cryptohomed to perform offline login,
// AuthenticateToCreate() calls cryptohomed to create new cryptohome.
class CHROMEOS_EXPORT ExtendedAuthenticator
    : public base::RefCountedThreadSafe<ExtendedAuthenticator> {
 public:
  enum AuthState {
    SUCCESS,       // Login succeeded.
    NO_MOUNT,      // No cryptohome exist for user.
    FAILED_MOUNT,  // Failed to mount existing cryptohome - login failed.
    FAILED_TPM,    // Failed to mount/create cryptohome because of TPM error.
  };

  typedef base::Callback<void(const std::string& result)> ResultCallback;
  typedef base::Callback<void(const UserContext& context)> ContextCallback;

  class NewAuthStatusConsumer {
   public:
    virtual ~NewAuthStatusConsumer() {}
    // The current login attempt has ended in failure, with error.
    virtual void OnAuthenticationFailure(AuthState state) = 0;
  };

  explicit ExtendedAuthenticator(NewAuthStatusConsumer* consumer);
  explicit ExtendedAuthenticator(AuthStatusConsumer* consumer);

  // Updates consumer of the class.
  void SetConsumer(AuthStatusConsumer* consumer);

  // This call will attempt to mount the home dir for the user, key (and key
  // label) in |context|. If the key is of type KEY_TYPE_PASSWORD_PLAIN, it will
  // be hashed with the system salt before being passed to cryptohomed. This
  // call assumes that the home dir already exist for the user and will return
  // an error otherwise. On success, the user ID hash (used as the mount point)
  // will be passed to |success_callback|.
  void AuthenticateToMount(const UserContext& context,
                           const ResultCallback& success_callback);

  // This call will attempt to authenticate the user with the key (and key
  // label) in |context|. No further actions are taken after authentication.
  void AuthenticateToCheck(const UserContext& context,
                           const base::Closure& success_callback);

  // This call will create and mount the home dir for |user_id| with the given
  // |keys| if the home dir is missing. If the home dir exists already, a mount
  // attempt will be performed using the first key in |keys| for authentication.
  // Note that all |keys| should have been transformed from plain text already.
  // This method does not alter them.
  void CreateMount(const std::string& user_id,
                   const std::vector<cryptohome::KeyDefinition>& keys,
                   const ResultCallback& success_callback);

  // Attempts to add a new |key| for the user identified/authorized by
  // |context|. If a key with the same label already exists, the behavior
  // depends on the |replace_existing| flag. If the flag is set, the old key is
  // replaced. If the flag is not set, an error occurs. It is not allowed to
  // replace the key used for authorization.
  void AddKey(const UserContext& context,
              const cryptohome::KeyDefinition& key,
              bool replace_existing,
              const base::Closure& success_callback);

  // Attempts to perform an authorized update of the key in |context| with the
  // new |key|. The update is authorized by providing the |signature| of the
  // key. The original key must have the |PRIV_AUTHORIZED_UPDATE| privilege to
  // perform this operation. The key labels in |context| and in |key| should be
  // the same.
  void UpdateKeyAuthorized(const UserContext& context,
                           const cryptohome::KeyDefinition& key,
                           const std::string& signature,
                           const base::Closure& success_callback);

  // Attempts to remove the key labeled |key_to_remove| for the user identified/
  // authorized by |context|. It is possible to remove the key used for
  // authorization, although it should be done with extreme care.
  void RemoveKey(const UserContext& context,
                 const std::string& key_to_remove,
                 const base::Closure& success_callback);

  // Hashes the key in |user_context| with the system salt it its type is
  // KEY_TYPE_PASSWORD_PLAIN and passes the resulting UserContext to the
  // |callback|.
  void TransformKeyIfNeeded(const UserContext& user_context,
                            const ContextCallback& callback);

 private:
  friend class base::RefCountedThreadSafe<ExtendedAuthenticator>;

  ~ExtendedAuthenticator();

  // Callback for system salt getter.
  void OnSaltObtained(const std::string& system_salt);

  // Performs actual operation with fully configured |context|.
  void DoAuthenticateToMount(const ResultCallback& success_callback,
                             const UserContext& context);
  void DoAuthenticateToCheck(const base::Closure& success_callback,
                             const UserContext& context);
  void DoAddKey(const cryptohome::KeyDefinition& key,
                bool replace_existing,
                const base::Closure& success_callback,
                const UserContext& context);
  void DoUpdateKeyAuthorized(const cryptohome::KeyDefinition& key,
                             const std::string& signature,
                             const base::Closure& success_callback,
                             const UserContext& context);
  void DoRemoveKey(const std::string& key_to_remove,
                   const base::Closure& success_callback,
                   const UserContext& context);

  // Inner operation callbacks.
  void OnMountComplete(const std::string& time_marker,
                       const UserContext& context,
                       const ResultCallback& success_callback,
                       bool success,
                       cryptohome::MountError return_code,
                       const std::string& mount_hash);
  void OnOperationComplete(const std::string& time_marker,
                           const UserContext& context,
                           const base::Closure& success_callback,
                           bool success,
                           cryptohome::MountError return_code);

  bool salt_obtained_;
  std::string system_salt_;
  std::vector<base::Closure> system_salt_callbacks_;

  NewAuthStatusConsumer* consumer_;
  AuthStatusConsumer* old_consumer_;

  DISALLOW_COPY_AND_ASSIGN(ExtendedAuthenticator);
};

}  // namespace chromeos

#endif  // CHROMEOS_LOGIN_AUTH_EXTENDED_AUTHENTICATOR_H_
