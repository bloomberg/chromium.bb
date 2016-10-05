// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_PREFERENCE_VALIDATION_DELEGATE_H_
#define CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_PREFERENCE_VALIDATION_DELEGATE_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/user_prefs/tracked/tracked_preference_validation_delegate.h"

class Profile;

namespace safe_browsing {

class IncidentReceiver;

// A preference validation delegate that adds incidents to a given receiver
// for preference validation failures. The profile for which the delegate
// operates must outlive the delegate itself.
class PreferenceValidationDelegate
    : public TrackedPreferenceValidationDelegate {
 public:
  PreferenceValidationDelegate(
      Profile* profile,
      std::unique_ptr<IncidentReceiver> incident_receiver);
  ~PreferenceValidationDelegate() override;

 private:
  // TrackedPreferenceValidationDelegate methods.
  void OnAtomicPreferenceValidation(
      const std::string& pref_path,
      const base::Value* value,
      PrefHashStoreTransaction::ValueState value_state,
      PrefHashStoreTransaction::ValueState external_validation_value_state,
      bool is_personal) override;
  void OnSplitPreferenceValidation(
      const std::string& pref_path,
      const base::DictionaryValue* dict_value,
      const std::vector<std::string>& invalid_keys,
      const std::vector<std::string>& external_validation_invalid_keys,
      PrefHashStoreTransaction::ValueState value_state,
      PrefHashStoreTransaction::ValueState external_validation_value_state,
      bool is_personal) override;

  Profile* profile_;
  std::unique_ptr<IncidentReceiver> incident_receiver_;

  DISALLOW_COPY_AND_ASSIGN(PreferenceValidationDelegate);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_PREFERENCE_VALIDATION_DELEGATE_H_
