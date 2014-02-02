// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_TRACKED_TRACKED_PREFERENCE_H_
#define CHROME_BROWSER_PREFS_TRACKED_TRACKED_PREFERENCE_H_

namespace base {
class DictionaryValue;
class Value;
}

// A TrackedPreference tracks changes to an individual preference, reporting and
// reacting to them according to preference-specific and browser-wide policies.
class TrackedPreference {
 public:
  virtual ~TrackedPreference() {}

  // Notifies the underlying TrackedPreference about its new |value|.
  virtual void OnNewValue(const base::Value* value) const = 0;

  // Verifies that the value of this TrackedPreference in |pref_store_contents|
  // is valid. Responds to verification failures according to
  // preference-specific and browser-wide policy and reports results to via UMA.
  virtual void EnforceAndReport(
      base::DictionaryValue* pref_store_contents) const = 0;
};

#endif  // CHROME_BROWSER_PREFS_TRACKED_TRACKED_PREFERENCE_H_
