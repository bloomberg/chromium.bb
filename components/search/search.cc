// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search/search.h"

#include <stddef.h>

#include "base/metrics/field_trial.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "components/google/core/browser/google_util.h"
#include "components/search_engines/template_url.h"
#include "url/gurl.h"

namespace search {

namespace {

// Configuration options for Embedded Search.
// EmbeddedSearch field trials are named in such a way that we can parse out
// the experiment configuration from the trial's group name in order to give
// us maximum flexability in running experiments.
// Field trial groups should be named things like "Group7 espv:2 instant:1".
// The first token is always GroupN for some integer N, followed by a
// space-delimited list of key:value pairs which correspond to these flags:
const char kEmbeddedPageVersionFlagName[] = "espv";

#if defined(OS_IOS) || defined(OS_ANDROID)
const uint64_t kEmbeddedPageVersionDefault = 1;
#else
const uint64_t kEmbeddedPageVersionDefault = 2;
#endif

// Constants for the field trial name and group prefix.
// Note in M30 and below this field trial was named "InstantExtended" and in
// M31 was renamed to EmbeddedSearch for clarity and cleanliness.  Since we
// can't easilly sync up Finch configs with the pushing of this change to
// Dev & Canary, for now the code accepts both names.
// TODO(dcblack): Remove the InstantExtended name once M31 hits the Beta
// channel.
const char kInstantExtendedFieldTrialName[] = "InstantExtended";
const char kEmbeddedSearchFieldTrialName[] = "EmbeddedSearch";

// If the field trial's group name ends with this string its configuration will
// be ignored and Instant Extended will not be enabled by default.
const char kDisablingSuffix[] = "DISABLED";

}  // namespace

bool IsInstantExtendedAPIEnabled() {
#if defined(OS_IOS) || defined(OS_ANDROID)
  return false;
#else
  return true;
#endif
}

// Determine what embedded search page version to request from the user's
// default search provider. If 0, the embedded search UI should not be enabled.
uint64_t EmbeddedSearchPageVersion() {
  FieldTrialFlags flags;
  if (GetFieldTrialInfo(&flags)) {
    return GetUInt64ValueForFlagWithDefault(kEmbeddedPageVersionFlagName,
                                            kEmbeddedPageVersionDefault,
                                            flags);
  }
  return kEmbeddedPageVersionDefault;
}

bool GetFieldTrialInfo(FieldTrialFlags* flags) {
  // Get the group name.  If the EmbeddedSearch trial doesn't exist, look for
  // the older InstantExtended name.
  std::string group_name = base::FieldTrialList::FindFullName(
      kEmbeddedSearchFieldTrialName);
  if (group_name.empty()) {
    group_name = base::FieldTrialList::FindFullName(
        kInstantExtendedFieldTrialName);
  }

  if (base::EndsWith(group_name, kDisablingSuffix,
                     base::CompareCase::SENSITIVE))
    return false;

  // We have a valid trial that isn't disabled. Extract the flags.
  std::string group_prefix(group_name);
  size_t first_space = group_name.find(" ");
  if (first_space != std::string::npos) {
    // There is a flags section of the group name. Split that out and parse it.
    group_prefix = group_name.substr(0, first_space);
    if (!base::SplitStringIntoKeyValuePairs(group_name.substr(first_space),
                                            ':', ' ', flags)) {
      // Failed to parse the flags section. Assume the whole group name is
      // invalid.
      return false;
    }
  }
  return true;
}

// Given a FieldTrialFlags object, returns the uint64_t value of the provided
// flag.
uint64_t GetUInt64ValueForFlagWithDefault(const std::string& flag,
                                          uint64_t default_value,
                                          const FieldTrialFlags& flags) {
  for (const std::pair<std::string, std::string>& flag_and_value : flags) {
    if (flag_and_value.first == flag) {
      const std::string& str_value = flag_and_value.second;
      uint64_t value;
      if (base::StringToUint64(str_value, &value))
        return value;
    }
  }
  return default_value;
}

std::string InstantExtendedEnabledParam() {
  return std::string(google_util::kInstantExtendedAPIParam) + "=" +
         base::Uint64ToString(EmbeddedSearchPageVersion()) + "&";
}

std::string ForceInstantResultsParam(bool for_prerender) {
  return (for_prerender || !IsInstantExtendedAPIEnabled()) ? "ion=1&"
                                                           : std::string();
}

}  // namespace search
