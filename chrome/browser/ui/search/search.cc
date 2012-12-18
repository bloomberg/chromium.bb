// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"

namespace chrome {
namespace search {

bool IsInstantExtendedAPIEnabled(const Profile* profile) {
  return EmbeddedSearchPageVersion(profile) != 0;
}

uint64 EmbeddedSearchPageVersion(const Profile* profile) {
  // Incognito windows do not currently use the embedded search API.
  if (!profile || profile->IsOffTheRecord())
    return 0;

  // Check Finch field trials.
  base::FieldTrial* trial = base::FieldTrialList::Find("InstantExtended");
  if (trial && StartsWithASCII(trial->group_name(), "Group", true)) {
    uint64 group_number;
    if (base::StringToUint64(trial->group_name().substr(5), &group_number)) {
      return group_number;
    }
  }

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableInstantExtendedAPI)) {
    // The user has manually flipped the about:flags switch - give the default
    // UI version.
    return 1;
  }
  return 0;
}

void EnableInstantExtendedAPIForTesting() {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableInstantExtendedAPI);
}

bool IsQueryExtractionEnabled(const Profile* profile) {
  return IsInstantExtendedAPIEnabled(profile) ||
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableInstantExtendedAPI);
}

void EnableQueryExtractionForTesting() {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableInstantExtendedAPI);
}

}  // namespace search
}  // namespace chrome
