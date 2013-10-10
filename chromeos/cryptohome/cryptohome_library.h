// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CRYPTOHOME_CRYPTOHOME_LIBRARY_H_
#define CHROMEOS_CRYPTOHOME_CRYPTOHOME_LIBRARY_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "chromeos/chromeos_export.h"

namespace chromeos {

// This interface defines the interaction with the ChromeOS cryptohome library
// APIs.
class CHROMEOS_EXPORT CryptohomeLibrary {
 public:
  typedef base::Callback<void(const std::string& system_salt)>
      GetSystemSaltCallback;

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
  virtual void GetSystemSalt(const GetSystemSaltCallback& callback) = 0;

  // Synchronous version of GetSystemSalt().
  // Blocks the UI thread until the Cryptohome service returns the result.
  // DEPRECATED: DO NOT USE.
  virtual std::string GetSystemSaltSync() = 0;

  // Returns system hash in hex encoded ascii format, cached by a prior call
  // to GetSystemSalt(). Note: this may return an empty string (e.g. if
  // GetSystemSalt() is not yet called).
  virtual std::string GetCachedSystemSalt() = 0;

 protected:
  CryptohomeLibrary();

 private:
  DISALLOW_COPY_AND_ASSIGN(CryptohomeLibrary);
};

}  // namespace chromeos

#endif  // CHROMEOS_CRYPTOHOME_CRYPTOHOME_LIBRARY_H_
