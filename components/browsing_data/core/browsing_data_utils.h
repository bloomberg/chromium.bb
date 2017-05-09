// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_CORE_BROWSING_DATA_UTILS_H_
#define COMPONENTS_BROWSING_DATA_CORE_BROWSING_DATA_UTILS_H_

#include "base/strings/string16.h"
#include "base/time/time.h"
#include "components/browsing_data/core/clear_browsing_data_tab.h"
#include "components/browsing_data/core/counters/browsing_data_counter.h"

namespace browsing_data {

// Browsing data types as seen in the Android UI.
// TODO(msramek): Reuse this enum as the canonical representation of the
// user-facing browsing data types in the Desktop UI as well.
//
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.browsing_data
enum class BrowsingDataType {
  HISTORY,
  CACHE,
  COOKIES,
  PASSWORDS,
  FORM_DATA,
  BOOKMARKS,
  SITE_SETTINGS,
  NUM_TYPES
};

// Time period ranges available when doing browsing data removals.
//
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.browsing_data
enum class TimePeriod {
  LAST_HOUR = 0,
  LAST_DAY,
  LAST_WEEK,
  FOUR_WEEKS,
  ALL_TIME,
  TIME_PERIOD_LAST = ALL_TIME
};

// Calculate the begin time for the deletion range specified by |time_period|.
base::Time CalculateBeginDeleteTime(TimePeriod time_period);

// Calculate the end time for the deletion range specified by |time_period|.
base::Time CalculateEndDeleteTime(TimePeriod time_period);

// Records the UMA action of UI-triggered data deletion for |time_period|.
void RecordDeletionForPeriod(TimePeriod time_period);

// Constructs the text to be displayed by a counter from the given |result|.
// Currently this can only be used for counters for which the Result is defined
// in components/browsing_data/core/counters.
base::string16 GetCounterTextFromResult(
    const BrowsingDataCounter::Result* result);

// Returns the preference that stores the time period.
const char* GetTimePeriodPreferenceName(
    ClearBrowsingDataTab clear_browsing_data_tab);

// Copies the name of the deletion preference corresponding to the given
// |data_type| to |out_pref|. Returns false if no such preference exists.
bool GetDeletionPreferenceFromDataType(
    BrowsingDataType data_type,
    ClearBrowsingDataTab clear_browsing_data_tab,
    std::string* out_pref);

// Copies the deletion preferences for timeperiod, cache, history and cookies
// to a separate preferences that are used to on the basic CBD tab.
// This only happens the first time this method is called.
void MigratePreferencesToBasic(PrefService* prefs);

}  // namespace browsing_data

#endif  // COMPONENTS_BROWSING_DATA_CORE_BROWSING_DATA_UTILS_H_
