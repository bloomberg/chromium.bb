// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/hash_password_manager.h"

#include "base/base64.h"
#include "base/strings/string_number_conversions.h"
#include "components/os_crypt/os_crypt.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "crypto/random.h"

namespace {
constexpr size_t kSyncPasswordSaltLength = 16;
constexpr char kSeparator = '.';
}  // namespace

namespace password_manager {

bool HashPasswordManager::SavePasswordHash(const base::string16& password) {
  if (!prefs_)
    return false;

  base::Optional<SyncPasswordData> current_sync_password_data =
      RetrievePasswordHash();
  // If it is the same password, no need to save password hash again.
  if (current_sync_password_data.has_value() &&
      password_manager_util::CalculateSyncPasswordHash(
          password, current_sync_password_data->salt) ==
          current_sync_password_data->hash) {
    return true;
  }

  std::string salt = CreateRandomSalt();
  std::string hash = base::Uint64ToString(
      password_manager_util::CalculateSyncPasswordHash(password, salt));
  EncryptAndSaveToPrefs(prefs::kSyncPasswordHash, hash);

  // Password length and salt are stored together.
  std::string length_salt = LengthAndSaltToString(salt, password.size());
  return EncryptAndSaveToPrefs(prefs::kSyncPasswordLengthAndHashSalt,
                               length_salt);
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

void HashPasswordManager::ReportIsSyncPasswordHashSavedMetric() {
  if (!prefs_)
    return;
  auto hash_password_state =
      prefs_->HasPrefPath(prefs::kSyncPasswordHash)
          ? metrics_util::IsSyncPasswordHashSaved::SAVED
          : metrics_util::IsSyncPasswordHashSaved::NOT_SAVED;
  metrics_util::LogIsSyncPasswordHashSaved(hash_password_state);
}

std::string HashPasswordManager::CreateRandomSalt() {
  char buffer[kSyncPasswordSaltLength];
  crypto::RandBytes(buffer, kSyncPasswordSaltLength);
  // Explicit std::string constructor with a string length must be used in order
  // to avoid treating '\0' symbols as a string ends.
  std::string result(buffer, kSyncPasswordSaltLength);
  return result;
}

std::string HashPasswordManager::LengthAndSaltToString(const std::string& salt,
                                                       size_t password_length) {
  return base::SizeTToString(password_length) + kSeparator + salt;
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
