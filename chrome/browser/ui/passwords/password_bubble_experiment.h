// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_PASSWORD_BUBBLE_EXPERIMENT_H_
#define CHROME_BROWSER_UI_PASSWORDS_PASSWORD_BUBBLE_EXPERIMENT_H_

#include "base/macros.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"

class Profile;

namespace password_bubble_experiment {

// Should be called when user dismisses the "Save Password?" dialog. It stores
// the statistics about interactions with the bubble.
void RecordBubbleClosed(
    PrefService* prefs,
    password_manager::metrics_util::UIDismissalReason reason);

// Returns true if the password manager should be referred to as Smart Lock.
// This is only true for signed-in users.
bool IsSmartLockBrandingEnabled(Profile* profile);

}  // namespace password_bubble_experiment

#endif  // CHROME_BROWSER_UI_PASSWORDS_PASSWORD_BUBBLE_EXPERIMENT_H_
