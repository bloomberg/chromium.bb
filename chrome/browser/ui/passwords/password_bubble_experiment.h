// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_PASSWORD_BUBBLE_EXPERIMENT_H_
#define CHROME_BROWSER_UI_PASSWORDS_PASSWORD_BUBBLE_EXPERIMENT_H_

#include "base/macros.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"

namespace user_prefs {
class PrefRegistrySyncable;
}

class PrefService;

// These functions handle the algorithms according to which the "Save password?"
// bubble is shown to user.
namespace password_bubble_experiment {

void RegisterPrefs(user_prefs::PrefRegistrySyncable* registry);

// The decision is made based on the "PasswordBubbleAlgorithm" finch experiment.
// The default value is true.
// It should be called before showing the "Save Password?" dialog.
bool ShouldShowBubble(PrefService* prefs);

// Should be called when user dismisses the "Save Password?" dialog. It stores
// the statistics about interactions with the bubble.
void RecordBubbleClosed(
    PrefService* prefs,
    password_manager::metrics_util::UIDismissalReason reason);

// The name of the finch experiment controlling the algorithm.
extern const char kExperimentName[];

// The group name for the time based algorithm.
extern const char kGroupTimeSpanBased[];

// The group name for the probability algorithm.
extern const char kGroupProbabilityBased[];

// For "Probability" group. The additional "Saves" to be added to the model.
extern const char kParamProbabilityFakeSaves[];

// For "Probability" group. The interaction history length.
extern const char kParamProbabilityInteractionsCount[];

// For "TimeSpan" group. The time span until the nope counter is zeroed.
extern const char kParamTimeSpan[];

// For "TimeSpan" group. The nopes threshold.
extern const char kParamTimeSpanNopeThreshold[];

}  // namespace password_bubble_experiment

#endif  // CHROME_BROWSER_UI_PASSWORDS_PASSWORD_BUBBLE_EXPERIMENT_H_
