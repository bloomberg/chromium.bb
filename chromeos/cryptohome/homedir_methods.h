// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CRYPTOHOME_HOMEDIR_METHODS_H_
#define CHROMEOS_CRYPTOHOME_HOMEDIR_METHODS_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/cryptohome/key.pb.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace cryptohome {

// This class manages calls to Cryptohome service's home directory methods:
// Mount, CheckKey, Add/UpdateKey.
class CHROMEOS_EXPORT HomedirMethods {
 public:
  // Callbacks that are called back on the UI thread when the results of the
  // respective method calls are ready.
  typedef base::Callback<void(bool success, MountError return_code)> Callback;
  typedef base::Callback<void(
      bool success,
      MountError return_code,
      const std::vector<KeyDefinition>& key_definitions)> GetKeyDataCallback;
  typedef base::Callback<void(bool success, int64_t size)>
      GetAccountDiskUsageCallback;

  virtual ~HomedirMethods() {}

  // Asks cryptohomed to return data about the key identified by |request| for
  // the user identified by |id|. At present, this does not return any secret
  // information and the request does not need to be authenticated, so an empty
  // authorization request is sufficient.
  virtual void GetKeyDataEx(const Identification& id,
                            const AuthorizationRequest& auth,
                            const GetKeyDataRequest& request,
                            const GetKeyDataCallback& callback) = 0;

  // Asks cryptohomed to attempt authorization for user identified by |id| using
  // |auth|. This can be used to unlock a user session.
  virtual void CheckKeyEx(const Identification& id,
                          const AuthorizationRequest& auth,
                          const CheckKeyRequest& request,
                          const Callback& callback) = 0;

  // Asks cryptohomed to try to add another key for the user identified by |id|
  // using |auth| to unlock the key.
  // Key used in |auth| should have the PRIV_ADD privilege.
  // |callback| will be called with status info on completion.
  virtual void AddKeyEx(const Identification& id,
                        const AuthorizationRequest& auth,
                        const AddKeyRequest& request,
                        const Callback& callback) = 0;

  // Asks cryptohomed to update the key for the user identified by |id| using
  // |auth| to unlock the key. Label for |auth| and the requested key have to be
  // the same. Key used in |auth| should have PRIV_AUTHORIZED_UPDATE privilege.
  // |callback| will be called with status info on completion.
  virtual void UpdateKeyEx(const Identification& id,
                           const AuthorizationRequest& auth,
                           const UpdateKeyRequest& request,
                           const Callback& callback) = 0;

  // Asks cryptohomed to remove a specific key for the user identified by |id|
  // using |auth|.
  virtual void RemoveKeyEx(const Identification& id,
                           const AuthorizationRequest& auth,
                           const RemoveKeyRequest& request,
                           const Callback& callback) = 0;

  // Asks cryptohomed to change cryptohome identification |id_from| to |id_to|,
  // which results in cryptohome directory renaming.
  virtual void RenameCryptohome(const Identification& id_from,
                                const Identification& id_to,
                                const Callback& callback) = 0;

  // Asks cryptohomed to compute the size of cryptohome for user identified by
  // |id|.
  virtual void GetAccountDiskUsage(
      const Identification& id,
      const GetAccountDiskUsageCallback& callback) = 0;

  // Creates the global HomedirMethods instance.
  static void Initialize();

  // Similar to Initialize(), but can inject an alternative
  // HomedirMethods such as MockHomedirMethods for testing.
  // The injected object will be owned by the internal pointer and deleted
  // by Shutdown().
  static void InitializeForTesting(HomedirMethods* homedir_methods);

  // Destroys the global HomedirMethods instance if it exists.
  static void Shutdown();

  // Returns a pointer to the global HomedirMethods instance.
  // Initialize() should already have been called.
  static HomedirMethods* GetInstance();
};

}  // namespace cryptohome

#endif  // CHROMEOS_CRYPTOHOME_HOMEDIR_METHODS_H_
