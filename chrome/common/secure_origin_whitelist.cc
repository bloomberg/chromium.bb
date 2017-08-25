// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/secure_origin_whitelist.h"

#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_split.h"
#include "chrome/common/chrome_switches.h"
#include "extensions/common/constants.h"

std::vector<GURL> GetSecureOriginWhitelist() {
  std::vector<GURL> origins;
  // If kUnsafelyTreatInsecureOriginAsSecure option is given, then treat the
  // value as a comma-separated list of origins:
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kUnsafelyTreatInsecureOriginAsSecure)) {
    std::string origins_str = command_line.GetSwitchValueASCII(
        switches::kUnsafelyTreatInsecureOriginAsSecure);
    for (const std::string& origin : base::SplitString(
             origins_str, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL))
      origins.push_back(GURL(origin));
  }

  UMA_HISTOGRAM_COUNTS_100("Security.TreatInsecureOriginAsSecure",
                           origins.size());

  return origins;
}

std::set<std::string> GetSchemesBypassingSecureContextCheckWhitelist() {
  std::set<std::string> schemes;
  schemes.insert(extensions::kExtensionScheme);
  return schemes;
}
