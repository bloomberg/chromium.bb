// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/cryptohome_library.h"

#include "base/message_loop.h"
#include "chrome/browser/chrome_thread.h"

namespace chromeos {

// This class handles the interaction with the ChromeOS cryptohome library APIs.
class CryptohomeLibraryImpl: public CryptohomeLibrary {
 public:
  CryptohomeLibraryImpl() {}
  virtual ~CryptohomeLibraryImpl() {}

  bool CheckKey(const std::string& user_email, const std::string& passhash) {
    return chromeos::CryptohomeCheckKey(user_email.c_str(), passhash.c_str());
  }

  bool MigrateKey(const std::string& user_email,
                  const std::string& old_hash,
                  const std::string& new_hash) {
    return chromeos::CryptohomeMigrateKey(user_email.c_str(), old_hash.c_str(),
        new_hash.c_str());
  }

  bool Remove(const std::string& user_email) {
    return chromeos::CryptohomeRemove(user_email.c_str());
  }

  bool Mount(const std::string& user_email,
             const std::string& passhash,
             int* error_code) {
    return chromeos::CryptohomeMountAllowFail(user_email.c_str(),
        passhash.c_str(), error_code);
  }

  bool MountForBwsi(int* error_code) {
    return chromeos::CryptohomeMountGuest(error_code);
  }

  bool IsMounted() {
    return chromeos::CryptohomeIsMounted();
  }

  CryptohomeBlob GetSystemSalt() {
    return chromeos::CryptohomeGetSystemSalt();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CryptohomeLibraryImpl);
};

class CryptohomeLibraryStubImpl: public CryptohomeLibrary {
 public:
  CryptohomeLibraryStubImpl() {}
  virtual ~CryptohomeLibraryStubImpl() {}

  bool CheckKey(const std::string& user_email, const std::string& passhash) {
    return true;
  }

  bool MigrateKey(const std::string& user_email,
                  const std::string& old_hash,
                  const std::string& new_hash) {
    return true;
  }

  bool Remove(const std::string& user_email) {
    return true;
  }

  bool Mount(const std::string& user_email,
             const std::string& passhash,
             int* error_code) {
    return true;
  }

  bool MountForBwsi(int* error_code) {
    return true;
  }

  bool IsMounted() {
    return true;
  }

  CryptohomeBlob GetSystemSalt() {
    CryptohomeBlob salt = CryptohomeBlob();
    salt.push_back(0);
    salt.push_back(0);
    return salt;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CryptohomeLibraryStubImpl);
};

// static
CryptohomeLibrary* CryptohomeLibrary::GetImpl(bool stub) {
  if (stub)
    return new CryptohomeLibraryStubImpl();
  else
    return new CryptohomeLibraryImpl();
}

} // namespace chromeos
