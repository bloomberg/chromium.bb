// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_PREF_OBSERVER_MOCK_H_
#define CHROME_BROWSER_PREFS_PREF_OBSERVER_MOCK_H_
#pragma once

#include <string>

#include "chrome/browser/prefs/pref_service.h"
#include "content/common/notification_details.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_source.h"
#include "content/common/notification_type.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::Pointee;
using testing::Property;
using testing::Truly;

// Matcher that checks whether the current value of the preference named
// |pref_name| in |prefs| matches |value|. If |value| is NULL, the matcher
// checks that the value is not set.
MATCHER_P3(PrefValueMatches, prefs, pref_name, value, "") {
  const PrefService::Preference* pref =
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
class PrefObserverMock : public NotificationObserver {
 public:
  PrefObserverMock();
  virtual ~PrefObserverMock();

  MOCK_METHOD3(Observe, void(NotificationType type,
                             const NotificationSource& source,
                             const NotificationDetails& details));

  void Expect(const PrefService* prefs,
              const std::string& pref_name,
              const Value* value);
};

#endif  // CHROME_BROWSER_PREFS_PREF_OBSERVER_MOCK_H_
