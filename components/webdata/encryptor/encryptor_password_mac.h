// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBDATA_ENCRYPTOR_ENCRYPTOR_PASSWORD_MAC_H_
#define COMPONENTS_WEBDATA_ENCRYPTOR_ENCRYPTOR_PASSWORD_MAC_H_

#include <string>

#include "base/basictypes.h"

namespace crypto {
class AppleKeychain;
}  // namespace crypto

class EncryptorPassword {
 public:
  explicit EncryptorPassword(const crypto::AppleKeychain& keychain)
      : keychain_(keychain) {
  }

  // Get the Encryptor password for this system.  If no password exists
  // in the Keychain then one is generated, stored in the Mac keychain, and
  // returned.
  // If one exists then it is fetched from the Keychain and returned.
  // If the user disallows access to the Keychain (or an error occurs) then an
  // empty string is returned.
  std::string GetEncryptorPassword() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(EncryptorPassword);
  const crypto::AppleKeychain& keychain_;
};

#endif  // COMPONENTS_WEBDATA_ENCRYPTOR_ENCRYPTOR_PASSWORD_MAC_H_
