// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_CRYPTOHOME_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_CRYPTOHOME_LIBRARY_H_
#pragma once

#include <string>

#include "base/memory/singleton.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "third_party/cros/chromeos_cryptohome.h"

namespace chromeos {

// This interface defines the interaction with the ChromeOS cryptohome library
// APIs.
class CryptohomeLibrary {
 public:
  class Delegate {
   public:
    // This will be called back on the UI thread.  Consult |return_code| for
    // further information beyond mere success or failure.
    virtual void OnComplete(bool success, int return_code) = 0;
  };

  CryptohomeLibrary();
  virtual ~CryptohomeLibrary();

  virtual void Init() = 0;

  // Asks cryptohomed to asynchronously try to find the cryptohome for
  // |user_email| and then use |passhash| to unlock the key.
  // Returns true if the attempt is successfully initiated.
  // d->OnComplete() will be called with status info on completion.
  virtual bool AsyncCheckKey(const std::string& user_email,
                             const std::string& passhash,
                             Delegate* callback) = 0;

  // Asks cryptohomed to asynchronously try to find the cryptohome for
  // |user_email| and then change from using |old_hash| to lock the
  // key to using |new_hash|.
  // Returns true if the attempt is successfully initiated.
  // d->OnComplete() will be called with status info on completion.
  virtual bool AsyncMigrateKey(const std::string& user_email,
                               const std::string& old_hash,
                               const std::string& new_hash,
                               Delegate* callback) = 0;

  // Asks cryptohomed to asynchronously try to find the cryptohome for
  // |user_email| and then mount it using |passhash| to unlock the key.
  // |create_if_missing| controls whether or not we ask cryptohomed to
  // create a new home dir if one does not yet exist for |user_email|.
  // Returns true if the attempt is successfully initiated.
  // d->OnComplete() will be called with status info on completion.
  // If |create_if_missing| is false, and no cryptohome exists for |user_email|,
  // we'll get d->OnComplete(false, kCryptohomeMountErrorUserDoesNotExist).
  // Otherwise, we expect the normal range of return codes.
  virtual bool AsyncMount(const std::string& user_email,
                          const std::string& passhash,
                          const bool create_if_missing,
                          Delegate* callback) = 0;

  // Asks cryptohomed to asynchronously to mount a tmpfs for BWSI mode.
  // Returns true if the attempt is successfully initiated.
  // d->OnComplete() will be called with status info on completion.
  virtual bool AsyncMountForBwsi(Delegate* callback) = 0;

  // Asks cryptohomed to asynchronously try to find the cryptohome for
  // |user_email| and then nuke it.
  virtual bool AsyncRemove(const std::string& user_email,
                           Delegate* callback) = 0;

  // Asks cryptohomed if a drive is currently mounted.
  virtual bool IsMounted() = 0;

  // Asks cryptohomed for the system salt.
  virtual CryptohomeBlob GetSystemSalt() = 0;

  // Passes cryptohomed the owner user. It is used to prevent
  // deletion of the owner in low disk space cleanup (see above).
  virtual bool AsyncSetOwnerUser(const std::string& username,
                                 Delegate* callback) = 0;

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

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via CrosLibrary::Get().
  static CryptohomeLibrary* GetImpl(bool stub);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_CRYPTOHOME_LIBRARY_H_
