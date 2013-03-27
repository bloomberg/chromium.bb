// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CRYPTOHOME_ASYNC_METHOD_CALLER_H_
#define CHROMEOS_CRYPTOHOME_ASYNC_METHOD_CALLER_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace cryptohome {

// Note: This file is placed in ::cryptohome instead of ::chromeos::cryptohome
// since there is already a namespace ::cryptohome which holds the error code
// enum (MountError) and referencing ::chromeos::cryptohome and ::cryptohome
// within the same code is confusing.

// Flags for the AsyncMount method.
enum MountFlags {
    MOUNT_FLAGS_NONE = 0,       // Used to explicitly denote that no flags are
                                // set.
    CREATE_IF_MISSING = 1,      // Create a cryptohome if it does not exist yet.
    ENSURE_EPHEMERAL = 1 << 1,  // Ensure that the mount is ephemeral.
};

// This class manages calls to Cryptohome service's 'async' methods.
class CHROMEOS_EXPORT AsyncMethodCaller {
 public:
  // A callback type which is called back on the UI thread when the results of
  // method calls are ready.
  typedef base::Callback<void(bool success, MountError return_code)> Callback;
  typedef base::Callback<void(bool success, const std::string& data)>
      DataCallback;

  virtual ~AsyncMethodCaller() {}

  // Asks cryptohomed to asynchronously try to find the cryptohome for
  // |user_email| and then use |passhash| to unlock the key.
  // |callback| will be called with status info on completion.
  virtual void AsyncCheckKey(const std::string& user_email,
                             const std::string& passhash,
                             Callback callback) = 0;

  // Asks cryptohomed to asynchronously try to find the cryptohome for
  // |user_email| and then change from using |old_hash| to lock the
  // key to using |new_hash|.
  // |callback| will be called with status info on completion.
  virtual void AsyncMigrateKey(const std::string& user_email,
                               const std::string& old_hash,
                               const std::string& new_hash,
                               Callback callback) = 0;

  // Asks cryptohomed to asynchronously try to find the cryptohome for
  // |user_email| and then mount it using |passhash| to unlock the key.
  // The |flags| are a combination of |MountFlags|:
  // * CREATE_IF_MISSING Controls whether or not cryptohomed is asked to create
  //                     a new cryptohome if one does not exist yet for
  //                     |user_email|.
  // * ENSURE_EPHEMERAL  If |true|, the mounted cryptohome will be backed by
  //                     tmpfs. If |false|, the ephemeral users policy decides
  //                     whether tmpfs or an encrypted directory is used as the
  //                     backend.
  // |callback| will be called with status info on completion.
  // If the |CREATE_IF_MISSING| flag is not given and no cryptohome exists
  // for |user_email|, the expected result is
  // callback.Run(false, kCryptohomeMountErrorUserDoesNotExist). Otherwise,
  // the normal range of return codes is expected.
  virtual void AsyncMount(const std::string& user_email,
                          const std::string& passhash,
                          int flags,
                          Callback callback) = 0;

  // Asks cryptohomed to asynchronously to mount a tmpfs for guest mode.
  // |callback| will be called with status info on completion.
  virtual void AsyncMountGuest(Callback callback) = 0;

  // Asks cryptohomed to asynchronously try to find the cryptohome for
  // |user_email| and then nuke it.
  virtual void AsyncRemove(const std::string& user_email,
                           Callback callback) = 0;

  // Asks cryptohomed to asynchronously create an attestation enrollment
  // request.  On success the data sent to |callback| is a request to be sent
  // to the Privacy CA.
  virtual void AsyncTpmAttestationCreateEnrollRequest(
      const DataCallback& callback) = 0;

  // Asks cryptohomed to asynchronously finish an attestation enrollment.
  // |pca_response| is the response to the enrollment request emitted by the
  // Privacy CA.
  virtual void AsyncTpmAttestationEnroll(const std::string& pca_response,
                                         const Callback& callback) = 0;

  // Asks cryptohomed to asynchronously create an attestation certificate
  // request according to |options|, which is a combination of
  // CryptohomeClient::AttestationCertificateOptions.  On success the data sent
  // to |callback| is a request to be sent to the Privacy CA.
  virtual void AsyncTpmAttestationCreateCertRequest(
      int options,
      const DataCallback& callback) = 0;

  // Asks cryptohomed to asynchronously finish an attestation certificate
  // request.  On success the data sent to |callback| is a certificate chain
  // in PEM format.  |pca_response| is the response to the certificate request
  // emitted by the Privacy CA.  |key_type| determines whether the certified key
  // is to be associated with the current user.  |key_name| is a name for the
  // key.
  virtual void AsyncTpmAttestationFinishCertRequest(
      const std::string& pca_response,
      chromeos::CryptohomeClient::AttestationKeyType key_type,
      const std::string& key_name,
      const DataCallback& callback) = 0;

  // Asks cryptohome  to asynchronously retrieve a string associated with given
  // |user| that would be used in mount path instead of |user|.
  // On success the data is sent to |callback|.
  virtual void AsyncGetSanitizedUsername(
      const std::string& user,
      const DataCallback& callback) = 0;

  // Creates the global AsyncMethodCaller instance.
  static void Initialize();

  // Similar to Initialize(), but can inject an alternative
  // AsyncMethodCaller such as MockAsyncMethodCaller for testing.
  // The injected object will be owned by the internal pointer and deleted
  // by Shutdown().
  static void InitializeForTesting(AsyncMethodCaller* async_method_caller);

  // Destroys the global AsyncMethodCaller instance if it exists.
  static void Shutdown();

  // Returns a pointer to the global AsyncMethodCaller instance.
  // Initialize() should already have been called.
  static AsyncMethodCaller* GetInstance();
};

}  // namespace cryptohome

#endif  // CHROMEOS_CRYPTOHOME_ASYNC_METHOD_CALLER_H_
