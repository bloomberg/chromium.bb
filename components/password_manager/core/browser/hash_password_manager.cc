// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/hash_password_manager.h"

#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"

namespace password_manager {

void HashPasswordManager::SavePasswordHash(const base::string16& password) {
  if (prefs_) {
    // TODO(crbug.com/657041) Implement creating a salt, hash calculation,
    // encrypting and saving of hash of |password| into preference
    // kSyncPasswordHash.
    prefs_->SetString(prefs::kSyncPasswordHash, std::string());
  }
}

void HashPasswordManager::ClearSavedPasswordHash() {
  if (prefs_)
    prefs_->ClearPref(prefs::kSyncPasswordHash);
}

base::Optional<SyncPasswordData> HashPasswordManager::RetrievePasswordHash() {
  if (!prefs_ || !prefs_->HasPrefPath(prefs::kSyncPasswordHash))
    return base::Optional<SyncPasswordData>();
  SyncPasswordData result;
  if (!base::StringToUint64(prefs_->GetString(prefs::kSyncPasswordHash),
                            &result.hash))
    return base::Optional<SyncPasswordData>();
  return result;
}

}  // namespace password_manager
