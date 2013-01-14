// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#endif

namespace chrome {
namespace search {

// Configuration options for Embedded Search.
// InstantExtended field trials are named in such a way that we can parse out
// the experiment configuration from the trial's group name in order to give
// us maximum flexability in running experiments.
// Field trials should be named things like "Group7 espv:2 themes:0".
// The first token is always GroupN for some integer N, followed by a
// space-delimited list of key:value pairs which correspond to these flags:
const char kEnableOnThemesFlagName[] = "themes";
const bool kEnableOnThemesDefault = false;

const char kEmbeddedPageVersionFlagName[] = "espv";
const int kEmbeddedPageVersionDefault = 1;

// Constants for the field trial name and group prefix.
const char kInstantExtendedFieldTrialName[] = "InstantExtended";
const char kGroupNumberPrefix[] = "Group";

// If the field trial's group name ends with this string its configuration will
// be ignored and Instant Extended will not be enabled by default.
const char kDisablingSuffix[] = "DISABLED";

// Given a field trial group name in the above format, parses out the group
// number and configuration flags.  Will return a group number of 0 on error.
void GetFieldTrialInfo(const std::string& group_name,
                       FieldTrialFlags* flags,
                       uint64* group_number) {
  if (!EndsWith(group_name, kDisablingSuffix, true) &&
      StartsWithASCII(group_name, kGroupNumberPrefix, true)) {
    // We have a valid trial that starts with "Group" and isn't disabled.
    size_t first_space = group_name.find(" ");
    std::string group_prefix = group_name;
    if (first_space != std::string::npos) {
      // There is a flags section of the group name.  Split that out and parse
      // it.
      group_prefix = group_name.substr(0, first_space);
      base::SplitStringIntoKeyValuePairs(
          group_name.substr(first_space), ':', ' ', flags);
    }
    if (!base::StringToUint64(group_prefix.substr(strlen(kGroupNumberPrefix)),
                              group_number)) {
      // Could not parse group number.
      *group_number = 0;
    }
  }
}

// Given a FieldTrialFlags object, returns the string value of the provided
// flag.
std::string GetStringValueForFlagWithDefault(
    const std::string& flag,
    const std::string& default_value,
    FieldTrialFlags& flags) {
  FieldTrialFlags::const_iterator i;
  for (i = flags.begin(); i != flags.end(); i++) {
    if (i->first == flag)
      return i->second;
  }
  return default_value;
}

// Given a FieldTrialFlags object, returns the uint64 value of the provided
// flag.
uint64 GetUInt64ValueForFlagWithDefault(
    const std::string& flag, uint64 default_value, FieldTrialFlags& flags) {
  uint64 value;
  if (!base::StringToUint64(GetStringValueForFlagWithDefault(flag, "", flags),
                            &value))
    return default_value;
  return value;
}

// Given a FieldTrialFlags object, returns the boolean value of the provided
// flag.
bool GetBoolValueForFlagWithDefault(
    const std::string& flag, bool default_value, FieldTrialFlags& flags) {
  return !!GetUInt64ValueForFlagWithDefault(flag, default_value ? 1 : 0, flags);
}

// Check whether or not the Extended API should be used on the given profile.
bool IsInstantExtendedAPIEnabled(Profile* profile) {
  return EmbeddedSearchPageVersion(profile) != 0;
}

// Determine what embedded search page version to request from the user's
// default search provider.  If 0, the embedded search UI should not be enabled.
// Note that the profile object here isn't const because we need to determine
// whether or not the user has a theme installed as part of this check, and
// that logic requires a non-const profile for whatever reason.
uint64 EmbeddedSearchPageVersion(Profile* profile) {
  // Incognito windows do not currently use the embedded search API.
  if (!profile || profile->IsOffTheRecord())
    return 0;

  // Check Finch field trials.
  FieldTrialFlags flags;
  uint64 group_number = 0;
  base::FieldTrial* trial =
      base::FieldTrialList::Find(kInstantExtendedFieldTrialName);
  if (trial) {
    std::string group_name = trial->group_name();
    GetFieldTrialInfo(group_name, &flags, &group_number);
  }

  if (group_number > 0) {
    uint64 espv = GetUInt64ValueForFlagWithDefault(
        kEmbeddedPageVersionFlagName,
        kEmbeddedPageVersionDefault,
        flags);

    // Check for themes.
    bool has_theme = false;
#if !defined(OS_ANDROID)
    has_theme =
        !ThemeServiceFactory::GetForProfile(profile)->UsingDefaultTheme();
#endif

    bool enable_for_themes =
        GetBoolValueForFlagWithDefault(kEnableOnThemesFlagName,
                                       kEnableOnThemesDefault,
                                       flags);
    if (!has_theme || enable_for_themes)
      return espv;
  }

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableInstantExtendedAPI)) {
    // The user has manually flipped the about:flags switch - give the default
    // UI version.
    return kEmbeddedPageVersionDefault;
  }
  return 0;
}

void EnableInstantExtendedAPIForTesting() {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableInstantExtendedAPI);
}

bool IsQueryExtractionEnabled(Profile* profile) {
#if defined(OS_IOS)
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableQueryExtraction);
#else
  if (!profile || profile->IsOffTheRecord())
    return false;

  // On desktop, query extraction is controlled by the instant-extended-api
  // flag.
  bool enabled = IsInstantExtendedAPIEnabled(profile);

  // Running with --enable-query-extraction but not
  // --enable-instant-extended-api is an error.
  DCHECK(!(CommandLine::ForCurrentProcess()->HasSwitch(
               switches::kEnableQueryExtraction) &&
           !enabled));
  return enabled;
#endif
}

void EnableQueryExtractionForTesting() {
#if defined(OS_IOS)
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableQueryExtraction);
#else
  // On desktop, query extraction is controlled by the instant-extended-api
  // flag.
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableInstantExtendedAPI);
#endif
}

}  // namespace search
}  // namespace chrome
