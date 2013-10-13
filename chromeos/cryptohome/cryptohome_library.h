// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CRYPTOHOME_CRYPTOHOME_LIBRARY_H_
#define CHROMEOS_CRYPTOHOME_CRYPTOHOME_LIBRARY_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "chromeos/chromeos_export.h"

namespace chromeos {

// This class is used to get the system salt from cryptohome and cache it.
// TODO(satorux): Rename this to SystemSaltGetter. crbug.com/305906
class CHROMEOS_EXPORT CryptohomeLibrary {
 public:
  typedef base::Callback<void(const std::string& system_salt)>
      GetSystemSaltCallback;

  // Manage an explicitly initialized global instance.
  static void Initialize();
  static bool IsInitialized();
  static void Shutdown();
  static CryptohomeLibrary* Get();

  // Converts |salt| to a hex encoded string.
  static std::string ConvertRawSaltToHexString(const std::vector<uint8>& salt);

  // Returns system hash in hex encoded ascii format. Note: this may return
  // an empty string (e.g. if cryptohome is not running). It is up to the
  // calling function to try again after a delay if desired.
  void GetSystemSalt(const GetSystemSaltCallback& callback);

  // Synchronous version of GetSystemSalt().
  // Blocks the UI thread until the Cryptohome service returns the result.
  // DEPRECATED: DO NOT USE.
  std::string GetSystemSaltSync();

  // Returns system hash in hex encoded ascii format, cached by a prior call
  // to GetSystemSalt(). Note: this may return an empty string (e.g. if
  // GetSystemSalt() is not yet called).
  std::string GetCachedSystemSalt();

 protected:
  CryptohomeLibrary();
  ~CryptohomeLibrary();

 private:
  // Loads the system salt from cryptohome and caches it.
  void LoadSystemSalt();

  std::string system_salt_;

  DISALLOW_COPY_AND_ASSIGN(CryptohomeLibrary);
};

}  // namespace chromeos

#endif  // CHROMEOS_CRYPTOHOME_CRYPTOHOME_LIBRARY_H_
