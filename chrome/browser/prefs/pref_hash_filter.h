// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_PREF_HASH_FILTER_H_
#define CHROME_BROWSER_PREFS_PREF_HASH_FILTER_H_

#include <map>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_filter.h"
#include "chrome/browser/prefs/pref_hash_store.h"

namespace base {
class DictionaryValue;
class Value;
}  // namespace base

// Intercepts preference values as they are loaded from disk and verifies them
// using a PrefHashStore. Keeps the PrefHashStore contents up to date as values
// are changed.
class PrefHashFilter : public PrefFilter {
 public:
  // Enforcement levels are defined in order of intensity; the next level always
  // implies the previous one and more.
  enum EnforcementLevel {
    NO_ENFORCEMENT,
    ENFORCE,
    ENFORCE_NO_SEEDING,
    ENFORCE_NO_SEEDING_NO_MIGRATION,
    // ENFORCE_ALL must always remain last; it is meant to be used when the
    // desired level is underdetermined and the caller wants to enforce the
    // strongest setting to be safe.
    ENFORCE_ALL
  };

  struct TrackedPreference {
    size_t reporting_id;
    const char* name;
    // Whether to actually reset this specific preference when everything else
    // indicates that it should be.
    bool allow_enforcement;
  };

  // Constructs a PrefHashFilter tracking the specified |tracked_preferences|
  // using |pref_hash_store| to check/store hashes.
  // |reporting_ids_count| is the count of all possible IDs (possibly greater
  // than |tracked_preferences_size|). |enforcement_level| determines when this
  // filter will enforce factory defaults upon detecting an untrusted preference
  // value.
  PrefHashFilter(scoped_ptr<PrefHashStore> pref_hash_store,
                 const TrackedPreference tracked_preferences[],
                 size_t tracked_preferences_size,
                 size_t reporting_ids_count,
                 EnforcementLevel enforcement_level);

  virtual ~PrefHashFilter();

  // PrefFilter implementation.
  virtual void FilterOnLoad(base::DictionaryValue* pref_store_contents)
      OVERRIDE;
  virtual void FilterUpdate(const std::string& path) OVERRIDE;
  virtual void FilterSerializeData(
      const base::DictionaryValue* pref_store_contents) OVERRIDE;

 private:
  typedef std::map<std::string, const TrackedPreference> TrackedPreferencesMap;

  scoped_ptr<PrefHashStore> pref_hash_store_;

  TrackedPreferencesMap tracked_paths_;

  // The set of all paths whose value has changed since the last call to
  // FilterSerializeData.
  std::set<std::string> changed_paths_;

  size_t reporting_ids_count_;

  // Prevent setting changes.
  bool enforce_;
  // Do not seed unknown values.
  bool no_seeding_;
  // Do not migrate values validated by the old MAC algorithm.
  bool no_migration_;

  DISALLOW_COPY_AND_ASSIGN(PrefHashFilter);
};

#endif  // CHROME_BROWSER_PREFS_PREF_HASH_FILTER_H_
