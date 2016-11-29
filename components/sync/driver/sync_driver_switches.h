// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_SYNC_DRIVER_SWITCHES_H_
#define COMPONENTS_SYNC_DRIVER_SYNC_DRIVER_SWITCHES_H_

#include "base/feature_list.h"

namespace switches {

// Defines all the command-line switches used by sync driver. All switches in
// alphabetical order. The switches should be documented alongside the
// definition of their values in the .cc file.
extern const char kSyncDeferredStartupTimeoutSeconds[];
extern const char kSyncDisableDeferredStartup[];
extern const char kSyncEnableGetUpdateAvoidance[];
extern const char kSyncServiceURL[];
extern const char kSyncShortInitialRetryOverride[];
extern const char kSyncShortNudgeDelayForTest[];

extern const base::Feature kSyncClearDataOnPassphraseEncryption;
extern const base::Feature kSyncUSSDeviceInfo;
extern const base::Feature kSyncUSSAutocomplete;

}  // namespace switches

#endif  // COMPONENTS_SYNC_DRIVER_SYNC_DRIVER_SWITCHES_H_
