// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/tracked/tracked_split_preference.h"

#include <vector>

#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/prefs/pref_hash_store.h"

TrackedSplitPreference::TrackedSplitPreference(
    const std::string& pref_path,
    size_t reporting_id,
    size_t reporting_ids_count,
    PrefHashFilter::EnforcementLevel enforcement_level,
    PrefHashStore* pref_hash_store)
    : pref_path_(pref_path),
      helper_(pref_path, reporting_id, reporting_ids_count, enforcement_level),
      pref_hash_store_(pref_hash_store) {
}

void TrackedSplitPreference::OnNewValue(const base::Value* value) const {
  const base::DictionaryValue* dict_value = NULL;
  if (value && !value->GetAsDictionary(&dict_value)) {
    NOTREACHED();
    return;
  }
  pref_hash_store_->StoreSplitHash(pref_path_, dict_value);
}

void TrackedSplitPreference::EnforceAndReport(
    base::DictionaryValue* pref_store_contents) const {
  base::DictionaryValue* dict_value = NULL;
  if (!pref_store_contents->GetDictionary(pref_path_, &dict_value) &&
      pref_store_contents->Get(pref_path_, NULL)) {
    // There should be a dictionary or nothing at |pref_path_|.
    NOTREACHED();
    return;
  }

  std::vector<std::string> invalid_keys;
  PrefHashStore::ValueState value_state =
      pref_hash_store_->CheckSplitValue(pref_path_, dict_value, &invalid_keys);

  if (value_state == PrefHashStore::CHANGED)
    helper_.ReportSplitPreferenceChangedCount(invalid_keys.size());

  helper_.ReportValidationResult(value_state);

  TrackedPreferenceHelper::ResetAction reset_action =
      helper_.GetAction(value_state);
  helper_.ReportAction(reset_action);

  if (reset_action == TrackedPreferenceHelper::DO_RESET) {
    if (value_state == PrefHashStore::CHANGED) {
      DCHECK(!invalid_keys.empty());

      for (std::vector<std::string>::const_iterator it =
               invalid_keys.begin(); it != invalid_keys.end(); ++it) {
        dict_value->Remove(*it, NULL);
      }
    } else {
      pref_store_contents->RemovePath(pref_path_, NULL);
    }
  }

  if (value_state != PrefHashStore::UNCHANGED) {
    // Store the hash for the new value (whether it was reset or not).
    const base::DictionaryValue* new_dict_value = NULL;
    pref_store_contents->GetDictionary(pref_path_, &new_dict_value);
    pref_hash_store_->StoreSplitHash(pref_path_, new_dict_value);
  }
}
