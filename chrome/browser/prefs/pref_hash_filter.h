// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_PREF_HASH_FILTER_H_
#define CHROME_BROWSER_PREFS_PREF_HASH_FILTER_H_

#include <map>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_filter.h"
#include "chrome/browser/prefs/pref_hash_store.h"
#include "chrome/browser/prefs/tracked/tracked_preference.h"

class PrefStore;

namespace base {
class DictionaryValue;
class Value;
}  // namespace base

// Intercepts preference values as they are loaded from disk and verifies them
// using a PrefHashStore. Keeps the PrefHashStore contents up to date as values
// are changed.
class PrefHashFilter : public PrefFilter {
 public:
  enum EnforcementLevel {
    NO_ENFORCEMENT,
    ENFORCE_ON_LOAD
  };

  enum PrefTrackingStrategy {
    // Atomic preferences are tracked as a whole.
    TRACKING_STRATEGY_ATOMIC,
    // Split preferences are dictionaries for which each top-level entry is
    // tracked independently. Note: preferences using this strategy must be kept
    // in sync with TrackedSplitPreferences in histograms.xml.
    TRACKING_STRATEGY_SPLIT,
  };

  struct TrackedPreferenceMetadata {
    size_t reporting_id;
    const char* name;
    // This preference will not be enforced above this level no matter what the
    // |enforcement_level| is set to.
    EnforcementLevel max_enforcement_level;
    PrefTrackingStrategy strategy;
  };

  // Constructs a PrefHashFilter tracking the specified |tracked_preferences|
  // using |pref_hash_store| to check/store hashes.
  // |reporting_ids_count| is the count of all possible IDs (possibly greater
  // than |tracked_preferences_size|). |enforcement_level| determines when this
  // filter will enforce factory defaults upon detecting an untrusted preference
  // value. |reset_callback| is called when a reset event occurs.
  PrefHashFilter(scoped_ptr<PrefHashStore> pref_hash_store,
                 const TrackedPreferenceMetadata tracked_preferences[],
                 size_t tracked_preferences_size,
                 size_t reporting_ids_count,
                 EnforcementLevel enforcement_level,
                 const base::Closure& reset_callback);

  virtual ~PrefHashFilter();

  // Initializes the PrefHashStore with hashes of the tracked preferences in
  // |pref_store|.
  void Initialize(const PrefStore& pref_store);

  // PrefFilter implementation.
  virtual void FilterOnLoad(base::DictionaryValue* pref_store_contents)
      OVERRIDE;
  virtual void FilterUpdate(const std::string& path) OVERRIDE;
  virtual void FilterSerializeData(
      const base::DictionaryValue* pref_store_contents) OVERRIDE;

 private:
  // A map of paths to TrackedPreferences; this map owns this individual
  // TrackedPreference objects.
  typedef base::ScopedPtrHashMap<std::string, TrackedPreference>
      TrackedPreferencesMap;
  // A map from changed paths to their corresponding TrackedPreferences (which
  // aren't owned by this map).
  typedef std::map<std::string, const TrackedPreference*> ChangedPathsMap;

  scoped_ptr<PrefHashStore> pref_hash_store_;

  TrackedPreferencesMap tracked_paths_;

  // The set of all paths whose value has changed since the last call to
  // FilterSerializeData.
  ChangedPathsMap changed_paths_;

  base::Closure reset_callback_;

  DISALLOW_COPY_AND_ASSIGN(PrefHashFilter);
};

#endif  // CHROME_BROWSER_PREFS_PREF_HASH_FILTER_H_
