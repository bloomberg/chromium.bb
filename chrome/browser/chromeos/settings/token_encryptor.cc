// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/token_encryptor.h"

#include <vector>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/sys_info.h"
#include "chromeos/cryptohome/cryptohome_library.h"
#include "crypto/encryptor.h"
#include "crypto/nss_util.h"
#include "crypto/sha2.h"
#include "crypto/symmetric_key.h"

namespace chromeos {

namespace {
const size_t kNonceSize = 16;
}  // namespace

CryptohomeTokenEncryptor::CryptohomeTokenEncryptor() {
}

CryptohomeTokenEncryptor::~CryptohomeTokenEncryptor() {
}

std::string CryptohomeTokenEncryptor::EncryptWithSystemSalt(
    const std::string& token) {
  // Don't care about token encryption while debugging.
  if (!base::SysInfo::IsRunningOnChromeOS())
    return token;

  if (!LoadSystemSaltKey()) {
    LOG(WARNING) << "System salt key is not available for encrypt.";
    return std::string();
  }
  return EncryptTokenWithKey(system_salt_key_.get(),
                             system_salt_,
                             token);
}

std::string CryptohomeTokenEncryptor::DecryptWithSystemSalt(
    const std::string& encrypted_token_hex) {
  // Don't care about token encryption while debugging.
  if (!base::SysInfo::IsRunningOnChromeOS())
    return encrypted_token_hex;

  if (!LoadSystemSaltKey()) {
    LOG(WARNING) << "System salt key is not available for decrypt.";
    return std::string();
  }
  return DecryptTokenWithKey(system_salt_key_.get(),
                             system_salt_,
                             encrypted_token_hex);
}

// TODO: should this use the system salt for both the password and the salt
// value, or should this use a separate salt value?
bool CryptohomeTokenEncryptor::LoadSystemSaltKey() {
  // Assume the system salt should be obtained beforehand at login time.
  if (system_salt_.empty())
    system_salt_ = CryptohomeLibrary::Get()->GetCachedSystemSalt();
  if (system_salt_.empty())
    return false;
  if (!system_salt_key_.get())
    system_salt_key_.reset(PassphraseToKey(system_salt_, system_salt_));
  return system_salt_key_.get();
}

crypto::SymmetricKey* CryptohomeTokenEncryptor::PassphraseToKey(
    const std::string& passphrase,
    const std::string& salt) {
  return crypto::SymmetricKey::DeriveKeyFromPassword(
      crypto::SymmetricKey::AES, passphrase, salt, 1000, 256);
}

std::string CryptohomeTokenEncryptor::EncryptTokenWithKey(
    crypto::SymmetricKey* key,
    const std::string& salt,
    const std::string& token) {
  crypto::Encryptor encryptor;
  if (!encryptor.Init(key, crypto::Encryptor::CTR, std::string())) {
    LOG(WARNING) << "Failed to initialize Encryptor.";
    return std::string();
  }
  std::string nonce = salt.substr(0, kNonceSize);
  std::string encoded_token;
  CHECK(encryptor.SetCounter(nonce));
  if (!encryptor.Encrypt(token, &encoded_token)) {
    LOG(WARNING) << "Failed to encrypt token.";
    return std::string();
  }

  return StringToLowerASCII(base::HexEncode(
      reinterpret_cast<const void*>(encoded_token.data()),
      encoded_token.size()));
}

std::string CryptohomeTokenEncryptor::DecryptTokenWithKey(
    crypto::SymmetricKey* key,
    const std::string& salt,
    const std::string& encrypted_token_hex) {
  std::vector<uint8> encrypted_token_bytes;
  if (!base::HexStringToBytes(encrypted_token_hex, &encrypted_token_bytes)) {
    LOG(WARNING) << "Corrupt encrypted token found.";
    return std::string();
  }

  std::string encrypted_token(
      reinterpret_cast<char*>(encrypted_token_bytes.data()),
      encrypted_token_bytes.size());
  crypto::Encryptor encryptor;
  if (!encryptor.Init(key, crypto::Encryptor::CTR, std::string())) {
    LOG(WARNING) << "Failed to initialize Encryptor.";
    return std::string();
  }

  std::string nonce = salt.substr(0, kNonceSize);
  std::string token;
  CHECK(encryptor.SetCounter(nonce));
  if (!encryptor.Decrypt(encrypted_token, &token)) {
    LOG(WARNING) << "Failed to decrypt token.";
    return std::string();
  }
  return token;
}

}  // namespace chromeos
