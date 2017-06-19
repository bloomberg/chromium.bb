// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of about_flags for iOS that sets flags based on experimental
// settings.

#include "ios/chrome/browser/about_flags.h"

#include <stddef.h>
#include <stdint.h>
#import <UIKit/UIKit.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/sys_info.h"
#include "base/task_scheduler/switches.h"
#include "components/dom_distiller/core/dom_distiller_switches.h"
#include "components/flags_ui/feature_entry.h"
#include "components/flags_ui/feature_entry_macros.h"
#include "components/flags_ui/flags_storage.h"
#include "components/flags_ui/flags_ui_switches.h"
#include "components/ntp_tiles/switches.h"
#include "components/security_state/core/switches.h"
#include "components/signin/core/common/signin_switches.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/chrome_switches.h"
#include "ios/chrome/browser/ios_chrome_flag_descriptions.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/web/public/user_agent.h"
#include "ios/web/public/web_view_creation_util.h"

#if !defined(OFFICIAL_BUILD)
#include "components/variations/variations_switches.h"
#endif

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using flags_ui::FeatureEntry;

namespace {
const FeatureEntry::Choice kMarkHttpAsChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flag_descriptions::kMarkHttpAsNonSecureAfterEditing,
     security_state::switches::kMarkHttpAs,
     security_state::switches::kMarkHttpAsNonSecureAfterEditing},
    {flag_descriptions::kMarkHttpAsNonSecureWhileIncognito,
     security_state::switches::kMarkHttpAs,
     security_state::switches::kMarkHttpAsNonSecureWhileIncognito},
    {flag_descriptions::kMarkHttpAsNonSecureWhileIncognitoOrEditing,
     security_state::switches::kMarkHttpAs,
     security_state::switches::kMarkHttpAsNonSecureWhileIncognitoOrEditing},
    {flag_descriptions::kMarkHttpAsDangerous,
     security_state::switches::kMarkHttpAs,
     security_state::switches::kMarkHttpAsDangerous}};

// To add a new entry, add to the end of kFeatureEntries. There are two
// distinct types of entries:
// . SINGLE_VALUE: entry is either on or off. Use the SINGLE_VALUE_TYPE
//   macro for this type supplying the command line to the macro.
// . MULTI_VALUE: a list of choices, the first of which should correspond to a
//   deactivated state for this lab (i.e. no command line option).  To specify
//   this type of entry use the macro MULTI_VALUE_TYPE supplying it the
//   array of choices.
// See the documentation of FeatureEntry for details on the fields.
//
// When adding a new choice, add it to the end of the list.
const flags_ui::FeatureEntry kFeatureEntries[] = {
    {"contextual-search", flag_descriptions::kContextualSearch,
     flag_descriptions::kContextualSearchDescription, flags_ui::kOsIos,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableContextualSearch,
                               switches::kDisableContextualSearch)},
    {"ios-physical-web", flag_descriptions::kPhysicalWeb,
     flag_descriptions::kPhysicalWebDescription, flags_ui::kOsIos,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableIOSPhysicalWeb,
                               switches::kDisableIOSPhysicalWeb)},
    {"browser-task-scheduler", flag_descriptions::kBrowserTaskScheduler,
     flag_descriptions::kBrowserTaskSchedulerDescription, flags_ui::kOsIos,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableBrowserTaskScheduler,
                               switches::kDisableBrowserTaskScheduler)},
    {"mark-non-secure-as", flag_descriptions::kMarkHttpAsName,
     flag_descriptions::kMarkHttpAsDescription, flags_ui::kOsIos,
     MULTI_VALUE_TYPE(kMarkHttpAsChoices)},
};

// Add all switches from experimental flags to |command_line|.
void AppendSwitchesFromExperimentalSettings(base::CommandLine* command_line) {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];

  // Populate command line flag for the tab strip auto scroll new tabs
  // experiment from the configuration plist.
  if ([defaults boolForKey:@"TabStripAutoScrollNewTabsDisabled"])
    command_line->AppendSwitch(switches::kDisableTabStripAutoScrollNewTabs);

  // Populate command line flag for the SnapshotLRUCache experiment from the
  // configuration plist.
  NSString* enableLRUSnapshotCache =
      [defaults stringForKey:@"SnapshotLRUCache"];
  if ([enableLRUSnapshotCache isEqualToString:@"Enabled"]) {
    command_line->AppendSwitch(switches::kEnableLRUSnapshotCache);
  } else if ([enableLRUSnapshotCache isEqualToString:@"Disabled"]) {
    command_line->AppendSwitch(switches::kDisableLRUSnapshotCache);
  }

  // Populate command line flags from PasswordGenerationEnabled.
  NSString* enablePasswordGenerationValue =
      [defaults stringForKey:@"PasswordGenerationEnabled"];
  if ([enablePasswordGenerationValue isEqualToString:@"Enabled"]) {
    command_line->AppendSwitch(switches::kEnableIOSPasswordGeneration);
  } else if ([enablePasswordGenerationValue isEqualToString:@"Disabled"]) {
    command_line->AppendSwitch(switches::kDisableIOSPasswordGeneration);
  }

  // Populate command line flags from PhysicalWebEnabled.
  NSString* enablePhysicalWebValue =
      [defaults stringForKey:@"PhysicalWebEnabled"];
  if ([enablePhysicalWebValue isEqualToString:@"Enabled"]) {
    command_line->AppendSwitch(switches::kEnableIOSPhysicalWeb);
  } else if ([enablePhysicalWebValue isEqualToString:@"Disabled"]) {
    command_line->AppendSwitch(switches::kDisableIOSPhysicalWeb);
  }

  // Web page replay flags.
  BOOL webPageReplayEnabled = [defaults boolForKey:@"WebPageReplayEnabled"];
  NSString* webPageReplayProxy =
      [defaults stringForKey:@"WebPageReplayProxyAddress"];
  if (webPageReplayEnabled && [webPageReplayProxy length]) {
    command_line->AppendSwitch(switches::kIOSIgnoreCertificateErrors);
    // 80 and 443 are the default ports from web page replay.
    command_line->AppendSwitchASCII(switches::kIOSTestingFixedHttpPort, "80");
    command_line->AppendSwitchASCII(switches::kIOSTestingFixedHttpsPort, "443");
    command_line->AppendSwitchASCII(
        switches::kIOSHostResolverRules,
        "MAP * " + base::SysNSStringToUTF8(webPageReplayProxy));
  }

  NSString* autoReloadEnabledValue =
      [defaults stringForKey:@"AutoReloadEnabled"];
  if ([autoReloadEnabledValue isEqualToString:@"Enabled"]) {
    command_line->AppendSwitch(switches::kEnableOfflineAutoReload);
  } else if ([autoReloadEnabledValue isEqualToString:@"Disabled"]) {
    command_line->AppendSwitch(switches::kDisableOfflineAutoReload);
  }

  // Populate command line flags from EnableFastWebScrollViewInsets.
  NSString* enableFastWebScrollViewInsets =
      [defaults stringForKey:@"EnableFastWebScrollViewInsets"];
  if ([enableFastWebScrollViewInsets isEqualToString:@"Enabled"]) {
    command_line->AppendSwitch(switches::kEnableIOSFastWebScrollViewInsets);
  } else if ([enableFastWebScrollViewInsets isEqualToString:@"Disabled"]) {
    command_line->AppendSwitch(switches::kDisableIOSFastWebScrollViewInsets);
  }

  // Populate command line flags from ReaderModeEnabled.
  if ([defaults boolForKey:@"ReaderModeEnabled"]) {
    command_line->AppendSwitch(switches::kEnableReaderModeToolbarIcon);

    // Populate command line from ReaderMode Heuristics detection.
    NSString* readerModeDetectionHeuristics =
        [defaults stringForKey:@"ReaderModeDetectionHeuristics"];
    if (!readerModeDetectionHeuristics) {
      command_line->AppendSwitchASCII(
          switches::kReaderModeHeuristics,
          switches::reader_mode_heuristics::kOGArticle);
    } else if ([readerModeDetectionHeuristics isEqualToString:@"AdaBoost"]) {
      command_line->AppendSwitchASCII(
          switches::kReaderModeHeuristics,
          switches::reader_mode_heuristics::kAdaBoost);
    } else {
      DCHECK([readerModeDetectionHeuristics isEqualToString:@"Off"]);
      command_line->AppendSwitchASCII(switches::kReaderModeHeuristics,
                                      switches::reader_mode_heuristics::kNone);
    }
  }

  // Populate command line flags from EnablePopularSites.
  NSString* EnablePopularSites = [defaults stringForKey:@"EnablePopularSites"];
  if ([EnablePopularSites isEqualToString:@"Enabled"]) {
    command_line->AppendSwitch(ntp_tiles::switches::kEnableNTPPopularSites);
  } else if ([EnablePopularSites isEqualToString:@"Disabled"]) {
    command_line->AppendSwitch(ntp_tiles::switches::kDisableNTPPopularSites);
  }

  // Set the UA flag if UseMobileSafariUA is enabled.
  if ([defaults boolForKey:@"UseMobileSafariUA"]) {
    // Safari uses "Vesion/", followed by the OS version excluding bugfix, where
    // Chrome puts its product token.
    int32_t major = 0;
    int32_t minor = 0;
    int32_t bugfix = 0;
    base::SysInfo::OperatingSystemVersionNumbers(&major, &minor, &bugfix);
    std::string product = base::StringPrintf("Version/%d.%d", major, minor);

    command_line->AppendSwitchASCII(switches::kUserAgent,
                                    web::BuildUserAgentFromProduct(product));
  }

  // Populate command line flag for the Payment Request API.
  NSString* enable_payment_request =
      [defaults stringForKey:@"EnablePaymentRequest"];
  if ([enable_payment_request isEqualToString:@"Enabled"]) {
    command_line->AppendSwitch(switches::kEnablePaymentRequest);
  } else if ([enable_payment_request isEqualToString:@"Disabled"]) {
    command_line->AppendSwitch(switches::kDisablePaymentRequest);
  }

  // Populate command line flag for Suggestions UI display.
  NSString* enableSuggestions = [defaults stringForKey:@"EnableSuggestions"];
  if ([enableSuggestions isEqualToString:@"Enabled"]) {
    command_line->AppendSwitch(switches::kEnableSuggestionsUI);
  } else if ([enableSuggestions isEqualToString:@"Disabled"]) {
    command_line->AppendSwitch(switches::kDisableSuggestionsUI);
  }

  // Populate command line flag for fetching missing favicons for NTP tiles.
  NSString* enableMostLikelyFaviconsFromServer =
      [defaults stringForKey:@"EnableNtpMostLikelyFaviconsFromServer"];
  if ([enableMostLikelyFaviconsFromServer isEqualToString:@"Enabled"]) {
    command_line->AppendSwitch(
        ntp_tiles::switches::kEnableNtpMostLikelyFaviconsFromServer);
  } else if ([enableMostLikelyFaviconsFromServer isEqualToString:@"Disabled"]) {
    command_line->AppendSwitch(
        ntp_tiles::switches::kDisableNtpMostLikelyFaviconsFromServer);
  }

  // Freeform commandline flags.  These are added last, so that any flags added
  // earlier in this function take precedence.
  if ([defaults boolForKey:@"EnableFreeformCommandLineFlags"]) {
    base::CommandLine::StringVector flags;
    // Append an empty "program" argument.
    flags.push_back("");

    // The number of flags corresponds to the number of text fields in
    // Experimental.plist.
    const int kNumFreeformFlags = 5;
    for (int i = 1; i <= kNumFreeformFlags; ++i) {
      NSString* key =
          [NSString stringWithFormat:@"FreeformCommandLineFlag%d", i];
      NSString* flag = [defaults stringForKey:key];
      if ([flag length]) {
        flags.push_back(base::SysNSStringToUTF8(flag));
      }
    }

    base::CommandLine temp_command_line(flags);
    command_line->AppendArguments(temp_command_line, false);
  }

  // Populate command line flag for Sign-in promo.
  NSString* enableSigninPromo = [defaults stringForKey:@"EnableSigninPromo"];
  if ([enableSigninPromo isEqualToString:@"Enabled"]) {
    command_line->AppendSwitch(switches::kEnableSigninPromo);
  } else if ([enableSigninPromo isEqualToString:@"Disabled"]) {
    command_line->AppendSwitch(switches::kDisableSigninPromo);
  }

  // Populate command line flag for Bookmark reordering.
  NSString* enableBookmarkReordering =
      [defaults stringForKey:@"EnableBookmarkReordering"];
  if ([enableBookmarkReordering isEqualToString:@"Enabled"]) {
    command_line->AppendSwitch(switches::kEnableBookmarkReordering);
  } else if ([enableBookmarkReordering isEqualToString:@"Disabled"]) {
    command_line->AppendSwitch(switches::kDisableBookmarkReordering);
  }

  // Populate command line flag for the request mobile site experiment from the
  // configuration plist.
  if ([defaults boolForKey:@"RequestMobileSiteDisabled"])
    command_line->AppendSwitch(switches::kDisableRequestMobileSite);

  ios::GetChromeBrowserProvider()->AppendSwitchesFromExperimentalSettings(
      defaults, command_line);
}

bool SkipConditionalFeatureEntry(const flags_ui::FeatureEntry& entry) {
  return false;
}

class FlagsStateSingleton {
 public:
  FlagsStateSingleton()
      : flags_state_(kFeatureEntries, arraysize(kFeatureEntries)) {}
  ~FlagsStateSingleton() {}

  static FlagsStateSingleton* GetInstance() {
    return base::Singleton<FlagsStateSingleton>::get();
  }

  static flags_ui::FlagsState* GetFlagsState() {
    return &GetInstance()->flags_state_;
  }

 private:
  flags_ui::FlagsState flags_state_;

  DISALLOW_COPY_AND_ASSIGN(FlagsStateSingleton);
};
}  // namespace

void ConvertFlagsToSwitches(flags_ui::FlagsStorage* flags_storage,
                            base::CommandLine* command_line) {
  FlagsStateSingleton::GetFlagsState()->ConvertFlagsToSwitches(
      flags_storage, command_line, flags_ui::kAddSentinels,
      switches::kEnableIOSFeatures, switches::kDisableIOSFeatures);
  AppendSwitchesFromExperimentalSettings(command_line);
}

std::vector<std::string> RegisterAllFeatureVariationParameters(
    flags_ui::FlagsStorage* flags_storage,
    base::FeatureList* feature_list) {
  return FlagsStateSingleton::GetFlagsState()
      ->RegisterAllFeatureVariationParameters(flags_storage, feature_list);
}

void GetFlagFeatureEntries(flags_ui::FlagsStorage* flags_storage,
                           flags_ui::FlagAccess access,
                           base::ListValue* supported_entries,
                           base::ListValue* unsupported_entries) {
  FlagsStateSingleton::GetFlagsState()->GetFlagFeatureEntries(
      flags_storage, access, supported_entries, unsupported_entries,
      base::Bind(&SkipConditionalFeatureEntry));
}

void SetFeatureEntryEnabled(flags_ui::FlagsStorage* flags_storage,
                            const std::string& internal_name,
                            bool enable) {
  FlagsStateSingleton::GetFlagsState()->SetFeatureEntryEnabled(
      flags_storage, internal_name, enable);
}

void ResetAllFlags(flags_ui::FlagsStorage* flags_storage) {
  FlagsStateSingleton::GetFlagsState()->ResetAllFlags(flags_storage);
}

namespace testing {

const flags_ui::FeatureEntry* GetFeatureEntries(size_t* count) {
  *count = arraysize(kFeatureEntries);
  return kFeatureEntries;
}

}  // namespace testing
