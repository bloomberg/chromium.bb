// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_PREF_OBSERVER_MOCK_H_
#define CHROME_BROWSER_PREFS_PREF_OBSERVER_MOCK_H_

#include <string>

#include "base/prefs/public/pref_observer.h"
#include "chrome/browser/prefs/pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::Pointee;
using testing::Property;
using testing::Truly;

// Matcher that checks whether the current value of the preference named
// |pref_name| in |prefs| matches |value|. If |value| is NULL, the matcher
// checks that the value is not set.
MATCHER_P3(PrefValueMatches, prefs, pref_name, value, "") {
  const PrefServiceBase::Preference* pref =
      prefs->FindPreference(pref_name.c_str());
  if (!pref)
    return false;

  const Value* actual_value = pref->GetValue();
  if (!actual_value)
    return value == NULL;
  if (!value)
    return actual_value == NULL;
  return value->Equals(actual_value);
}

// A mock for testing preference notifications and easy setup of expectations.
class PrefObserverMock : public PrefObserver {
 public:
  PrefObserverMock();
  virtual ~PrefObserverMock();

  MOCK_METHOD2(OnPreferenceChanged, void(PrefServiceBase*, const std::string&));

  void Expect(PrefServiceBase* prefs,
              const std::string& pref_name,
              const Value* value);
};

#endif  // CHROME_BROWSER_PREFS_PREF_OBSERVER_MOCK_H_
