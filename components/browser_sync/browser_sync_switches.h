// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the command-line switches used by //components/browser_sync.

#ifndef COMPONENTS_BROWSER_SYNC_BROWSER_SYNC_SWITCHES_H_
#define COMPONENTS_BROWSER_SYNC_BROWSER_SYNC_SWITCHES_H_

namespace switches {

extern const char kDisableSync[];
extern const char kDisableSyncTypes[];
extern const char kEnableLocalSyncBackend[];
extern const char kLocalSyncBackendDir[];

// Returns whether sync is allowed to run based on command-line switches.
// Profile::IsSyncAllowed() is probably a better signal than this function.
// This function can be called from any thread, and the implementation doesn't
// assume it's running on the UI thread.
bool IsSyncAllowedByFlag();

}  // namespace switches

#endif  // COMPONENTS_BROWSER_SYNC_BROWSER_SYNC_SWITCHES_H_
