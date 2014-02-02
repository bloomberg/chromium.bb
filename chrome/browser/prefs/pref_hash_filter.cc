// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_hash_filter.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_store.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/prefs/tracked/tracked_atomic_preference.h"
#include "chrome/browser/prefs/tracked/tracked_split_preference.h"

PrefHashFilter::PrefHashFilter(
    scoped_ptr<PrefHashStore> pref_hash_store,
    const TrackedPreferenceMetadata tracked_preferences[],
    size_t tracked_preferences_size,
    size_t reporting_ids_count,
    EnforcementLevel enforcement_level) {
  DCHECK(pref_hash_store);
  DCHECK_GE(reporting_ids_count, tracked_preferences_size);

  for (size_t i = 0; i < tracked_preferences_size; ++i) {
    const TrackedPreferenceMetadata& metadata = tracked_preferences[i];

    EnforcementLevel enforcement_level_for_pref =
        metadata.allow_enforcement ? enforcement_level : NO_ENFORCEMENT;

    scoped_ptr<TrackedPreference> tracked_preference;
    switch (metadata.strategy) {
      case TRACKING_STRATEGY_ATOMIC:
        tracked_preference.reset(
            new TrackedAtomicPreference(metadata.name, metadata.reporting_id,
                                        reporting_ids_count,
                                        enforcement_level_for_pref,
                                        pref_hash_store.get()));
        break;
      case TRACKING_STRATEGY_SPLIT:
        tracked_preference.reset(
            new TrackedSplitPreference(metadata.name, metadata.reporting_id,
                                       reporting_ids_count,
                                       enforcement_level_for_pref,
                                       pref_hash_store.get()));
        break;
    }
    DCHECK(tracked_preference);

    bool is_new = tracked_paths_.add(metadata.name,
                                     tracked_preference.Pass()).second;
    DCHECK(is_new);
  }

  // Retain const-ownership of |pref_hash_store| in this class, non-const access
  // was granted to this class' TrackedPreferences above which should be used
  // for any further interaction with the |pref_hash_store|.
  pref_hash_store_ = pref_hash_store.Pass();
}

PrefHashFilter::~PrefHashFilter() {
  // Ensure new values for all |changed_paths_| have been flushed to
  // |pref_hash_store_| already.
  DCHECK(changed_paths_.empty());
}

void PrefHashFilter::Initialize(PrefStore* pref_store) {
  UMA_HISTOGRAM_BOOLEAN(
      "Settings.TrackedPreferencesInitializedForUnloadedProfile", true);

  for (TrackedPreferencesMap::const_iterator it = tracked_paths_.begin();
       it != tracked_paths_.end(); ++it) {
    const std::string& initialized_path = it->first;
    const TrackedPreference* initialized_preference = it->second;
    const base::Value* value = NULL;
    pref_store->GetValue(initialized_path, &value);
    initialized_preference->OnNewValue(value);
  }
}

// Validates loaded preference values according to stored hashes, reports
// validation results via UMA, and updates hashes in case of mismatch.
void PrefHashFilter::FilterOnLoad(base::DictionaryValue* pref_store_contents) {
  DCHECK(pref_store_contents);
  base::TimeTicks checkpoint = base::TimeTicks::Now();
  for (TrackedPreferencesMap::const_iterator it = tracked_paths_.begin();
       it != tracked_paths_.end(); ++it) {
    it->second->EnforceAndReport(pref_store_contents);
  }
  // TODO(gab): Remove this histogram by Feb 21 2014; after sufficient timing
  // data has been gathered from the wild to be confident this doesn't
  // significantly affect startup.
  UMA_HISTOGRAM_TIMES("Settings.FilterOnLoadTime",
                      base::TimeTicks::Now() - checkpoint);
}

// Marks |path| has having changed if it is part of |tracked_paths_|. A new hash
// will be stored for it the next time FilterSerializeData() is invoked.
void PrefHashFilter::FilterUpdate(const std::string& path) {
  TrackedPreferencesMap::const_iterator it = tracked_paths_.find(path);
  if (it != tracked_paths_.end())
    changed_paths_.insert(std::make_pair(path, it->second));
}

// Updates the stored hashes for |changed_paths_| before serializing data to
// disk. This is required as storing the hash everytime a pref's value changes
// is too expensive (see perf regression @ http://crbug.com/331273).
void PrefHashFilter::FilterSerializeData(
    const base::DictionaryValue* pref_store_contents) {
  if (!changed_paths_.empty()) {
    base::TimeTicks checkpoint = base::TimeTicks::Now();
    for (ChangedPathsMap::const_iterator it = changed_paths_.begin();
         it != changed_paths_.end(); ++it) {
      const std::string& changed_path = it->first;
      const TrackedPreference* changed_preference = it->second;
      const base::Value* value = NULL;
      pref_store_contents->Get(changed_path, &value);
      changed_preference->OnNewValue(value);
    }
    changed_paths_.clear();
    // TODO(gab): Remove this histogram by Feb 21 2014; after sufficient timing
    // data has been gathered from the wild to be confident this doesn't
    // significantly affect performance on the UI thread.
    UMA_HISTOGRAM_TIMES("Settings.FilterSerializeDataTime",
                        base::TimeTicks::Now() - checkpoint);
  }
}
