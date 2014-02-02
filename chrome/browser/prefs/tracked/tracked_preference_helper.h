// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_TRACKED_TRACKED_PREFERENCE_HELPER_H_
#define CHROME_BROWSER_PREFS_TRACKED_TRACKED_PREFERENCE_HELPER_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/prefs/pref_hash_filter.h"
#include "chrome/browser/prefs/pref_hash_store.h"

// A TrackedPreferenceHelper is a helper class for TrackedPreference which
// handles decision making and reporting for TrackedPreference's
// implementations.
class TrackedPreferenceHelper {
 public:
  enum ResetAction {
    DONT_RESET,
    // WANTED_RESET is reported when DO_RESET would have been reported but the
    // current |enforcement_level| doesn't allow a reset for the detected state.
    WANTED_RESET,
    DO_RESET,
  };

  TrackedPreferenceHelper(const std::string& pref_path,
                          size_t reporting_id,
                          size_t reporting_ids_count,
                          PrefHashFilter::EnforcementLevel enforcement_level);

  // Returns a ResetAction stating whether a reset is desired (DO_RESET) based
  // on observing |value_state| or not (DONT_RESET). |allow_changes_|,
  // |allow_seeding_|, and |allow_migration_| make the decision softer in favor
  // of WANTED_RESET over DO_RESET in various scenarios.
  ResetAction GetAction(PrefHashStore::ValueState value_state) const;

  // Reports |value_state| via UMA under |reporting_id_|.
  void ReportValidationResult(PrefHashStore::ValueState value_state) const;

  // Reports |reset_action| via UMA under |reporting_id_|.
  void ReportAction(ResetAction reset_action) const;

  // Reports, via UMA, the |count| of split preference entries that were
  // considered invalid in a CHANGED event.
  void ReportSplitPreferenceChangedCount(size_t count) const;

 private:
  const std::string pref_path_;

  const size_t reporting_id_;
  const size_t reporting_ids_count_;

  // Allow setting changes.
  const bool allow_changes_;
  // Allow seeding unknown values for atomic preferences.
  const bool allow_seeding_;
  // Allow migration of values validated by the old MAC algorithm.
  const bool allow_migration_;

  DISALLOW_COPY_AND_ASSIGN(TrackedPreferenceHelper);
};

#endif  // CHROME_BROWSER_PREFS_TRACKED_TRACKED_PREFERENCE_HELPER_H_
