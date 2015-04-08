// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/experimental/supervised_user_filtering_switches.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "chrome/common/chrome_switches.h"

namespace {

enum class SafeSitesState {
  ENABLED,
  DISABLED,
  BLACKLIST_ONLY,
  ONLINE_CHECK_ONLY
};

const char kSafeSitesFieldTrialName[] = "SafeSites";

SafeSitesState GetState() {
  // Note: It's important to query the field trial state first, to ensure that
  // UMA reports the correct group.
  std::string trial_group =
      base::FieldTrialList::FindFullName(kSafeSitesFieldTrialName);

  std::string arg = base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      switches::kSupervisedUserSafeSites);
  if (!arg.empty()) {
    if (arg == "enabled")
      return SafeSitesState::ENABLED;
    else if (arg == "disabled")
      return SafeSitesState::DISABLED;
    else if (arg == "blacklist-only")
      return SafeSitesState::BLACKLIST_ONLY;
    else if (arg == "online-check-only")
      return SafeSitesState::ONLINE_CHECK_ONLY;

    LOG(WARNING) << "Invalid value \"" << arg << "\" specified for flag \""
                 << switches::kSupervisedUserSafeSites
                 << "\", defaulting to \"disabled\"";
    return SafeSitesState::DISABLED;
  }

  // If no cmdline arg is specified, evaluate the field trial.
  if (trial_group == "Disabled")
    return SafeSitesState::DISABLED;
  else if (trial_group == "BlacklistOnly")
    return SafeSitesState::BLACKLIST_ONLY;
  else if (trial_group == "OnlineCheckOnly")
    return SafeSitesState::ONLINE_CHECK_ONLY;
  else
    return SafeSitesState::ENABLED;
}

}  // namespace

namespace supervised_users {

bool IsSafeSitesBlacklistEnabled() {
  SafeSitesState state = GetState();
  return state == SafeSitesState::ENABLED ||
         state == SafeSitesState::BLACKLIST_ONLY;
}

bool IsSafeSitesOnlineCheckEnabled() {
  SafeSitesState state = GetState();
  return state == SafeSitesState::ENABLED ||
         state == SafeSitesState::ONLINE_CHECK_ONLY;
}

}  // namespace supervised_users
