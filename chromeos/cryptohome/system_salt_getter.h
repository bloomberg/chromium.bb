// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CRYPTOHOME_SYSTEM_SALT_GETTER_H_
#define CHROMEOS_CRYPTOHOME_SYSTEM_SALT_GETTER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/chromeos_export.h"

namespace chromeos {

// This class is used to get the system salt from cryptohome and cache it.
class CHROMEOS_EXPORT SystemSaltGetter {
 public:
  typedef base::Callback<void(const std::string& system_salt)>
      GetSystemSaltCallback;

  // Manage an explicitly initialized global instance.
  static void Initialize();
  static bool IsInitialized();
  static void Shutdown();
  static SystemSaltGetter* Get();

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
  SystemSaltGetter();
  ~SystemSaltGetter();

 private:
  // Used to implement GetSystemSalt().
  void GetSystemSaltInternal(const GetSystemSaltCallback& callback,
                             bool service_is_available);

  // Loads the system salt from cryptohome and caches it.
  void LoadSystemSalt();

  std::string system_salt_;

  base::WeakPtrFactory<SystemSaltGetter> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SystemSaltGetter);
};

}  // namespace chromeos

#endif  // CHROMEOS_CRYPTOHOME_SYSTEM_SALT_GETTER_H_
