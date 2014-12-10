// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_URL_COLLECTION_EXPERIMENT_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_URL_COLLECTION_EXPERIMENT_H_

namespace user_prefs {
class PrefRegistrySyncable;
}

class PrefService;

// These functions implement the algorithms according to which the "Allow to
// collect URL?" bubble is shown to user.
namespace password_manager {
namespace urls_collection_experiment {

void RegisterPrefs(user_prefs::PrefRegistrySyncable* registry);

// Based on |prefs| and experiment settings, decides whether to show the
// "Allow to collect URL?" bubble and should be called before showing it.
// The default value is false.
bool ShouldShowBubble(PrefService* prefs);

// Should be called when user dismisses the "Allow to collect URL?" bubble.
// It stores the statistics about interactions with the bubble in |prefs|.
void RecordBubbleClosed(PrefService* prefs);

// The name of the finch experiment controlling the algorithm.
extern const char kExperimentName[];

}  // namespace urls_collection_experiment
}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_URL_COLLECTION_EXPERIMENT_H_
