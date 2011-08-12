// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/util/oauth.h"

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/net/gaia/gaia_constants.h"

namespace browser_sync {

bool IsUsingOAuth() {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableSyncOAuth);
}

const char* SyncServiceName() {
  return IsUsingOAuth() ?
      GaiaConstants::kSyncServiceOAuth :
      GaiaConstants::kSyncService;
}

}  // namespace browser_sync
