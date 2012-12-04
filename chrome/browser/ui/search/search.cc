// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search.h"

#include "base/command_line.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"

namespace chrome {
namespace search {

bool IsInstantExtendedAPIEnabled(const Profile* profile) {
  return !profile->IsOffTheRecord() &&
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableInstantExtendedAPI);
}

void EnableInstantExtendedAPIForTesting() {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableInstantExtendedAPI);
}

}  // namespace search
}  // namespace chrome
