// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_BROWSING_DATA_UTILS_H_
#define COMPONENTS_BROWSING_DATA_BROWSING_DATA_UTILS_H_

#include "base/strings/string16.h"
#include "base/time/time.h"

namespace browsing_data {

// Browsing data types as seen in the Android UI.
// TODO(msramek): Reuse this enum as the canonical representation of the
// user-facing browsing data types in the Desktop UI as well.
//
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser
enum BrowsingDataType {
  HISTORY,
  CACHE,
  COOKIES,
  PASSWORDS,
  FORM_DATA,
  BOOKMARKS,
  NUM_TYPES
};

// Time period ranges available when doing browsing data removals.
//
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser
enum TimePeriod {
  LAST_HOUR = 0,
  LAST_DAY,
  LAST_WEEK,
  FOUR_WEEKS,
  EVERYTHING,
  TIME_PERIOD_LAST = EVERYTHING
};

// Calculate the begin time for the deletion range specified by |time_period|.
base::Time CalculateBeginDeleteTime(TimePeriod time_period);

}  // namespace browsing_data

#endif  // COMPONENTS_BROWSING_DATA_BROWSING_DATA_UTILS_H_
