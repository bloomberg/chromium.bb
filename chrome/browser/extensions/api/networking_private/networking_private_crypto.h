// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_CRYPTO_H_
#define CHROME_BROWSER_EXTENSIONS_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_CRYPTO_H_

#include <string>
#include "base/basictypes.h"

// Implementation of Crypto support for networking private API.
// Based on chromeos_public//src/platform/shill/shims/crypto_util.cc
class NetworkingPrivateCrypto {
 public:
  NetworkingPrivateCrypto();
  ~NetworkingPrivateCrypto();

  // Verify that credentials described by |certificate| and |signed_data| are
  // valid.
  //
  // 1) The MAC address listed in the certificate matches |connected_mac|.
  // 2) The certificate is a valid PEM encoded certificate signed by trusted CA.
  // 3) |signature| is a valid signature for |data|, using the public key in
  // |certificate|
  bool VerifyCredentials(const std::string& certificate,
                         const std::string& signature,
                         const std::string& data,
                         const std::string& connected_mac);

  // Encrypt |data| with |public_key|. |public_key| is a DER-encoded
  // RSAPublicKey. |data| is some string of bytes at smaller than the
  // maximum length permissable for encryption with a key of |public_key| size.
  //
  // Returns true on success, storing the encrypted result in
  // |encrypted_output|.
  bool EncryptByteString(const std::string& public_key,
                         const std::string& data,
                         std::string* encrypted_output);

 private:
  friend class NetworkingPrivateCryptoTest;

  // Decrypt |encrypted_data| with |private_key_pem|. |private_key_pem| is the
  // PKCS8 PEM-encoded private key. |encrypted_data| is data encrypted with
  // EncryptByteString. Used in NetworkingPrivateCryptoTest::EncryptString test.
  //
  // Returns true on success, storing the decrypted result in
  // |decrypted_output|.
  bool DecryptByteString(const std::string& private_key_pem,
                         const std::string& encrypted_data,
                         std::string* decrypted_output);

  DISALLOW_COPY_AND_ASSIGN(NetworkingPrivateCrypto);
};

#endif  // CHROME_BROWSER_EXTENSIONS_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_CRYPTO_H_

