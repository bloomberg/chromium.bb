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
  // TODO(crbug.com/722414): Add salt.
};

// Responsible for saving, clearing, retrieving and encryption of a sync
// password hash in preferences.
// All methods should be called on UI thread.
class HashPasswordManager {
 public:
  HashPasswordManager() = default;
  ~HashPasswordManager() = default;

  void SavePasswordHash(const base::string16& password);
  void ClearSavedPasswordHash();

  // Returns empty if no hash is available.
  base::Optional<SyncPasswordData> RetrievePasswordHash();

  void set_prefs(PrefService* prefs) { prefs_ = prefs; }

 private:
  PrefService* prefs_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(HashPasswordManager);
};

}  // namespace password_manager
#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_HASH_PASSWORD_MANAGER_H_
