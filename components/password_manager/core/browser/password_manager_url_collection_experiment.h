// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_URL_COLLECTION_EXPERIMENT_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_URL_COLLECTION_EXPERIMENT_H_

#include "base/time/time.h"

namespace user_prefs {
class PrefRegistrySyncable;
}

class PrefService;

// These functions implement the algorithms according to which the "Allow to
// collect URL?" bubble is shown to user.
namespace password_manager {
namespace urls_collection_experiment {

void RegisterPrefs(user_prefs::PrefRegistrySyncable* registry);

// Implements an algorithm determining when the period starts, in which "Allow
// to collect URL?" bubble can be shown.
base::Time DetermineStartOfActivityPeriod(PrefService* prefs,
                                          int experiment_length_in_days);
// Based on |prefs| and experiment settings, decides whether to show the
// "Allow to collect URL?" bubble and should be called before showing it.
// The default value is false.
bool ShouldShowBubble(PrefService* prefs);

// Should be called when the "Allow to collect URL?" bubble was shown.
// It stores the fact that bubble was shown in |prefs|.
void RecordBubbleOpened(PrefService* prefs);

// The name of the finch experiment controlling the algorithm.
extern const char kExperimentName[];

// The name of the experiment parameter, value of which determines determines
// how long the experiment is active.
extern const char kParamExperimentLengthInDays[];

// The bubble is shown only once and only within a certain period. The length of
// the period is the value of the experiment parameter |kParamTimePeriodInDays|.
extern const char kParamActivePeriodInDays[];
/// The name of the experiment parameter, value of which defines whether
// the bubble should appear or not.
extern const char kParamBubbleStatus[];

}  // namespace urls_collection_experiment
}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_URL_COLLECTION_EXPERIMENT_H_
