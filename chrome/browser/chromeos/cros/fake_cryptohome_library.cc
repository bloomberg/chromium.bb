// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/fake_cryptohome_library.h"

namespace chromeos {

FakeCryptohomeLibrary::FakeCryptohomeLibrary() {
  // The salt must be non-empty and an even length.
  salt_.push_back(0);
  salt_.push_back(0);
}

bool FakeCryptohomeLibrary::CheckKey(const std::string& user_email,
                                     const std::string& passhash) {
  return true;
}

bool FakeCryptohomeLibrary::MigrateKey(const std::string& user_email,
                                       const std::string& old_hash,
                                       const std::string& new_hash) {
  return true;
}

bool FakeCryptohomeLibrary::Remove(const std::string& user_email) {
  return true;
}

bool FakeCryptohomeLibrary::Mount(const std::string& user_email,
                                  const std::string& passhash,
                                  int* error_code) {
  return true;
}

bool FakeCryptohomeLibrary::MountForBwsi(int* error_code) {
  return true;
}

bool FakeCryptohomeLibrary::IsMounted() {
  return true;
}

CryptohomeBlob FakeCryptohomeLibrary::GetSystemSalt() {
  return salt_;
}

}  // namespace chromeos
