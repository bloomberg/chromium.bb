// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store_factory_util.h"

#include "base/command_line.h"
#include "components/password_manager/core/browser/affiliated_match_helper.h"
#include "components/password_manager/core/browser/affiliation_service.h"
#include "components/password_manager/core/browser/affiliation_utils.h"
#include "components/password_manager/core/browser/password_manager_constants.h"

namespace password_manager {

namespace {

bool ShouldAffiliationBasedMatchingBeActive(
    sync_driver::SyncService* sync_service) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!IsAffiliationBasedMatchingEnabled(*command_line))
    return false;

  return sync_service && sync_service->CanSyncStart() &&
         sync_service->IsSyncActive() &&
         sync_service->GetPreferredDataTypes().Has(syncer::PASSWORDS) &&
         !sync_service->IsUsingSecondaryPassphrase();
}

void ActivateAffiliationBasedMatching(
    PasswordStore* password_store,
    net::URLRequestContextGetter* request_context_getter,
    const base::FilePath& db_path,
    scoped_refptr<base::SingleThreadTaskRunner> db_thread_runner) {
  // The PasswordStore is so far the only consumer of the AffiliationService,
  // therefore the service is owned by the AffiliatedMatchHelper, which in
  // turn is owned by the PasswordStore.
  scoped_ptr<AffiliationService> affiliation_service(
      new AffiliationService(db_thread_runner));
  affiliation_service->Initialize(request_context_getter, db_path);
  scoped_ptr<AffiliatedMatchHelper> affiliated_match_helper(
      new AffiliatedMatchHelper(password_store, affiliation_service.Pass()));
  affiliated_match_helper->Initialize();
  password_store->SetAffiliatedMatchHelper(affiliated_match_helper.Pass());

  password_store->enable_propagating_password_changes_to_web_credentials(
      IsPropagatingPasswordChangesToWebCredentialsEnabled(
          *base::CommandLine::ForCurrentProcess()));
}

base::FilePath GetAffiliationDatabasePath(const base::FilePath& profile_path) {
  return profile_path.Append(kAffiliationDatabaseFileName);
}

}  // namespace

void ToggleAffiliationBasedMatchingBasedOnPasswordSyncedState(
    PasswordStore* password_store,
    sync_driver::SyncService* sync_service,
    net::URLRequestContextGetter* request_context_getter,
    const base::FilePath& profile_path,
    scoped_refptr<base::SingleThreadTaskRunner> db_thread_runner) {
  DCHECK(password_store);

  const bool matching_should_be_active =
      ShouldAffiliationBasedMatchingBeActive(sync_service);
  const bool matching_is_active =
      password_store->affiliated_match_helper() != nullptr;

  if (matching_should_be_active && !matching_is_active) {
    ActivateAffiliationBasedMatching(password_store, request_context_getter,
                                     GetAffiliationDatabasePath(profile_path),
                                     db_thread_runner);
  } else if (!matching_should_be_active && matching_is_active) {
    password_store->SetAffiliatedMatchHelper(
        make_scoped_ptr<AffiliatedMatchHelper>(nullptr));
  }
}

void TrimOrDeleteAffiliationCacheForStoreAndPath(
    PasswordStore* password_store,
    const base::FilePath& profile_path,
    scoped_refptr<base::SingleThreadTaskRunner> db_thread_runner) {
  if (password_store && password_store->affiliated_match_helper()) {
    password_store->TrimAffiliationCache();
  } else {
    AffiliationService::DeleteCache(GetAffiliationDatabasePath(profile_path),
                                    db_thread_runner.get());
  }
}

scoped_refptr<PasswordStore> GetPasswordStoreFromService(
    password_manager::PasswordStoreService* service,
    ServiceAccessType access_type,
    bool is_off_the_record) {
  if (access_type == ServiceAccessType::IMPLICIT_ACCESS && is_off_the_record) {
    NOTREACHED() << "Incognito state does not have a password store associated";
    return nullptr;
  }

  return service ? service->GetPasswordStore() : nullptr;
}

scoped_ptr<LoginDatabase> CreateLoginDatabase(
    const base::FilePath& profile_path) {
  base::FilePath login_db_file_path = profile_path.Append(kLoginDataFileName);
  return make_scoped_ptr(new LoginDatabase(login_db_file_path));
}

scoped_ptr<KeyedService> BuildServiceInstanceFromStore(
    scoped_refptr<PasswordStore> store,
    syncer::SyncableService::StartSyncFlare sync_flare) {
  DCHECK(store);
  if (!store->Init(sync_flare)) {
    NOTREACHED() << "Could not initialize password store.";
    return nullptr;
  }

  return make_scoped_ptr(new PasswordStoreService(store));
}

}  // namespace password_manager
