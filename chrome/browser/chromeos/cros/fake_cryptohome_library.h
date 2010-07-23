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
  FakeCryptohomeLibrary();
  virtual ~FakeCryptohomeLibrary() {}
  virtual bool Mount(const std::string& user_email,
                     const std::string& passhash,
                     int* error_code);
  virtual bool MountForBwsi(int* error_code);
  virtual bool CheckKey(const std::string& user_email,
                        const std::string& passhash);
  virtual bool MigrateKey(const std::string& user_email,
                          const std::string& old_hash,
                          const std::string& new_hash);
  virtual bool Remove(const std::string& user_email);
  virtual bool IsMounted();
  virtual CryptohomeBlob GetSystemSalt();

 private:
  CryptohomeBlob salt_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_FAKE_CRYPTOHOME_LIBRARY_H_
