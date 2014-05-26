// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_TRACKED_TRACKED_ATOMIC_PREFERENCE_H_
#define CHROME_BROWSER_PREFS_TRACKED_TRACKED_ATOMIC_PREFERENCE_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/prefs/pref_hash_filter.h"
#include "chrome/browser/prefs/tracked/tracked_preference.h"
#include "chrome/browser/prefs/tracked/tracked_preference_helper.h"

class TrackedPreferenceValidationDelegate;

// A TrackedAtomicPreference is tracked as a whole. A hash is stored for its
// entire value and it is entirely reset on mismatch. An optional delegate is
// notified of the status of the preference during enforcement.
class TrackedAtomicPreference : public TrackedPreference {
 public:
  TrackedAtomicPreference(const std::string& pref_path,
                          size_t reporting_id,
                          size_t reporting_ids_count,
                          PrefHashFilter::EnforcementLevel enforcement_level,
                          TrackedPreferenceValidationDelegate* delegate);

  // TrackedPreference implementation.
  virtual void OnNewValue(const base::Value* value,
                          PrefHashStoreTransaction* transaction) const OVERRIDE;
  virtual bool EnforceAndReport(
      base::DictionaryValue* pref_store_contents,
      PrefHashStoreTransaction* transaction) const OVERRIDE;

 private:
  const std::string pref_path_;
  const TrackedPreferenceHelper helper_;
  TrackedPreferenceValidationDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(TrackedAtomicPreference);
};

#endif  // CHROME_BROWSER_PREFS_TRACKED_TRACKED_ATOMIC_PREFERENCE_H_
