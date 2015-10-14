// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_UTIL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_UTIL_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_vector.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "ui/gfx/native_widget_types.h"

namespace autofill {
struct PasswordForm;
}

namespace sync_driver {
class SyncService;
}

namespace password_manager_util {

// Reports whether and how passwords are currently synced. In particular, for a
// null |sync_service| returns NOT_SYNCING_PASSWORDS.
password_manager::PasswordSyncState GetPasswordSyncState(
    const sync_driver::SyncService* sync_service);

// Finds the forms with a duplicate sync tags in |forms|. The first one of
// the duplicated entries stays in |forms|, the others are moved to
// |duplicates|.
// |tag_groups| is optional. It will contain |forms| and |duplicates| grouped by
// the sync tag. The first element in each group is one from |forms|. It's
// followed by the duplicates.
void FindDuplicates(
    ScopedVector<autofill::PasswordForm>* forms,
    ScopedVector<autofill::PasswordForm>* duplicates,
    std::vector<std::vector<autofill::PasswordForm*>>* tag_groups);

// Removes Android username-only credentials from |android_credentials|.
// Transforms federated credentials into non zero-click ones.
void TrimUsernameOnlyCredentials(
    ScopedVector<autofill::PasswordForm>* android_credentials);

}  // namespace password_manager_util

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_UTIL_H_
