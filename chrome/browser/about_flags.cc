// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/about_flags.h"

#include <iterator>
#include <map>
#include <set>
#include <utility>

#include "base/command_line.h"
#include "base/memory/singleton.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "cc/base/switches.h"
#include "chrome/browser/bookmarks/enhanced_bookmarks_features.h"
#include "chrome/browser/flags_storage.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_switches.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/cloud_devices/common/cloud_devices_switches.h"
#include "components/nacl/common/nacl_switches.h"
#include "components/search/search_switches.h"
#include "content/public/browser/user_metrics.h"
#include "extensions/common/switches.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/google_chrome_strings.h"
#include "media/base/media_switches.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_switches.h"
#include "ui/display/display_switches.h"
#include "ui/events/event_switches.h"
#include "ui/gfx/switches.h"
#include "ui/gl/gl_switches.h"
#include "ui/keyboard/keyboard_switches.h"
#include "ui/native_theme/native_theme_switches.h"
#include "ui/views/views_switches.h"

#if defined(OS_ANDROID)
#include "chrome/common/chrome_version_info.h"
#include "components/data_reduction_proxy/common/data_reduction_proxy_switches.h"
#include "components/omnibox/omnibox_switches.h"
#endif

#if defined(USE_ASH)
#include "ash/ash_switches.h"
#endif

#if defined(OS_CHROMEOS)
#include "chromeos/chromeos_switches.h"
#include "third_party/cros_system_api/switches/chrome_switches.h"
#endif

#if defined(ENABLE_APP_LIST)
#include "ui/app_list/app_list_switches.h"
#endif

using base::UserMetricsAction;

namespace about_flags {

// Macros to simplify specifying the type.
#define SINGLE_VALUE_TYPE_AND_VALUE(command_line_switch, switch_value) \
    Experiment::SINGLE_VALUE, \
    command_line_switch, switch_value, NULL, NULL, NULL, 0
#define SINGLE_VALUE_TYPE(command_line_switch) \
    SINGLE_VALUE_TYPE_AND_VALUE(command_line_switch, "")
#define ENABLE_DISABLE_VALUE_TYPE_AND_VALUE(enable_switch, enable_value, \
                                            disable_switch, disable_value) \
    Experiment::ENABLE_DISABLE_VALUE, enable_switch, enable_value, \
    disable_switch, disable_value, NULL, 3
#define ENABLE_DISABLE_VALUE_TYPE(enable_switch, disable_switch) \
    ENABLE_DISABLE_VALUE_TYPE_AND_VALUE(enable_switch, "", disable_switch, "")
#define MULTI_VALUE_TYPE(choices) \
    Experiment::MULTI_VALUE, NULL, NULL, NULL, NULL, choices, arraysize(choices)

namespace {

const unsigned kOsAll = kOsMac | kOsWin | kOsLinux | kOsCrOS | kOsAndroid;
const unsigned kOsDesktop = kOsMac | kOsWin | kOsLinux | kOsCrOS;

// Adds a |StringValue| to |list| for each platform where |bitmask| indicates
// whether the experiment is available on that platform.
void AddOsStrings(unsigned bitmask, base::ListValue* list) {
  struct {
    unsigned bit;
    const char* const name;
  } kBitsToOs[] = {
    {kOsMac, "Mac"},
    {kOsWin, "Windows"},
    {kOsLinux, "Linux"},
    {kOsCrOS, "Chrome OS"},
    {kOsAndroid, "Android"},
    {kOsCrOSOwnerOnly, "Chrome OS (owner only)"},
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kBitsToOs); ++i)
    if (bitmask & kBitsToOs[i].bit)
      list->Append(new base::StringValue(kBitsToOs[i].name));
}

// Convert switch constants to proper CommandLine::StringType strings.
CommandLine::StringType GetSwitchString(const std::string& flag) {
  CommandLine cmd_line(CommandLine::NO_PROGRAM);
  cmd_line.AppendSwitch(flag);
  DCHECK_EQ(2U, cmd_line.argv().size());
  return cmd_line.argv()[1];
}

// Scoops flags from a command line.
std::set<CommandLine::StringType> ExtractFlagsFromCommandLine(
    const CommandLine& cmdline) {
  std::set<CommandLine::StringType> flags;
  // First do the ones between --flag-switches-begin and --flag-switches-end.
  CommandLine::StringVector::const_iterator first =
      std::find(cmdline.argv().begin(), cmdline.argv().end(),
                GetSwitchString(switches::kFlagSwitchesBegin));
  CommandLine::StringVector::const_iterator last =
      std::find(cmdline.argv().begin(), cmdline.argv().end(),
                GetSwitchString(switches::kFlagSwitchesEnd));
  if (first != cmdline.argv().end() && last != cmdline.argv().end())
    flags.insert(first + 1, last);
#if defined(OS_CHROMEOS)
  // Then add those between --policy-switches-begin and --policy-switches-end.
  first = std::find(cmdline.argv().begin(), cmdline.argv().end(),
                    GetSwitchString(chromeos::switches::kPolicySwitchesBegin));
  last = std::find(cmdline.argv().begin(), cmdline.argv().end(),
                   GetSwitchString(chromeos::switches::kPolicySwitchesEnd));
  if (first != cmdline.argv().end() && last != cmdline.argv().end())
    flags.insert(first + 1, last);
#endif
  return flags;
}

const Experiment::Choice kEnableCompositingForFixedPositionChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", "" },
  { IDS_GENERIC_EXPERIMENT_CHOICE_ENABLED,
    switches::kEnableCompositingForFixedPosition, ""},
  { IDS_GENERIC_EXPERIMENT_CHOICE_DISABLED,
    switches::kDisableCompositingForFixedPosition, ""},
  { IDS_FLAGS_COMPOSITING_FOR_FIXED_POSITION_HIGH_DPI,
    switches::kEnableHighDpiCompositingForFixedPosition, ""}
};

const Experiment::Choice kEnableCompositingForTransitionChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", "" },
  { IDS_GENERIC_EXPERIMENT_CHOICE_ENABLED,
    switches::kEnableCompositingForTransition, ""},
  { IDS_GENERIC_EXPERIMENT_CHOICE_DISABLED,
    switches::kDisableCompositingForTransition, ""},
};

const Experiment::Choice kEnableAcceleratedFixedRootBackgroundChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", "" },
  { IDS_GENERIC_EXPERIMENT_CHOICE_ENABLED,
    switches::kEnableAcceleratedFixedRootBackground, ""},
  { IDS_GENERIC_EXPERIMENT_CHOICE_DISABLED,
    switches::kDisableAcceleratedFixedRootBackground, ""},
};

const Experiment::Choice kTouchEventsChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_AUTOMATIC, "", "" },
  { IDS_GENERIC_EXPERIMENT_CHOICE_ENABLED,
    switches::kTouchEvents,
    switches::kTouchEventsEnabled },
  { IDS_GENERIC_EXPERIMENT_CHOICE_DISABLED,
    switches::kTouchEvents,
    switches::kTouchEventsDisabled }
};

#if defined(USE_AURA)
const Experiment::Choice kOverscrollHistoryNavigationChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_ENABLED, "", "" },
  { IDS_GENERIC_EXPERIMENT_CHOICE_DISABLED,
    switches::kOverscrollHistoryNavigation,
    "0" },
  { IDS_OVERSCROLL_HISTORY_NAVIGATION_SIMPLE_UI,
    switches::kOverscrollHistoryNavigation,
    "2" }
};
#endif
#if defined(OS_CHROMEOS) || defined(OS_WIN) || defined(OS_LINUX)
const Experiment::Choice kDeviceScaleFactorChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", ""},
  { IDS_DEVICE_SCALE_FACTOR_1_1, switches::kForceDeviceScaleFactor, "1.1"},
  { IDS_DEVICE_SCALE_FACTOR_1_2, switches::kForceDeviceScaleFactor, "1.2"},
  { IDS_DEVICE_SCALE_FACTOR_1_25, switches::kForceDeviceScaleFactor, "1.25"},
  { IDS_DEVICE_SCALE_FACTOR_1_3, switches::kForceDeviceScaleFactor, "1.3"},
  { IDS_DEVICE_SCALE_FACTOR_1_4, switches::kForceDeviceScaleFactor, "1.4"},
  { IDS_DEVICE_SCALE_FACTOR_2, switches::kForceDeviceScaleFactor, "2"},
};
#endif

#if !defined(DISABLE_NACL)
const Experiment::Choice kNaClDebugMaskChoices[] = {
  // Secure shell can be used on ChromeOS for forwarding the TCP port opened by
  // debug stub to a remote machine. Since secure shell uses NaCl, we usually
  // want to avoid debugging that. The PNaCl translator is also a NaCl module,
  // so by default we want to avoid debugging that.
  // NOTE: As the default value must be the empty string, the mask excluding
  // the PNaCl translator and secure shell is substituted elsewhere.
  { IDS_NACL_DEBUG_MASK_CHOICE_EXCLUDE_UTILS_PNACL, "", "" },
  { IDS_NACL_DEBUG_MASK_CHOICE_DEBUG_ALL, switches::kNaClDebugMask, "*://*" },
  { IDS_NACL_DEBUG_MASK_CHOICE_INCLUDE_DEBUG,
      switches::kNaClDebugMask, "*://*/*debug.nmf" }
};
#endif

const Experiment::Choice kImplSidePaintingChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", "" },
  { IDS_GENERIC_EXPERIMENT_CHOICE_ENABLED,
    switches::kEnableImplSidePainting, ""},
  { IDS_GENERIC_EXPERIMENT_CHOICE_DISABLED,
    switches::kDisableImplSidePainting, ""}
};

const Experiment::Choice kLCDTextChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", "" },
  { IDS_GENERIC_EXPERIMENT_CHOICE_ENABLED, switches::kEnableLCDText, ""},
  { IDS_GENERIC_EXPERIMENT_CHOICE_DISABLED, switches::kDisableLCDText, ""}
};

const Experiment::Choice kDistanceFieldTextChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", "" },
  { IDS_GENERIC_EXPERIMENT_CHOICE_ENABLED,
    switches::kEnableDistanceFieldText, "" },
  { IDS_GENERIC_EXPERIMENT_CHOICE_DISABLED,
    switches::kDisableDistanceFieldText, "" }
};

#ifndef USE_AURA
const Experiment::Choice kDelegatedRendererChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", "" },
  { IDS_GENERIC_EXPERIMENT_CHOICE_ENABLED,
    switches::kEnableDelegatedRenderer, ""},
  { IDS_GENERIC_EXPERIMENT_CHOICE_DISABLED,
    switches::kDisableDelegatedRenderer, ""}
};
#endif

const Experiment::Choice kMaxTilesForInterestAreaChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", "" },
  { IDS_FLAGS_MAX_TILES_FOR_INTEREST_AREA_SHORT,
    cc::switches::kMaxTilesForInterestArea, "64"},
  { IDS_FLAGS_MAX_TILES_FOR_INTEREST_AREA_TALL,
    cc::switches::kMaxTilesForInterestArea, "128"},
  { IDS_FLAGS_MAX_TILES_FOR_INTEREST_AREA_GRANDE,
    cc::switches::kMaxTilesForInterestArea, "256"},
  { IDS_FLAGS_MAX_TILES_FOR_INTEREST_AREA_VENTI,
    cc::switches::kMaxTilesForInterestArea, "512"}
};

const Experiment::Choice kDefaultTileWidthChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", "" },
  { IDS_FLAGS_DEFAULT_TILE_WIDTH_SHORT,
    switches::kDefaultTileWidth, "128"},
  { IDS_FLAGS_DEFAULT_TILE_WIDTH_TALL,
    switches::kDefaultTileWidth, "256"},
  { IDS_FLAGS_DEFAULT_TILE_WIDTH_GRANDE,
    switches::kDefaultTileWidth, "512"},
  { IDS_FLAGS_DEFAULT_TILE_WIDTH_VENTI,
    switches::kDefaultTileWidth, "1024"}
};

const Experiment::Choice kDefaultTileHeightChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", "" },
  { IDS_FLAGS_DEFAULT_TILE_HEIGHT_SHORT,
    switches::kDefaultTileHeight, "128"},
  { IDS_FLAGS_DEFAULT_TILE_HEIGHT_TALL,
    switches::kDefaultTileHeight, "256"},
  { IDS_FLAGS_DEFAULT_TILE_HEIGHT_GRANDE,
    switches::kDefaultTileHeight, "512"},
  { IDS_FLAGS_DEFAULT_TILE_HEIGHT_VENTI,
    switches::kDefaultTileHeight, "1024"}
};

const Experiment::Choice kSimpleCacheBackendChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", "" },
  { IDS_GENERIC_EXPERIMENT_CHOICE_DISABLED,
    switches::kUseSimpleCacheBackend, "off" },
  { IDS_GENERIC_EXPERIMENT_CHOICE_ENABLED,
    switches::kUseSimpleCacheBackend, "on"}
};

#if defined(USE_AURA)
const Experiment::Choice kTabCaptureUpscaleQualityChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", "" },
  { IDS_FLAGS_TAB_CAPTURE_SCALE_QUALITY_FAST,
    switches::kTabCaptureUpscaleQuality, "fast" },
  { IDS_FLAGS_TAB_CAPTURE_SCALE_QUALITY_GOOD,
    switches::kTabCaptureUpscaleQuality, "good" },
  { IDS_FLAGS_TAB_CAPTURE_SCALE_QUALITY_BEST,
    switches::kTabCaptureUpscaleQuality, "best" },
};

const Experiment::Choice kTabCaptureDownscaleQualityChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", "" },
  { IDS_FLAGS_TAB_CAPTURE_SCALE_QUALITY_FAST,
    switches::kTabCaptureDownscaleQuality, "fast" },
  { IDS_FLAGS_TAB_CAPTURE_SCALE_QUALITY_GOOD,
    switches::kTabCaptureDownscaleQuality, "good" },
  { IDS_FLAGS_TAB_CAPTURE_SCALE_QUALITY_BEST,
    switches::kTabCaptureDownscaleQuality, "best" },
};
#endif

#if defined(USE_AURA) || defined(OS_LINUX)
const Experiment::Choice kOverlayScrollbarChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", "" },
  { IDS_GENERIC_EXPERIMENT_CHOICE_ENABLED,
    switches::kEnableOverlayScrollbar, ""},
  { IDS_GENERIC_EXPERIMENT_CHOICE_DISABLED,
    switches::kDisableOverlayScrollbar, ""}
};
#endif

const Experiment::Choice kZeroCopyChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", "" },
  { IDS_GENERIC_EXPERIMENT_CHOICE_ENABLED,
    switches::kEnableZeroCopy, ""},
  { IDS_GENERIC_EXPERIMENT_CHOICE_DISABLED,
    switches::kDisableZeroCopy, ""}
};

#if defined(OS_ANDROID)
const Experiment::Choice kZeroSuggestExperimentsChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", "" },
  { IDS_FLAGS_ZERO_SUGGEST_MOST_VISITED,
    switches::kEnableZeroSuggestMostVisited, ""},
  { IDS_FLAGS_ZERO_SUGGEST_ETHER_SERP,
    switches::kEnableZeroSuggestEtherSerp, ""},
  { IDS_FLAGS_ZERO_SUGGEST_ETHER_NO_SERP,
    switches::kEnableZeroSuggestEtherNoSerp, ""},
  { IDS_FLAGS_ZERO_SUGGEST_PERSONALIZED,
    switches::kEnableZeroSuggestPersonalized, ""},
  { IDS_GENERIC_EXPERIMENT_CHOICE_DISABLED,
    switches::kDisableZeroSuggest, ""}
};
#endif

const Experiment::Choice kNumRasterThreadsChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", "" },
  { IDS_FLAGS_NUM_RASTER_THREADS_ONE, switches::kNumRasterThreads, "1" },
  { IDS_FLAGS_NUM_RASTER_THREADS_TWO, switches::kNumRasterThreads, "2" },
  { IDS_FLAGS_NUM_RASTER_THREADS_THREE, switches::kNumRasterThreads, "3" },
  { IDS_FLAGS_NUM_RASTER_THREADS_FOUR, switches::kNumRasterThreads, "4" }
};

const Experiment::Choice kEnableGpuRasterizationChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", "" },
  { IDS_GENERIC_EXPERIMENT_CHOICE_ENABLED,
    switches::kEnableGpuRasterization, "" },
  { IDS_GENERIC_EXPERIMENT_CHOICE_DISABLED,
    switches::kDisableGpuRasterization, "" },
  { IDS_FLAGS_FORCE_GPU_RASTERIZATION,
    switches::kForceGpuRasterization, "" },
};

// We're using independent flags here (as opposed to a common flag with
// different values) to be able to enable/disable the entire experience
// associated with this feature server-side from the FieldTrial (the complete
// experience includes other flag changes as well). It is not currently possible
// to do that with "flag=value" flags.
const Experiment::Choice kSearchButtonInOmniboxChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", "" },
  { IDS_GENERIC_EXPERIMENT_CHOICE_DISABLED,
    switches::kDisableSearchButtonInOmnibox, ""},
  { IDS_FLAGS_SEARCH_BUTTON_IN_OMNIBOX_ENABLE_FOR_STR,
    switches::kEnableSearchButtonInOmniboxForStr, ""},
  { IDS_FLAGS_SEARCH_BUTTON_IN_OMNIBOX_ENABLE_FOR_STR_OR_IIP,
    switches::kEnableSearchButtonInOmniboxForStrOrIip, ""},
  { IDS_FLAGS_SEARCH_BUTTON_IN_OMNIBOX_ENABLED,
    switches::kEnableSearchButtonInOmniboxAlways, ""}
};

// See comment above for kSearchButtonInOmniboxChoices. The same reasoning
// applies here.
const Experiment::Choice kOriginChipChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", "" },
  { IDS_GENERIC_EXPERIMENT_CHOICE_DISABLED, switches::kDisableOriginChip, ""},
  { IDS_FLAGS_ORIGIN_CHIP_ALWAYS, switches::kEnableOriginChipAlways, ""},
  { IDS_FLAGS_ORIGIN_CHIP_ON_SRP, switches::kEnableOriginChipOnSrp, ""}
};

const Experiment::Choice kTouchScrollingModeChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", "" },
  { IDS_FLAGS_TOUCH_SCROLLING_MODE_TOUCHCANCEL,
    switches::kTouchScrollingMode,
    switches::kTouchScrollingModeTouchcancel },
  { IDS_FLAGS_TOUCH_SCROLLING_MODE_ASYNC_TOUCHMOVE,
    switches::kTouchScrollingMode,
    switches::kTouchScrollingModeAsyncTouchmove },
  { IDS_FLAGS_TOUCH_SCROLLING_MODE_SYNC_TOUCHMOVE,
    switches::kTouchScrollingMode,
    switches::kTouchScrollingModeSyncTouchmove },
};

#if defined(ENABLE_APP_LIST)
const Experiment::Choice kEnableSyncAppListChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", "" },
  { IDS_GENERIC_EXPERIMENT_CHOICE_ENABLED,
    app_list::switches::kEnableSyncAppList, "" },
  { IDS_GENERIC_EXPERIMENT_CHOICE_DISABLED,
    app_list::switches::kDisableSyncAppList, "" },
};
#endif

const Experiment::Choice kExtensionContentVerificationChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", "" },
  { IDS_FLAGS_EXTENSION_CONTENT_VERIFICATION_BOOTSTRAP,
    switches::kExtensionContentVerification,
    switches::kExtensionContentVerificationBootstrap },
  { IDS_FLAGS_EXTENSION_CONTENT_VERIFICATION_ENFORCE,
    switches::kExtensionContentVerification,
    switches::kExtensionContentVerificationEnforce },
  { IDS_FLAGS_EXTENSION_CONTENT_VERIFICATION_ENFORCE_STRICT,
    switches::kExtensionContentVerification,
    switches::kExtensionContentVerificationEnforceStrict },
};

#if defined(OS_ANDROID)
const Experiment::Choice kAnswersInSuggestChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", "" },
  { IDS_GENERIC_EXPERIMENT_CHOICE_ENABLED,
    switches::kEnableAnswersInSuggest, ""},
  { IDS_GENERIC_EXPERIMENT_CHOICE_DISABLED,
    switches::kDisableAnswersInSuggest, ""}
};
#endif

// Using independent flags (instead of flag=value flags) to be able to
// associate the version with a FieldTrial. FieldTrials don't currently support
// flag=value flags.
const Experiment::Choice kMalwareInterstitialVersions[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", "" },
  { IDS_FLAGS_MALWARE_INTERSTITIAL_VERSION_V2,
    switches::kMalwareInterstitialV2, "" },
  { IDS_FLAGS_MALWARE_INTERSTITIAL_VERSION_V3,
    switches::kMalwareInterstitialV3, "" },
  { IDS_FLAGS_MALWARE_INTERSTITIAL_VERSION_V3_ADVICE,
    switches::kMalwareInterstitialV3Advice, "" },
  { IDS_FLAGS_MALWARE_INTERSTITIAL_VERSION_V3_SOCIAL,
    switches::kMalwareInterstitialV3Social, "" },
  { IDS_FLAGS_MALWARE_INTERSTITIAL_VERSION_V3_NOTRECOMMEND,
    switches::kMalwareInterstitialV3NotRecommend, "" },
  { IDS_FLAGS_MALWARE_INTERSTITIAL_VERSION_V3_HISTORY,
    switches::kMalwareInterstitialV3History, "" },
};

#if defined(OS_CHROMEOS)
const Experiment::Choice kEnableFileManagerMTPChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", "" },
  { IDS_GENERIC_EXPERIMENT_CHOICE_ENABLED,
    chromeos::switches::kEnableFileManagerMTP, "true" },
  { IDS_GENERIC_EXPERIMENT_CHOICE_DISABLED,
    chromeos::switches::kEnableFileManagerMTP, "false" }
};

const Experiment::Choice kEnableFileManagerNewGalleryChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", ""},
  { IDS_GENERIC_EXPERIMENT_CHOICE_ENABLED,
    chromeos::switches::kFileManagerEnableNewGallery, "true"},
  { IDS_GENERIC_EXPERIMENT_CHOICE_DISABLED,
    chromeos::switches::kFileManagerEnableNewGallery, "false"}
};
#endif

const Experiment::Choice kEnableSettingsWindowChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", "" },
  { IDS_GENERIC_EXPERIMENT_CHOICE_ENABLED,
    ::switches::kEnableSettingsWindow, "" },
  { IDS_GENERIC_EXPERIMENT_CHOICE_DISABLED,
    ::switches::kDisableSettingsWindow, "" },
};

// Note that the value is specified in seconds (where 0 is equivalent to
// disabled).
const Experiment::Choice kRememberCertificateErrorDecisionsChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", "" },
  { IDS_GENERIC_EXPERIMENT_CHOICE_DISABLED,
    switches::kRememberCertErrorDecisions,
    "-1" },
  { IDS_REMEMBER_CERTIFICATE_ERROR_DECISION_CHOICE_ONE_DAY,
    switches::kRememberCertErrorDecisions,
    "86400" },
  { IDS_REMEMBER_CERTIFICATE_ERROR_DECISION_CHOICE_THREE_DAYS,
    switches::kRememberCertErrorDecisions,
    "259200" },
  { IDS_REMEMBER_CERTIFICATE_ERROR_DECISION_CHOICE_ONE_WEEK,
    switches::kRememberCertErrorDecisions,
    "604800" },
  { IDS_REMEMBER_CERTIFICATE_ERROR_DECISION_CHOICE_ONE_MONTH,
    switches::kRememberCertErrorDecisions,
    "2592000" },
  { IDS_REMEMBER_CERTIFICATE_ERROR_DECISION_CHOICE_THREE_MONTHS,
    switches::kRememberCertErrorDecisions,
    "7776000" },
};

const Experiment::Choice kEnableDropSyncCredentialChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", ""},
  { IDS_GENERIC_EXPERIMENT_CHOICE_ENABLED,
    password_manager::switches::kEnableDropSyncCredential, "" },
  { IDS_GENERIC_EXPERIMENT_CHOICE_DISABLED,
    password_manager::switches::kDisableDropSyncCredential, "" },
};

// RECORDING USER METRICS FOR FLAGS:
// -----------------------------------------------------------------------------
// The first line of the experiment is the internal name. If you'd like to
// gather statistics about the usage of your flag, you should append a marker
// comment to the end of the feature name, like so:
//   "my-special-feature",  // FLAGS:RECORD_UMA
//
// After doing that, run
//   tools/metrics/actions/extract_actions.py
// to add the metric to actions.xml (which will enable UMA to record your
// feature flag), then update the <owner>s and <description> sections. Make sure
// to include the actions.xml file when you upload your code for review!
//
// After your feature has shipped under a flag, you can locate the metrics under
// the action name AboutFlags_internal-action-name. Actions are recorded once
// per startup, so you should divide this number by AboutFlags_StartupTick to
// get a sense of usage. Note that this will not be the same as number of users
// with a given feature enabled because users can quit and relaunch the
// application multiple times over a given time interval. The dashboard also
// shows you how many (metrics reporting) users have enabled the flag over the
// last seven days. However, note that this is not the same as the number of
// users who have the flag enabled, since enabling the flag happens once,
// whereas running with the flag enabled happens until the user flips the flag
// again.

// To add a new experiment add to the end of kExperiments. There are two
// distinct types of experiments:
// . SINGLE_VALUE: experiment is either on or off. Use the SINGLE_VALUE_TYPE
//   macro for this type supplying the command line to the macro.
// . MULTI_VALUE: a list of choices, the first of which should correspond to a
//   deactivated state for this lab (i.e. no command line option).  To specify
//   this type of experiment use the macro MULTI_VALUE_TYPE supplying it the
//   array of choices.
// See the documentation of Experiment for details on the fields.
//
// When adding a new choice, add it to the end of the list.
const Experiment kExperiments[] = {
  {
    "ignore-gpu-blacklist",
    IDS_FLAGS_IGNORE_GPU_BLACKLIST_NAME,
    IDS_FLAGS_IGNORE_GPU_BLACKLIST_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kIgnoreGpuBlacklist)
  },
  {
    "force-accelerated-composited-scrolling",
     IDS_FLAGS_FORCE_ACCELERATED_OVERFLOW_SCROLL_MODE_NAME,
     IDS_FLAGS_FORCE_ACCELERATED_OVERFLOW_SCROLL_MODE_DESCRIPTION,
     kOsAll,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableAcceleratedOverflowScroll,
                               switches::kDisableAcceleratedOverflowScroll)
  },
  {
    "disable_layer_squashing",
    IDS_FLAGS_DISABLE_LAYER_SQUASHING_NAME,
    IDS_FLAGS_DISABLE_LAYER_SQUASHING_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kDisableLayerSquashing)
  },
#if defined(OS_WIN)
  {
    "disable-direct-write",
    IDS_FLAGS_DISABLE_DIRECT_WRITE_NAME,
    IDS_FLAGS_DISABLE_DIRECT_WRITE_DESCRIPTION,
    kOsWin,
    SINGLE_VALUE_TYPE(switches::kDisableDirectWrite)
  },
#endif
  {
    "enable-experimental-canvas-features",
    IDS_FLAGS_ENABLE_EXPERIMENTAL_CANVAS_FEATURES_NAME,
    IDS_FLAGS_ENABLE_EXPERIMENTAL_CANVAS_FEATURES_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kEnableExperimentalCanvasFeatures)
  },
  {
    "disable-accelerated-2d-canvas",
    IDS_FLAGS_DISABLE_ACCELERATED_2D_CANVAS_NAME,
    IDS_FLAGS_DISABLE_ACCELERATED_2D_CANVAS_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kDisableAccelerated2dCanvas)
  },
  {
    "enable-display-list-2d-canvas",
    IDS_FLAGS_ENABLE_DISPLAY_LIST_2D_CANVAS_NAME,
    IDS_FLAGS_ENABLE_DISPLAY_LIST_2D_CANVAS_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kEnableDisplayList2dCanvas)
  },
  {
    "composited-layer-borders",
    IDS_FLAGS_COMPOSITED_LAYER_BORDERS,
    IDS_FLAGS_COMPOSITED_LAYER_BORDERS_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(cc::switches::kShowCompositedLayerBorders)
  },
  {
    "show-fps-counter",
    IDS_FLAGS_SHOW_FPS_COUNTER,
    IDS_FLAGS_SHOW_FPS_COUNTER_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(cc::switches::kShowFPSCounter)
  },
  {
    "disable-webgl",
    IDS_FLAGS_DISABLE_WEBGL_NAME,
    IDS_FLAGS_DISABLE_WEBGL_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kDisableExperimentalWebGL)
  },
  {
    "disable-webrtc",
    IDS_FLAGS_DISABLE_WEBRTC_NAME,
    IDS_FLAGS_DISABLE_WEBRTC_DESCRIPTION,
    kOsAndroid,
#if defined(OS_ANDROID)
    SINGLE_VALUE_TYPE(switches::kDisableWebRTC)
#else
    SINGLE_VALUE_TYPE("")
#endif
  },
#if defined(ENABLE_WEBRTC)
  {
    "disable-webrtc-hw-decoding",
    IDS_FLAGS_DISABLE_WEBRTC_HW_DECODING_NAME,
    IDS_FLAGS_DISABLE_WEBRTC_HW_DECODING_DESCRIPTION,
    kOsAndroid | kOsCrOS,
    SINGLE_VALUE_TYPE(switches::kDisableWebRtcHWDecoding)
  },
  {
    "disable-webrtc-hw-encoding",
    IDS_FLAGS_DISABLE_WEBRTC_HW_ENCODING_NAME,
    IDS_FLAGS_DISABLE_WEBRTC_HW_ENCODING_DESCRIPTION,
    kOsAndroid | kOsCrOS,
    SINGLE_VALUE_TYPE(switches::kDisableWebRtcHWEncoding)
  },
#endif
#if defined(OS_ANDROID)
  {
    "disable-webaudio",
    IDS_FLAGS_DISABLE_WEBAUDIO_NAME,
    IDS_FLAGS_DISABLE_WEBAUDIO_DESCRIPTION,
    kOsAndroid,
    SINGLE_VALUE_TYPE(switches::kDisableWebAudio)
  },
#endif
  {
    "enable-compositing-for-fixed-position",
    IDS_FLAGS_COMPOSITING_FOR_FIXED_POSITION_NAME,
    IDS_FLAGS_COMPOSITING_FOR_FIXED_POSITION_DESCRIPTION,
    kOsAll,
    MULTI_VALUE_TYPE(kEnableCompositingForFixedPositionChoices)
  },
  {
    "enable-compositing-for-transition",
    IDS_FLAGS_COMPOSITING_FOR_TRANSITION_NAME,
    IDS_FLAGS_COMPOSITING_FOR_TRANSITION_DESCRIPTION,
    kOsAll,
    MULTI_VALUE_TYPE(kEnableCompositingForTransitionChoices)
  },
  {
    "enable-accelerated-fixed-root-background",
    IDS_FLAGS_ACCELERATED_FIXED_ROOT_BACKGROUND_NAME,
    IDS_FLAGS_ACCELERATED_FIXED_ROOT_BACKGROUND_DESCRIPTION,
    kOsAll,
    MULTI_VALUE_TYPE(kEnableAcceleratedFixedRootBackgroundChoices)
  },
  // Native client is compiled out when DISABLE_NACL is defined.
#if !defined(DISABLE_NACL)
  {
    "enable-nacl",  // FLAGS:RECORD_UMA
    IDS_FLAGS_ENABLE_NACL_NAME,
    IDS_FLAGS_ENABLE_NACL_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kEnableNaCl)
  },
  {
    "enable-nacl-debug",  // FLAGS:RECORD_UMA
    IDS_FLAGS_ENABLE_NACL_DEBUG_NAME,
    IDS_FLAGS_ENABLE_NACL_DEBUG_DESCRIPTION,
    kOsDesktop,
    SINGLE_VALUE_TYPE(switches::kEnableNaClDebug)
  },
  {
    "nacl-debug-mask",  // FLAGS:RECORD_UMA
    IDS_FLAGS_NACL_DEBUG_MASK_NAME,
    IDS_FLAGS_NACL_DEBUG_MASK_DESCRIPTION,
    kOsDesktop,
    MULTI_VALUE_TYPE(kNaClDebugMaskChoices)
  },
#endif
  {
    "extension-apis",  // FLAGS:RECORD_UMA
    IDS_FLAGS_EXPERIMENTAL_EXTENSION_APIS_NAME,
    IDS_FLAGS_EXPERIMENTAL_EXTENSION_APIS_DESCRIPTION,
    kOsDesktop,
    SINGLE_VALUE_TYPE(extensions::switches::kEnableExperimentalExtensionApis)
  },
  {
    "extensions-on-chrome-urls",
    IDS_FLAGS_EXTENSIONS_ON_CHROME_URLS_NAME,
    IDS_FLAGS_EXTENSIONS_ON_CHROME_URLS_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(extensions::switches::kExtensionsOnChromeURLs)
  },
  {
    "enable-fast-unload",
    IDS_FLAGS_ENABLE_FAST_UNLOAD_NAME,
    IDS_FLAGS_ENABLE_FAST_UNLOAD_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kEnableFastUnload)
  },
  {
    "enable-app-window-controls",
    IDS_FLAGS_ENABLE_APP_WINDOW_CONTROLS_NAME,
    IDS_FLAGS_ENABLE_APP_WINDOW_CONTROLS_DESCRIPTION,
    kOsDesktop,
    SINGLE_VALUE_TYPE(switches::kEnableAppWindowControls)
  },
  {
    "disable-hyperlink-auditing",
    IDS_FLAGS_DISABLE_HYPERLINK_AUDITING_NAME,
    IDS_FLAGS_DISABLE_HYPERLINK_AUDITING_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kNoPings)
  },
#if defined(OS_ANDROID)
  {
    "contextual-search",
    IDS_FLAGS_ENABLE_CONTEXTUAL_SEARCH,
    IDS_FLAGS_ENABLE_CONTEXTUAL_SEARCH_DESCRIPTION,
    kOsAndroid,
    ENABLE_DISABLE_VALUE_TYPE(switches::kEnableContextualSearch,
                              switches::kDisableContextualSearch)
  },
#endif
  {
    "show-autofill-type-predictions",
    IDS_FLAGS_SHOW_AUTOFILL_TYPE_PREDICTIONS_NAME,
    IDS_FLAGS_SHOW_AUTOFILL_TYPE_PREDICTIONS_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(autofill::switches::kShowAutofillTypePredictions)
  },
  {
    "enable-smooth-scrolling",  // FLAGS:RECORD_UMA
    IDS_FLAGS_ENABLE_SMOOTH_SCROLLING_NAME,
    IDS_FLAGS_ENABLE_SMOOTH_SCROLLING_DESCRIPTION,
    // Can't expose the switch unless the code is compiled in.
    // On by default for the Mac (different implementation in WebKit).
    kOsLinux,
    SINGLE_VALUE_TYPE(switches::kEnableSmoothScrolling)
  },
#if defined(USE_AURA) || defined(OS_LINUX)
  {
    "overlay-scrollbars",
    IDS_FLAGS_ENABLE_OVERLAY_SCROLLBARS_NAME,
    IDS_FLAGS_ENABLE_OVERLAY_SCROLLBARS_DESCRIPTION,
    // Uses the system preference on Mac (a different implementation).
    // On Android, this is always enabled.
    kOsLinux | kOsCrOS | kOsWin,
    MULTI_VALUE_TYPE(kOverlayScrollbarChoices)
  },
#endif
  {
    "enable-panels",
    IDS_FLAGS_ENABLE_PANELS_NAME,
    IDS_FLAGS_ENABLE_PANELS_DESCRIPTION,
    kOsDesktop,
    SINGLE_VALUE_TYPE(switches::kEnablePanels)
  },
  {
    // See http://crbug.com/120416 for how to remove this flag.
    "save-page-as-mhtml",  // FLAGS:RECORD_UMA
    IDS_FLAGS_SAVE_PAGE_AS_MHTML_NAME,
    IDS_FLAGS_SAVE_PAGE_AS_MHTML_DESCRIPTION,
    kOsMac | kOsWin | kOsLinux,
    SINGLE_VALUE_TYPE(switches::kSavePageAsMHTML)
  },
  {
    "enable-quic",
    IDS_FLAGS_ENABLE_QUIC_NAME,
    IDS_FLAGS_ENABLE_QUIC_DESCRIPTION,
    kOsAll,
    ENABLE_DISABLE_VALUE_TYPE(switches::kEnableQuic,
                              switches::kDisableQuic)
  },
  {
    "enable-spdy4",
    IDS_FLAGS_ENABLE_SPDY4_NAME,
    IDS_FLAGS_ENABLE_SPDY4_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kEnableSpdy4)
  },
  {
    "enable-async-dns",
    IDS_FLAGS_ENABLE_ASYNC_DNS_NAME,
    IDS_FLAGS_ENABLE_ASYNC_DNS_DESCRIPTION,
    kOsWin | kOsMac | kOsLinux | kOsCrOS,
    ENABLE_DISABLE_VALUE_TYPE(switches::kEnableAsyncDns,
                              switches::kDisableAsyncDns)
  },
  {
    "disable-media-source",
    IDS_FLAGS_DISABLE_MEDIA_SOURCE_NAME,
    IDS_FLAGS_DISABLE_MEDIA_SOURCE_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kDisableMediaSource)
  },
  {
    "enable-encrypted-media",
    IDS_FLAGS_ENABLE_ENCRYPTED_MEDIA_NAME,
    IDS_FLAGS_ENABLE_ENCRYPTED_MEDIA_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kEnableEncryptedMedia)
  },
  {
    "disable-prefixed-encrypted-media",
    IDS_FLAGS_DISABLE_PREFIXED_ENCRYPTED_MEDIA_NAME,
    IDS_FLAGS_DISABLE_PREFIXED_ENCRYPTED_MEDIA_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kDisablePrefixedEncryptedMedia)
  },
#if defined(OS_ANDROID)
  {
    "disable-infobar-for-protected-media-identifier",
    IDS_FLAGS_DISABLE_INFOBAR_FOR_PROTECTED_MEDIA_IDENTIFIER_NAME,
    IDS_FLAGS_DISABLE_INFOBAR_FOR_PROTECTED_MEDIA_IDENTIFIER_DESCRIPTION,
    kOsAndroid,
    SINGLE_VALUE_TYPE(switches::kDisableInfobarForProtectedMediaIdentifier)
  },
  {
    "mediadrm-enable-non-compositing",
    IDS_FLAGS_MEDIADRM_ENABLE_NON_COMPOSITING_NAME,
    IDS_FLAGS_MEDIADRM_ENABLE_NON_COMPOSITING_DESCRIPTION,
    kOsAndroid,
    SINGLE_VALUE_TYPE(switches::kMediaDrmEnableNonCompositing)
  },
#endif  // defined(OS_ANDROID)
  {
    "enable-javascript-harmony",
    IDS_FLAGS_ENABLE_JAVASCRIPT_HARMONY_NAME,
    IDS_FLAGS_ENABLE_JAVASCRIPT_HARMONY_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE_AND_VALUE(switches::kJavaScriptFlags, "--harmony")
  },
  {
    "disable-software-rasterizer",
    IDS_FLAGS_DISABLE_SOFTWARE_RASTERIZER_NAME,
    IDS_FLAGS_DISABLE_SOFTWARE_RASTERIZER_DESCRIPTION,
#if defined(ENABLE_SWIFTSHADER)
    kOsAll,
#else
    0,
#endif
    SINGLE_VALUE_TYPE(switches::kDisableSoftwareRasterizer)
  },
  {
    "enable-gpu-rasterization",
    IDS_FLAGS_ENABLE_GPU_RASTERIZATION_NAME,
    IDS_FLAGS_ENABLE_GPU_RASTERIZATION_DESCRIPTION,
    kOsAndroid,
    MULTI_VALUE_TYPE(kEnableGpuRasterizationChoices)
  },
  {
    "enable-experimental-web-platform-features",
    IDS_FLAGS_EXPERIMENTAL_WEB_PLATFORM_FEATURES_NAME,
    IDS_FLAGS_EXPERIMENTAL_WEB_PLATFORM_FEATURES_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kEnableExperimentalWebPlatformFeatures)
  },
  {
    "disable-ntp-other-sessions-menu",
    IDS_FLAGS_NTP_OTHER_SESSIONS_MENU_NAME,
    IDS_FLAGS_NTP_OTHER_SESSIONS_MENU_DESCRIPTION,
    kOsDesktop,
    SINGLE_VALUE_TYPE(switches::kDisableNTPOtherSessionsMenu)
  },
  {
    "enable-material-design-ntp",
    IDS_FLAGS_ENABLE_MATERIAL_DESIGN_NTP_NAME,
    IDS_FLAGS_ENABLE_MATERIAL_DESIGN_NTP_DESCRIPTION,
    kOsDesktop,
    ENABLE_DISABLE_VALUE_TYPE(switches::kEnableMaterialDesignNTP,
                              switches::kDisableMaterialDesignNTP)
  },
  {
    "enable-devtools-experiments",
    IDS_FLAGS_ENABLE_DEVTOOLS_EXPERIMENTS_NAME,
    IDS_FLAGS_ENABLE_DEVTOOLS_EXPERIMENTS_DESCRIPTION,
    kOsDesktop,
    SINGLE_VALUE_TYPE(switches::kEnableDevToolsExperiments)
  },
  {
    "silent-debugger-extension-api",
    IDS_FLAGS_SILENT_DEBUGGER_EXTENSION_API_NAME,
    IDS_FLAGS_SILENT_DEBUGGER_EXTENSION_API_DESCRIPTION,
    kOsDesktop,
    SINGLE_VALUE_TYPE(switches::kSilentDebuggerExtensionAPI)
  },
  {
    "spellcheck-autocorrect",
    IDS_FLAGS_SPELLCHECK_AUTOCORRECT,
    IDS_FLAGS_SPELLCHECK_AUTOCORRECT_DESCRIPTION,
    kOsWin | kOsLinux | kOsCrOS,
    SINGLE_VALUE_TYPE(switches::kEnableSpellingAutoCorrect)
  },
  {
    "enable-scroll-prediction",
    IDS_FLAGS_ENABLE_SCROLL_PREDICTION_NAME,
    IDS_FLAGS_ENABLE_SCROLL_PREDICTION_DESCRIPTION,
    kOsDesktop,
    SINGLE_VALUE_TYPE(switches::kEnableScrollPrediction)
  },
  {
    "touch-events",
    IDS_TOUCH_EVENTS_NAME,
    IDS_TOUCH_EVENTS_DESCRIPTION,
    kOsDesktop,
    MULTI_VALUE_TYPE(kTouchEventsChoices)
  },
  {
    "disable-touch-adjustment",
    IDS_DISABLE_TOUCH_ADJUSTMENT_NAME,
    IDS_DISABLE_TOUCH_ADJUSTMENT_DESCRIPTION,
    kOsWin | kOsLinux | kOsCrOS,
    SINGLE_VALUE_TYPE(switches::kDisableTouchAdjustment)
  },
#if defined(OS_CHROMEOS)
  {
    "network-portal-notification",
    IDS_FLAGS_NETWORK_PORTAL_NOTIFICATION_NAME,
    IDS_FLAGS_NETWORK_PORTAL_NOTIFICATION_DESCRIPTION,
    kOsCrOS,
    ENABLE_DISABLE_VALUE_TYPE(
        chromeos::switches::kEnableNetworkPortalNotification,
        chromeos::switches::kDisableNetworkPortalNotification)
  },
#endif
  {
    "enable-download-resumption",
    IDS_FLAGS_ENABLE_DOWNLOAD_RESUMPTION_NAME,
    IDS_FLAGS_ENABLE_DOWNLOAD_RESUMPTION_DESCRIPTION,
    kOsDesktop,
    SINGLE_VALUE_TYPE(switches::kEnableDownloadResumption)
  },
#if defined(ENABLE_PLUGINS)
  {
    "allow-nacl-socket-api",
    IDS_FLAGS_ALLOW_NACL_SOCKET_API_NAME,
    IDS_FLAGS_ALLOW_NACL_SOCKET_API_DESCRIPTION,
    kOsDesktop,
    SINGLE_VALUE_TYPE_AND_VALUE(switches::kAllowNaClSocketAPI, "*")
  },
#endif
#if defined(OS_CHROMEOS) || defined(OS_WIN) || defined(OS_LINUX)
  {
    "force-device-scale-factor",
    IDS_FLAGS_FORCE_DEVICE_SCALE_FACTOR_NAME,
    IDS_FLAGS_FORCE_DEVICE_SCALE_FACTOR_DESCRIPTION,
    kOsLinux | kOsWin | kOsCrOS,
    MULTI_VALUE_TYPE(kDeviceScaleFactorChoices)
  },
#endif
#if defined(OS_CHROMEOS)
  {
    "allow-touchpad-three-finger-click",
    IDS_FLAGS_ALLOW_TOUCHPAD_THREE_FINGER_CLICK_NAME,
    IDS_FLAGS_ALLOW_TOUCHPAD_THREE_FINGER_CLICK_DESCRIPTION,
    kOsCrOS,
    SINGLE_VALUE_TYPE(chromeos::switches::kEnableTouchpadThreeFingerClick)
  },
#endif
#if defined(USE_ASH)
  {
    "disable-minimize-on-second-launcher-item-click",
    IDS_FLAGS_DISABLE_MINIMIZE_ON_SECOND_LAUNCHER_ITEM_CLICK_NAME,
    IDS_FLAGS_DISABLE_MINIMIZE_ON_SECOND_LAUNCHER_ITEM_CLICK_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kDisableMinimizeOnSecondLauncherItemClick)
  },
  {
    "show-touch-hud",
    IDS_FLAGS_SHOW_TOUCH_HUD_NAME,
    IDS_FLAGS_SHOW_TOUCH_HUD_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(ash::switches::kAshTouchHud)
  },
  {
    "enable-pinch",
    IDS_FLAGS_ENABLE_PINCH_SCALE_NAME,
    IDS_FLAGS_ENABLE_PINCH_SCALE_DESCRIPTION,
    kOsLinux | kOsWin | kOsCrOS,
    ENABLE_DISABLE_VALUE_TYPE(switches::kEnablePinch, switches::kDisablePinch),
  },
#endif  // defined(USE_ASH)
  {
    "enable-pinch-virtual-viewport",
    IDS_FLAGS_ENABLE_PINCH_VIRTUAL_VIEWPORT_NAME,
    IDS_FLAGS_ENABLE_PINCH_VIRTUAL_VIEWPORT_DESCRIPTION,
    kOsLinux | kOsWin | kOsCrOS | kOsAndroid,
    ENABLE_DISABLE_VALUE_TYPE(
        cc::switches::kEnablePinchVirtualViewport,
        cc::switches::kDisablePinchVirtualViewport),
  },
  {
    "enable-viewport-meta",
    IDS_FLAGS_ENABLE_VIEWPORT_META_NAME,
    IDS_FLAGS_ENABLE_VIEWPORT_META_DESCRIPTION,
    kOsLinux | kOsWin | kOsCrOS | kOsMac,
    SINGLE_VALUE_TYPE(switches::kEnableViewportMeta),
  },
#if defined(OS_CHROMEOS)
  {
    "disable-boot-animation",
    IDS_FLAGS_DISABLE_BOOT_ANIMATION,
    IDS_FLAGS_DISABLE_BOOT_ANIMATION_DESCRIPTION,
    kOsCrOSOwnerOnly,
    SINGLE_VALUE_TYPE(chromeos::switches::kDisableBootAnimation),
  },
  {
    "enable-new-gallery",
    IDS_FLAGS_FILE_MANAGER_ENABLE_NEW_GALLERY_NAME,
    IDS_FLAGS_FILE_MANAGER_ENABLE_NEW_GALLERY_DESCRIPTION,
    kOsCrOS,
    MULTI_VALUE_TYPE(kEnableFileManagerNewGalleryChoices)
  },
  {
    "enable-video-player-chromecast-support",
    IDS_FLAGS_ENABLE_VIDEO_PLAYER_CHROMECAST_SUPPORT_NAME,
    IDS_FLAGS_ENABLE_VIDEO_PLAYER_CHROMECAST_SUPPORT_DESCRIPTION,
    kOsCrOS,
    SINGLE_VALUE_TYPE(chromeos::switches::kEnableVideoPlayerChromecastSupport)
  },
  {
    "disable-office-editing-component-app",
    IDS_FLAGS_DISABLE_OFFICE_EDITING_COMPONENT_APP_NAME,
    IDS_FLAGS_DISABLE_OFFICE_EDITING_COMPONENT_APP_DESCRIPTION,
    kOsCrOS,
    SINGLE_VALUE_TYPE(chromeos::switches::kDisableOfficeEditingComponentApp),
  },
  {
    "disable-display-color-calibration",
    IDS_FLAGS_DISABLE_DISPLAY_COLOR_CALIBRATION_NAME,
    IDS_FLAGS_DISABLE_DISPLAY_COLOR_CALIBRATION_DESCRIPTION,
    kOsCrOS,
    SINGLE_VALUE_TYPE(ui::switches::kDisableDisplayColorCalibration),
  },
#endif  // defined(OS_CHROMEOS)
  { "disable-accelerated-video-decode",
    IDS_FLAGS_DISABLE_ACCELERATED_VIDEO_DECODE_NAME,
    IDS_FLAGS_DISABLE_ACCELERATED_VIDEO_DECODE_DESCRIPTION,
    kOsWin | kOsCrOS,
    SINGLE_VALUE_TYPE(switches::kDisableAcceleratedVideoDecode),
  },
#if defined(USE_ASH)
  {
    "ash-debug-shortcuts",
    IDS_FLAGS_DEBUG_SHORTCUTS_NAME,
    IDS_FLAGS_DEBUG_SHORTCUTS_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(ash::switches::kAshDebugShortcuts),
  },
  { "ash-enable-touch-view-testing",
    IDS_FLAGS_ASH_ENABLE_TOUCH_VIEW_TESTING_NAME,
    IDS_FLAGS_ASH_ENABLE_TOUCH_VIEW_TESTING_DESCRIPTION,
    kOsCrOS,
    SINGLE_VALUE_TYPE(ash::switches::kAshEnableTouchViewTesting),
  },
  { "ash-disable-text-filtering-in-overview-mode",
    IDS_FLAGS_ASH_DISABLE_TEXT_FILTERING_IN_OVERVIEW_MODE_NAME,
    IDS_FLAGS_ASH_DISABLE_TEXT_FILTERING_IN_OVERVIEW_MODE_DESCRIPTION,
    kOsCrOS,
    SINGLE_VALUE_TYPE(ash::switches::kAshDisableTextFilteringInOverviewMode),
  },
#endif
#if defined(OS_CHROMEOS)
  {
    "enable-carrier-switching",
    IDS_FLAGS_ENABLE_CARRIER_SWITCHING,
    IDS_FLAGS_ENABLE_CARRIER_SWITCHING_DESCRIPTION,
    kOsCrOS,
    SINGLE_VALUE_TYPE(chromeos::switches::kEnableCarrierSwitching)
  },
  {
    "enable-request-tablet-site",
    IDS_FLAGS_ENABLE_REQUEST_TABLET_SITE_NAME,
    IDS_FLAGS_ENABLE_REQUEST_TABLET_SITE_DESCRIPTION,
    kOsCrOS,
    SINGLE_VALUE_TYPE(chromeos::switches::kEnableRequestTabletSite)
  },
#endif
  {
    "debug-packed-apps",
    IDS_FLAGS_DEBUG_PACKED_APP_NAME,
    IDS_FLAGS_DEBUG_PACKED_APP_DESCRIPTION,
    kOsDesktop,
    SINGLE_VALUE_TYPE(switches::kDebugPackedApps)
  },
  {
    "enable-password-generation",
    IDS_FLAGS_ENABLE_PASSWORD_GENERATION_NAME,
    IDS_FLAGS_ENABLE_PASSWORD_GENERATION_DESCRIPTION,
    kOsWin | kOsLinux | kOsCrOS,
    ENABLE_DISABLE_VALUE_TYPE(autofill::switches::kEnablePasswordGeneration,
                              autofill::switches::kDisablePasswordGeneration)
  },
  {
    "enable-automatic-password-saving",
    IDS_FLAGS_ENABLE_AUTOMATIC_PASSWORD_SAVING_NAME,
    IDS_FLAGS_ENABLE_AUTOMATIC_PASSWORD_SAVING_DESCRIPTION,
    kOsDesktop,
    SINGLE_VALUE_TYPE(
        password_manager::switches::kEnableAutomaticPasswordSaving)
  },
  {
    "password-manager-reauthentication",
    IDS_FLAGS_PASSWORD_MANAGER_REAUTHENTICATION_NAME,
    IDS_FLAGS_PASSWORD_MANAGER_REAUTHENTICATION_DESCRIPTION,
    kOsMac | kOsWin,
    SINGLE_VALUE_TYPE(switches::kDisablePasswordManagerReauthentication)
  },
  {
    "enable-deferred-image-decoding",
    IDS_FLAGS_ENABLE_DEFERRED_IMAGE_DECODING_NAME,
    IDS_FLAGS_ENABLE_DEFERRED_IMAGE_DECODING_DESCRIPTION,
    kOsMac | kOsLinux | kOsCrOS,
    SINGLE_VALUE_TYPE(switches::kEnableDeferredImageDecoding)
  },
  {
    "performance-monitor-gathering",
    IDS_FLAGS_PERFORMANCE_MONITOR_GATHERING_NAME,
    IDS_FLAGS_PERFORMANCE_MONITOR_GATHERING_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kPerformanceMonitorGathering)
  },
  {
    "wallet-service-use-sandbox",
    IDS_FLAGS_WALLET_SERVICE_USE_SANDBOX_NAME,
    IDS_FLAGS_WALLET_SERVICE_USE_SANDBOX_DESCRIPTION,
    kOsAndroid | kOsDesktop,
    ENABLE_DISABLE_VALUE_TYPE_AND_VALUE(
        autofill::switches::kWalletServiceUseSandbox, "1",
        autofill::switches::kWalletServiceUseSandbox, "0")
  },
#if defined(USE_AURA)
  {
    "overscroll-history-navigation",
    IDS_FLAGS_OVERSCROLL_HISTORY_NAVIGATION_NAME,
    IDS_FLAGS_OVERSCROLL_HISTORY_NAVIGATION_DESCRIPTION,
    kOsAll,
    MULTI_VALUE_TYPE(kOverscrollHistoryNavigationChoices)
  },
#endif
  {
    "scroll-end-effect",
    IDS_FLAGS_SCROLL_END_EFFECT_NAME,
    IDS_FLAGS_SCROLL_END_EFFECT_DESCRIPTION,
    kOsCrOS,
    ENABLE_DISABLE_VALUE_TYPE_AND_VALUE(
        switches::kScrollEndEffect, "1",
        switches::kScrollEndEffect, "0")
  },
  {
    "enable-renderer-mojo-channel",
    IDS_FLAGS_ENABLE_RENDERER_MOJO_CHANNEL_NAME,
    IDS_FLAGS_ENABLE_RENDERER_MOJO_CHANNEL_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kEnableRendererMojoChannel)
  },
  {
    "enable-touch-drag-drop",
    IDS_FLAGS_ENABLE_TOUCH_DRAG_DROP_NAME,
    IDS_FLAGS_ENABLE_TOUCH_DRAG_DROP_DESCRIPTION,
    kOsWin | kOsCrOS,
    ENABLE_DISABLE_VALUE_TYPE(switches::kEnableTouchDragDrop,
                              switches::kDisableTouchDragDrop)
  },
  {
    "enable-touch-editing",
    IDS_FLAGS_ENABLE_TOUCH_EDITING_NAME,
    IDS_FLAGS_ENABLE_TOUCH_EDITING_DESCRIPTION,
    kOsCrOS | kOsWin | kOsLinux,
    ENABLE_DISABLE_VALUE_TYPE(switches::kEnableTouchEditing,
                              switches::kDisableTouchEditing)
  },
  {
    "enable-suggestions-service",
    IDS_FLAGS_ENABLE_SUGGESTIONS_SERVICE_NAME,
    IDS_FLAGS_ENABLE_SUGGESTIONS_SERVICE_DESCRIPTION,
    kOsAndroid | kOsCrOS,
    ENABLE_DISABLE_VALUE_TYPE(switches::kEnableSuggestionsService,
                              switches::kDisableSuggestionsService)
  },
  {
    "enable-sync-synced-notifications",
    IDS_FLAGS_ENABLE_SYNCED_NOTIFICATIONS_NAME,
    IDS_FLAGS_ENABLE_SYNCED_NOTIFICATIONS_DESCRIPTION,
    kOsDesktop,
    ENABLE_DISABLE_VALUE_TYPE(switches::kEnableSyncSyncedNotifications,
                              switches::kDisableSyncSyncedNotifications)
  },
#if defined(ENABLE_APP_LIST)
  {
    "enable-sync-app-list",
    IDS_FLAGS_ENABLE_SYNC_APP_LIST_NAME,
    IDS_FLAGS_ENABLE_SYNC_APP_LIST_DESCRIPTION,
    kOsDesktop,
    MULTI_VALUE_TYPE(kEnableSyncAppListChoices)
  },
#endif
#if defined(OS_MACOSX)
  {
    "enable-avfoundation",
    IDS_FLAGS_ENABLE_AVFOUNDATION_NAME,
    IDS_FLAGS_ENABLE_AVFOUNDATION_DESCRIPTION,
    kOsMac,
    SINGLE_VALUE_TYPE(switches::kEnableAVFoundation)
  },
#endif
  {
    "impl-side-painting",
    IDS_FLAGS_IMPL_SIDE_PAINTING_NAME,
    IDS_FLAGS_IMPL_SIDE_PAINTING_DESCRIPTION,
    kOsAll,
    MULTI_VALUE_TYPE(kImplSidePaintingChoices)
  },
  {
    "lcd-text-aa",
    IDS_FLAGS_LCD_TEXT_NAME,
    IDS_FLAGS_LCD_TEXT_DESCRIPTION,
    kOsDesktop,
    MULTI_VALUE_TYPE(kLCDTextChoices)
  },
#if defined(OS_ANDROID) || defined(OS_MACOSX)
  {
    "delegated-renderer",
    IDS_FLAGS_DELEGATED_RENDERER_NAME,
    IDS_FLAGS_DELEGATED_RENDERER_DESCRIPTION,
    kOsAndroid,  // TODO(ccameron) Add mac support soon.
    MULTI_VALUE_TYPE(kDelegatedRendererChoices)
  },
#endif
  {
    "max-tiles-for-interest-area",
    IDS_FLAGS_MAX_TILES_FOR_INTEREST_AREA_NAME,
    IDS_FLAGS_MAX_TILES_FOR_INTEREST_AREA_DESCRIPTION,
    kOsAll,
    MULTI_VALUE_TYPE(kMaxTilesForInterestAreaChoices)
  },
  {
    "enable-offline-auto-reload",
    IDS_FLAGS_ENABLE_OFFLINE_AUTO_RELOAD_NAME,
    IDS_FLAGS_ENABLE_OFFLINE_AUTO_RELOAD_DESCRIPTION,
    kOsAll,
    ENABLE_DISABLE_VALUE_TYPE(switches::kEnableOfflineAutoReload,
                              switches::kDisableOfflineAutoReload)
  },
  {
    "enable-offline-auto-reload-visible-only",
    IDS_FLAGS_ENABLE_OFFLINE_AUTO_RELOAD_VISIBLE_ONLY_NAME,
    IDS_FLAGS_ENABLE_OFFLINE_AUTO_RELOAD_VISIBLE_ONLY_DESCRIPTION,
    kOsAll,
    ENABLE_DISABLE_VALUE_TYPE(switches::kEnableOfflineAutoReloadVisibleOnly,
                              switches::kDisableOfflineAutoReloadVisibleOnly)
  },
  {
    "enable-offline-load-stale-cache",
    IDS_FLAGS_ENABLE_OFFLINE_LOAD_STALE_NAME,
    IDS_FLAGS_ENABLE_OFFLINE_LOAD_STALE_DESCRIPTION,
    kOsLinux | kOsMac | kOsWin | kOsAndroid,
    ENABLE_DISABLE_VALUE_TYPE(switches::kEnableOfflineLoadStaleCache,
                              switches::kDisableOfflineLoadStaleCache)
  },
  {
    "default-tile-width",
    IDS_FLAGS_DEFAULT_TILE_WIDTH_NAME,
    IDS_FLAGS_DEFAULT_TILE_WIDTH_DESCRIPTION,
    kOsAll,
    MULTI_VALUE_TYPE(kDefaultTileWidthChoices)
  },
  {
    "default-tile-height",
    IDS_FLAGS_DEFAULT_TILE_HEIGHT_NAME,
    IDS_FLAGS_DEFAULT_TILE_HEIGHT_DESCRIPTION,
    kOsAll,
    MULTI_VALUE_TYPE(kDefaultTileHeightChoices)
  },
#if defined(OS_CHROMEOS)
  {
    "enable-virtual-keyboard",
    IDS_FLAGS_ENABLE_VIRTUAL_KEYBOARD_NAME,
    IDS_FLAGS_ENABLE_VIRTUAL_KEYBOARD_DESCRIPTION,
    kOsCrOS,
    SINGLE_VALUE_TYPE(keyboard::switches::kEnableVirtualKeyboard)
  },
  {
    "enable-virtual-keyboard-overscroll",
    IDS_FLAGS_ENABLE_VIRTUAL_KEYBOARD_OVERSCROLL_NAME,
    IDS_FLAGS_ENABLE_VIRTUAL_KEYBOARD_OVERSCROLL_DESCRIPTION,
    kOsCrOS,
    ENABLE_DISABLE_VALUE_TYPE(
        keyboard::switches::kEnableVirtualKeyboardOverscroll,
        keyboard::switches::kDisableVirtualKeyboardOverscroll)
  },
  {
    "enable-swipe-selection",
    IDS_FLAGS_ENABLE_SWIPE_SELECTION_NAME,
    IDS_FLAGS_ENABLE_SWIPE_SELECTION_DESCRIPTION,
    kOsCrOS,
    SINGLE_VALUE_TYPE(keyboard::switches::kEnableSwipeSelection)
  },
  {
    "enable-input-view",
    IDS_FLAGS_ENABLE_INPUT_VIEW_NAME,
    IDS_FLAGS_ENABLE_INPUT_VIEW_DESCRIPTION,
    kOsCrOS,
    ENABLE_DISABLE_VALUE_TYPE(keyboard::switches::kEnableInputView,
                              keyboard::switches::kDisableInputView)
  },
  {
    "enable-experimental-input-view-features",
    IDS_FLAGS_ENABLE_EXPERIMENTAL_INPUT_VIEW_FEATURES_NAME,
    IDS_FLAGS_ENABLE_EXPERIMENTAL_INPUT_VIEW_FEATURES_DESCRIPTION,
    kOsCrOS,
    SINGLE_VALUE_TYPE(keyboard::switches::kEnableExperimentalInputViewFeatures)
  },
#endif
  {
    "enable-simple-cache-backend",
    IDS_FLAGS_ENABLE_SIMPLE_CACHE_BACKEND_NAME,
    IDS_FLAGS_ENABLE_SIMPLE_CACHE_BACKEND_DESCRIPTION,
    kOsWin | kOsMac | kOsLinux | kOsCrOS,
    MULTI_VALUE_TYPE(kSimpleCacheBackendChoices)
  },
  {
    "enable-tcp-fast-open",
    IDS_FLAGS_ENABLE_TCP_FAST_OPEN_NAME,
    IDS_FLAGS_ENABLE_TCP_FAST_OPEN_DESCRIPTION,
    kOsLinux | kOsCrOS | kOsAndroid,
    SINGLE_VALUE_TYPE(switches::kEnableTcpFastOpen)
  },
#if defined(ENABLE_SERVICE_DISCOVERY)
  {
    "disable-device-discovery",
    IDS_FLAGS_DISABLE_DEVICE_DISCOVERY_NAME,
    IDS_FLAGS_DISABLE_DEVICE_DISCOVERY_DESCRIPTION,
    kOsDesktop,
    SINGLE_VALUE_TYPE(switches::kDisableDeviceDiscovery)
  },
  {
    "device-discovery-notifications",
    IDS_FLAGS_DEVICE_DISCOVERY_NOTIFICATIONS_NAME,
    IDS_FLAGS_DEVICE_DISCOVERY_NOTIFICATIONS_DESCRIPTION,
    kOsDesktop,
    ENABLE_DISABLE_VALUE_TYPE(switches::kEnableDeviceDiscoveryNotifications,
                              switches::kDisableDeviceDiscoveryNotifications)
  },
  {
    "enable-cloud-devices",
    IDS_FLAGS_ENABLE_CLOUD_DEVICES_NAME,
    IDS_FLAGS_ENABLE_CLOUD_DEVICES_DESCRIPTION,
    kOsDesktop,
    SINGLE_VALUE_TYPE(switches::kEnableCloudDevices)
  },
  {
    "enable-print-preview-register-promos",
    IDS_FLAGS_ENABLE_PRINT_PREVIEW_REGISTER_PROMOS_NAME,
    IDS_FLAGS_ENABLE_PRINT_PREVIEW_REGISTER_PROMOS_DESCRIPTION,
    kOsDesktop,
    SINGLE_VALUE_TYPE(switches::kEnablePrintPreviewRegisterPromos)
  },
#endif  // ENABLE_SERVICE_DISCOVERY
#if defined(OS_WIN)
  {
    "enable-cloud-print-xps",
    IDS_FLAGS_ENABLE_CLOUD_PRINT_XPS_NAME,
    IDS_FLAGS_ENABLE_CLOUD_PRINT_XPS_DESCRIPTION,
    kOsWin,
    SINGLE_VALUE_TYPE(switches::kEnableCloudPrintXps)
  },
#endif
#if defined(OS_MACOSX)
  {
    "enable-simplified-fullscreen",
    IDS_FLAGS_ENABLE_SIMPLIFIED_FULLSCREEN_NAME,
    IDS_FLAGS_ENABLE_SIMPLIFIED_FULLSCREEN_DESCRIPTION,
    kOsMac,
    SINGLE_VALUE_TYPE(switches::kEnableSimplifiedFullscreen)
  },
#endif
#if defined(USE_AURA)
  {
    "tab-capture-upscale-quality",
    IDS_FLAGS_TAB_CAPTURE_UPSCALE_QUALITY_NAME,
    IDS_FLAGS_TAB_CAPTURE_UPSCALE_QUALITY_DESCRIPTION,
    kOsAll,
    MULTI_VALUE_TYPE(kTabCaptureUpscaleQualityChoices)
  },
  {
    "tab-capture-downscale-quality",
    IDS_FLAGS_TAB_CAPTURE_DOWNSCALE_QUALITY_NAME,
    IDS_FLAGS_TAB_CAPTURE_DOWNSCALE_QUALITY_DESCRIPTION,
    kOsAll,
    MULTI_VALUE_TYPE(kTabCaptureDownscaleQualityChoices)
  },
#endif
  {
    "enable-spelling-feedback-field-trial",
    IDS_FLAGS_ENABLE_SPELLING_FEEDBACK_FIELD_TRIAL_NAME,
    IDS_FLAGS_ENABLE_SPELLING_FEEDBACK_FIELD_TRIAL_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kEnableSpellingFeedbackFieldTrial)
  },
  {
    "enable-webgl-draft-extensions",
    IDS_FLAGS_ENABLE_WEBGL_DRAFT_EXTENSIONS_NAME,
    IDS_FLAGS_ENABLE_WEBGL_DRAFT_EXTENSIONS_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kEnableWebGLDraftExtensions)
  },
  {
    "enable-web-midi",
    IDS_FLAGS_ENABLE_WEB_MIDI_NAME,
    IDS_FLAGS_ENABLE_WEB_MIDI_DESCRIPTION,
    kOsMac | kOsWin | kOsLinux | kOsCrOS | kOsAndroid,
    SINGLE_VALUE_TYPE(switches::kEnableWebMIDI)
  },
  {
    "enable-new-profile-management",
    IDS_FLAGS_ENABLE_NEW_PROFILE_MANAGEMENT_NAME,
    IDS_FLAGS_ENABLE_NEW_PROFILE_MANAGEMENT_DESCRIPTION,
    kOsAndroid | kOsMac | kOsWin | kOsLinux | kOsCrOS,
    ENABLE_DISABLE_VALUE_TYPE(switches::kEnableNewProfileManagement,
                              switches::kDisableNewProfileManagement)
  },
  {
    "enable-account-consistency",
    IDS_FLAGS_ENABLE_ACCOUNT_CONSISTENCY_NAME,
    IDS_FLAGS_ENABLE_ACCOUNT_CONSISTENCY_DESCRIPTION,
    kOsAndroid | kOsMac | kOsWin | kOsLinux | kOsCrOS,
    ENABLE_DISABLE_VALUE_TYPE(switches::kEnableAccountConsistency,
                              switches::kDisableAccountConsistency)
  },
  {
    "enable-fast-user-switching",
    IDS_FLAGS_ENABLE_FAST_USER_SWITCHING_NAME,
    IDS_FLAGS_ENABLE_FAST_USER_SWITCHING_DESCRIPTION,
    kOsMac | kOsWin | kOsLinux,
    SINGLE_VALUE_TYPE(switches::kFastUserSwitching)
  },
  {
    "enable-new-avatar-menu",
    IDS_FLAGS_ENABLE_NEW_AVATAR_MENU_NAME,
    IDS_FLAGS_ENABLE_NEW_AVATAR_MENU_DESCRIPTION,
    kOsMac | kOsWin | kOsLinux,
    ENABLE_DISABLE_VALUE_TYPE(switches::kEnableNewAvatarMenu,
                              switches::kDisableNewAvatarMenu)
  },
  {
    "enable-web-based-signin",
    IDS_FLAGS_ENABLE_WEB_BASED_SIGNIN_NAME,
    IDS_FLAGS_ENABLE_WEB_BASED_SIGNIN_DESCRIPTION,
    kOsMac | kOsWin | kOsLinux,
    SINGLE_VALUE_TYPE(switches::kEnableWebBasedSignin)
  },
  {
    "enable-google-profile-info",
    IDS_FLAGS_ENABLE_GOOGLE_PROFILE_INFO_NAME,
    IDS_FLAGS_ENABLE_GOOGLE_PROFILE_INFO_DESCRIPTION,
    kOsMac | kOsWin | kOsLinux,
    SINGLE_VALUE_TYPE(switches::kGoogleProfileInfo)
  },
  {
    "reset-app-list-install-state",
    IDS_FLAGS_RESET_APP_LIST_INSTALL_STATE_NAME,
    IDS_FLAGS_RESET_APP_LIST_INSTALL_STATE_DESCRIPTION,
    kOsMac | kOsWin | kOsLinux,
    SINGLE_VALUE_TYPE(switches::kResetAppListInstallState)
  },
#if defined(ENABLE_APP_LIST)
#if defined(OS_LINUX)
  {
    // This is compiled out on non-Linux platforms because otherwise it would be
    // visible on Win/Mac/CrOS but not on Linux GTK, which would be confusing.
    // TODO(mgiuca): Remove the #if when Aura is the default on Linux.
    "enable-app-list",
    IDS_FLAGS_ENABLE_APP_LIST_NAME,
    IDS_FLAGS_ENABLE_APP_LIST_DESCRIPTION,
    kOsLinux,
    SINGLE_VALUE_TYPE(switches::kEnableAppList)
  },
#endif
  {
    "enable-app-view",
    IDS_FLAGS_ENABLE_APP_VIEW_NAME,
    IDS_FLAGS_ENABLE_APP_VIEW_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kEnableAppView)
  },
  {
    "disable-app-list-app-info",
    IDS_FLAGS_DISABLE_APP_INFO_IN_APP_LIST,
    IDS_FLAGS_DISABLE_APP_INFO_IN_APP_LIST_DESCRIPTION,
    kOsLinux | kOsWin | kOsCrOS,
    SINGLE_VALUE_TYPE(app_list::switches::kDisableAppInfo)
  },
  {
    "enable-drive-apps-in-app-list",
    IDS_FLAGS_ENABLE_DRIVE_APPS_IN_APP_LIST_NAME,
    IDS_FLAGS_ENABLE_DRIVE_APPS_IN_APP_LIST_DESCRIPTION,
    kOsDesktop,
    SINGLE_VALUE_TYPE(app_list::switches::kEnableDriveAppsInAppList)
  },
#endif
#if defined(OS_ANDROID)
  {
    "enable-accessibility-tab-switcher",
    IDS_FLAGS_ENABLE_ACCESSIBILITY_TAB_SWITCHER_NAME,
    IDS_FLAGS_ENABLE_ACCESSIBILITY_TAB_SWITCHER_DESCRIPTION,
    kOsAndroid,
    SINGLE_VALUE_TYPE(switches::kEnableAccessibilityTabSwitcher)
  },
  {
    // TODO(dmazzoni): remove this flag when native android accessibility
    // ships in the stable channel. http://crbug.com/356775
    "enable-accessibility-script-injection",
    IDS_FLAGS_ENABLE_ACCESSIBILITY_SCRIPT_INJECTION_NAME,
    IDS_FLAGS_ENABLE_ACCESSIBILITY_SCRIPT_INJECTION_DESCRIPTION,
    kOsAndroid,
    // Java-only switch: ContentSwitches.ENABLE_ACCESSIBILITY_SCRIPT_INJECTION.
    SINGLE_VALUE_TYPE("enable-accessibility-script-injection")
  },
#endif
  {
    "enable-one-copy",
    IDS_FLAGS_ONE_COPY_NAME,
    IDS_FLAGS_ONE_COPY_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kEnableOneCopy)
  },
  {
    "enable-zero-copy",
    IDS_FLAGS_ZERO_COPY_NAME,
    IDS_FLAGS_ZERO_COPY_DESCRIPTION,
    kOsAll,
    MULTI_VALUE_TYPE(kZeroCopyChoices)
  },
#if defined(OS_CHROMEOS)
  {
    "enable-first-run-ui-transitions",
    IDS_FLAGS_ENABLE_FIRST_RUN_UI_TRANSITIONS_NAME,
    IDS_FLAGS_ENABLE_FIRST_RUN_UI_TRANSITIONS_DESCRIPTION,
    kOsCrOS,
    SINGLE_VALUE_TYPE(chromeos::switches::kEnableFirstRunUITransitions)
  },
#endif
  {
    "enable-streamlined-hosted-apps",
    IDS_FLAGS_ENABLE_STREAMLINED_HOSTED_APPS_NAME,
    IDS_FLAGS_ENABLE_STREAMLINED_HOSTED_APPS_DESCRIPTION,
    kOsWin | kOsCrOS | kOsLinux,
    SINGLE_VALUE_TYPE(switches::kEnableStreamlinedHostedApps)
  },
  {
    "enable-prominent-url-app-flow",
    IDS_FLAGS_ENABLE_PROMINENT_URL_APP_FLOW_NAME,
    IDS_FLAGS_ENABLE_PROMINENT_URL_APP_FLOW_DESCRIPTION,
    kOsWin | kOsCrOS | kOsLinux,
    SINGLE_VALUE_TYPE(switches::kEnableProminentURLAppFlow)
  },
  {
    "enable-ephemeral-apps",
    IDS_FLAGS_ENABLE_EPHEMERAL_APPS_NAME,
    IDS_FLAGS_ENABLE_EPHEMERAL_APPS_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kEnableEphemeralApps)
  },
  {
    "enable-linkable-ephemeral-apps",
    IDS_FLAGS_ENABLE_LINKABLE_EPHEMERAL_APPS_NAME,
    IDS_FLAGS_ENABLE_LINKABLE_EPHEMERAL_APPS_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kEnableLinkableEphemeralApps)
  },
  {
    "enable-service-worker-sync",
    IDS_FLAGS_ENABLE_SERVICE_WORKER_SYNC_NAME,
    IDS_FLAGS_ENABLE_SERVICE_WORKER_SYNC_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kEnableServiceWorkerSync)
  },
#if defined(OS_ANDROID)
  {
    "disable-click-delay",
    IDS_FLAGS_DISABLE_CLICK_DELAY_NAME,
    IDS_FLAGS_DISABLE_CLICK_DELAY_DESCRIPTION,
    kOsAndroid,
    // Java-only switch: CommandLine.DISABLE_CLICK_DELAY
    SINGLE_VALUE_TYPE("disable-click-delay")
  },
#endif
#if defined(OS_MACOSX)
  {
    "enable-translate-new-ux",
    IDS_FLAGS_ENABLE_TRANSLATE_NEW_UX_NAME,
    IDS_FLAGS_ENABLE_TRANSLATE_NEW_UX_DESCRIPTION,
    kOsMac,
    SINGLE_VALUE_TYPE(switches::kEnableTranslateNewUX)
  },
#endif
#if defined(TOOLKIT_VIEWS)
  {
    "disable-views-rect-based-targeting",  // FLAGS:RECORD_UMA
    IDS_FLAGS_DISABLE_VIEWS_RECT_BASED_TARGETING_NAME,
    IDS_FLAGS_DISABLE_VIEWS_RECT_BASED_TARGETING_DESCRIPTION,
    kOsCrOS | kOsWin | kOsLinux,
    SINGLE_VALUE_TYPE(views::switches::kDisableViewsRectBasedTargeting)
  },
#endif
  {
    "enable-apps-show-on-first-paint",
    IDS_FLAGS_ENABLE_APPS_SHOW_ON_FIRST_PAINT_NAME,
    IDS_FLAGS_ENABLE_APPS_SHOW_ON_FIRST_PAINT_DESCRIPTION,
    kOsDesktop,
    SINGLE_VALUE_TYPE(switches::kEnableAppsShowOnFirstPaint)
  },
  {
    "enhanced-bookmarks-experiment",
    IDS_FLAGS_ENABLE_ENHANCED_BOOKMARKS_NAME,
    IDS_FLAGS_ENABLE_ENHANCED_BOOKMARKS_DESCRIPTION,
    kOsDesktop,
    ENABLE_DISABLE_VALUE_TYPE_AND_VALUE(
        switches::kEnhancedBookmarksExperiment, "1",
        switches::kEnhancedBookmarksExperiment, "0")
  },
  {
    "manual-enhanced-bookmarks",
    IDS_FLAGS_ENABLE_ENHANCED_BOOKMARKS_NAME,
    IDS_FLAGS_ENABLE_ENHANCED_BOOKMARKS_DESCRIPTION,
    kOsDesktop,
    SINGLE_VALUE_TYPE(switches::kManualEnhancedBookmarks)
  },
  {
    "manual-enhanced-bookmarks-optout",
    IDS_FLAGS_ENABLE_ENHANCED_BOOKMARKS_NAME,
    IDS_FLAGS_ENABLE_ENHANCED_BOOKMARKS_DESCRIPTION,
    kOsDesktop,
    SINGLE_VALUE_TYPE(switches::kManualEnhancedBookmarksOptout)
  },
#if defined(OS_ANDROID)
  {
    "enable-zero-suggest-experiment",
    IDS_FLAGS_ZERO_SUGGEST_EXPERIMENT_NAME,
    IDS_FLAGS_ZERO_SUGGEST_EXPERIMENT_DESCRIPTION,
    kOsAndroid,
    MULTI_VALUE_TYPE(kZeroSuggestExperimentsChoices)
  },
#endif
  {
    "num-raster-threads",
    IDS_FLAGS_NUM_RASTER_THREADS_NAME,
    IDS_FLAGS_NUM_RASTER_THREADS_DESCRIPTION,
    kOsAll,
    MULTI_VALUE_TYPE(kNumRasterThreadsChoices)
  },
  {
    "origin-chip-in-omnibox",
    IDS_FLAGS_ORIGIN_CHIP_NAME,
    IDS_FLAGS_ORIGIN_CHIP_DESCRIPTION,
    kOsCrOS | kOsMac | kOsWin | kOsLinux,
    MULTI_VALUE_TYPE(kOriginChipChoices)
  },
  {
    "search-button-in-omnibox",
    IDS_FLAGS_SEARCH_BUTTON_IN_OMNIBOX_NAME,
    IDS_FLAGS_SEARCH_BUTTON_IN_OMNIBOX_DESCRIPTION,
    kOsCrOS | kOsMac | kOsWin | kOsLinux,
    MULTI_VALUE_TYPE(kSearchButtonInOmniboxChoices)
  },
  {
    "disable-ignore-autocomplete-off",
    IDS_FLAGS_DISABLE_IGNORE_AUTOCOMPLETE_OFF_NAME,
    IDS_FLAGS_DISABLE_IGNORE_AUTOCOMPLETE_OFF_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(autofill::switches::kDisableIgnoreAutocompleteOff)
  },
  {
    "enable-permissions-bubbles",
    IDS_FLAGS_ENABLE_PERMISSIONS_BUBBLES_NAME,
    IDS_FLAGS_ENABLE_PERMISSIONS_BUBBLES_DESCRIPTION,
    kOsCrOS | kOsMac | kOsWin | kOsLinux,
    ENABLE_DISABLE_VALUE_TYPE(switches::kEnablePermissionsBubbles,
                              switches::kDisablePermissionsBubbles)
  },
  {
    "enable-session-crashed-bubble",
    IDS_FLAGS_ENABLE_SESSION_CRASHED_BUBBLE_NAME,
    IDS_FLAGS_ENABLE_SESSION_CRASHED_BUBBLE_DESCRIPTION,
    kOsWin | kOsLinux,
    ENABLE_DISABLE_VALUE_TYPE(switches::kEnableSessionCrashedBubble,
                              switches::kDisableSessionCrashedBubble)
  },
  {
    "out-of-process-pdf",
    IDS_FLAGS_OUT_OF_PROCESS_PDF_NAME,
    IDS_FLAGS_OUT_OF_PROCESS_PDF_DESCRIPTION,
    kOsDesktop,
    SINGLE_VALUE_TYPE(switches::kOutOfProcessPdf)
  },
#if defined(OS_ANDROID)
  {
    "disable-cast",
    IDS_FLAGS_DISABLE_CAST_NAME,
    IDS_FLAGS_DISABLE_CAST_DESCRIPTION,
    kOsAndroid,
    SINGLE_VALUE_TYPE(switches::kDisableCast)
  },
  {
    "prefetch-search-results",
    IDS_FLAGS_PREFETCH_SEARCH_RESULTS_NAME,
    IDS_FLAGS_PREFETCH_SEARCH_RESULTS_DESCRIPTION,
    kOsAndroid,
    SINGLE_VALUE_TYPE(switches::kPrefetchSearchResults)
  },
#endif
#if defined(ENABLE_APP_LIST)
  {
    "enable-experimental-app-list",
    IDS_FLAGS_ENABLE_EXPERIMENTAL_APP_LIST_NAME,
    IDS_FLAGS_ENABLE_EXPERIMENTAL_APP_LIST_DESCRIPTION,
    kOsWin | kOsLinux | kOsCrOS,
    SINGLE_VALUE_TYPE(app_list::switches::kEnableExperimentalAppList)
  },
  {
    "enable-centered-app-list",
    IDS_FLAGS_ENABLE_CENTERED_APP_LIST_NAME,
    IDS_FLAGS_ENABLE_CENTERED_APP_LIST_DESCRIPTION,
    kOsWin | kOsLinux | kOsCrOS,
    SINGLE_VALUE_TYPE(app_list::switches::kEnableCenteredAppList)
  },
#endif
  {
    "touch-scrolling-mode",
    IDS_FLAGS_TOUCH_SCROLLING_MODE_NAME,
    IDS_FLAGS_TOUCH_SCROLLING_MODE_DESCRIPTION,
    kOsWin | kOsLinux | kOsCrOS | kOsAndroid,
    MULTI_VALUE_TYPE(kTouchScrollingModeChoices)
  },
  {
    "bleeding-edge-renderer-mode",
    IDS_FLAGS_BLEEDING_RENDERER_NAME,
    IDS_FLAGS_BLEEDING_RENDERER_DESCRIPTION,
    kOsAndroid,
    SINGLE_VALUE_TYPE(switches::kEnableBleedingEdgeRenderingFastPaths)
  },
  {
    "enable-settings-window",
    IDS_FLAGS_ENABLE_SETTINGS_WINDOW_NAME,
    IDS_FLAGS_ENABLE_SETTINGS_WINDOW_DESCRIPTION,
    kOsDesktop,
    MULTI_VALUE_TYPE(kEnableSettingsWindowChoices)
  },
#if defined(OS_ANDROID)
  {
    "enable-instant-search-clicks",
    IDS_FLAGS_ENABLE_INSTANT_SEARCH_CLICKS_NAME,
    IDS_FLAGS_ENABLE_INSTANT_SEARCH_CLICKS_DESCRIPTION,
    kOsAndroid,
    SINGLE_VALUE_TYPE(switches::kEnableInstantSearchClicks)
  },
#endif
  {
    "enable-save-password-bubble",
    IDS_FLAGS_ENABLE_SAVE_PASSWORD_BUBBLE_NAME,
    IDS_FLAGS_ENABLE_SAVE_PASSWORD_BUBBLE_DESCRIPTION,
    kOsWin | kOsLinux | kOsCrOS,
    ENABLE_DISABLE_VALUE_TYPE(switches::kEnableSavePasswordBubble,
                              switches::kDisableSavePasswordBubble)
  },
#if defined(OS_CHROMEOS)
  {
    "enable-filemanager-mtp",
    IDS_FLAGS_ENABLE_FILE_MANAGER_MTP_NAME,
    IDS_FLAGS_ENABLE_FILE_MANAGER_MTP_DESCRIPTION,
    kOsCrOS,
    MULTI_VALUE_TYPE(kEnableFileManagerMTPChoices)
  },
#endif
  // TODO(tyoshino): Remove this temporary flag and command line switch. See
  // crbug.com/366483 for the target milestone.
  {
    "allow-insecure-websocket-from-https-origin",
    IDS_FLAGS_ALLOW_INSECURE_WEBSOCKET_FROM_HTTPS_ORIGIN_NAME,
    IDS_FLAGS_ALLOW_INSECURE_WEBSOCKET_FROM_HTTPS_ORIGIN_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kAllowInsecureWebSocketFromHttpsOrigin)
  },
  {
    "enable-apps-file-associations",
    IDS_FLAGS_ENABLE_APPS_FILE_ASSOCIATIONS_NAME,
    IDS_FLAGS_ENABLE_APPS_FILE_ASSOCIATIONS_DESCRIPTION,
    kOsMac,
    SINGLE_VALUE_TYPE(switches::kEnableAppsFileAssociations)
  },
#if defined(OS_ANDROID)
  {
    "enable-embeddedsearch-api",
    IDS_FLAGS_ENABLE_EMBEDDEDSEARCH_API_NAME,
    IDS_FLAGS_ENABLE_EMBEDDEDSEARCH_API_DESCRIPTION,
    kOsAndroid,
    SINGLE_VALUE_TYPE(switches::kEnableEmbeddedSearchAPI)
  },
  {
    "enable-app-install-alerts",
    IDS_FLAGS_ENABLE_APP_INSTALL_ALERTS_NAME,
    IDS_FLAGS_ENABLE_APP_INSTALL_ALERTS_DESCRIPTION,
    kOsAndroid,
    SINGLE_VALUE_TYPE(switches::kEnableAppInstallAlerts)
  },
#endif
  {
    "distance-field-text",
    IDS_FLAGS_DISTANCE_FIELD_TEXT_NAME,
    IDS_FLAGS_DISTANCE_FIELD_TEXT_DESCRIPTION,
    kOsAll,
    MULTI_VALUE_TYPE(kDistanceFieldTextChoices)
  },
  {
    "extension-content-verification",
    IDS_FLAGS_EXTENSION_CONTENT_VERIFICATION_NAME,
    IDS_FLAGS_EXTENSION_CONTENT_VERIFICATION_DESCRIPTION,
    kOsDesktop,
    MULTI_VALUE_TYPE(kExtensionContentVerificationChoices)
  },
#if defined(USE_AURA)
  {
    "text-input-focus-manager",
    IDS_FLAGS_TEXT_INPUT_FOCUS_MANAGER_NAME,
    IDS_FLAGS_TEXT_INPUT_FOCUS_MANAGER_DESCRIPTION,
    kOsCrOS | kOsLinux | kOsWin,
    ENABLE_DISABLE_VALUE_TYPE(switches::kEnableTextInputFocusManager,
                              switches::kDisableTextInputFocusManager)
  },
#endif
  {
    "extension-active-script-permission",
    IDS_FLAGS_USER_CONSENT_FOR_EXTENSION_SCRIPTS_NAME,
    IDS_FLAGS_USER_CONSENT_FOR_EXTENSION_SCRIPTS_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(extensions::switches::kEnableScriptsRequireAction)
  },
  {
    "harfbuzz-rendertext",
    IDS_FLAGS_HARFBUZZ_RENDERTEXT_NAME,
    IDS_FLAGS_HARFBUZZ_RENDERTEXT_DESCRIPTION,
    kOsDesktop,
    ENABLE_DISABLE_VALUE_TYPE(switches::kEnableHarfBuzzRenderText,
                              switches::kDisableHarfBuzzRenderText)
  },
#if defined(OS_ANDROID)
  {
    "answers-in-suggest",
    IDS_FLAGS_ENABLE_ANSWERS_IN_SUGGEST_NAME,
    IDS_FLAGS_ENABLE_ANSWERS_IN_SUGGEST_DESCRIPTION,
    kOsAndroid,
    MULTI_VALUE_TYPE(kAnswersInSuggestChoices)
  },
#endif
  {
    "malware-interstitial-version",
    IDS_FLAGS_MALWARE_INTERSTITIAL_TRIAL_NAME,
    IDS_FLAGS_MALWARE_INTERSTITIAL_TRIAL_DESCRIPTION,
    kOsAll,
    MULTI_VALUE_TYPE(kMalwareInterstitialVersions)
  },
#if defined(OS_ANDROID)
  {
    "enable-data-reduction-proxy-dev",
    IDS_FLAGS_ENABLE_DATA_REDUCTION_PROXY_DEV_NAME,
    IDS_FLAGS_ENABLE_DATA_REDUCTION_PROXY_DEV_DESCRIPTION,
    kOsAndroid,
    ENABLE_DISABLE_VALUE_TYPE(
        data_reduction_proxy::switches::kEnableDataReductionProxyDev,
        data_reduction_proxy::switches::kDisableDataReductionProxyDev)
  },
#endif
#if defined(OS_CHROMEOS)
  {
    "enable-ok-google-voice-search",
    IDS_FLAGS_OK_GOOGLE_NAME,
    IDS_FLAGS_OK_GOOGLE_DESCRIPTION,
    kOsCrOS,
    SINGLE_VALUE_TYPE(chromeos::switches::kEnableOkGoogleVoiceSearch)
  },
#endif
  {
    "enable-embedded-extension-options",
    IDS_FLAGS_ENABLE_EMBEDDED_EXTENSION_OPTIONS_NAME,
    IDS_FLAGS_ENABLE_EMBEDDED_EXTENSION_OPTIONS_DESCRIPTION,
    kOsDesktop,
    SINGLE_VALUE_TYPE(extensions::switches::kEnableEmbeddedExtensionOptions)
  },
  {
    "enable-website-settings-manager",
    IDS_FLAGS_ENABLE_WEBSITE_SETTINGS_NAME,
    IDS_FLAGS_ENABLE_WEBSITE_SETTINGS_DESCRIPTION,
    kOsDesktop,
    SINGLE_VALUE_TYPE(switches::kEnableWebsiteSettingsManager)
  },
  {
    "remember-cert-error-decisions",
    IDS_FLAGS_REMEMBER_CERTIFICATE_ERROR_DECISIONS_NAME,
    IDS_FLAGS_REMEMBER_CERTIFICATE_ERROR_DECISIONS_DESCRIPTION,
    kOsAll,
    MULTI_VALUE_TYPE(kRememberCertificateErrorDecisionsChoices)
  },
  {
    "enable-drop-sync-credential",
    IDS_FLAGS_ENABLE_DROP_SYNC_CREDENTIAL_NAME,
    IDS_FLAGS_ENABLE_DROP_SYNC_CREDENTIAL_DESCRIPTION,
    kOsAll,
    MULTI_VALUE_TYPE(kEnableDropSyncCredentialChoices)
  }
};

const Experiment* experiments = kExperiments;
size_t num_experiments = arraysize(kExperiments);

// Stores and encapsulates the little state that about:flags has.
class FlagsState {
 public:
  FlagsState() : needs_restart_(false) {}
  void ConvertFlagsToSwitches(FlagsStorage* flags_storage,
                              CommandLine* command_line,
                              SentinelsMode sentinels);
  bool IsRestartNeededToCommitChanges();
  void SetExperimentEnabled(
      FlagsStorage* flags_storage,
      const std::string& internal_name,
      bool enable);
  void RemoveFlagsSwitches(
      std::map<std::string, CommandLine::StringType>* switch_list);
  void ResetAllFlags(FlagsStorage* flags_storage);
  void reset();

  // Returns the singleton instance of this class
  static FlagsState* GetInstance() {
    return Singleton<FlagsState>::get();
  }

 private:
  bool needs_restart_;
  std::map<std::string, std::string> flags_switches_;

  DISALLOW_COPY_AND_ASSIGN(FlagsState);
};

// Adds the internal names for the specified experiment to |names|.
void AddInternalName(const Experiment& e, std::set<std::string>* names) {
  if (e.type == Experiment::SINGLE_VALUE) {
    names->insert(e.internal_name);
  } else {
    DCHECK(e.type == Experiment::MULTI_VALUE ||
           e.type == Experiment::ENABLE_DISABLE_VALUE);
    for (int i = 0; i < e.num_choices; ++i)
      names->insert(e.NameForChoice(i));
  }
}

// Confirms that an experiment is valid, used in a DCHECK in
// SanitizeList below.
bool ValidateExperiment(const Experiment& e) {
  switch (e.type) {
    case Experiment::SINGLE_VALUE:
      DCHECK_EQ(0, e.num_choices);
      DCHECK(!e.choices);
      break;
    case Experiment::MULTI_VALUE:
      DCHECK_GT(e.num_choices, 0);
      DCHECK(e.choices);
      DCHECK(e.choices[0].command_line_switch);
      DCHECK_EQ('\0', e.choices[0].command_line_switch[0]);
      break;
    case Experiment::ENABLE_DISABLE_VALUE:
      DCHECK_EQ(3, e.num_choices);
      DCHECK(!e.choices);
      DCHECK(e.command_line_switch);
      DCHECK(e.command_line_value);
      DCHECK(e.disable_command_line_switch);
      DCHECK(e.disable_command_line_value);
      break;
    default:
      NOTREACHED();
  }
  return true;
}

// Removes all experiments from prefs::kEnabledLabsExperiments that are
// unknown, to prevent this list to become very long as experiments are added
// and removed.
void SanitizeList(FlagsStorage* flags_storage) {
  std::set<std::string> known_experiments;
  for (size_t i = 0; i < num_experiments; ++i) {
    DCHECK(ValidateExperiment(experiments[i]));
    AddInternalName(experiments[i], &known_experiments);
  }

  std::set<std::string> enabled_experiments = flags_storage->GetFlags();

  std::set<std::string> new_enabled_experiments =
      base::STLSetIntersection<std::set<std::string> >(
          known_experiments, enabled_experiments);

  if (new_enabled_experiments != enabled_experiments)
    flags_storage->SetFlags(new_enabled_experiments);
}

void GetSanitizedEnabledFlags(
    FlagsStorage* flags_storage, std::set<std::string>* result) {
  SanitizeList(flags_storage);
  *result = flags_storage->GetFlags();
}

bool SkipConditionalExperiment(const Experiment& experiment) {
  if (experiment.internal_name ==
      std::string("enhanced-bookmarks-experiment")) {
    CommandLine* command_line = CommandLine::ForCurrentProcess();
    // Dont't skip experiment if it has non default value.
    // It means user selected it.
    if (command_line->HasSwitch(switches::kEnhancedBookmarksExperiment))
      return false;

    return !IsEnhancedBookmarksExperimentEnabled();
  }
  if ((experiment.internal_name == std::string("manual-enhanced-bookmarks")) ||
      (experiment.internal_name ==
           std::string("manual-enhanced-bookmarks-optout"))) {
    return true;
  }

#if defined(OS_ANDROID)
  // enable-data-reduction-proxy-dev is only available for the Dev channel.
  if (!strcmp("enable-data-reduction-proxy-dev", experiment.internal_name) &&
      chrome::VersionInfo::GetChannel() != chrome::VersionInfo::CHANNEL_DEV) {
    return true;
  }
#endif

  return false;
}


// Variant of GetSanitizedEnabledFlags that also removes any flags that aren't
// enabled on the current platform.
void GetSanitizedEnabledFlagsForCurrentPlatform(
    FlagsStorage* flags_storage, std::set<std::string>* result) {
  GetSanitizedEnabledFlags(flags_storage, result);

  // Filter out any experiments that aren't enabled on the current platform.  We
  // don't remove these from prefs else syncing to a platform with a different
  // set of experiments would be lossy.
  std::set<std::string> platform_experiments;
  int current_platform = GetCurrentPlatform();
  for (size_t i = 0; i < num_experiments; ++i) {
    if (experiments[i].supported_platforms & current_platform)
      AddInternalName(experiments[i], &platform_experiments);
#if defined(OS_CHROMEOS)
    if (experiments[i].supported_platforms & kOsCrOSOwnerOnly)
      AddInternalName(experiments[i], &platform_experiments);
#endif
  }

  std::set<std::string> new_enabled_experiments =
      base::STLSetIntersection<std::set<std::string> >(
          platform_experiments, *result);

  result->swap(new_enabled_experiments);
}

// Returns the Value representing the choice data in the specified experiment.
base::Value* CreateChoiceData(
    const Experiment& experiment,
    const std::set<std::string>& enabled_experiments) {
  DCHECK(experiment.type == Experiment::MULTI_VALUE ||
         experiment.type == Experiment::ENABLE_DISABLE_VALUE);
  base::ListValue* result = new base::ListValue;
  for (int i = 0; i < experiment.num_choices; ++i) {
    base::DictionaryValue* value = new base::DictionaryValue;
    const std::string name = experiment.NameForChoice(i);
    value->SetString("internal_name", name);
    value->SetString("description", experiment.DescriptionForChoice(i));
    value->SetBoolean("selected", enabled_experiments.count(name) > 0);
    result->Append(value);
  }
  return result;
}

}  // namespace

std::string Experiment::NameForChoice(int index) const {
  DCHECK(type == Experiment::MULTI_VALUE ||
         type == Experiment::ENABLE_DISABLE_VALUE);
  DCHECK_LT(index, num_choices);
  return std::string(internal_name) + testing::kMultiSeparator +
         base::IntToString(index);
}

base::string16 Experiment::DescriptionForChoice(int index) const {
  DCHECK(type == Experiment::MULTI_VALUE ||
         type == Experiment::ENABLE_DISABLE_VALUE);
  DCHECK_LT(index, num_choices);
  int description_id;
  if (type == Experiment::ENABLE_DISABLE_VALUE) {
    const int kEnableDisableDescriptionIds[] = {
      IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT,
      IDS_GENERIC_EXPERIMENT_CHOICE_ENABLED,
      IDS_GENERIC_EXPERIMENT_CHOICE_DISABLED,
    };
    description_id = kEnableDisableDescriptionIds[index];
  } else {
    description_id = choices[index].description_id;
  }
  return l10n_util::GetStringUTF16(description_id);
}

void ConvertFlagsToSwitches(FlagsStorage* flags_storage,
                            CommandLine* command_line,
                            SentinelsMode sentinels) {
  FlagsState::GetInstance()->ConvertFlagsToSwitches(flags_storage,
                                                    command_line,
                                                    sentinels);
}

bool AreSwitchesIdenticalToCurrentCommandLine(
    const CommandLine& new_cmdline, const CommandLine& active_cmdline) {
  std::set<CommandLine::StringType> new_flags =
      ExtractFlagsFromCommandLine(new_cmdline);
  std::set<CommandLine::StringType> active_flags =
      ExtractFlagsFromCommandLine(active_cmdline);

  // Needed because std::equal doesn't check if the 2nd set is empty.
  if (new_flags.size() != active_flags.size())
    return false;

  return std::equal(new_flags.begin(), new_flags.end(), active_flags.begin());
}

void GetFlagsExperimentsData(FlagsStorage* flags_storage,
                             FlagAccess access,
                             base::ListValue* supported_experiments,
                             base::ListValue* unsupported_experiments) {
  std::set<std::string> enabled_experiments;
  GetSanitizedEnabledFlags(flags_storage, &enabled_experiments);

  int current_platform = GetCurrentPlatform();

  for (size_t i = 0; i < num_experiments; ++i) {
    const Experiment& experiment = experiments[i];
    if (SkipConditionalExperiment(experiment))
      continue;

    base::DictionaryValue* data = new base::DictionaryValue();
    data->SetString("internal_name", experiment.internal_name);
    data->SetString("name",
                    l10n_util::GetStringUTF16(experiment.visible_name_id));
    data->SetString("description",
                    l10n_util::GetStringUTF16(
                        experiment.visible_description_id));

    base::ListValue* supported_platforms = new base::ListValue();
    AddOsStrings(experiment.supported_platforms, supported_platforms);
    data->Set("supported_platforms", supported_platforms);

    switch (experiment.type) {
      case Experiment::SINGLE_VALUE:
        data->SetBoolean(
            "enabled",
            enabled_experiments.count(experiment.internal_name) > 0);
        break;
      case Experiment::MULTI_VALUE:
      case Experiment::ENABLE_DISABLE_VALUE:
        data->Set("choices", CreateChoiceData(experiment, enabled_experiments));
        break;
      default:
        NOTREACHED();
    }

    bool supported = (experiment.supported_platforms & current_platform) != 0;
#if defined(OS_CHROMEOS)
    if (access == kOwnerAccessToFlags &&
        (experiment.supported_platforms & kOsCrOSOwnerOnly) != 0) {
      supported = true;
    }
#endif
    if (supported)
      supported_experiments->Append(data);
    else
      unsupported_experiments->Append(data);
  }
}

bool IsRestartNeededToCommitChanges() {
  return FlagsState::GetInstance()->IsRestartNeededToCommitChanges();
}

void SetExperimentEnabled(FlagsStorage* flags_storage,
                          const std::string& internal_name,
                          bool enable) {
  FlagsState::GetInstance()->SetExperimentEnabled(flags_storage,
                                                  internal_name, enable);
}

void RemoveFlagsSwitches(
    std::map<std::string, CommandLine::StringType>* switch_list) {
  FlagsState::GetInstance()->RemoveFlagsSwitches(switch_list);
}

void ResetAllFlags(FlagsStorage* flags_storage) {
  FlagsState::GetInstance()->ResetAllFlags(flags_storage);
}

int GetCurrentPlatform() {
#if defined(OS_MACOSX)
  return kOsMac;
#elif defined(OS_WIN)
  return kOsWin;
#elif defined(OS_CHROMEOS)  // Needs to be before the OS_LINUX check.
  return kOsCrOS;
#elif defined(OS_LINUX) || defined(OS_OPENBSD)
  return kOsLinux;
#elif defined(OS_ANDROID)
  return kOsAndroid;
#else
#error Unknown platform
#endif
}

void RecordUMAStatistics(FlagsStorage* flags_storage) {
  std::set<std::string> flags = flags_storage->GetFlags();
  for (std::set<std::string>::iterator it = flags.begin(); it != flags.end();
       ++it) {
    std::string action("AboutFlags_");
    action += *it;
    content::RecordComputedAction(action);
  }
  // Since flag metrics are recorded every startup, add a tick so that the
  // stats can be made meaningful.
  if (flags.size())
    content::RecordAction(UserMetricsAction("AboutFlags_StartupTick"));
  content::RecordAction(UserMetricsAction("StartupTick"));
}

//////////////////////////////////////////////////////////////////////////////
// FlagsState implementation.

namespace {

typedef std::map<std::string, std::pair<std::string, std::string> >
    NameToSwitchAndValueMap;

void SetFlagToSwitchMapping(const std::string& key,
                            const std::string& switch_name,
                            const std::string& switch_value,
                            NameToSwitchAndValueMap* name_to_switch_map) {
  DCHECK(name_to_switch_map->end() == name_to_switch_map->find(key));
  (*name_to_switch_map)[key] = std::make_pair(switch_name, switch_value);
}

void FlagsState::ConvertFlagsToSwitches(FlagsStorage* flags_storage,
                                        CommandLine* command_line,
                                        SentinelsMode sentinels) {
  if (command_line->HasSwitch(switches::kNoExperiments))
    return;

  std::set<std::string> enabled_experiments;

  GetSanitizedEnabledFlagsForCurrentPlatform(flags_storage,
                                             &enabled_experiments);

  NameToSwitchAndValueMap name_to_switch_map;
  for (size_t i = 0; i < num_experiments; ++i) {
    const Experiment& e = experiments[i];
    if (e.type == Experiment::SINGLE_VALUE) {
      SetFlagToSwitchMapping(e.internal_name, e.command_line_switch,
                             e.command_line_value, &name_to_switch_map);
    } else if (e.type == Experiment::MULTI_VALUE) {
      for (int j = 0; j < e.num_choices; ++j) {
        SetFlagToSwitchMapping(e.NameForChoice(j),
                               e.choices[j].command_line_switch,
                               e.choices[j].command_line_value,
                               &name_to_switch_map);
      }
    } else {
      DCHECK_EQ(e.type, Experiment::ENABLE_DISABLE_VALUE);
      SetFlagToSwitchMapping(e.NameForChoice(0), std::string(), std::string(),
                             &name_to_switch_map);
      SetFlagToSwitchMapping(e.NameForChoice(1), e.command_line_switch,
                             e.command_line_value, &name_to_switch_map);
      SetFlagToSwitchMapping(e.NameForChoice(2), e.disable_command_line_switch,
                             e.disable_command_line_value, &name_to_switch_map);
    }
  }

  if (sentinels == kAddSentinels) {
    command_line->AppendSwitch(switches::kFlagSwitchesBegin);
    flags_switches_.insert(
        std::pair<std::string, std::string>(switches::kFlagSwitchesBegin,
                                            std::string()));
  }
  for (std::set<std::string>::iterator it = enabled_experiments.begin();
       it != enabled_experiments.end();
       ++it) {
    const std::string& experiment_name = *it;
    NameToSwitchAndValueMap::const_iterator name_to_switch_it =
        name_to_switch_map.find(experiment_name);
    if (name_to_switch_it == name_to_switch_map.end()) {
      NOTREACHED();
      continue;
    }

    const std::pair<std::string, std::string>&
        switch_and_value_pair = name_to_switch_it->second;

    CHECK(!switch_and_value_pair.first.empty());
    command_line->AppendSwitchASCII(switch_and_value_pair.first,
                                    switch_and_value_pair.second);
    flags_switches_[switch_and_value_pair.first] = switch_and_value_pair.second;
  }
  if (sentinels == kAddSentinels) {
    command_line->AppendSwitch(switches::kFlagSwitchesEnd);
    flags_switches_.insert(
        std::pair<std::string, std::string>(switches::kFlagSwitchesEnd,
                                            std::string()));
  }
}

bool FlagsState::IsRestartNeededToCommitChanges() {
  return needs_restart_;
}

void FlagsState::SetExperimentEnabled(FlagsStorage* flags_storage,
                                      const std::string& internal_name,
                                      bool enable) {
  size_t at_index = internal_name.find(testing::kMultiSeparator);
  if (at_index != std::string::npos) {
    DCHECK(enable);
    // We're being asked to enable a multi-choice experiment. Disable the
    // currently selected choice.
    DCHECK_NE(at_index, 0u);
    const std::string experiment_name = internal_name.substr(0, at_index);
    SetExperimentEnabled(flags_storage, experiment_name, false);

    // And enable the new choice, if it is not the default first choice.
    if (internal_name != experiment_name + "@0") {
      std::set<std::string> enabled_experiments;
      GetSanitizedEnabledFlags(flags_storage, &enabled_experiments);
      needs_restart_ |= enabled_experiments.insert(internal_name).second;
      flags_storage->SetFlags(enabled_experiments);
    }
    return;
  }

  std::set<std::string> enabled_experiments;
  GetSanitizedEnabledFlags(flags_storage, &enabled_experiments);

  const Experiment* e = NULL;
  for (size_t i = 0; i < num_experiments; ++i) {
    if (experiments[i].internal_name == internal_name) {
      e = experiments + i;
      break;
    }
  }
  DCHECK(e);

  if (e->type == Experiment::SINGLE_VALUE) {
    if (enable)
      needs_restart_ |= enabled_experiments.insert(internal_name).second;
    else
      needs_restart_ |= (enabled_experiments.erase(internal_name) > 0);
  } else {
    if (enable) {
      // Enable the first choice.
      needs_restart_ |= enabled_experiments.insert(e->NameForChoice(0)).second;
    } else {
      // Find the currently enabled choice and disable it.
      for (int i = 0; i < e->num_choices; ++i) {
        std::string choice_name = e->NameForChoice(i);
        if (enabled_experiments.find(choice_name) !=
            enabled_experiments.end()) {
          needs_restart_ = true;
          enabled_experiments.erase(choice_name);
          // Continue on just in case there's a bug and more than one
          // experiment for this choice was enabled.
        }
      }
    }
  }

  flags_storage->SetFlags(enabled_experiments);
}

void FlagsState::RemoveFlagsSwitches(
    std::map<std::string, CommandLine::StringType>* switch_list) {
  for (std::map<std::string, std::string>::const_iterator
           it = flags_switches_.begin(); it != flags_switches_.end(); ++it) {
    switch_list->erase(it->first);
  }
}

void FlagsState::ResetAllFlags(FlagsStorage* flags_storage) {
  needs_restart_ = true;

  std::set<std::string> no_experiments;
  flags_storage->SetFlags(no_experiments);
}

void FlagsState::reset() {
  needs_restart_ = false;
  flags_switches_.clear();
}

}  // namespace

namespace testing {

// WARNING: '@' is also used in the html file. If you update this constant you
// also need to update the html file.
const char kMultiSeparator[] = "@";

void ClearState() {
  FlagsState::GetInstance()->reset();
}

void SetExperiments(const Experiment* e, size_t count) {
  if (!e) {
    experiments = kExperiments;
    num_experiments = arraysize(kExperiments);
  } else {
    experiments = e;
    num_experiments = count;
  }
}

const Experiment* GetExperiments(size_t* count) {
  *count = num_experiments;
  return experiments;
}

}  // namespace testing

}  // namespace about_flags
