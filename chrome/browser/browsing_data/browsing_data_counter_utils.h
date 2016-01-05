// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_COUNTER_UTILS_H_
#define CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_COUNTER_UTILS_H_

#include "base/strings/string16.h"
#include "chrome/browser/browsing_data/browsing_data_counter.h"

// Whether the browsing data counters experiment is enabled.
bool AreCountersEnabled();

// Constructs the text to be displayed by a counter from the given |result|.
base::string16 GetCounterTextFromResult(
    const BrowsingDataCounter::Result* result);

// Creates a new instance of BrowsingDataCounter that is counting the data
// related to a given deletion preference |pref_name|.
BrowsingDataCounter* CreateCounterForPreference(std::string pref_name);

#endif  // CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_COUNTER_UTILS_H_
