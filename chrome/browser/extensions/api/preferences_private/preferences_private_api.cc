// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/preferences_private/preferences_private_api.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/extensions/api/preferences_private.h"
#include "components/sync_driver/sync_prefs.h"

namespace extensions {

namespace GetSyncCategoriesWithoutPassphrase =
    api::preferences_private::GetSyncCategoriesWithoutPassphrase;

PreferencesPrivateGetSyncCategoriesWithoutPassphraseFunction::
PreferencesPrivateGetSyncCategoriesWithoutPassphraseFunction() {}

PreferencesPrivateGetSyncCategoriesWithoutPassphraseFunction::
~PreferencesPrivateGetSyncCategoriesWithoutPassphraseFunction() {}

void
PreferencesPrivateGetSyncCategoriesWithoutPassphraseFunction::OnStateChanged() {
  ProfileSyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(GetProfile());
  if (sync_service->sync_initialized()) {
    sync_service->RemoveObserver(this);
    RunAsync();
    Release();  // Balanced in RunAsync().
  }
}

bool PreferencesPrivateGetSyncCategoriesWithoutPassphraseFunction::RunAsync() {
  ProfileSyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(GetProfile());
  if (!sync_service)
    return false;
  if (!sync_service->sync_initialized()) {
    AddRef();  // Balanced in OnStateChanged().
    sync_service->AddObserver(this);
    return true;
  }

  syncer::ModelTypeSet result_set = syncer::UserSelectableTypes();

  // Only include categories that are synced.
  sync_driver::SyncPrefs sync_prefs(GetProfile()->GetPrefs());
  if (!sync_prefs.HasKeepEverythingSynced()) {
    result_set = syncer::Intersection(result_set,
                                      sync_service->GetPreferredDataTypes());
  }
  // Don't include encrypted categories.
  result_set = syncer::Difference(result_set,
                                  sync_service->GetEncryptedDataTypes());

  std::vector<std::string> categories;
  for (syncer::ModelTypeSet::Iterator it = result_set.First(); it.Good();
      it.Inc()) {
    categories.push_back(syncer::ModelTypeToString(it.Get()));
  }

  results_ = GetSyncCategoriesWithoutPassphrase::Results::Create(categories);
  SendResponse(true);
  return true;
}

}  // namespace extensions
