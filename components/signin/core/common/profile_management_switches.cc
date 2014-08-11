// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/common/profile_management_switches.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "build/build_config.h"
#include "components/signin/core/common/signin_switches.h"

namespace {

const char kNewProfileManagementFieldTrialName[] = "NewProfileManagement";

// Different state of new profile management/identity consistency.  The code
// below assumes the order of the values in this enum.  That is, new profile
// management is included in consistent identity.
enum State {
  STATE_OLD_AVATAR_MENU,
  STATE_NEW_AVATAR_MENU,
  STATE_NEW_PROFILE_MANAGEMENT,
  STATE_ACCOUNT_CONSISTENCY,
};

State GetProcessState() {
  // Find the state of both command line args.
  bool is_new_avatar_menu =
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableNewAvatarMenu);
  bool is_new_profile_management =
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableNewProfileManagement);
  bool is_consistent_identity =
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableAccountConsistency);
  bool not_new_avatar_menu =
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableNewAvatarMenu);
  bool not_new_profile_management =
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableNewProfileManagement);
  bool not_consistent_identity =
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableAccountConsistency);
  int count_args = (is_new_avatar_menu ? 1 : 0) +
      (is_new_profile_management ? 1 : 0) +
      (is_consistent_identity ? 1 : 0) +
      (not_new_avatar_menu ? 1 : 0) +
      (not_new_profile_management ? 1 : 0) +
      (not_consistent_identity ? 1 : 0);
  bool invalid_commandline = count_args > 1;

  // At most only one of the command line args should be specified, otherwise
  // the finch group assignment is undefined.  If this is the case, disable
  // the field trial so that data is not collected in the wrong group.
  std::string trial_type;
  if (invalid_commandline) {
    base::FieldTrial* field_trial =
        base::FieldTrialList::Find(kNewProfileManagementFieldTrialName);
    if (field_trial)
      field_trial->Disable();

    trial_type.clear();
  } else {
    // Since the experiment is not being disabled, get the full name of the
    // field trial which will initialize the underlying mechanism.
    trial_type =
        base::FieldTrialList::FindFullName(kNewProfileManagementFieldTrialName);
  }

  // Forcing the old avatar menu takes precedent over other args.
  // Enable command line args take precedent over disable command line args.
  // Consistent identity args take precedent over new profile management args.
  if (not_new_avatar_menu) {
    return STATE_OLD_AVATAR_MENU;
  } else if (is_consistent_identity) {
    return STATE_ACCOUNT_CONSISTENCY;
  } else if (is_new_profile_management) {
    return STATE_NEW_PROFILE_MANAGEMENT;
  } else if (is_new_avatar_menu) {
    return STATE_NEW_AVATAR_MENU;
  } else if (not_new_profile_management) {
    return STATE_OLD_AVATAR_MENU;
  } else if (not_consistent_identity) {
    return STATE_OLD_AVATAR_MENU;
  }

  // Set the default state
#if defined(OS_ANDROID)
  State state = STATE_ACCOUNT_CONSISTENCY;
#else
  State state = STATE_OLD_AVATAR_MENU;
#endif

  if (!trial_type.empty()) {
    if (trial_type == "Enabled") {
      state = STATE_NEW_PROFILE_MANAGEMENT;
    } else if (trial_type == "AccountConsistency") {
      state = STATE_ACCOUNT_CONSISTENCY;
    } else if (trial_type == "NewAvatarMenu") {
      state = STATE_NEW_AVATAR_MENU;
    } else {
      state = STATE_OLD_AVATAR_MENU;
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
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kFastUserSwitching);
}

bool IsGoogleProfileInfo() {
  return IsNewAvatarMenu() ||
      CheckFlag(switches::kGoogleProfileInfo, STATE_OLD_AVATAR_MENU);
}

bool IsNewAvatarMenu() {
  return GetProcessState() >= STATE_NEW_AVATAR_MENU;
}

bool IsNewProfileManagement() {
  return GetProcessState() >= STATE_NEW_PROFILE_MANAGEMENT;
}

bool IsNewProfileManagementPreviewEnabled() {
  // No promotion to Enable Account Consistency.
  return false;
}

void EnableNewAvatarMenuForTesting(base::CommandLine* command_line) {
  command_line->AppendSwitch(switches::kEnableNewAvatarMenu);
  DCHECK(!command_line->HasSwitch(switches::kDisableNewAvatarMenu));
}

void DisableNewAvatarMenuForTesting(base::CommandLine* command_line) {
  command_line->AppendSwitch(switches::kDisableNewAvatarMenu);
  DCHECK(!command_line->HasSwitch(switches::kEnableNewAvatarMenu));
}

void EnableNewProfileManagementForTesting(base::CommandLine* command_line) {
  command_line->AppendSwitch(switches::kEnableNewProfileManagement);
  DCHECK(!command_line->HasSwitch(switches::kDisableNewProfileManagement));
}

void EnableAccountConsistencyForTesting(base::CommandLine* command_line) {
  command_line->AppendSwitch(switches::kEnableAccountConsistency);
  DCHECK(!command_line->HasSwitch(switches::kDisableAccountConsistency));
}

}  // namespace switches
