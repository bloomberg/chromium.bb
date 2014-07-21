// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_hash_filter.h"

#include <algorithm>

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/pref_store.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/prefs/pref_hash_store.h"
#include "chrome/browser/prefs/pref_hash_store_transaction.h"
#include "chrome/browser/prefs/tracked/dictionary_hash_store_contents.h"
#include "chrome/browser/prefs/tracked/tracked_atomic_preference.h"
#include "chrome/browser/prefs/tracked/tracked_split_preference.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"

PrefHashFilter::PrefHashFilter(
    scoped_ptr<PrefHashStore> pref_hash_store,
    const std::vector<TrackedPreferenceMetadata>& tracked_preferences,
    const base::Closure& on_reset_on_load,
    TrackedPreferenceValidationDelegate* delegate,
    size_t reporting_ids_count,
    bool report_super_mac_validity)
    : pref_hash_store_(pref_hash_store.Pass()),
      on_reset_on_load_(on_reset_on_load),
      report_super_mac_validity_(report_super_mac_validity) {
  DCHECK(pref_hash_store_);
  DCHECK_GE(reporting_ids_count, tracked_preferences.size());

  for (size_t i = 0; i < tracked_preferences.size(); ++i) {
    const TrackedPreferenceMetadata& metadata = tracked_preferences[i];

    scoped_ptr<TrackedPreference> tracked_preference;
    switch (metadata.strategy) {
      case TRACKING_STRATEGY_ATOMIC:
        tracked_preference.reset(
            new TrackedAtomicPreference(metadata.name,
                                        metadata.reporting_id,
                                        reporting_ids_count,
                                        metadata.enforcement_level,
                                        delegate));
        break;
      case TRACKING_STRATEGY_SPLIT:
        tracked_preference.reset(
            new TrackedSplitPreference(metadata.name,
                                       metadata.reporting_id,
                                       reporting_ids_count,
                                       metadata.enforcement_level,
                                       delegate));
        break;
    }
    DCHECK(tracked_preference);

    bool is_new = tracked_paths_.add(metadata.name,
                                     tracked_preference.Pass()).second;
    DCHECK(is_new);
  }
}

PrefHashFilter::~PrefHashFilter() {
  // Ensure new values for all |changed_paths_| have been flushed to
  // |pref_hash_store_| already.
  DCHECK(changed_paths_.empty());
}

// static
void PrefHashFilter::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  // See GetResetTime for why this is a StringPref and not Int64Pref.
  registry->RegisterStringPref(
      prefs::kPreferenceResetTime,
      base::Int64ToString(base::Time().ToInternalValue()),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

// static
base::Time PrefHashFilter::GetResetTime(PrefService* user_prefs) {
  // Provide our own implementation (identical to the PrefService::GetInt64) in
  // order to ensure it remains consistent with the way we store this value
  // (which we do via a PrefStore, preventing us from reusing
  // PrefService::SetInt64).
  int64 internal_value = base::Time().ToInternalValue();
  if (!base::StringToInt64(
          user_prefs->GetString(prefs::kPreferenceResetTime),
          &internal_value)) {
    // Somehow the value stored on disk is not a valid int64.
    NOTREACHED();
    return base::Time();
  }
  return base::Time::FromInternalValue(internal_value);
}

// static
void PrefHashFilter::ClearResetTime(PrefService* user_prefs) {
  user_prefs->ClearPref(prefs::kPreferenceResetTime);
}

void PrefHashFilter::Initialize(base::DictionaryValue* pref_store_contents) {
  scoped_ptr<PrefHashStoreTransaction> hash_store_transaction(
      pref_hash_store_->BeginTransaction(scoped_ptr<HashStoreContents>(
          new DictionaryHashStoreContents(pref_store_contents))));
  for (TrackedPreferencesMap::const_iterator it = tracked_paths_.begin();
       it != tracked_paths_.end(); ++it) {
    const std::string& initialized_path = it->first;
    const TrackedPreference* initialized_preference = it->second;
    const base::Value* value = NULL;
    pref_store_contents->Get(initialized_path, &value);
    initialized_preference->OnNewValue(value, hash_store_transaction.get());
  }
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
    base::DictionaryValue* pref_store_contents) {
  if (!changed_paths_.empty()) {
    base::TimeTicks checkpoint = base::TimeTicks::Now();
    {
      scoped_ptr<PrefHashStoreTransaction> hash_store_transaction(
          pref_hash_store_->BeginTransaction(scoped_ptr<HashStoreContents>(
              new DictionaryHashStoreContents(pref_store_contents))));
      for (ChangedPathsMap::const_iterator it = changed_paths_.begin();
           it != changed_paths_.end(); ++it) {
        const std::string& changed_path = it->first;
        const TrackedPreference* changed_preference = it->second;
        const base::Value* value = NULL;
        pref_store_contents->Get(changed_path, &value);
        changed_preference->OnNewValue(value, hash_store_transaction.get());
      }
      changed_paths_.clear();
    }
    // TODO(gab): Remove this histogram by Feb 21 2014; after sufficient timing
    // data has been gathered from the wild to be confident this doesn't
    // significantly affect performance on the UI thread.
    UMA_HISTOGRAM_TIMES("Settings.FilterSerializeDataTime",
                        base::TimeTicks::Now() - checkpoint);
  }
}

void PrefHashFilter::FinalizeFilterOnLoad(
    const PostFilterOnLoadCallback& post_filter_on_load_callback,
    scoped_ptr<base::DictionaryValue> pref_store_contents,
    bool prefs_altered) {
  DCHECK(pref_store_contents);
  base::TimeTicks checkpoint = base::TimeTicks::Now();

  bool did_reset = false;
  {
    scoped_ptr<PrefHashStoreTransaction> hash_store_transaction(
        pref_hash_store_->BeginTransaction(scoped_ptr<HashStoreContents>(
            new DictionaryHashStoreContents(pref_store_contents.get()))));
    if (report_super_mac_validity_) {
      UMA_HISTOGRAM_BOOLEAN("Settings.HashesDictionaryTrusted",
                            hash_store_transaction->IsSuperMACValid());
    }

    for (TrackedPreferencesMap::const_iterator it = tracked_paths_.begin();
         it != tracked_paths_.end(); ++it) {
      if (it->second->EnforceAndReport(pref_store_contents.get(),
                                       hash_store_transaction.get())) {
        did_reset = true;
        prefs_altered = true;
      }
    }
    if (hash_store_transaction->StampSuperMac())
      prefs_altered = true;
  }

  if (did_reset) {
    pref_store_contents->Set(prefs::kPreferenceResetTime,
                             new base::StringValue(base::Int64ToString(
                                 base::Time::Now().ToInternalValue())));
    FilterUpdate(prefs::kPreferenceResetTime);

    if (!on_reset_on_load_.is_null())
      on_reset_on_load_.Run();
  }

  // TODO(gab): Remove this histogram by Feb 21 2014; after sufficient timing
  // data has been gathered from the wild to be confident this doesn't
  // significantly affect startup.
  UMA_HISTOGRAM_TIMES("Settings.FilterOnLoadTime",
                      base::TimeTicks::Now() - checkpoint);

  post_filter_on_load_callback.Run(pref_store_contents.Pass(), prefs_altered);
}
