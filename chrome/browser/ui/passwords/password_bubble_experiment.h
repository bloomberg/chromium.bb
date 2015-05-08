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
class Profile;

// These functions handle the algorithms according to which the "Save password?"
// bubble shows "No thanks" or "Never for this site" button default.
namespace password_bubble_experiment {

void RegisterPrefs(user_prefs::PrefRegistrySyncable* registry);

// Returns true if "Never for this site" should be the default negative option.
// Otherwise it's "No thanks". The default value is false.
// It should be called before showing the "Save Password?" dialog.
bool ShouldShowNeverForThisSiteDefault(PrefService* prefs);

// Should be called when user dismisses the "Save Password?" dialog. It stores
// the statistics about interactions with the bubble.
void RecordBubbleClosed(
    PrefService* prefs,
    password_manager::metrics_util::UIDismissalReason reason);

// Returns true if the Save bubble should mention Smart Lock instead of Chrome.
// This is only true for signed-in users.
bool IsEnabledSmartLockBranding(Profile* profile);

// The name of the finch experiment controlling the algorithm.
extern const char kExperimentName[];

// The name of the finch parameter. It signifies the consecutive nopes
// threshold after which the user sees "Never for this site" button by default.
extern const char kParamNopeThreshold[];

}  // namespace password_bubble_experiment

#endif  // CHROME_BROWSER_UI_PASSWORDS_PASSWORD_BUBBLE_EXPERIMENT_H_
