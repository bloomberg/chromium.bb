// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_TRACKED_TRACKED_PREFERENCE_VALIDATION_DELEGATE_H_
#define CHROME_BROWSER_PREFS_TRACKED_TRACKED_PREFERENCE_VALIDATION_DELEGATE_H_

#include <string>
#include <vector>

#include "chrome/browser/prefs/pref_hash_store_transaction.h"
#include "chrome/browser/prefs/tracked/tracked_preference_helper.h"

namespace base {
class DictionaryValue;
class Value;
}

// A TrackedPreferenceValidationDelegate is notified of the results of each
// tracked preference validation event.
class TrackedPreferenceValidationDelegate {
 public:
  virtual ~TrackedPreferenceValidationDelegate() {}

  // Notifies observes of the result (|value_state|) of checking the atomic
  // |value| (which may be NULL) at |pref_path|. |reset_action| indicates
  // whether or not a reset will occur based on |value_state| and the
  // enforcement level in place.
  virtual void OnAtomicPreferenceValidation(
      const std::string& pref_path,
      const base::Value* value,
      PrefHashStoreTransaction::ValueState value_state,
      TrackedPreferenceHelper::ResetAction reset_action) = 0;

  // Notifies observes of the result (|value_state|) of checking the split
  // |dict_value| (which may be NULL) at |pref_path|. |reset_action| indicates
  // whether or not a reset of |value_keys| will occur based on |value_state|
  // and the enforcement level in place.
  virtual void OnSplitPreferenceValidation(
      const std::string& pref_path,
      const base::DictionaryValue* dict_value,
      const std::vector<std::string>& invalid_keys,
      PrefHashStoreTransaction::ValueState value_state,
      TrackedPreferenceHelper::ResetAction reset_action) = 0;
};

#endif  // CHROME_BROWSER_PREFS_TRACKED_TRACKED_PREFERENCE_VALIDATION_DELEGATE_H_
