// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/switch_utils.h"

#include "base/basictypes.h"
#include "chrome/common/chrome_switches.h"

namespace switches {

// Switches enumerated here will be removed when a background instance of
// Chrome restarts itself. If your key is designed to only be used once,
// or if it does not make sense when restarting a background instance to
// pick up an automatic update, be sure to add it to this list.
const char* const kSwitchesToRemoveOnAutorestart[] = {
  switches::kApp,
  switches::kAppId,
  switches::kForceFirstRun,
  switches::kMakeDefaultBrowser,
  switches::kNoStartupWindow,
  switches::kRestoreLastSession,
  switches::kShowAppList,
};

void RemoveSwitchesForAutostart(
    std::map<std::string, CommandLine::StringType>* switch_list) {
  for (size_t i = 0; i < arraysize(kSwitchesToRemoveOnAutorestart); ++i)
    switch_list->erase(kSwitchesToRemoveOnAutorestart[i]);
}

}  // namespace switches
