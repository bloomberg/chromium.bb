// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/secure_origin_whitelist.h"

#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_split.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "extensions/common/constants.h"
#include "url/gurl.h"

namespace secure_origin_whitelist {

std::vector<url::Origin> GetWhitelist() {
  std::vector<url::Origin> origins;
  // If kUnsafelyTreatInsecureOriginAsSecure option is given, then treat the
  // value as a comma-separated list of origins:
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kUnsafelyTreatInsecureOriginAsSecure)) {
    std::string origins_str = command_line.GetSwitchValueASCII(
        switches::kUnsafelyTreatInsecureOriginAsSecure);
    for (const std::string& origin_str : base::SplitString(
             origins_str, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
      // Drop .unique() origins, as they are unequal to any other origins.
      url::Origin origin(url::Origin::Create(GURL(origin_str)));
      if (!origin.unique())
        origins.push_back(std::move(origin));
    }
  }

  UMA_HISTOGRAM_COUNTS_100("Security.TreatInsecureOriginAsSecure",
                           origins.size());

  return origins;
}

std::set<std::string> GetSchemesBypassingSecureContextCheck() {
  std::set<std::string> schemes;
  schemes.insert(extensions::kExtensionScheme);
  return schemes;
}

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kUnsafelyTreatInsecureOriginAsSecure,
                               /* default_value */ "");
}

}  // namespace secure_origin_whitelist
