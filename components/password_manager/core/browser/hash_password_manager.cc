// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/hash_password_manager.h"

#include "base/base64.h"
#include "base/strings/string_number_conversions.h"
#include "components/os_crypt/os_crypt.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "crypto/openssl_util.h"
#include "crypto/random.h"
#include "third_party/boringssl/src/include/openssl/evp.h"

namespace {
constexpr size_t kSyncPasswordSaltLength = 16;
constexpr char kSeparator = '.';
}  // namespace

namespace password_manager {

SyncPasswordData::SyncPasswordData(const base::string16& password,
                                   bool force_update)
    : length(password.size()),
      salt(HashPasswordManager::CreateRandomSalt()),
      hash(HashPasswordManager::CalculateSyncPasswordHash(password, salt)),
      force_update(force_update) {}

bool SyncPasswordData::MatchesPassword(const base::string16& password) {
  if (password.size() != this->length)
    return false;
  return HashPasswordManager::CalculateSyncPasswordHash(password, this->salt) ==
         this->hash;
}

HashPasswordManager::HashPasswordManager(PrefService* prefs) : prefs_(prefs) {}

bool HashPasswordManager::SavePasswordHash(const base::string16& password) {
  if (!prefs_)
    return false;

  base::Optional<SyncPasswordData> current_sync_password_data =
      RetrievePasswordHash();
  // If it is the same password, no need to save password hash again.
  if (current_sync_password_data.has_value() &&
      current_sync_password_data->MatchesPassword(password)) {
    return true;
  }

  return SavePasswordHash(SyncPasswordData(password, true));
}

bool HashPasswordManager::SavePasswordHash(
    const SyncPasswordData& sync_password_data) {
  bool should_save = sync_password_data.force_update ||
                     !prefs_->HasPrefPath(prefs::kSyncPasswordHash);
  return should_save ? (EncryptAndSaveToPrefs(
                            prefs::kSyncPasswordHash,
                            base::NumberToString(sync_password_data.hash)) &&
                        EncryptAndSaveToPrefs(
                            prefs::kSyncPasswordLengthAndHashSalt,
                            LengthAndSaltToString(sync_password_data.salt,
                                                  sync_password_data.length)))
                     : false;
}

void HashPasswordManager::ClearSavedPasswordHash() {
  if (prefs_)
    prefs_->ClearPref(prefs::kSyncPasswordHash);
}

base::Optional<SyncPasswordData> HashPasswordManager::RetrievePasswordHash() {
  if (!prefs_ || !prefs_->HasPrefPath(prefs::kSyncPasswordHash))
    return base::nullopt;

  SyncPasswordData result;
  std::string hash_str =
      RetrivedDecryptedStringFromPrefs(prefs::kSyncPasswordHash);
  if (!base::StringToUint64(hash_str, &result.hash))
    return base::nullopt;

  StringToLengthAndSalt(
      RetrivedDecryptedStringFromPrefs(prefs::kSyncPasswordLengthAndHashSalt),
      &result.length, &result.salt);
  return result;
}

bool HashPasswordManager::HasPasswordHash() {
  return prefs_ ? prefs_->HasPrefPath(prefs::kSyncPasswordHash) : false;
}

// static
std::string HashPasswordManager::CreateRandomSalt() {
  char buffer[kSyncPasswordSaltLength];
  crypto::RandBytes(buffer, kSyncPasswordSaltLength);
  // Explicit std::string constructor with a string length must be used in order
  // to avoid treating '\0' symbols as a string ends.
  std::string result(buffer, kSyncPasswordSaltLength);
  return result;
}

// static
uint64_t HashPasswordManager::CalculateSyncPasswordHash(
    const base::StringPiece16& text,
    const std::string& salt) {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);
  constexpr size_t kBytesFromHash = 8;
  constexpr uint64_t kScryptCost = 32;  // It must be power of 2.
  constexpr uint64_t kScryptBlockSize = 8;
  constexpr uint64_t kScryptParallelization = 1;
  constexpr size_t kScryptMaxMemory = 1024 * 1024;

  uint8_t hash[kBytesFromHash];
  base::StringPiece text_8bits(reinterpret_cast<const char*>(text.data()),
                               text.size() * 2);
  const uint8_t* salt_ptr = reinterpret_cast<const uint8_t*>(salt.c_str());

  int scrypt_ok = EVP_PBE_scrypt(text_8bits.data(), text_8bits.size(), salt_ptr,
                                 salt.size(), kScryptCost, kScryptBlockSize,
                                 kScryptParallelization, kScryptMaxMemory, hash,
                                 kBytesFromHash);

  // EVP_PBE_scrypt can only fail due to memory allocation error (which aborts
  // Chromium) or invalid parameters. In case of a failure a hash could leak
  // information from the stack, so using CHECK is better than DCHECK.
  CHECK(scrypt_ok);

  // Take 37 bits of |hash|.
  uint64_t hash37 = ((static_cast<uint64_t>(hash[0]))) |
                    ((static_cast<uint64_t>(hash[1])) << 8) |
                    ((static_cast<uint64_t>(hash[2])) << 16) |
                    ((static_cast<uint64_t>(hash[3])) << 24) |
                    (((static_cast<uint64_t>(hash[4])) & 0x1F) << 32);

  return hash37;
}

std::string HashPasswordManager::LengthAndSaltToString(const std::string& salt,
                                                       size_t password_length) {
  return base::NumberToString(password_length) + kSeparator + salt;
}

void HashPasswordManager::StringToLengthAndSalt(const std::string& s,
                                                size_t* password_length,
                                                std::string* salt) {
  DCHECK(s.find(kSeparator) != std::string::npos);
  DCHECK(salt);
  size_t separator_index = s.find(kSeparator);
  std::string prefix = s.substr(0, separator_index);

  bool is_converted = base::StringToSizeT(prefix, password_length);
  DCHECK(is_converted);

  *salt = s.substr(separator_index + 1);
}

bool HashPasswordManager::EncryptAndSaveToPrefs(const std::string& pref_name,
                                                const std::string& s) {
  DCHECK(prefs_);
  std::string encrypted_text;
  if (!OSCrypt::EncryptString(s, &encrypted_text))
    return false;
  std::string encrypted_base64_text;
  base::Base64Encode(encrypted_text, &encrypted_base64_text);
  prefs_->SetString(pref_name, encrypted_base64_text);
  return true;
}

std::string HashPasswordManager::RetrivedDecryptedStringFromPrefs(
    const std::string& pref_name) {
  DCHECK(prefs_);
  std::string encrypted_base64_text = prefs_->GetString(pref_name);
  if (encrypted_base64_text.empty())
    return std::string();

  std::string encrypted_text;
  if (!base::Base64Decode(encrypted_base64_text, &encrypted_text))
    return std::string();

  std::string plain_text;
  if (!OSCrypt::DecryptString(encrypted_text, &plain_text))
    return std::string();

  return plain_text;
}

}  // namespace password_manager
