// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SETTINGS_TOKEN_ENCRYPTOR_H_
#define CHROME_BROWSER_CHROMEOS_SETTINGS_TOKEN_ENCRYPTOR_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

namespace crypto {
class SymmetricKey;
}

namespace chromeos {

// Interface class for classes that encrypt and decrypt tokens using the
// system salt.
class TokenEncryptor {
 public:
  virtual ~TokenEncryptor() {}

  // Encrypts |token| with the system salt key (stable for the lifetime
  // of the device).  Useful to avoid storing plain text in place like
  // Local State.
  virtual std::string EncryptWithSystemSalt(const std::string& token) = 0;

  // Decrypts |token| with the system salt key (stable for the lifetime
  // of the device).
  virtual std::string DecryptWithSystemSalt(
      const std::string& encrypted_token_hex) = 0;
};

// TokenEncryptor based on the system salt from cryptohome daemon. This
// implementation is used in production.
class CryptohomeTokenEncryptor : public TokenEncryptor {
 public:
  explicit CryptohomeTokenEncryptor(const std::string& system_salt);
  virtual ~CryptohomeTokenEncryptor();

  // TokenEncryptor overrides:
  virtual std::string EncryptWithSystemSalt(const std::string& token) OVERRIDE;
  virtual std::string DecryptWithSystemSalt(
      const std::string& encrypted_token_hex) OVERRIDE;

 private:
  // Converts |passphrase| to a SymmetricKey using the given |salt|.
  crypto::SymmetricKey* PassphraseToKey(const std::string& passphrase,
                                        const std::string& salt);

  // Encrypts (AES) the token given |key| and |salt|.
  std::string EncryptTokenWithKey(crypto::SymmetricKey* key,
                                  const std::string& salt,
                                  const std::string& token);

  // Decrypts (AES) hex encoded encrypted token given |key| and |salt|.
  std::string DecryptTokenWithKey(crypto::SymmetricKey* key,
                                  const std::string& salt,
                                  const std::string& encrypted_token_hex);

  // The cached system salt passed to the constructor, originally coming
  // from cryptohome daemon.
  std::string system_salt_;

  // A key based on the system salt.  Useful for encrypting device-level
  // data for which we have no additional credentials.
  scoped_ptr<crypto::SymmetricKey> system_salt_key_;

  DISALLOW_COPY_AND_ASSIGN(CryptohomeTokenEncryptor);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SETTINGS_TOKEN_ENCRYPTOR_H_
