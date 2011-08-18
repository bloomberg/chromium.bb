// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/util/oauth.h"

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/net/gaia/gaia_constants.h"

namespace browser_sync {

static bool has_local_override = false;
static bool local_override = false;

// TODO(rickcam): Bug(92948): Remove IsUsingOAuth post-ClientLogin
bool IsUsingOAuth() {
  if (has_local_override)
    return local_override;
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableSyncOAuth);
}

// TODO(rickcam): Bug(92948): Remove SyncServiceName post-ClientLogin
const char* SyncServiceName() {
  return IsUsingOAuth() ?
      GaiaConstants::kSyncServiceOAuth :
      GaiaConstants::kSyncService;
}

// TODO(rickcam): Bug(92948): Remove SetIsUsingOAuthForTest post-ClientLogin
void SetIsUsingOAuthForTest(bool is_using_oauth) {
  has_local_override = true;
  local_override = is_using_oauth;
}

}  // namespace browser_sync
