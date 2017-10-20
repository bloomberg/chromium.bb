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
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/flags_ui/feature_entry.h"
#include "components/flags_ui/feature_entry_macros.h"
#include "components/flags_ui/flags_storage.h"
#include "components/flags_ui/flags_ui_switches.h"
#include "components/ntp_tiles/switches.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/payments/core/features.h"
#include "components/search_provider_logos/features.h"
#include "components/security_state/core/switches.h"
#include "components/signin/core/common/signin_switches.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/bookmarks/bookmark_new_generation_features.h"
#include "ios/chrome/browser/chrome_switches.h"
#include "ios/chrome/browser/drag_and_drop/drag_and_drop_flag.h"
#include "ios/chrome/browser/ios_chrome_flag_descriptions.h"
#include "ios/chrome/browser/ssl/captive_portal_features.h"
#include "ios/chrome/browser/ui/external_search/features.h"
#include "ios/chrome/browser/ui/main/main_feature_flags.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_controller_base_feature.h"
#include "ios/chrome/browser/web/features.h"
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
    {flag_descriptions::kMarkHttpAsDangerous,
     security_state::switches::kMarkHttpAs,
     security_state::switches::kMarkHttpAsDangerous}};

const FeatureEntry::FeatureParam kUseDdljsonApiTest0[] = {
    {search_provider_logos::features::kDdljsonOverrideUrlParam,
     "https://www.gstatic.com/chrome/ntp/doodle_test/ddljson_ios0.json"}};
const FeatureEntry::FeatureParam kUseDdljsonApiTest1[] = {
    {search_provider_logos::features::kDdljsonOverrideUrlParam,
     "https://www.gstatic.com/chrome/ntp/doodle_test/ddljson_ios1.json"}};
const FeatureEntry::FeatureParam kUseDdljsonApiTest2[] = {
    {search_provider_logos::features::kDdljsonOverrideUrlParam,
     "https://www.gstatic.com/chrome/ntp/doodle_test/ddljson_ios2.json"}};
const FeatureEntry::FeatureParam kUseDdljsonApiTest3[] = {
    {search_provider_logos::features::kDdljsonOverrideUrlParam,
     "https://www.gstatic.com/chrome/ntp/doodle_test/ddljson_ios3.json"}};
const FeatureEntry::FeatureParam kUseDdljsonApiTest4[] = {
    {search_provider_logos::features::kDdljsonOverrideUrlParam,
     "https://www.gstatic.com/chrome/ntp/doodle_test/ddljson_ios4.json"}};

const FeatureEntry::FeatureVariation kUseDdljsonApiVariations[] = {
    {"(force test doodle 0)", kUseDdljsonApiTest0,
     arraysize(kUseDdljsonApiTest0), nullptr},
    {"(force test doodle 1)", kUseDdljsonApiTest1,
     arraysize(kUseDdljsonApiTest1), nullptr},
    {"(force test doodle 2)", kUseDdljsonApiTest2,
     arraysize(kUseDdljsonApiTest2), nullptr},
    {"(force test doodle 3)", kUseDdljsonApiTest3,
     arraysize(kUseDdljsonApiTest3), nullptr},
    {"(force test doodle 4)", kUseDdljsonApiTest4,
     arraysize(kUseDdljsonApiTest4), nullptr}};

// To add a new entry, add to the end of kFeatureEntries. There are four
// distinct types of entries:
// . ENABLE_DISABLE_VALUE: entry is either enabled, disabled, or uses the
//   default value for this feature. Use the ENABLE_DISABLE_VALUE_TYPE
//   macro for this type supplying the command line to the macro.
// . MULTI_VALUE: a list of choices, the first of which should correspond to a
//   deactivated state for this lab (i.e. no command line option).  To specify
//   this type of entry use the macro MULTI_VALUE_TYPE supplying it the
//   array of choices.
// . FEATURE_VALUE: entry is associated with a base::Feature instance. Entry is
//   either enabled, disabled, or uses the default value of the associated
//   base::Feature instance. To specify this type of entry use the macro
//   FEATURE_VALUE_TYPE supplying it the base::Feature instance.
// . FEATURE_WITH_PARAM_VALUES: a list of choices associated with a
//   base::Feature instance. Choices corresponding to the default state, a
//   universally enabled state, and a universally disabled state are
//   automatically included. To specify this type of entry use the macro
//   FEATURE_WITH_PARAMS_VALUE_TYPE supplying it the base::Feature instance and
//   the array of choices.
//
// See the documentation of FeatureEntry for details on the fields.
//
// When adding a new choice, add it to the end of the list.
const flags_ui::FeatureEntry kFeatureEntries[] = {
    {"contextual-search", flag_descriptions::kContextualSearch,
     flag_descriptions::kContextualSearchDescription, flags_ui::kOsIos,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableContextualSearch,
                               switches::kDisableContextualSearch)},
    {"browser-task-scheduler", flag_descriptions::kBrowserTaskScheduler,
     flag_descriptions::kBrowserTaskSchedulerDescription, flags_ui::kOsIos,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableBrowserTaskScheduler,
                               switches::kDisableBrowserTaskScheduler)},
    {"mark-non-secure-as", flag_descriptions::kMarkHttpAsName,
     flag_descriptions::kMarkHttpAsDescription, flags_ui::kOsIos,
     MULTI_VALUE_TYPE(kMarkHttpAsChoices)},
    {"web-payments", flag_descriptions::kWebPaymentsName,
     flag_descriptions::kWebPaymentsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(payments::features::kWebPayments)},
    {"web-payments-native-apps", flag_descriptions::kWebPaymentsNativeAppsName,
     flag_descriptions::kWebPaymentsNativeAppsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(payments::features::kWebPaymentsNativeApps)},
    {"ios-captive-portal", flag_descriptions::kCaptivePortalName,
     flag_descriptions::kCaptivePortalDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kCaptivePortalFeature)},
    {"in-product-help-demo-mode-choice",
     flag_descriptions::kInProductHelpDemoModeName,
     flag_descriptions::kInProductHelpDemoModeDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         feature_engagement::kIPHDemoMode,
         feature_engagement::kIPHDemoModeChoiceVariations,
         "IPH_DemoMode")},
    {"use-ddljson-api", flag_descriptions::kUseDdljsonApiName,
     flag_descriptions::kUseDdljsonApiDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         search_provider_logos::features::kUseDdljsonApi,
         kUseDdljsonApiVariations,
         "NTPUseDdljsonApi")},
    {"omnibox-ui-elide-suggestion-url-after-host",
     flag_descriptions::kOmniboxUIElideSuggestionUrlAfterHostName,
     flag_descriptions::kOmniboxUIElideSuggestionUrlAfterHostDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(omnibox::kUIExperimentElideSuggestionUrlAfterHost)},
    {"omnibox-ui-hide-suggestion-url-scheme",
     flag_descriptions::kOmniboxUIHideSuggestionUrlSchemeName,
     flag_descriptions::kOmniboxUIHideSuggestionUrlSchemeDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(omnibox::kUIExperimentHideSuggestionUrlScheme)},
    {"omnibox-ui-hide-suggestion-url-trivial-subdomains",
     flag_descriptions::kOmniboxUIHideSuggestionUrlTrivialSubdomainsName,
     flag_descriptions::kOmniboxUIHideSuggestionUrlTrivialSubdomainsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         omnibox::kUIExperimentHideSuggestionUrlTrivialSubdomains)},
    {"bookmark-new-generation", flag_descriptions::kBookmarkNewGenerationName,
     flag_descriptions::kBookmarkNewGenerationDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kBookmarkNewGeneration)},
    {"mailto-prompt-for-user-choice",
     flag_descriptions::kMailtoPromptForUserChoiceName,
     flag_descriptions::kMailtoPromptForUserChoiceDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kMailtoPromptForUserChoice)},
#if defined(__IPHONE_11_0) && (__IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_11_0)
    {"drag_and_drop", flag_descriptions::kDragAndDropName,
     flag_descriptions::kDragAndDropDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kDragAndDrop)},
#endif
    {"tab_switcher_presents_bvc",
     flag_descriptions::kTabSwitcherPresentsBVCName,
     flag_descriptions::kTabSwitcherPresentsBVCDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kTabSwitcherPresentsBVC)},
    {"safe_area_compatible_toolbar",
     flag_descriptions::kSafeAreaCompatibleToolbarName,
     flag_descriptions::kSafeAreaCompatibleToolbarDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kSafeAreaCompatibleToolbar)},
    {"external-search", flag_descriptions::kExternalSearchName,
     flag_descriptions::kExternalSearchDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kExternalSearch)},
};

// Add all switches from experimental flags to |command_line|.
void AppendSwitchesFromExperimentalSettings(base::CommandLine* command_line) {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];

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

  // Populate command line flag for the native to WKBackForwardList based
  // navigation manager experiment.
  NSString* enableSlimNavigationManager =
      [defaults stringForKey:@"EnableSlimNavigationManager"];
  if ([enableSlimNavigationManager isEqualToString:@"Enabled"]) {
    command_line->AppendSwitch(switches::kEnableSlimNavigationManager);
  } else if ([enableSlimNavigationManager isEqualToString:@"Disabled"]) {
    command_line->AppendSwitch(switches::kDisableSlimNavigationManager);
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

  // Populate command line flag for 3rd party keyboard omnibox workaround.
  NSString* enableThirdPartyKeyboardWorkaround =
      [defaults stringForKey:@"EnableThirdPartyKeyboardWorkaround"];
  if ([enableThirdPartyKeyboardWorkaround isEqualToString:@"Enabled"]) {
    command_line->AppendSwitch(switches::kEnableThirdPartyKeyboardWorkaround);
  } else if ([enableThirdPartyKeyboardWorkaround isEqualToString:@"Disabled"]) {
    command_line->AppendSwitch(switches::kDisableThirdPartyKeyboardWorkaround);
  }

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
