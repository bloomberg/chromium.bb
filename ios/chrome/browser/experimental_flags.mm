// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file can be empty. Its purpose is to contain the relatively short lived
// definitions required for experimental flags.

#include "ios/chrome/browser/experimental_flags.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#include <dispatch/dispatch.h>

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

NSString* const kEnableStartupCrash = @"EnableStartupCrash";
NSString* const kEnableViewCopyPasswords = @"EnableViewCopyPasswords";
NSString* const kFirstRunForceEnabled = @"FirstRunForceEnabled";
NSString* const kGaiaEnvironment = @"GAIAEnvironment";
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

bool IsAutoReloadEnabled() {
  // TODO(crbug.com/752084): Remove this function and its associated code.
  return false;
}

bool IsLRUSnapshotCacheEnabled() {
  // TODO(crbug.com/751553): Remove this function and its associated code.
  return NO;
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

// TODO(crbug.com/760084): Remove this method and replace with base::Feature or
// remove it all.
bool IsNewClearBrowsingDataUIEnabled() {
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
  // TODO(crbug.com/752077): Remove this function and its associated code.
  // Either by replacing it with a base::Feature or by removing all its uses.
  return false;
}

bool IsPhysicalWebEnabled() {
  // TODO(crbug.com/760104): Remove this function and its associated code.
  // Either by replacing it with a base::Feature or by removing all its uses.
  return false;
}

bool IsSafariVCSignInEnabled() {
  return ![[NSUserDefaults standardUserDefaults]
      boolForKey:kSafariVCSignInDisabled];
}

bool IsStartupCrashEnabled() {
  return [[NSUserDefaults standardUserDefaults] boolForKey:kEnableStartupCrash];
}

// This feature is on by default. Finch and experimental settings can be used to
// disable it.
// TODO(crbug.com/739404): Remove this method and the experimental flag once the
// feature spends a couple of releases in stable.
bool IsViewCopyPasswordsEnabled() {
  if (!base::FeatureList::IsEnabled(password_manager::features::kViewPasswords))
    return false;
  NSString* viewCopyPasswordFlag = [[NSUserDefaults standardUserDefaults]
      objectForKey:kEnableViewCopyPasswords];
  return ![viewCopyPasswordFlag isEqualToString:@"Disabled"];
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

bool IsNewFeedbackKitEnabled() {
  return [[NSUserDefaults standardUserDefaults]
      boolForKey:@"NewFeedbackKitEnabled"];
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
