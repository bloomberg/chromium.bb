// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_CRYPTOHOME_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_CRYPTOHOME_LIBRARY_H_
#pragma once

#include <string>

#include "base/callback.h"

namespace chromeos {

// This interface defines the interaction with the ChromeOS cryptohome library
// APIs.
class CryptohomeLibrary {
 public:
  // A callback type which is called back on the UI thread when the results of
  // AsyncXXX methods are ready.
  typedef base::Callback<void(bool success, int return_code)
                         > AsyncMethodCallback;

  CryptohomeLibrary();
  virtual ~CryptohomeLibrary();

  // Asks cryptohomed to asynchronously try to find the cryptohome for
  // |user_email| and then use |passhash| to unlock the key.
  // Returns true if the attempt is successfully initiated.
  // callback->OnComplete() will be called with status info on completion.
  virtual void AsyncCheckKey(const std::string& user_email,
                             const std::string& passhash,
                             AsyncMethodCallback callback) = 0;

  // Asks cryptohomed to asynchronously try to find the cryptohome for
  // |user_email| and then change from using |old_hash| to lock the
  // key to using |new_hash|.
  // Returns true if the attempt is successfully initiated.
  // callback->OnComplete() will be called with status info on completion.
  virtual void AsyncMigrateKey(const std::string& user_email,
                               const std::string& old_hash,
                               const std::string& new_hash,
                               AsyncMethodCallback callback) = 0;

  // Asks cryptohomed to asynchronously try to find the cryptohome for
  // |user_email| and then mount it using |passhash| to unlock the key.
  // |create_if_missing| controls whether or not we ask cryptohomed to
  // create a new home dir if one does not yet exist for |user_email|.
  // Returns true if the attempt is successfully initiated.
  // callback->OnComplete() will be called with status info on completion.
  // If |create_if_missing| is false, and no cryptohome exists for |user_email|,
  // we'll get
  // callback->OnComplete(false, kCryptohomeMountErrorUserDoesNotExist).
  // Otherwise, we expect the normal range of return codes.
  virtual void AsyncMount(const std::string& user_email,
                          const std::string& passhash,
                          const bool create_if_missing,
                          AsyncMethodCallback callback) = 0;

  // Asks cryptohomed to asynchronously to mount a tmpfs for guest mode.
  // Returns true if the attempt is successfully initiated.
  // callback->OnComplete() will be called with status info on completion.
  virtual void AsyncMountGuest(AsyncMethodCallback callback) = 0;

  // Asks cryptohomed to asynchronously try to find the cryptohome for
  // |user_email| and then nuke it.
  virtual void AsyncRemove(const std::string& user_email,
                           AsyncMethodCallback callback) = 0;

  // Asks cryptohomed if a drive is currently mounted.
  virtual bool IsMounted() = 0;

  // Wrappers of the functions for working with Tpm.

  // Returns whether Tpm is ready.
  virtual bool TpmIsReady() = 0;

  // Returns whether Tpm is presented and enabled.
  virtual bool TpmIsEnabled() = 0;

  // Returns whether device has already been owned.
  virtual bool TpmIsOwned() = 0;

  // Returns whether device is being owned (Tpm password is generating).
  virtual bool TpmIsBeingOwned() = 0;

  // Returns Tpm password (if password was cleared empty one is returned).
  // Return value is true if password was successfully acquired.
  virtual bool TpmGetPassword(std::string* password) = 0;

  // Attempts to start owning (if device isn't owned and isn't being owned).
  virtual void TpmCanAttemptOwnership() = 0;

  // Clears Tpm password. Password should be cleared after it was generated and
  // shown to user.
  virtual void TpmClearStoredPassword() = 0;

  virtual bool InstallAttributesGet(const std::string& name,
                                    std::string* value) = 0;
  virtual bool InstallAttributesSet(const std::string& name,
                                    const std::string& value) = 0;
  virtual bool InstallAttributesFinalize() = 0;
  virtual bool InstallAttributesIsReady() = 0;
  virtual bool InstallAttributesIsInvalid() = 0;
  virtual bool InstallAttributesIsFirstInstall() = 0;

  // Get the PKCS#11 token info from the TPM.  This is different from
  // the TpmGetPassword because it's getting the PKCS#11 user PIN and
  // not the TPM password.
  virtual void Pkcs11GetTpmTokenInfo(std::string* label,
                                     std::string* user_pin) = 0;

  // Gets the status of the TPM.  This is different from TpmIsReady
  // because it's getting the staus of the PKCS#11 initialization of
  // the TPM token, not the TPM itself.
  virtual bool Pkcs11IsTpmTokenReady() = 0;

  // Returns hash of |password|, salted with the system salt.
  virtual std::string HashPassword(const std::string& password) = 0;

  // Returns system hash in hex encoded ascii format.
  virtual std::string GetSystemSalt() = 0;

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via CrosLibrary::Get().
  static CryptohomeLibrary* GetImpl(bool stub);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_CRYPTOHOME_LIBRARY_H_
