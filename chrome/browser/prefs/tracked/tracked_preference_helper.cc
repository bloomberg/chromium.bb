// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/tracked/tracked_preference_helper.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"

TrackedPreferenceHelper::TrackedPreferenceHelper(
    const std::string& pref_path,
    size_t reporting_id,
    size_t reporting_ids_count,
    PrefHashFilter::EnforcementLevel enforcement_level)
    : pref_path_(pref_path),
      reporting_id_(reporting_id), reporting_ids_count_(reporting_ids_count),
      enforce_(enforcement_level == PrefHashFilter::ENFORCE_ON_LOAD) {
}

TrackedPreferenceHelper::ResetAction TrackedPreferenceHelper::GetAction(
    PrefHashStoreTransaction::ValueState value_state) const {
  switch (value_state) {
    case PrefHashStoreTransaction::UNCHANGED:
      // Desired case, nothing to do.
      return DONT_RESET;
    case PrefHashStoreTransaction::CLEARED:
      // Unfortunate case, but there is nothing we can do.
      return DONT_RESET;
    case PrefHashStoreTransaction::TRUSTED_UNKNOWN_VALUE:
      // It is okay to seed the hash in this case.
      return DONT_RESET;
    case PrefHashStoreTransaction::SECURE_LEGACY:
      // Accept secure legacy device ID based hashes.
      return DONT_RESET;
    case PrefHashStoreTransaction::WEAK_LEGACY:  // Falls through.
    case PrefHashStoreTransaction::UNTRUSTED_UNKNOWN_VALUE:  // Falls through.
    case PrefHashStoreTransaction::CHANGED:
      return enforce_ ? DO_RESET : WANTED_RESET;
  }
  NOTREACHED() << "Unexpected PrefHashStoreTransaction::ValueState: "
               << value_state;
  return DONT_RESET;
}

void TrackedPreferenceHelper::ReportValidationResult(
    PrefHashStoreTransaction::ValueState value_state) const {
  switch (value_state) {
    case PrefHashStoreTransaction::UNCHANGED:
      UMA_HISTOGRAM_ENUMERATION("Settings.TrackedPreferenceUnchanged",
                                reporting_id_, reporting_ids_count_);
      return;
    case PrefHashStoreTransaction::CLEARED:
      UMA_HISTOGRAM_ENUMERATION("Settings.TrackedPreferenceCleared",
                                reporting_id_, reporting_ids_count_);
      return;
    case PrefHashStoreTransaction::WEAK_LEGACY:
      UMA_HISTOGRAM_ENUMERATION("Settings.TrackedPreferenceMigrated",
                                reporting_id_, reporting_ids_count_);
      return;
    case PrefHashStoreTransaction::SECURE_LEGACY:
      UMA_HISTOGRAM_ENUMERATION(
          "Settings.TrackedPreferenceMigratedLegacyDeviceId", reporting_id_,
          reporting_ids_count_);
      return;
    case PrefHashStoreTransaction::CHANGED:
      UMA_HISTOGRAM_ENUMERATION("Settings.TrackedPreferenceChanged",
                                reporting_id_, reporting_ids_count_);
      return;
    case PrefHashStoreTransaction::UNTRUSTED_UNKNOWN_VALUE:
      UMA_HISTOGRAM_ENUMERATION("Settings.TrackedPreferenceInitialized",
                                reporting_id_, reporting_ids_count_);
      return;
    case PrefHashStoreTransaction::TRUSTED_UNKNOWN_VALUE:
      UMA_HISTOGRAM_ENUMERATION("Settings.TrackedPreferenceTrustedInitialized",
                                reporting_id_, reporting_ids_count_);
      return;
  }
  NOTREACHED() << "Unexpected PrefHashStoreTransaction::ValueState: "
               << value_state;
}

void TrackedPreferenceHelper::ReportAction(ResetAction reset_action) const {
  switch (reset_action) {
    case DONT_RESET:
      // No report for DONT_RESET.
      break;
    case WANTED_RESET:
      UMA_HISTOGRAM_ENUMERATION("Settings.TrackedPreferenceWantedReset",
                                reporting_id_, reporting_ids_count_);
      break;
    case DO_RESET:
      UMA_HISTOGRAM_ENUMERATION("Settings.TrackedPreferenceReset",
                                reporting_id_, reporting_ids_count_);
      break;
  }
}

void TrackedPreferenceHelper::ReportSplitPreferenceChangedCount(
    size_t count) const {
  // The histogram below is an expansion of the UMA_HISTOGRAM_COUNTS_100 macro
  // adapted to allow for a dynamically suffixed histogram name.
  // Note: The factory creates and owns the histogram.
  base::HistogramBase* histogram =
      base::LinearHistogram::FactoryGet(
          "Settings.TrackedSplitPreferenceChanged." + pref_path_,
          1,
          100,  // Allow counts up to 100.
          101,
          base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->Add(count);
}
