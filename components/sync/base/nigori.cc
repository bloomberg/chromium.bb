// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/nigori.h"

#include <stdint.h>

#include <sstream>
#include <vector>

#include "base/base64.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/sys_byteorder.h"
#include "components/sync/base/sync_base_switches.h"
#include "crypto/encryptor.h"
#include "crypto/hmac.h"
#include "crypto/random.h"
#include "crypto/symmetric_key.h"

using base::Base64Encode;
using base::Base64Decode;
using crypto::HMAC;
using crypto::SymmetricKey;

const size_t kDerivedKeySizeInBits = 128;
const size_t kDerivedKeySizeInBytes = kDerivedKeySizeInBits / 8;
const size_t kHashSize = 32;

namespace syncer {

// NigoriStream simplifies the concatenation operation of the Nigori protocol.
class NigoriStream {
 public:
  // Append the big-endian representation of the length of |value| with 32 bits,
  // followed by |value| itself to the stream.
  NigoriStream& operator<<(const std::string& value) {
    uint32_t size = base::HostToNet32(value.size());

    stream_.write(reinterpret_cast<char*>(&size), sizeof(uint32_t));
    stream_ << value;
    return *this;
  }

  // Append the big-endian representation of the length of |type| with 32 bits,
  // followed by the big-endian representation of the value of |type|, with 32
  // bits, to the stream.
  NigoriStream& operator<<(const Nigori::Type type) {
    uint32_t size = base::HostToNet32(sizeof(uint32_t));
    stream_.write(reinterpret_cast<char*>(&size), sizeof(uint32_t));
    uint32_t value = base::HostToNet32(type);
    stream_.write(reinterpret_cast<char*>(&value), sizeof(uint32_t));
    return *this;
  }

  std::string str() { return stream_.str(); }

 private:
  std::ostringstream stream_;
};

KeyDerivationParams::KeyDerivationParams(KeyDerivationMethod method,
                                         const std::string& pbkdf2_hostname,
                                         const std::string& pbkdf2_username,
                                         const std::string& scrypt_salt)
    : method_(method),
      pbkdf2_hostname_(pbkdf2_hostname),
      pbkdf2_username_(pbkdf2_username),
      scrypt_salt_(scrypt_salt) {}

KeyDerivationParams::KeyDerivationParams(const KeyDerivationParams& other) =
    default;
KeyDerivationParams::KeyDerivationParams(KeyDerivationParams&& other) = default;

KeyDerivationParams& KeyDerivationParams::operator=(
    const KeyDerivationParams& other) = default;

bool KeyDerivationParams::operator==(const KeyDerivationParams& other) const {
  return method_ == other.method_ &&
         pbkdf2_hostname_ == other.pbkdf2_hostname_ &&
         pbkdf2_username_ == other.pbkdf2_username_ &&
         scrypt_salt_ == other.scrypt_salt_;
}

const std::string& KeyDerivationParams::pbkdf2_username() const {
  DCHECK(method_ == KeyDerivationMethod::PBKDF2_HMAC_SHA1_1003);
  return pbkdf2_username_;
}
const std::string& KeyDerivationParams::pbkdf2_hostname() const {
  DCHECK(method_ == KeyDerivationMethod::PBKDF2_HMAC_SHA1_1003);
  return pbkdf2_hostname_;
}
const std::string& KeyDerivationParams::scrypt_salt() const {
  DCHECK(method_ == KeyDerivationMethod::SCRYPT_8192_8_11);
  return scrypt_salt_;
}

KeyDerivationParams KeyDerivationParams::CreateForPbkdf2(
    const std::string& hostname,
    const std::string& username) {
  return {KeyDerivationMethod::PBKDF2_HMAC_SHA1_1003, hostname, username,
          /* scrypt_salt_ = */ ""};
}

KeyDerivationParams KeyDerivationParams::CreateForScrypt(
    const std::string& salt) {
  return {KeyDerivationMethod::SCRYPT_8192_8_11,
          /* pbkdf2_hostname_ = */ "", /* pbkf2_username = */ "", salt};
}

KeyDerivationParams KeyDerivationParams::CreateWithUnsupportedMethod() {
  return {KeyDerivationMethod::UNSUPPORTED, /* pbkdf2_hostname_ = */ "",
          /* pbkdf2_username_ = */ "", /* scrypt_salt_ = */ ""};
}

Nigori::Keys::Keys() = default;
Nigori::Keys::~Keys() = default;

bool Nigori::Keys::InitByDerivationUsingPbkdf2(const std::string& hostname,
                                               const std::string& username,
                                               const std::string& password) {
  const char kSaltSalt[] =
      "saltsalt";  // The salt used to derive the user salt.
  const size_t kSaltIterations = 1001;
  const size_t kUserIterations = 1002;
  const size_t kEncryptionIterations = 1003;
  const size_t kSigningIterations = 1004;
  const size_t kSaltKeySizeInBits = 128;

  NigoriStream salt_password;
  salt_password << username << hostname;

  // Suser = PBKDF2(Username || Servername, "saltsalt", Nsalt, 8)
  std::unique_ptr<SymmetricKey> user_salt(
      SymmetricKey::DeriveKeyFromPasswordUsingPbkdf2(
          SymmetricKey::HMAC_SHA1, salt_password.str(), kSaltSalt,
          kSaltIterations, kSaltKeySizeInBits));
  DCHECK(user_salt);

  // Kuser = PBKDF2(P, Suser, Nuser, 16)
  user_key = SymmetricKey::DeriveKeyFromPasswordUsingPbkdf2(
      SymmetricKey::AES, password, user_salt->key(), kUserIterations,
      kDerivedKeySizeInBits);
  DCHECK(user_key);

  // Kenc = PBKDF2(P, Suser, Nenc, 16)
  encryption_key = SymmetricKey::DeriveKeyFromPasswordUsingPbkdf2(
      SymmetricKey::AES, password, user_salt->key(), kEncryptionIterations,
      kDerivedKeySizeInBits);
  DCHECK(encryption_key);

  // Kmac = PBKDF2(P, Suser, Nmac, 16)
  mac_key = SymmetricKey::DeriveKeyFromPasswordUsingPbkdf2(
      SymmetricKey::HMAC_SHA1, password, user_salt->key(), kSigningIterations,
      kDerivedKeySizeInBits);
  DCHECK(mac_key);

  return user_key && encryption_key && mac_key;
}

bool Nigori::Keys::InitByDerivationUsingScrypt(const std::string& salt,
                                               const std::string& password) {
  const size_t kCostParameter = 8192;  // 2^13.
  const size_t kBlockSize = 8;
  const size_t kParallelizationParameter = 11;
  const size_t kMaxMemoryBytes = 32 * 1024 * 1024;  // 32 MiB.

  // |user_key| is not used anymore. However, old clients may fail to import a
  // Nigori node without one. We initialize it to all zeroes to prevent a
  // failure on those clients.
  user_key = SymmetricKey::Import(SymmetricKey::AES,
                                  std::string(kDerivedKeySizeInBytes, '\0'));

  // Derive a master key twice as long as the required key size, and split it
  // into two to get the encryption and MAC keys.
  std::unique_ptr<SymmetricKey> master_key =
      SymmetricKey::DeriveKeyFromPasswordUsingScrypt(
          SymmetricKey::AES, password, salt, kCostParameter, kBlockSize,
          kParallelizationParameter, kMaxMemoryBytes,
          2 * kDerivedKeySizeInBits);
  std::string master_key_str = master_key->key();

  std::string encryption_key_str =
      master_key_str.substr(0, kDerivedKeySizeInBytes);
  DCHECK_EQ(encryption_key_str.length(), kDerivedKeySizeInBytes);
  encryption_key = SymmetricKey::Import(SymmetricKey::AES, encryption_key_str);

  std::string mac_key_str = master_key_str.substr(kDerivedKeySizeInBytes);
  DCHECK_EQ(mac_key_str.length(), kDerivedKeySizeInBytes);
  mac_key = SymmetricKey::Import(SymmetricKey::HMAC_SHA1, mac_key_str);

  return user_key && encryption_key && mac_key;
}

bool Nigori::Keys::InitByImport(const std::string& user_key_str,
                                const std::string& encryption_key_str,
                                const std::string& mac_key_str) {
  user_key = SymmetricKey::Import(SymmetricKey::AES, user_key_str);

  encryption_key = SymmetricKey::Import(SymmetricKey::AES, encryption_key_str);
  DCHECK(encryption_key);

  mac_key = SymmetricKey::Import(SymmetricKey::HMAC_SHA1, mac_key_str);
  DCHECK(mac_key);

  return encryption_key && mac_key;
}

Nigori::Nigori() {}

Nigori::~Nigori() {}

bool Nigori::InitByDerivation(const KeyDerivationParams& key_derivation_params,
                              const std::string& password) {
  switch (key_derivation_params.method()) {
    case KeyDerivationMethod::PBKDF2_HMAC_SHA1_1003:
      return keys_.InitByDerivationUsingPbkdf2(
          key_derivation_params.pbkdf2_hostname(),
          key_derivation_params.pbkdf2_username(), password);
    case KeyDerivationMethod::SCRYPT_8192_8_11:
      DCHECK(!base::FeatureList::IsEnabled(
          switches::kSyncForceDisableScryptForCustomPassphrase));
      return keys_.InitByDerivationUsingScrypt(
          key_derivation_params.scrypt_salt(), password);
    case KeyDerivationMethod::UNSUPPORTED:
      return false;
  }

  NOTREACHED();
  return false;
}

bool Nigori::InitByImport(const std::string& user_key,
                          const std::string& encryption_key,
                          const std::string& mac_key) {
  return keys_.InitByImport(user_key, encryption_key, mac_key);
}

// Permute[Kenc,Kmac](type || name)
bool Nigori::Permute(Type type,
                     const std::string& name,
                     std::string* permuted) const {
  DCHECK_LT(0U, name.size());

  NigoriStream plaintext;
  plaintext << type << name;

  crypto::Encryptor encryptor;
  if (!encryptor.Init(keys_.encryption_key.get(), crypto::Encryptor::CBC,
                      std::string(kIvSize, 0)))
    return false;

  std::string ciphertext;
  if (!encryptor.Encrypt(plaintext.str(), &ciphertext))
    return false;

  HMAC hmac(HMAC::SHA256);
  if (!hmac.Init(keys_.mac_key->key()))
    return false;

  std::vector<unsigned char> hash(kHashSize);
  if (!hmac.Sign(ciphertext, &hash[0], hash.size()))
    return false;

  std::string output;
  output.assign(ciphertext);
  output.append(hash.begin(), hash.end());

  Base64Encode(output, permuted);
  return true;
}

// Enc[Kenc,Kmac](value)
bool Nigori::Encrypt(const std::string& value, std::string* encrypted) const {
  if (0U >= value.size())
    return false;

  std::string iv;
  crypto::RandBytes(base::WriteInto(&iv, kIvSize + 1), kIvSize);

  crypto::Encryptor encryptor;
  if (!encryptor.Init(keys_.encryption_key.get(), crypto::Encryptor::CBC, iv))
    return false;

  std::string ciphertext;
  if (!encryptor.Encrypt(value, &ciphertext))
    return false;

  HMAC hmac(HMAC::SHA256);
  if (!hmac.Init(keys_.mac_key->key()))
    return false;

  std::vector<unsigned char> hash(kHashSize);
  if (!hmac.Sign(ciphertext, &hash[0], hash.size()))
    return false;

  std::string output;
  output.assign(iv);
  output.append(ciphertext);
  output.append(hash.begin(), hash.end());

  Base64Encode(output, encrypted);
  return true;
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
  std::string ciphertext(
      input.substr(kIvSize, input.size() - (kIvSize + kHashSize)));
  std::string hash(input.substr(input.size() - kHashSize, kHashSize));

  HMAC hmac(HMAC::SHA256);
  if (!hmac.Init(keys_.mac_key->key()))
    return false;

  std::vector<unsigned char> expected(kHashSize);
  if (!hmac.Sign(ciphertext, &expected[0], expected.size()))
    return false;

  if (hash.compare(0, hash.size(), reinterpret_cast<char*>(&expected[0]),
                   expected.size()))
    return false;

  crypto::Encryptor encryptor;
  if (!encryptor.Init(keys_.encryption_key.get(), crypto::Encryptor::CBC, iv))
    return false;

  if (!encryptor.Decrypt(ciphertext, value))
    return false;

  return true;
}

void Nigori::ExportKeys(std::string* user_key,
                        std::string* encryption_key,
                        std::string* mac_key) const {
  DCHECK(encryption_key);
  DCHECK(mac_key);
  DCHECK(user_key);

  if (keys_.user_key)
    *user_key = keys_.user_key->key();
  else
    user_key->clear();

  *encryption_key = keys_.encryption_key->key();
  *mac_key = keys_.mac_key->key();
}

// static
std::string Nigori::GenerateScryptSalt() {
  static const size_t kSaltSizeInBytes = 32;
  std::string salt;
  salt.resize(kSaltSizeInBytes);
  crypto::RandBytes(base::data(salt), salt.size());
  return salt;
}

}  // namespace syncer
