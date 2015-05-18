// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/secure_origin_whitelist.h"

#include <vector>

#include "base/command_line.h"
#include "base/strings/string_split.h"
#include "chrome/common/chrome_switches.h"

void GetSecureOriginWhitelist(std::set<GURL>* origins) {
  // If kUnsafelyTreatInsecureOriginAsSecure option is given and
  // kUserDataDir is present, add the given origins as trustworthy
  // for whitelisting.
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kUnsafelyTreatInsecureOriginAsSecure) &&
      command_line.HasSwitch(switches::kUserDataDir)) {
    std::vector<std::string> given_origins;
    base::SplitString(command_line.GetSwitchValueASCII(
        switches::kUnsafelyTreatInsecureOriginAsSecure), ',', &given_origins);
    for (const auto& origin : given_origins)
      origins->insert(GURL(origin));
  }
}
