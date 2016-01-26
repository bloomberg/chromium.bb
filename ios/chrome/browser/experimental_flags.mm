// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file can be empty. Its purpose is to contain the relatively short lived
// definitions required for experimental flags.

#include "ios/chrome/browser/experimental_flags.h"

#include <dispatch/dispatch.h>
#import <Foundation/Foundation.h>

#include <string>

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_util.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/enhanced_bookmarks/enhanced_bookmark_switches_ios.h"
#include "components/variations/variations_associated_data.h"
#include "ios/chrome/browser/chrome_switches.h"
#include "ios/web/public/web_view_creation_util.h"

namespace {
NSString* const kEnableAlertOnBackgroundUpload =
    @"EnableAlertsOnBackgroundUpload";
NSString* const kEnableViewCopyPasswords = @"EnableViewCopyPasswords";
NSString* const kHeuristicsForPasswordGeneration =
    @"HeuristicsForPasswordGeneration";
const char* const kWKWebViewTrialName = "IOSUseWKWebView";
NSString* const kEnableReadingList = @"EnableReadingList";

enum class WKWebViewEligibility {
  // UNSET indicates that no explicit call to set eligibility has been made,
  // nor has a default value been assumed due to checking eligibility.
  UNSET,
  ELIGIBLE,
  INELIGIBLE
};
WKWebViewEligibility g_wkwebview_trial_eligibility =
    WKWebViewEligibility::UNSET;
}  // namespace

namespace experimental_flags {

bool IsAlertOnBackgroundUploadEnabled() {
  return [[NSUserDefaults standardUserDefaults]
      boolForKey:kEnableAlertOnBackgroundUpload];
}

bool IsBookmarkCollectionEnabled() {
  // kEnhancedBookmarksExperiment flag could have values "", "1" and "0".
  // "" - default, "0" - user opted out, "1" - user opted in.  Tests also use
  // the command line flag to force enhanced bookmark to be on.
  // If none is specified, the finch experiment is checked. If not disabled in
  // finch, the default is opt-in.
  std::string switch_value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kEnhancedBookmarksExperiment);
  if (switch_value == "1")
    return true;
  if (switch_value == "0")
    return false;

  // Check if the finch experiment is turned on.
  std::string group_name =
      base::FieldTrialList::FindFullName("IOSNewBookmarksUI");
  return !base::StartsWith(group_name, "Disabled",
                           base::CompareCase::INSENSITIVE_ASCII);
}

void SetWKWebViewTrialEligibility(bool eligible) {
  // It's critical that the enabled state be consistently reported throughout
  // the life of the app, so ensure that this has not already been set.
  DCHECK(g_wkwebview_trial_eligibility == WKWebViewEligibility::UNSET);

  g_wkwebview_trial_eligibility = eligible ? WKWebViewEligibility::ELIGIBLE
                                           : WKWebViewEligibility::INELIGIBLE;
}

bool IsLRUSnapshotCacheEnabled() {
  // Check if the experimental flag is forced on or off.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnableLRUSnapshotCache)) {
    return true;
  } else if (command_line->HasSwitch(switches::kDisableLRUSnapshotCache)) {
    return false;
  }

  // Check if the finch experiment is turned on.
  std::string group_name =
      base::FieldTrialList::FindFullName("IOSLRUSnapshotCache");
  return base::StartsWith(group_name, "Enabled",
                          base::CompareCase::INSENSITIVE_ASCII);
}

// Helper method that returns true if it is safe to check the finch group for
// the IOSUseWKWebView experiment.  Some users are ineligible to be in the
// trial, so for those users, this method returns false.  If this method returns
// false, do not check for the current finch group, as doing so will incorrectly
// mark the current user as being in the experiment.
bool CanCheckWKWebViewExperiment() {
  // True if this user is eligible for the WKWebView experiment and it is ok to
  // check the experiment group.
  static bool ok_to_check_finch = false;
  static dispatch_once_t once;
  dispatch_once(&once, ^{
    // If g_wkwebview_trial_eligibility hasn't been set, default it to
    // ineligible. This ensures future calls to try to set it will DCHECK.
    if (g_wkwebview_trial_eligibility == WKWebViewEligibility::UNSET) {
      g_wkwebview_trial_eligibility = WKWebViewEligibility::INELIGIBLE;
    }

    // If WKWebView isn't supported, don't activate the experiment at all. This
    // avoids someone being slotted into the WKWebView bucket (and thus
    // reporting as WKWebView), but actually running UIWebView.
    if (!web::IsWKWebViewSupported()) {
      ok_to_check_finch = false;
      return;
    }

    // Check for a flag forcing a specific group.  Even ineligible users can be
    // opted into WKWebView if an override flag is set.
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    bool trial_overridden =
        command_line->HasSwitch(switches::kEnableIOSWKWebView) ||
        command_line->HasSwitch(switches::kDisableIOSWKWebView);

    // If the user isn't eligible for the trial (i.e., their state is such that
    // they should not be automatically selected for the group), and there's no
    // explicit override, don't check the group (again, to avoid having them
    // report as part of a group at all).
    if (g_wkwebview_trial_eligibility == WKWebViewEligibility::INELIGIBLE &&
        !trial_overridden) {
      ok_to_check_finch = false;
      return;
    }

    ok_to_check_finch = true;
  });

  return ok_to_check_finch;
}

bool IsWKWebViewEnabled() {
  if (!CanCheckWKWebViewExperiment()) {
    return false;
  }

  // Now that it's been established that user is a candidate, set up the trial
  // by checking the group.
  std::string group_name =
      base::FieldTrialList::FindFullName(kWKWebViewTrialName);

  // Check if the experimental flag is turned on.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnableIOSWKWebView))
    return true;
  else if (command_line->HasSwitch(switches::kDisableIOSWKWebView))
    return false;

  // Check if the finch experiment is turned on.
  return !base::StartsWith(group_name, "Disabled",
                           base::CompareCase::INSENSITIVE_ASCII) &&
         !base::StartsWith(group_name, "Control",
                           base::CompareCase::INSENSITIVE_ASCII);
}

bool IsTargetedToWKWebViewExperimentControlGroup() {
  base::FieldTrial* trial = base::FieldTrialList::Find(kWKWebViewTrialName);
  if (!trial)
    return false;
  std::string group_name = trial->GetGroupNameWithoutActivation();
  return base::StartsWith(group_name, "Control",
                          base::CompareCase::INSENSITIVE_ASCII);
}

bool IsInWKWebViewExperimentControlGroup() {
  if (!CanCheckWKWebViewExperiment()) {
    return false;
  }
  std::string group_name =
      base::FieldTrialList::FindFullName(kWKWebViewTrialName);
  return base::StartsWith(group_name, "Control",
                          base::CompareCase::INSENSITIVE_ASCII);
}

bool IsViewCopyPasswordsEnabled() {
  NSString* viewCopyPasswordFlag = [[NSUserDefaults standardUserDefaults]
      objectForKey:kEnableViewCopyPasswords];
  if ([viewCopyPasswordFlag isEqualToString:@"Enabled"])
    return true;
  return false;
}

bool IsPasswordGenerationEnabled() {
  // This call activates the field trial, if needed, so it must come before any
  // early returns.
  std::string group_name =
      base::FieldTrialList::FindFullName("PasswordGeneration");
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnableIOSPasswordGeneration))
    return true;
  if (command_line->HasSwitch(switches::kDisableIOSPasswordGeneration))
    return false;
  return group_name != "Disabled";
}

bool UseOnlyLocalHeuristicsForPasswordGeneration() {
  if ([[NSUserDefaults standardUserDefaults]
          boolForKey:kHeuristicsForPasswordGeneration]) {
    return true;
  }
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(
      autofill::switches::kLocalHeuristicsOnlyForPasswordGeneration);
}

bool IsTabSwitcherEnabled() {
  // Check if the experimental flag is forced on or off.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnableTabSwitcher)) {
    return true;
  } else if (command_line->HasSwitch(switches::kDisableTabSwitcher)) {
    return false;
  }

  // Check if the finch experiment is turned on.
  std::string group_name = base::FieldTrialList::FindFullName("IOSTabSwitcher");
  return base::StartsWith(group_name, "Enabled",
                          base::CompareCase::INSENSITIVE_ASCII);
}

bool IsReadingListEnabled() {
  return [[NSUserDefaults standardUserDefaults] boolForKey:kEnableReadingList];
}

}  // namespace experimental_flags
