// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_CRYPTOHOME_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_CRYPTOHOME_LIBRARY_H_

#include <string>

#include "base/singleton.h"
#include "third_party/cros/chromeos_cryptohome.h"

namespace chromeos {
class MockCryptohomeLibrary;

// This class handles the interaction with the ChromeOS cryptohome library APIs.
// Users can get an instance of this library class like this:
//   CryptohomeLibrary::Get()
class CryptohomeLibrary {
 public:
  // This gets the singleton CryptohomeLibrary.
  static CryptohomeLibrary* Get();

  // Asks cryptohomed to try to find the cryptohome for |user_email| and then
  // mount it using |passhash| to unlock the key.
  virtual bool Mount(const std::string& user_email,
                     const std::string& passhash);

  // Asks cryptohomed to try to find the cryptohome for |user_email| and then
  // use |passhash| to unlock the key.
  virtual bool CheckKey(const std::string& user_email,
                        const std::string& passhash);

  // Asks cryptohomed if a drive is currently mounted.
  virtual bool IsMounted();

 private:
  friend struct DefaultSingletonTraits<CryptohomeLibrary>;
  friend class MockCryptohomeLibrary;

  CryptohomeLibrary() {}
  ~CryptohomeLibrary() {}

  DISALLOW_COPY_AND_ASSIGN(CryptohomeLibrary);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_CRYPTOHOME_LIBRARY_H_
