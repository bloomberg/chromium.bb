// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_hash_filter.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/values.h"
#include "chrome/browser/prefs/pref_hash_store.h"
#include "chrome/common/pref_names.h"

namespace {

// These preferences must be kept in sync with the TrackedPreference enum in
// tools/metrics/histograms/histograms.xml. To add a new preference, append it
// to the array and add a corresponding value to the histogram enum. Replace
// removed preferences with "".
const char* kTrackedPrefs[] = {
  prefs::kShowHomeButton,
  prefs::kHomePageIsNewTabPage,
  prefs::kHomePage,
  prefs::kRestoreOnStartup,
  prefs::kURLsToRestoreOnStartup,
  prefs::kExtensionsPref,
  prefs::kGoogleServicesLastUsername,
  prefs::kSearchProviderOverrides,
  prefs::kDefaultSearchProviderSearchURL,
  prefs::kDefaultSearchProviderKeyword,
  prefs::kDefaultSearchProviderName,
#if !defined(OS_ANDROID)
  prefs::kPinnedTabs,
#else
  "",
#endif
  prefs::kExtensionKnownDisabled,
  prefs::kProfileResetPromptMemento,
};

void ReportValidationResult(PrefHashStore::ValueState value_state,
                            size_t value_index) {
  switch (value_state) {
    case PrefHashStore::UNCHANGED:
      UMA_HISTOGRAM_ENUMERATION("Settings.TrackedPreferenceUnchanged",
                                value_index, arraysize(kTrackedPrefs));
      return;
    case PrefHashStore::CLEARED:
      UMA_HISTOGRAM_ENUMERATION("Settings.TrackedPreferenceCleared",
                                value_index, arraysize(kTrackedPrefs));
      return;
    case PrefHashStore::MIGRATED:
      UMA_HISTOGRAM_ENUMERATION("Settings.TrackedPreferenceMigrated",
                                value_index, arraysize(kTrackedPrefs));
      return;
    case PrefHashStore::CHANGED:
      UMA_HISTOGRAM_ENUMERATION("Settings.TrackedPreferenceChanged",
                                value_index, arraysize(kTrackedPrefs));
      return;
    case PrefHashStore::UNKNOWN_VALUE:
      UMA_HISTOGRAM_ENUMERATION("Settings.TrackedPreferenceInitialized",
                                value_index, arraysize(kTrackedPrefs));
      return;
  }
  NOTREACHED() << "Unexpected PrefHashStore::ValueState: " << value_state;
}

}  // namespace

PrefHashFilter::PrefHashFilter(scoped_ptr<PrefHashStore> pref_hash_store)
    : pref_hash_store_(pref_hash_store.Pass()),
      tracked_paths_(kTrackedPrefs, kTrackedPrefs + arraysize(kTrackedPrefs)) {}

PrefHashFilter::~PrefHashFilter() {}

// Updates the stored hash to correspond to the updated preference value.
void PrefHashFilter::FilterUpdate(const std::string& path,
                                  const base::Value* value) {
  if (tracked_paths_.find(path) != tracked_paths_.end())
    pref_hash_store_->StoreHash(path, value);
}

// Validates loaded preference values according to stored hashes, reports
// validation results via UMA, and updates hashes in case of mismatch.
void PrefHashFilter::FilterOnLoad(base::DictionaryValue* pref_store_contents) {
  for (size_t i = 0; i < arraysize(kTrackedPrefs); ++i) {
    if (kTrackedPrefs[i][0] == '\0')
      continue;
    const base::Value* value = NULL;
    pref_store_contents->Get(kTrackedPrefs[i], &value);
    PrefHashStore::ValueState value_state =
        pref_hash_store_->CheckValue(kTrackedPrefs[i], value);
    ReportValidationResult(value_state, i);
    if (value_state != PrefHashStore::UNCHANGED)
      pref_hash_store_->StoreHash(kTrackedPrefs[i], value);
  }
}
