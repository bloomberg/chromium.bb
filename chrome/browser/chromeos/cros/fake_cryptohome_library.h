// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_FAKE_CRYPTOHOME_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_FAKE_CRYPTOHOME_LIBRARY_H_

#include <string>

#include "chrome/browser/chromeos/cros/cryptohome_library.h"

namespace chromeos {

class FakeCryptohomeLibrary : public CryptohomeLibrary {
 public:
  FakeCryptohomeLibrary() {
    // The salt must be non-empty and an even length.
    salt_.push_back(0);
    salt_.push_back(0);
  }

  virtual ~FakeCryptohomeLibrary() {}
  virtual bool Mount(const std::string& user_email,
                     const std::string& passhash,
                     int* error_code) {
    return true;
  }

  virtual bool MountForBwsi(int* error_code) {
    return true;
  }

  virtual bool CheckKey(const std::string& user_email,
                        const std::string& passhash) {
    return true;
  }

  virtual bool MigrateKey(const std::string& user_email,
                          const std::string& old_hash,
                          const std::string& new_hash) {
    return true;
  }

  virtual bool Remove(const std::string& user_email) {
    return true;
  }

  virtual bool IsMounted() {
    return true;
  }

  virtual CryptohomeBlob GetSystemSalt() {
    return salt_;
  }

 private:
  CryptohomeBlob salt_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_FAKE_CRYPTOHOME_LIBRARY_H_
