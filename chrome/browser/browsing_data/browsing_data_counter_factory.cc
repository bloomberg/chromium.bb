// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_counter_factory.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/browsing_data/browsing_data_counter_utils.h"
#include "chrome/browser/browsing_data/cache_counter.h"
#include "chrome/browser/browsing_data/downloads_counter.h"
#include "chrome/browser/browsing_data/media_licenses_counter.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/web_history_service_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/web_data_service_factory.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/browsing_data/core/counters/autofill_counter.h"
#include "components/browsing_data/core/counters/browsing_data_counter.h"
#include "components/browsing_data/core/counters/history_counter.h"
#include "components/browsing_data/core/counters/passwords_counter.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/history/core/browser/web_history_service.h"
#include "components/password_manager/core/browser/password_store.h"
#include "extensions/features/features.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/browsing_data/hosted_apps_counter.h"
#endif

namespace {

history::WebHistoryService* GetUpdatedWebHistoryService(Profile* profile) {
  return WebHistoryServiceFactory::GetForProfile(profile);
}

}  // namespace

// static
std::unique_ptr<browsing_data::BrowsingDataCounter>
BrowsingDataCounterFactory::GetForProfileAndPref(Profile* profile,
                                                 const std::string& pref_name) {
  if (!AreCountersEnabled())
    return nullptr;

  if (pref_name == browsing_data::prefs::kDeleteBrowsingHistory) {
    return base::MakeUnique<browsing_data::HistoryCounter>(
        HistoryServiceFactory::GetForProfile(
            profile, ServiceAccessType::EXPLICIT_ACCESS),
        base::Bind(&GetUpdatedWebHistoryService,
                   base::Unretained(profile)),
        ProfileSyncServiceFactory::GetForProfile(profile));
  }

  if (pref_name == browsing_data::prefs::kDeleteCache)
    return base::MakeUnique<CacheCounter>(profile);

  if (pref_name == browsing_data::prefs::kDeletePasswords) {
    return base::MakeUnique<browsing_data::PasswordsCounter>(
        PasswordStoreFactory::GetForProfile(
            profile, ServiceAccessType::EXPLICIT_ACCESS));
  }

  if (pref_name == browsing_data::prefs::kDeleteFormData) {
    return base::MakeUnique<browsing_data::AutofillCounter>(
        WebDataServiceFactory::GetAutofillWebDataForProfile(
            profile, ServiceAccessType::EXPLICIT_ACCESS));
  }

  if (pref_name == browsing_data::prefs::kDeleteDownloadHistory)
    return base::MakeUnique<DownloadsCounter>(profile);

  if (pref_name == browsing_data::prefs::kDeleteMediaLicenses)
    return base::MakeUnique<MediaLicensesCounter>(profile);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (pref_name == browsing_data::prefs::kDeleteHostedAppsData)
    return base::MakeUnique<HostedAppsCounter>(profile);
#endif

  return nullptr;
}
