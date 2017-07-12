// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_HASH_PASSWORD_MANAGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_HASH_PASSWORD_MANAGER_H_

#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string16.h"

class PrefService;

namespace password_manager {

struct SyncPasswordData {
  uint64_t hash;
  std::string salt;
  size_t length;
};

// Responsible for saving, clearing, retrieving and encryption of a sync
// password hash in preferences.
// All methods should be called on UI thread.
class HashPasswordManager {
 public:
  HashPasswordManager() = default;
  ~HashPasswordManager() = default;

  bool SavePasswordHash(const base::string16& password);
  void ClearSavedPasswordHash();

  // Returns empty if no hash is available.
  base::Optional<SyncPasswordData> RetrievePasswordHash();

  void ReportIsSyncPasswordHashSavedMetric();

  void set_prefs(PrefService* prefs) { prefs_ = prefs; }

 private:
  std::string CreateRandomSalt();

  // Packs |salt| and |password_length| to a string.
  std::string LengthAndSaltToString(const std::string& salt,
                                    size_t password_length);

  // Unpacks |salt| and |password_length| from a string |s|.
  void StringToLengthAndSalt(const std::string& s,
                             size_t* password_length,
                             std::string* salt);

  // Saves encrypted string |s| in a preference |pref_name|. Return true on
  // success.
  bool EncryptAndSaveToPrefs(const std::string& pref_name,
                             const std::string& s);

  // Retrieves and decrypts string value from a preference |pref_name|. Return
  // an empty string on failure.
  std::string RetrivedDecryptedStringFromPrefs(const std::string& pref_name);

  PrefService* prefs_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(HashPasswordManager);
};

}  // namespace password_manager
#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_HASH_PASSWORD_MANAGER_H_
