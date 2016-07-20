// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_COUNTER_UTILS_H_
#define CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_COUNTER_UTILS_H_

#include "base/strings/string16.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/browsing_data/core/counters/browsing_data_counter.h"

class Profile;

// Whether the browsing data counters experiment is enabled.
bool AreCountersEnabled();

// Constructs the text to be displayed by a counter from the given |result|.
base::string16 GetCounterTextFromResult(
    const browsing_data::BrowsingDataCounter::Result* result);

// Copies the name of the deletion preference corresponding to the given
// |data_type| to |out_pref|. Returns false if no such preference exists.
bool GetDeletionPreferenceFromDataType(
    browsing_data::BrowsingDataType data_type,
    std::string* out_pref);

#endif  // CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_COUNTER_UTILS_H_
