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
#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/signin/core/common/signin_switches.h"
#include "components/variations/variations_associated_data.h"
#include "ios/chrome/browser/chrome_switches.h"
#include "ios/web/public/web_view_creation_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

NSString* const kEnableAlertOnBackgroundUpload =
    @"EnableAlertsOnBackgroundUpload";
NSString* const kEnableNewClearBrowsingDataUI = @"EnableNewClearBrowsingDataUI";
NSString* const kEnableStartupCrash = @"EnableStartupCrash";
NSString* const kEnableViewCopyPasswords = @"EnableViewCopyPasswords";
NSString* const kFirstRunForceEnabled = @"FirstRunForceEnabled";
NSString* const kForceResetContextualSearch = @"ForceResetContextualSearch";
NSString* const kGaiaEnvironment = @"GAIAEnvironment";
NSString* const kHeuristicsForPasswordGeneration =
    @"HeuristicsForPasswordGeneration";
NSString* const kMDMIntegrationDisabled = @"MDMIntegrationDisabled";
NSString* const kOriginServerHost = @"AlternateOriginServerHost";
NSString* const kSafariVCSignInDisabled = @"SafariVCSignInDisabled";
NSString* const kWhatsNewPromoStatus = @"WhatsNewPromoStatus";
const base::Feature kEnableSlimNavigationManager{
    "EnableSlimNavigationManager", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kEnableThirdPartyKeyboardWorkaround{
    "EnableThirdPartyKeyboardWorkaround", base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kIOSNTPSuggestions{"IOSNTPSuggestions",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace

namespace experimental_flags {

bool AlwaysDisplayFirstRun() {
  return
      [[NSUserDefaults standardUserDefaults] boolForKey:kFirstRunForceEnabled];
}

GaiaEnvironment GetGaiaEnvironment() {
  NSString* gaia_environment =
      [[NSUserDefaults standardUserDefaults] objectForKey:kGaiaEnvironment];
  if ([gaia_environment isEqualToString:@"Staging"])
    return GAIA_ENVIRONMENT_STAGING;
  if ([gaia_environment isEqualToString:@"Test"])
    return GAIA_ENVIRONMENT_TEST;
  return GAIA_ENVIRONMENT_PROD;
}

std::string GetOriginServerHost() {
  NSString* alternateHost =
      [[NSUserDefaults standardUserDefaults] stringForKey:kOriginServerHost];
  return base::SysNSStringToUTF8(alternateHost);
}

WhatsNewPromoStatus GetWhatsNewPromoStatus() {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  NSInteger status = [defaults integerForKey:kWhatsNewPromoStatus];
  // If |status| is set to a value greater than or equal to the count of items
  // defined in WhatsNewPromoStatus, set it to |WHATS_NEW_DEFAULT| and correct
  // the value in NSUserDefaults. This error case can happen when a user
  // upgrades to a version with fewer Whats New Promo settings.
  if (status >= static_cast<NSInteger>(WHATS_NEW_PROMO_STATUS_COUNT)) {
    status = static_cast<NSInteger>(WHATS_NEW_DEFAULT);
    [defaults setInteger:status forKey:kWhatsNewPromoStatus];
  }
  return static_cast<WhatsNewPromoStatus>(status);
}

bool IsAlertOnBackgroundUploadEnabled() {
  return [[NSUserDefaults standardUserDefaults]
      boolForKey:kEnableAlertOnBackgroundUpload];
}

bool IsAutoReloadEnabled() {
  std::string group_name = base::FieldTrialList::FindFullName("IOSAutoReload");
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnableOfflineAutoReload))
    return true;
  if (command_line->HasSwitch(switches::kDisableOfflineAutoReload))
    return false;
  return base::StartsWith(group_name, "Enabled",
                          base::CompareCase::INSENSITIVE_ASCII);
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

bool IsMDMIntegrationEnabled() {
  return ![[NSUserDefaults standardUserDefaults]
      boolForKey:kMDMIntegrationDisabled];
}

bool IsMemoryDebuggingEnabled() {
// Always return true for Chromium builds, but check the user default for
// official builds because memory debugging should never be enabled on stable.
#if CHROMIUM_BUILD
  return true;
#else
  return [[NSUserDefaults standardUserDefaults]
      boolForKey:@"EnableMemoryDebugging"];
#endif  // CHROMIUM_BUILD
}

bool IsNewClearBrowsingDataUIEnabled() {
  NSString* countersFlag = [[NSUserDefaults standardUserDefaults]
      objectForKey:kEnableNewClearBrowsingDataUI];
  if ([countersFlag isEqualToString:@"Enabled"])
    return true;
  return false;
}

// Emergency switch for https://crbug.com/527084 in case of unforeseen UX
// regressions.
// Defaults to Enabled unless the Finch trial has explicitly disabled it.
bool IsPageIconForDowngradedHTTPSEnabled() {
  std::string group_name =
      base::FieldTrialList::FindFullName("IOSPageIconForDowngradedHTTPS");
  return !base::StartsWith(group_name, "Disabled",
                           base::CompareCase::INSENSITIVE_ASCII);
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

bool IsPhysicalWebEnabled() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnableIOSPhysicalWeb)) {
    return true;
  } else if (command_line->HasSwitch(switches::kDisableIOSPhysicalWeb)) {
    return false;
  }

  // Check if the finch experiment is turned on
  std::string group_name =
      base::FieldTrialList::FindFullName("PhysicalWebEnabled");
  return base::StartsWith(group_name, "Enabled",
                          base::CompareCase::INSENSITIVE_ASCII);
}

bool IsReaderModeEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableReaderModeToolbarIcon);
}

bool IsRequestMobileSiteEnabled() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return !command_line->HasSwitch(switches::kDisableRequestMobileSite);
}

bool IsSafariVCSignInEnabled() {
  return ![[NSUserDefaults standardUserDefaults]
      boolForKey:kSafariVCSignInDisabled];
}

bool IsStartupCrashEnabled() {
  return [[NSUserDefaults standardUserDefaults] boolForKey:kEnableStartupCrash];
}

bool IsTabStripAutoScrollNewTabsEnabled() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return !command_line->HasSwitch(switches::kDisableTabStripAutoScrollNewTabs);
}

bool IsViewCopyPasswordsEnabled() {
  NSString* viewCopyPasswordFlag = [[NSUserDefaults standardUserDefaults]
      objectForKey:kEnableViewCopyPasswords];
  if ([viewCopyPasswordFlag isEqualToString:@"Enabled"])
    return true;
  return base::FeatureList::IsEnabled(
      password_manager::features::kViewPasswords);
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

bool IsSuggestionsUIEnabled() {
  // Check if the experimental flag is forced on or off.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnableSuggestionsUI))
    return true;

  if (command_line->HasSwitch(switches::kDisableSuggestionsUI))
    return false;

  // Check if the Finch experiment is turned on.
  return base::FeatureList::IsEnabled(kIOSNTPSuggestions);
}

bool IsSigninPromoEnabled() {
  // Check if the experimental flag is forced on or off.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnableSigninPromo))
    return true;
  if (command_line->HasSwitch(switches::kDisableSigninPromo))
    return false;
  std::string group_name = base::FieldTrialList::FindFullName("IOSSigninPromo");
  return base::StartsWith(group_name, "Enabled",
                          base::CompareCase::INSENSITIVE_ASCII);
}

bool IsBookmarkReorderingEnabled() {
  // Check if the experimental flag is forced on or off.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnableBookmarkReordering))
    return true;
  if (command_line->HasSwitch(switches::kDisableBookmarkReordering))
    return false;

  // By default, disable it.
  return false;
}

bool IsNewFeedbackKitEnabled() {
  return [[NSUserDefaults standardUserDefaults]
      boolForKey:@"NewFeedbackKitEnabled"];
}

bool IsKeyboardAccessoryViewWithCameraSearchEnabled() {
  return ![[NSUserDefaults standardUserDefaults]
      boolForKey:@"NewKeyboardAccessoryViewDisabled"];
}

bool IsSlimNavigationManagerEnabled() {
  // Check if the experimental flag is forced on or off.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnableSlimNavigationManager)) {
    return true;
  } else if (command_line->HasSwitch(switches::kDisableSlimNavigationManager)) {
    return false;
  }

  // Check if the Finch experiment is turned on.
  return base::FeatureList::IsEnabled(kEnableSlimNavigationManager);
}

bool IsThirdPartyKeyboardWorkaroundEnabled() {
  // Check if the experimental flag is forced on or off.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnableThirdPartyKeyboardWorkaround)) {
    return true;
  } else if (command_line->HasSwitch(
                 switches::kDisableThirdPartyKeyboardWorkaround)) {
    return false;
  }

  // Check if the Finch experiment is turned on.
  return base::FeatureList::IsEnabled(kEnableThirdPartyKeyboardWorkaround);
}

}  // namespace experimental_flags
