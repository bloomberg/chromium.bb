// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CRYPTOHOME_ASYNC_METHOD_CALLER_H_
#define CHROMEOS_CRYPTOHOME_ASYNC_METHOD_CALLER_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "chromeos/chromeos_export.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace cryptohome {

// This class manages calls to Cryptohome service's 'async' methods.
// Note: This class is placed in ::cryptohome instead of ::chromeos::cryptohome
// since there is already a namespace ::cryptohome which holds the error code
// enum (MountError) and referencing ::chromeos::cryptohome and ::cryptohome
// within the same code is confusing.
class CHROMEOS_EXPORT AsyncMethodCaller {
 public:
  // A callback type which is called back on the UI thread when the results of
  // method calls are ready.
  typedef base::Callback<void(bool success, MountError return_code)> Callback;

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
  // |create_if_missing| controls whether or not we ask cryptohomed to
  // create a new home dir if one does not yet exist for |user_email|.
  // |callback| will be called with status info on completion.
  // If |create_if_missing| is false, and no cryptohome exists for |user_email|,
  // we'll get
  // callback.Run(false, kCryptohomeMountErrorUserDoesNotExist).
  // Otherwise, we expect the normal range of return codes.
  virtual void AsyncMount(const std::string& user_email,
                          const std::string& passhash,
                          const bool create_if_missing,
                          Callback callback) = 0;

  // Asks cryptohomed to asynchronously to mount a tmpfs for guest mode.
  // |callback| will be called with status info on completion.
  virtual void AsyncMountGuest(Callback callback) = 0;

  // Asks cryptohomed to asynchronously try to find the cryptohome for
  // |user_email| and then nuke it.
  virtual void AsyncRemove(const std::string& user_email,
                           Callback callback) = 0;

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
