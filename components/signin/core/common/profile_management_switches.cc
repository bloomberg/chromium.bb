// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/common/profile_management_switches.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "components/signin/core/common/signin_switches.h"

namespace {

const char kNewProfileManagementFieldTrialName[] = "NewProfileManagement";

// Different state of new profile management/identity consistency.  The code
// below assumes the order of the values in this enum.  That is, new profile
// management is included in consistent identity.
enum State {
  STATE_NONE,
  STATE_NEW_PROFILE_MANAGEMENT,
  STATE_ACCOUNT_CONSISTENCY
};

State GetProcessState() {
  // Get the full name of the field trial so that the underlying mechanism
  // is properly initialize.
  std::string trial_type =
      base::FieldTrialList::FindFullName(kNewProfileManagementFieldTrialName);

  // Find the state of both command line args.
  bool is_new_profile_management =
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kNewProfileManagement);
  bool is_consistent_identity =
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableAccountConsistency);

  State state = STATE_NONE;

  // If both command line args are set, disable the field trial completely
  // since the assigned group is undefined.  Otherwise use the state of the
  // command line flag specified.  If neither command line arg is specified,
  // see if the group was set from the server.
  if (is_new_profile_management && is_consistent_identity) {
    base::FieldTrial* field_trial =
        base::FieldTrialList::Find(kNewProfileManagementFieldTrialName);
    if (field_trial)
      field_trial->Disable();

    return STATE_ACCOUNT_CONSISTENCY;
  } else if (is_new_profile_management) {
    return STATE_NEW_PROFILE_MANAGEMENT;
  } else if (is_consistent_identity) {
    return STATE_ACCOUNT_CONSISTENCY;
  }

  if (state == STATE_NONE && !trial_type.empty()) {
    if (trial_type == "Enabled") {
      state = STATE_NEW_PROFILE_MANAGEMENT;
    } else if (trial_type == "AccountConsistency") {
      state = STATE_ACCOUNT_CONSISTENCY;
    }
  }

  return state;
}

bool CheckFlag(std::string command_switch, State min_state) {
  // Individiual flag settings take precedence.
  if (CommandLine::ForCurrentProcess()->HasSwitch(command_switch))
    return true;

  return GetProcessState() >= min_state;
}

}  // namespace

namespace switches {

bool IsEnableAccountConsistency() {
  return GetProcessState() >= STATE_ACCOUNT_CONSISTENCY;
}

bool IsEnableWebBasedSignin() {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableWebBasedSignin) && !IsNewProfileManagement();
}

bool IsExtensionsMultiAccount() {
  return CheckFlag(switches::kExtensionsMultiAccount,
                   STATE_NEW_PROFILE_MANAGEMENT);
}

bool IsFastUserSwitching() {
  bool use_mirror_promo_menu =
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kNewAvatarMenu) &&
      !IsNewProfileManagement();
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kFastUserSwitching) || use_mirror_promo_menu;
}

bool IsGoogleProfileInfo() {
  return CheckFlag(switches::kGoogleProfileInfo,
                   STATE_NEW_PROFILE_MANAGEMENT);
}

bool IsNewAvatarMenu() {
  bool is_new_avatar_menu =
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kNewAvatarMenu);
  return is_new_avatar_menu || IsNewProfileManagement();
}

bool IsNewProfileManagement() {
  return GetProcessState() >= STATE_NEW_PROFILE_MANAGEMENT;
}

bool IsNewProfileManagementPreviewEnabled() {
  bool is_new_avatar_menu =
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kNewAvatarMenu);
  return is_new_avatar_menu && IsNewProfileManagement();
}

}  // namespace switches
