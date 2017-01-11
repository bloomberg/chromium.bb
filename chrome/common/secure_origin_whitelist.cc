// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/secure_origin_whitelist.h"

#include "base/command_line.h"
#include "base/strings/string_split.h"
#include "chrome/common/chrome_switches.h"
#include "extensions/common/constants.h"

std::vector<GURL> GetSecureOriginWhitelist() {
  std::vector<GURL> origins;
  // If kUnsafelyTreatInsecureOriginAsSecure option is given and
  // kUserDataDir is present, add the given origins as trustworthy
  // for whitelisting.
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kUnsafelyTreatInsecureOriginAsSecure) &&
      command_line.HasSwitch(switches::kUserDataDir)) {
    std::string origins_str = command_line.GetSwitchValueASCII(
        switches::kUnsafelyTreatInsecureOriginAsSecure);
    for (const std::string& origin : base::SplitString(
             origins_str, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL))
      origins.push_back(GURL(origin));
  }

  return origins;
}

std::set<std::string> GetSchemesBypassingSecureContextCheckWhitelist() {
  std::set<std::string> schemes;
  schemes.insert(extensions::kExtensionScheme);
  return schemes;
}
