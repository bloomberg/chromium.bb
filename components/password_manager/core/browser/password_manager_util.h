// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_UTIL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_UTIL_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "ui/gfx/native_widget_types.h"

namespace autofill {
struct PasswordForm;
class AutofillClient;
}

namespace password_manager {
class PasswordManagerClient;
}

namespace syncer {
class SyncService;
}

namespace password_manager_util {

// Reports whether and how passwords are currently synced. In particular, for a
// null |sync_service| returns NOT_SYNCING_PASSWORDS.
password_manager::PasswordSyncState GetPasswordSyncState(
    const syncer::SyncService* sync_service);

// Finds the forms with a duplicate sync tags in |forms|. The first one of
// the duplicated entries stays in |forms|, the others are moved to
// |duplicates|.
// |tag_groups| is optional. It will contain |forms| and |duplicates| grouped by
// the sync tag. The first element in each group is one from |forms|. It's
// followed by the duplicates.
void FindDuplicates(
    std::vector<std::unique_ptr<autofill::PasswordForm>>* forms,
    std::vector<std::unique_ptr<autofill::PasswordForm>>* duplicates,
    std::vector<std::vector<autofill::PasswordForm*>>* tag_groups);

// Removes Android username-only credentials from |android_credentials|.
// Transforms federated credentials into non zero-click ones.
void TrimUsernameOnlyCredentials(
    std::vector<std::unique_ptr<autofill::PasswordForm>>* android_credentials);

// A convenience function for testing that |client| has a non-null LogManager
// and that that LogManager returns true for IsLoggingActive. This function can
// be removed once PasswordManagerClient::GetLogManager is implemented on iOS
// and required to always return non-null.
bool IsLoggingActive(const password_manager::PasswordManagerClient* client);

// Calculates 37 bits hash for a sync password. The calculation is based on a
// slow hash function. The running time is ~10^{-4} seconds on Desktop.
uint64_t CalculateSyncPasswordHash(const base::StringPiece16& text,
                                   const std::string& salt);

// True iff the manual password generation is enabled and the user is sync user
// without custom passphrase.
bool ManualPasswordGenerationEnabled(syncer::SyncService* sync_service);

// Returns true iff the "Show all saved passwords" option should be shown in
// Context Menu. Also records metric, that the Context Menu will have "Show all
// saved passwords" option.
bool ShowAllSavedPasswordsContextMenuEnabled();

// Opens Password Manager setting page and records the metrics.
void UserTriggeredShowAllSavedPasswordsFromContextMenu(
    autofill::AutofillClient* autofill_client);

// Triggers password generation flow and records the metrics.
void UserTriggeredManualGenerationFromContextMenu(
    password_manager::PasswordManagerClient* password_manager_client);

}  // namespace password_manager_util

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_UTIL_H_
