// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/cryptohome_library.h"

#include "base/message_loop.h"
#include "chrome/browser/chrome_thread.h"

namespace chromeos {

bool CryptohomeLibraryImpl::CheckKey(const std::string& user_email,
                                     const std::string& passhash) {
  return chromeos::CryptohomeCheckKey(user_email.c_str(), passhash.c_str());
}

bool CryptohomeLibraryImpl::MigrateKey(const std::string& user_email,
                                       const std::string& old_hash,
                                       const std::string& new_hash) {
  return chromeos::CryptohomeMigrateKey(user_email.c_str(),
                                        old_hash.c_str(),
                                        new_hash.c_str());
}

bool CryptohomeLibraryImpl::Remove(const std::string& user_email) {
  return chromeos::CryptohomeRemove(user_email.c_str());
}

bool CryptohomeLibraryImpl::Mount(const std::string& user_email,
                                  const std::string& passhash,
                                  int* error_code) {
  return chromeos::CryptohomeMountAllowFail(user_email.c_str(),
                                            passhash.c_str(),
                                            error_code);
}

bool CryptohomeLibraryImpl::MountForBwsi(int* error_code) {
  return chromeos::CryptohomeMountGuest(error_code);
}

bool CryptohomeLibraryImpl::IsMounted() {
  return chromeos::CryptohomeIsMounted();
}

CryptohomeBlob CryptohomeLibraryImpl::GetSystemSalt() {
  return chromeos::CryptohomeGetSystemSalt();
}

}  // namespace chromeos
