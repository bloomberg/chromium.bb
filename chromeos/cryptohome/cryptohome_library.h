// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CRYPTOHOME_CRYPTOHOME_LIBRARY_H_
#define CHROMEOS_CRYPTOHOME_CRYPTOHOME_LIBRARY_H_

#include <string>

#include "base/basictypes.h"
#include "chromeos/chromeos_export.h"

namespace chromeos {

// This interface defines the interaction with the ChromeOS cryptohome library
// APIs.
class CHROMEOS_EXPORT CryptohomeLibrary {
 public:
  // Manage an explicitly initialized global instance.
  static void Initialize();
  static bool IsInitialized();
  static void Shutdown();
  static CryptohomeLibrary* Get();

  // Sets up Get() to return |impl| for testing (e.g. with a mock
  // implementation). Call SetForTest(NULL) when |impl| is deleted.
  static void SetForTest(CryptohomeLibrary* impl);

  // Returns a CryptohomeLibrary instance for testing. Does not set or affect
  // the global instance.
  static CryptohomeLibrary* GetTestImpl();

  // Public so that result of GetTestImpl can be destroyed.
  virtual ~CryptohomeLibrary();

  // Returns system hash in hex encoded ascii format. Note: this may return
  // an empty string (e.g. if cryptohome is not running). It is up to the
  // calling function to try again after a delay if desired.
  virtual std::string GetSystemSalt() = 0;

  // Encrypts |token| with the system salt key (stable for the lifetime
  // of the device).  Useful to avoid storing plain text in place like
  // Local State.
  virtual std::string EncryptWithSystemSalt(const std::string& token) = 0;

  // Decrypts |token| with the system salt key (stable for the lifetime
  // of the device).
  virtual std::string DecryptWithSystemSalt(
      const std::string& encrypted_token_hex) = 0;

 protected:
  CryptohomeLibrary();

 private:
  DISALLOW_COPY_AND_ASSIGN(CryptohomeLibrary);
};

}  // namespace chromeos

#endif  // CHROMEOS_CRYPTOHOME_CRYPTOHOME_LIBRARY_H_
