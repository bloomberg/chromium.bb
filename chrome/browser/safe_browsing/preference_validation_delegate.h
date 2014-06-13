// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_PREFERENCE_VALIDATION_DELEGATE_H_
#define CHROME_BROWSER_SAFE_BROWSING_PREFERENCE_VALIDATION_DELEGATE_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/prefs/tracked/tracked_preference_validation_delegate.h"
#include "chrome/browser/safe_browsing/add_incident_callback.h"

namespace safe_browsing {

// A preference validation delegate that adds incidents to a given receiver
// for preference validation failures.
class PreferenceValidationDelegate
    : public TrackedPreferenceValidationDelegate {
 public:
  explicit PreferenceValidationDelegate(
      const AddIncidentCallback& add_incident);
  virtual ~PreferenceValidationDelegate();

 private:
  // TrackedPreferenceValidationDelegate methods.
  virtual void OnAtomicPreferenceValidation(
      const std::string& pref_path,
      const base::Value* value,
      PrefHashStoreTransaction::ValueState value_state,
      TrackedPreferenceHelper::ResetAction reset_action) OVERRIDE;
  virtual void OnSplitPreferenceValidation(
      const std::string& pref_path,
      const base::DictionaryValue* dict_value,
      const std::vector<std::string>& invalid_keys,
      PrefHashStoreTransaction::ValueState value_state,
      TrackedPreferenceHelper::ResetAction reset_action) OVERRIDE;

  AddIncidentCallback add_incident_;

  DISALLOW_COPY_AND_ASSIGN(PreferenceValidationDelegate);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_PREFERENCE_VALIDATION_DELEGATE_H_
