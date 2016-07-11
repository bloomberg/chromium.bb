// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_counter_factory.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/browsing_data/autofill_counter.h"
#include "chrome/browser/browsing_data/browsing_data_counter_utils.h"
#include "chrome/browser/browsing_data/cache_counter.h"
#include "chrome/browser/browsing_data/downloads_counter.h"
#include "chrome/browser/browsing_data/history_counter.h"
#include "chrome/browser/browsing_data/media_licenses_counter.h"
#include "chrome/browser/browsing_data/passwords_counter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/browsing_data/counters/browsing_data_counter.h"

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/browsing_data/hosted_apps_counter.h"
#endif

// static
std::unique_ptr<browsing_data::BrowsingDataCounter>
BrowsingDataCounterFactory::GetForProfileAndPref(Profile* profile,
                                                 const std::string& pref_name) {
  if (!AreCountersEnabled())
    return nullptr;

  if (pref_name == prefs::kDeleteBrowsingHistory)
    return base::MakeUnique<HistoryCounter>(profile);

  if (pref_name == prefs::kDeleteCache)
    return base::MakeUnique<CacheCounter>(profile);

  if (pref_name == prefs::kDeletePasswords)
    return base::MakeUnique<PasswordsCounter>(profile);

  if (pref_name == prefs::kDeleteFormData)
    return base::MakeUnique<AutofillCounter>(profile);

  if (pref_name == prefs::kDeleteDownloadHistory)
    return base::MakeUnique<DownloadsCounter>(profile);

  if (pref_name == prefs::kDeleteMediaLicenses)
    return base::MakeUnique<MediaLicensesCounter>(profile);

#if defined(ENABLE_EXTENSIONS)
  if (pref_name == prefs::kDeleteHostedAppsData)
    return base::MakeUnique<HostedAppsCounter>(profile);
#endif

  return nullptr;
}
