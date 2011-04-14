// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/util/nigori.h"

#if defined(OS_WIN)
#include <winsock2.h>  // for htonl
#else
#include <arpa/inet.h>
#endif

#include <sstream>
#include <vector>

#include "base/base64.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/string_util.h"
#include "crypto/encryptor.h"
#include "crypto/hmac.h"

using base::Base64Encode;
using base::Base64Decode;
using base::RandInt;
using crypto::Encryptor;
using crypto::HMAC;
using crypto::SymmetricKey;

namespace browser_sync {

// NigoriStream simplifies the concatenation operation of the Nigori protocol.
class NigoriStream {
 public:
  // Append the big-endian representation of the length of |value| with 32 bits,
  // followed by |value| itself to the stream.
  NigoriStream& operator<<(const std::string& value) {
    uint32 size = htonl(value.size());
    stream_.write((char *) &size, sizeof(uint32));
    stream_ << value;
    return *this;
  }

  // Append the big-endian representation of the length of |type| with 32 bits,
  // followed by the big-endian representation of the value of |type|, with 32
  // bits, to the stream.
  NigoriStream& operator<<(const Nigori::Type type) {
    uint32 size = htonl(sizeof(uint32));
    stream_.write((char *) &size, sizeof(uint32));
    uint32 value = htonl(type);
    stream_.write((char *) &value, sizeof(uint32));
    return *this;
  }

  std::string str() {
    return stream_.str();
  }

 private:
  std::ostringstream stream_;
};

// static
const char Nigori::kSaltSalt[] = "saltsalt";

Nigori::Nigori() {
}

Nigori::~Nigori() {
}

bool Nigori::InitByDerivation(const std::string& hostname,
                              const std::string& username,
                              const std::string& password) {
  NigoriStream salt_password;
  salt_password << username << hostname;

  // Suser = PBKDF2(Username || Servername, "saltsalt", Nsalt, 8)
  scoped_ptr<SymmetricKey> user_salt(SymmetricKey::DeriveKeyFromPassword(
      SymmetricKey::HMAC_SHA1, salt_password.str(),
      kSaltSalt,
      kSaltIterations,
      kSaltKeySizeInBits));
  DCHECK(user_salt.get());

  std::string raw_user_salt;
  if (!user_salt->GetRawKey(&raw_user_salt))
    return false;

  // Kuser = PBKDF2(P, Suser, Nuser, 16)
  user_key_.reset(SymmetricKey::DeriveKeyFromPassword(SymmetricKey::AES,
      password, raw_user_salt, kUserIterations, kDerivedKeySizeInBits));
  DCHECK(user_key_.get());

  // Kenc = PBKDF2(P, Suser, Nenc, 16)
  encryption_key_.reset(SymmetricKey::DeriveKeyFromPassword(SymmetricKey::AES,
      password, raw_user_salt, kEncryptionIterations, kDerivedKeySizeInBits));
  DCHECK(encryption_key_.get());

  // Kmac = PBKDF2(P, Suser, Nmac, 16)
  mac_key_.reset(SymmetricKey::DeriveKeyFromPassword(
      SymmetricKey::HMAC_SHA1, password, raw_user_salt, kSigningIterations,
      kDerivedKeySizeInBits));
  DCHECK(mac_key_.get());

  return true;
}

bool Nigori::InitByImport(const std::string& user_key,
                          const std::string& encryption_key,
                          const std::string& mac_key) {
  user_key_.reset(SymmetricKey::Import(SymmetricKey::AES, user_key));
  DCHECK(user_key_.get());

  encryption_key_.reset(SymmetricKey::Import(SymmetricKey::AES,
                                             encryption_key));
  DCHECK(encryption_key_.get());

  mac_key_.reset(SymmetricKey::Import(SymmetricKey::HMAC_SHA1, mac_key));
  DCHECK(mac_key_.get());

  return user_key_.get() && encryption_key_.get() && mac_key_.get();
}

// Permute[Kenc,Kmac](type || name)
bool Nigori::Permute(Type type, const std::string& name,
                     std::string* permuted) const {
  DCHECK_LT(0U, name.size());

  NigoriStream plaintext;
  plaintext << type << name;

  Encryptor encryptor;
  if (!encryptor.Init(encryption_key_.get(), Encryptor::CBC,
                      std::string(kIvSize, 0)))
    return false;

  std::string ciphertext;
  if (!encryptor.Encrypt(plaintext.str(), &ciphertext))
    return false;

  std::string raw_mac_key;
  if (!mac_key_->GetRawKey(&raw_mac_key))
    return false;

  HMAC hmac(HMAC::SHA256);
  if (!hmac.Init(raw_mac_key))
    return false;

  std::vector<unsigned char> hash(kHashSize);
  if (!hmac.Sign(ciphertext, &hash[0], hash.size()))
    return false;

  std::string output;
  output.assign(ciphertext);
  output.append(hash.begin(), hash.end());

  return Base64Encode(output, permuted);
}

std::string GenerateRandomString(size_t size) {
  // TODO(albertb): Use a secure random function.
  std::string random(size, 0);
  for (size_t i = 0; i < size; ++i)
    random[i] = RandInt(0, 0xff);
  return random;
}

// Enc[Kenc,Kmac](value)
bool Nigori::Encrypt(const std::string& value, std::string* encrypted) const {
  DCHECK_LT(0U, value.size());

  std::string iv = GenerateRandomString(kIvSize);

  Encryptor encryptor;
  if (!encryptor.Init(encryption_key_.get(), Encryptor::CBC, iv))
    return false;

  std::string ciphertext;
  if (!encryptor.Encrypt(value, &ciphertext))
    return false;

  std::string raw_mac_key;
  if (!mac_key_->GetRawKey(&raw_mac_key))
    return false;

  HMAC hmac(HMAC::SHA256);
  if (!hmac.Init(raw_mac_key))
    return false;

  std::vector<unsigned char> hash(kHashSize);
  if (!hmac.Sign(ciphertext, &hash[0], hash.size()))
    return false;

  std::string output;
  output.assign(iv);
  output.append(ciphertext);
  output.append(hash.begin(), hash.end());

  return Base64Encode(output, encrypted);
}

bool Nigori::Decrypt(const std::string& encrypted, std::string* value) const {
  std::string input;
  if (!Base64Decode(encrypted, &input))
    return false;

  if (input.size() < kIvSize * 2 + kHashSize)
    return false;

  // The input is:
  // * iv (16 bytes)
  // * ciphertext (multiple of 16 bytes)
  // * hash (32 bytes)
  std::string iv(input.substr(0, kIvSize));
  std::string ciphertext(input.substr(kIvSize,
                                      input.size() - (kIvSize + kHashSize)));
  std::string hash(input.substr(input.size() - kHashSize, kHashSize));

  std::string raw_mac_key;
  if (!mac_key_->GetRawKey(&raw_mac_key))
    return false;

  HMAC hmac(HMAC::SHA256);
  if (!hmac.Init(raw_mac_key))
    return false;

  std::vector<unsigned char> expected(kHashSize);
  if (!hmac.Sign(ciphertext, &expected[0], expected.size()))
    return false;

  if (hash.compare(0, hash.size(),
                   reinterpret_cast<char *>(&expected[0]),
                   expected.size()))
    return false;

  Encryptor encryptor;
  if (!encryptor.Init(encryption_key_.get(), Encryptor::CBC, iv))
    return false;

  std::string plaintext;
  if (!encryptor.Decrypt(ciphertext, value))
    return false;

  return true;
}

bool Nigori::ExportKeys(std::string* user_key,
                        std::string* encryption_key,
                        std::string* mac_key) const {
  DCHECK(user_key);
  DCHECK(encryption_key);
  DCHECK(mac_key);

  return user_key_->GetRawKey(user_key) &&
      encryption_key_->GetRawKey(encryption_key) &&
      mac_key_->GetRawKey(mac_key);
}

}  // namespace browser_sync
