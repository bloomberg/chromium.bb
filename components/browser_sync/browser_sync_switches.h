// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_SYNC_BROWSER_SYNC_SWITCHES_H_
#define COMPONENTS_BROWSER_SYNC_BROWSER_SYNC_SWITCHES_H_

// TODO(crbug.com/896303): Have clients that need it include this directly, and
// get rid of this temporary redirect.
#include "components/sync/driver/sync_driver_switches.h"

namespace switches {

extern const char kDisableSyncTypes[];
extern const char kEnableLocalSyncBackend[];
extern const char kLocalSyncBackendDir[];

}  // namespace switches

#endif  // COMPONENTS_BROWSER_SYNC_BROWSER_SYNC_SWITCHES_H_
