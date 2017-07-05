// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store_factory_util.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/task_scheduler/post_task.h"
#include "components/password_manager/core/browser/android_affiliation/affiliated_match_helper.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_service.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/password_manager/core/common/password_manager_features.h"

namespace password_manager {

namespace {

bool ShouldAffiliationBasedMatchingBeActive(syncer::SyncService* sync_service) {
  return base::FeatureList::IsEnabled(features::kAffiliationBasedMatching) &&
         sync_service && sync_service->CanSyncStart() &&
         sync_service->IsSyncActive() &&
         sync_service->GetPreferredDataTypes().Has(syncer::PASSWORDS) &&
         !sync_service->IsUsingSecondaryPassphrase();
}

void ActivateAffiliationBasedMatching(
    PasswordStore* password_store,
    net::URLRequestContextGetter* request_context_getter,
    const base::FilePath& db_path) {
  // The PasswordStore is so far the only consumer of the AffiliationService,
  // therefore the service is owned by the AffiliatedMatchHelper, which in
  // turn is owned by the PasswordStore.
  // Task priority is USER_VISIBLE, because AffiliationService-related tasks
  // block obtaining credentials from PasswordStore, which matches the
  // USER_VISIBLE example: "Loading data that might be shown in the UI after a
  // future user interaction."
  std::unique_ptr<AffiliationService> affiliation_service(
      new AffiliationService(base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE})));
  affiliation_service->Initialize(request_context_getter, db_path);
  std::unique_ptr<AffiliatedMatchHelper> affiliated_match_helper(
      new AffiliatedMatchHelper(password_store,
                                std::move(affiliation_service)));
  affiliated_match_helper->Initialize();
  password_store->SetAffiliatedMatchHelper(std::move(affiliated_match_helper));

  password_store->enable_propagating_password_changes_to_web_credentials(
      base::FeatureList::IsEnabled(features::kAffiliationBasedMatching));
}

base::FilePath GetAffiliationDatabasePath(const base::FilePath& profile_path) {
  return profile_path.Append(kAffiliationDatabaseFileName);
}

}  // namespace

void ToggleAffiliationBasedMatchingBasedOnPasswordSyncedState(
    PasswordStore* password_store,
    syncer::SyncService* sync_service,
    net::URLRequestContextGetter* request_context_getter,
    const base::FilePath& profile_path) {
  DCHECK(password_store);

  const bool matching_should_be_active =
      ShouldAffiliationBasedMatchingBeActive(sync_service);
  const bool matching_is_active =
      password_store->affiliated_match_helper() != nullptr;

  if (matching_should_be_active && !matching_is_active) {
    ActivateAffiliationBasedMatching(password_store, request_context_getter,
                                     GetAffiliationDatabasePath(profile_path));
  } else if (!matching_should_be_active && matching_is_active) {
    password_store->SetAffiliatedMatchHelper(
        base::WrapUnique<AffiliatedMatchHelper>(nullptr));
  }
}

std::unique_ptr<LoginDatabase> CreateLoginDatabase(
    const base::FilePath& profile_path) {
  base::FilePath login_db_file_path = profile_path.Append(kLoginDataFileName);
  return base::MakeUnique<LoginDatabase>(login_db_file_path);
}

}  // namespace password_manager
