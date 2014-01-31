// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_hash_filter.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_store.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"

namespace {

void ReportValidationResult(PrefHashStore::ValueState value_state,
                            size_t value_index,
                            size_t num_values) {
  switch (value_state) {
    case PrefHashStore::UNCHANGED:
      UMA_HISTOGRAM_ENUMERATION("Settings.TrackedPreferenceUnchanged",
                                value_index, num_values);
      return;
    case PrefHashStore::CLEARED:
      UMA_HISTOGRAM_ENUMERATION("Settings.TrackedPreferenceCleared",
                                value_index, num_values);
      return;
    case PrefHashStore::MIGRATED:
      UMA_HISTOGRAM_ENUMERATION("Settings.TrackedPreferenceMigrated",
                                value_index, num_values);
      return;
    case PrefHashStore::CHANGED:
      UMA_HISTOGRAM_ENUMERATION("Settings.TrackedPreferenceChanged",
                                value_index, num_values);
      return;
    case PrefHashStore::UNTRUSTED_UNKNOWN_VALUE:
      UMA_HISTOGRAM_ENUMERATION("Settings.TrackedPreferenceInitialized",
                                value_index, num_values);
      return;
    case PrefHashStore::TRUSTED_UNKNOWN_VALUE:
      UMA_HISTOGRAM_ENUMERATION("Settings.TrackedPreferenceTrustedInitialized",
                                value_index, num_values);
      return;
  }
  NOTREACHED() << "Unexpected PrefHashStore::ValueState: " << value_state;
}

}  // namespace

PrefHashFilter::PrefHashFilter(scoped_ptr<PrefHashStore> pref_hash_store,
                               const TrackedPreference tracked_preferences[],
                               size_t tracked_preferences_size,
                               size_t reporting_ids_count,
                               EnforcementLevel enforcement_level)
    : pref_hash_store_(pref_hash_store.Pass()),
      reporting_ids_count_(reporting_ids_count),
      enforce_(enforcement_level >= ENFORCE),
      no_seeding_(enforcement_level >= ENFORCE_NO_SEEDING),
      no_migration_(enforcement_level >= ENFORCE_NO_SEEDING_NO_MIGRATION) {
  DCHECK_GE(reporting_ids_count, tracked_preferences_size);

  for (size_t i = 0; i < tracked_preferences_size; ++i) {
    bool is_new = tracked_paths_.insert(std::make_pair(
        std::string(tracked_preferences[i].name),
        tracked_preferences[i])).second;
    DCHECK(is_new);
  }
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
    const base::Value* value = NULL;
    pref_store->GetValue(it->first, &value);
    pref_hash_store_->StoreHash(it->first, value);
  }
}

// Validates loaded preference values according to stored hashes, reports
// validation results via UMA, and updates hashes in case of mismatch.
void PrefHashFilter::FilterOnLoad(base::DictionaryValue* pref_store_contents) {
  DCHECK(pref_store_contents);
  base::TimeTicks checkpoint = base::TimeTicks::Now();
  for (TrackedPreferencesMap::const_iterator it = tracked_paths_.begin();
       it != tracked_paths_.end(); ++it) {
    const std::string& pref_path = it->first;
    const TrackedPreference& metadata = it->second;
    const base::Value* value = NULL;
    pref_store_contents->Get(pref_path, &value);

    PrefHashStore::ValueState value_state =
        pref_hash_store_->CheckValue(pref_path, value);
    ReportValidationResult(value_state, metadata.reporting_id,
                           reporting_ids_count_);

    enum {
      DONT_RESET,
      WANTED_RESET,
      DO_RESET,
    } reset_state = DONT_RESET;
    if (metadata.allow_enforcement) {
      switch (value_state) {
        case PrefHashStore::UNCHANGED:
          // Desired case, nothing to do.
          break;
        case PrefHashStore::CLEARED:
          // Unfortunate case, but there is nothing we can do.
          break;
        case PrefHashStore::TRUSTED_UNKNOWN_VALUE:
          // It is okay to seed the hash in this case.
          break;
        case PrefHashStore::MIGRATED:
          reset_state = no_migration_ ? DO_RESET : WANTED_RESET;
          break;
        case PrefHashStore::UNTRUSTED_UNKNOWN_VALUE:
          reset_state = no_seeding_ ? DO_RESET : WANTED_RESET;
          break;
        case PrefHashStore::CHANGED:
          reset_state = enforce_ ? DO_RESET : WANTED_RESET;
          break;
      }
    }

    if (reset_state == DO_RESET) {
      UMA_HISTOGRAM_ENUMERATION("Settings.TrackedPreferenceReset",
                                metadata.reporting_id, reporting_ids_count_);
      // TODO(gab): Store the |old_value| to provide an undo UI.
      scoped_ptr<base::Value> old_value;
      pref_store_contents->RemovePath(pref_path, &old_value);
    } else if (reset_state == WANTED_RESET) {
      UMA_HISTOGRAM_ENUMERATION("Settings.TrackedPreferenceWantedReset",
                                metadata.reporting_id, reporting_ids_count_);
    }

    if (value_state != PrefHashStore::UNCHANGED) {
      // Store the hash for the new value (whether it was reset or not).
      const base::Value* new_value = NULL;
      pref_store_contents->Get(pref_path, &new_value);
      pref_hash_store_->StoreHash(pref_path, new_value);
    }
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
  if (tracked_paths_.find(path) != tracked_paths_.end())
    changed_paths_.insert(path);
}

// Updates the stored hashes for |changed_paths_| before serializing data to
// disk. This is required as storing the hash everytime a pref's value changes
// is too expensive (see perf regression @ http://crbug.com/331273).
void PrefHashFilter::FilterSerializeData(
    const base::DictionaryValue* pref_store_contents) {
  if (!changed_paths_.empty()) {
    base::TimeTicks checkpoint = base::TimeTicks::Now();
    for (std::set<std::string>::const_iterator it = changed_paths_.begin();
         it != changed_paths_.end(); ++it) {
      const std::string& changed_path = *it;
      const base::Value* value = NULL;
      pref_store_contents->Get(changed_path, &value);
      pref_hash_store_->StoreHash(changed_path, value);
    }
    changed_paths_.clear();
    // TODO(gab): Remove this histogram by Feb 21 2014; after sufficient timing
    // data has been gathered from the wild to be confident this doesn't
    // significantly affect performance on the UI thread.
    UMA_HISTOGRAM_TIMES("Settings.FilterSerializeDataTime",
                        base::TimeTicks::Now() - checkpoint);
  }
}
