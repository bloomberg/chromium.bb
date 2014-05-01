// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/default_search_pref_migration.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/search_engines/default_search_manager.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_prepopulate_data.h"
#include "chrome/browser/search_engines/template_url_service.h"

namespace {

void MigrateDefaultSearchPref(PrefService* pref_service) {
  DefaultSearchManager default_search_manager(pref_service);

  TemplateURLData modern_user_dse;
  if (default_search_manager.GetDefaultSearchEngine(&modern_user_dse))
    return;

  scoped_ptr<TemplateURLData> legacy_dse_from_prefs;
  bool legacy_is_managed = false;
  bool has_legacy_dse_from_prefs =
      TemplateURLService::LoadDefaultSearchProviderFromPrefs(
          pref_service, &legacy_dse_from_prefs, &legacy_is_managed);

  if (!has_legacy_dse_from_prefs) {
    // The DSE is undefined. Nothing to migrate.
    return;
  }
  if (!legacy_dse_from_prefs) {
    // The DSE is defined as NULL. This can only really be done via policy.
    // Policy-defined values will be automatically projected into the new
    // format. Even if the user did somehow set this manually we do not have a
    // way to migrate it.
    return;
  }
  if (legacy_is_managed) {
    // The DSE is policy-managed, not user-selected. It will automatically be
    // projected into the new location.
    return;
  }

  // If the pre-populated DSE matches the DSE from prefs we assume it is not a
  // user-selected value.
  scoped_ptr<TemplateURLData> prepopulated_dse(
      TemplateURLPrepopulateData::GetPrepopulatedDefaultSearch(pref_service));
  if (prepopulated_dse &&
      legacy_dse_from_prefs->prepopulate_id ==
          prepopulated_dse->prepopulate_id) {
    return;
  }

  UMA_HISTOGRAM_BOOLEAN("Search.MigratedPrefToDictionaryValue", true);

  // This looks like a user-selected value, so let's migrate it. Subsequent
  // changes to this value will be automatically stored in the correct location.
  default_search_manager.SetUserSelectedDefaultSearchEngine(
      *legacy_dse_from_prefs);

  // TODO(erikwright): Clear the legacy value when the modern value is the
  // authority. Don't forget to do this even if we don't migrate (because we
  // migrated prior to implementing the clear.
}

void OnPrefsInitialized(PrefService* pref_service,
                        bool pref_service_initialization_success) {
  MigrateDefaultSearchPref(pref_service);
}

}  // namespace

void ConfigureDefaultSearchPrefMigrationToDictionaryValue(
    PrefService* pref_service) {
  if (pref_service->GetInitializationStatus() ==
      PrefService::INITIALIZATION_STATUS_WAITING) {
    pref_service->AddPrefInitObserver(
        base::Bind(&OnPrefsInitialized, base::Unretained(pref_service)));
  } else {
    MigrateDefaultSearchPref(pref_service);
  }
}
