// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_BUBBLE_EXPERIMENT_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_BUBBLE_EXPERIMENT_H_

#include "base/macros.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"

class PrefService;

namespace sync_driver {
class SyncService;
}

namespace password_bubble_experiment {

// Should be called when user dismisses the "Save Password?" dialog. It stores
// the statistics about interactions with the bubble.
void RecordBubbleClosed(
    PrefService* prefs,
    password_manager::metrics_util::UIDismissalReason reason);

// Returns true if the password manager should be referred to as Smart Lock.
// This is only true for signed-in users.
bool IsSmartLockBrandingEnabled(const sync_driver::SyncService* sync_service);

}  // namespace password_bubble_experiment

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_BUBBLE_EXPERIMENT_H_
