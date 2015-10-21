// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/about_flags.h"

#include <iterator>
#include <map>
#include <set>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/memory/singleton.h"
#include "base/metrics/sparse_histogram.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "cc/base/switches.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/google_chrome_strings.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/cloud_devices/common/cloud_devices_switches.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/dom_distiller/core/dom_distiller_switches.h"
#include "components/enhanced_bookmarks/enhanced_bookmark_switches.h"
#include "components/flags_ui/flags_storage.h"
#include "components/metrics/metrics_hashes.h"
#include "components/nacl/common/nacl_switches.h"
#include "components/offline_pages/offline_page_switches.h"
#include "components/omnibox/browser/omnibox_switches.h"
#include "components/password_manager/core/common/password_manager_switches.h"
#include "components/plugins/common/plugins_switches.h"
#include "components/proximity_auth/switches.h"
#include "components/search/search_switches.h"
#include "components/signin/core/common/signin_switches.h"
#include "components/sync_driver/sync_driver_switches.h"
#include "components/tracing/tracing_switches.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/content_switches.h"
#include "media/base/media_switches.h"
#include "media/midi/midi_switches.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_switches.h"
#include "ui/display/display_switches.h"
#include "ui/events/event_switches.h"
#include "ui/gfx/switches.h"
#include "ui/gl/gl_switches.h"
#include "ui/keyboard/keyboard_switches.h"
#include "ui/native_theme/native_theme_switches.h"
#include "ui/views/views_switches.h"

#if !defined(OS_ANDROID)
#include "ui/message_center/message_center_switches.h"
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

#if defined(ENABLE_EXTENSIONS)
#include "extensions/common/switches.h"
#endif

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_switches.h"
#endif

namespace about_flags {

// Macros to simplify specifying the type. Please refer to the comments on
// FeatureEntry::Type in the header file, which explain the different entry
// types and when they should be used.
#define SINGLE_VALUE_TYPE_AND_VALUE(command_line_switch, switch_value)    \
  FeatureEntry::SINGLE_VALUE, command_line_switch, switch_value, nullptr, \
      nullptr, nullptr, nullptr, 0
#define SINGLE_VALUE_TYPE(command_line_switch) \
    SINGLE_VALUE_TYPE_AND_VALUE(command_line_switch, "")
#define SINGLE_DISABLE_VALUE_TYPE_AND_VALUE(command_line_switch, switch_value) \
  FeatureEntry::SINGLE_DISABLE_VALUE, command_line_switch, switch_value,       \
      nullptr, nullptr, nullptr, nullptr, 0
#define SINGLE_DISABLE_VALUE_TYPE(command_line_switch) \
    SINGLE_DISABLE_VALUE_TYPE_AND_VALUE(command_line_switch, "")
#define ENABLE_DISABLE_VALUE_TYPE_AND_VALUE(enable_switch, enable_value,   \
                                            disable_switch, disable_value) \
  FeatureEntry::ENABLE_DISABLE_VALUE, enable_switch, enable_value,         \
      disable_switch, disable_value, nullptr, nullptr, 3
#define ENABLE_DISABLE_VALUE_TYPE(enable_switch, disable_switch) \
    ENABLE_DISABLE_VALUE_TYPE_AND_VALUE(enable_switch, "", disable_switch, "")
#define MULTI_VALUE_TYPE(choices)                                         \
  FeatureEntry::MULTI_VALUE, nullptr, nullptr, nullptr, nullptr, nullptr, \
      choices, arraysize(choices)
#define FEATURE_VALUE_TYPE(feature)                                \
  FeatureEntry::FEATURE_VALUE, nullptr, nullptr, nullptr, nullptr, \
      feature.name, nullptr, 3

namespace {

// Enumeration of OSs.
enum {
  kOsMac = 1 << 0,
  kOsWin = 1 << 1,
  kOsLinux = 1 << 2,
  kOsCrOS = 1 << 3,
  kOsAndroid = 1 << 4,
  kOsCrOSOwnerOnly = 1 << 5
};

const unsigned kOsAll = kOsMac | kOsWin | kOsLinux | kOsCrOS | kOsAndroid;
const unsigned kOsDesktop = kOsMac | kOsWin | kOsLinux | kOsCrOS;

// Adds a |StringValue| to |list| for each platform where |bitmask| indicates
// whether the entry is available on that platform.
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
  for (size_t i = 0; i < arraysize(kBitsToOs); ++i) {
    if (bitmask & kBitsToOs[i].bit)
      list->Append(new base::StringValue(kBitsToOs[i].name));
  }
}

// Convert switch constants to proper CommandLine::StringType strings.
base::CommandLine::StringType GetSwitchString(const std::string& flag) {
  base::CommandLine cmd_line(base::CommandLine::NO_PROGRAM);
  cmd_line.AppendSwitch(flag);
  DCHECK_EQ(2U, cmd_line.argv().size());
  return cmd_line.argv()[1];
}

// Scoops flags from a command line.
std::set<base::CommandLine::StringType> ExtractFlagsFromCommandLine(
    const base::CommandLine& cmdline) {
  std::set<base::CommandLine::StringType> flags;
  // First do the ones between --flag-switches-begin and --flag-switches-end.
  base::CommandLine::StringVector::const_iterator first =
      std::find(cmdline.argv().begin(), cmdline.argv().end(),
                GetSwitchString(switches::kFlagSwitchesBegin));
  base::CommandLine::StringVector::const_iterator last =
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

const FeatureEntry::Choice kTouchEventsChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_AUTOMATIC, "", "" },
  { IDS_GENERIC_EXPERIMENT_CHOICE_ENABLED,
    switches::kTouchEvents,
    switches::kTouchEventsEnabled },
  { IDS_GENERIC_EXPERIMENT_CHOICE_DISABLED,
    switches::kTouchEvents,
    switches::kTouchEventsDisabled }
};

#if defined(USE_AURA)
const FeatureEntry::Choice kOverscrollHistoryNavigationChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_ENABLED, "", "" },
  { IDS_GENERIC_EXPERIMENT_CHOICE_DISABLED,
    switches::kOverscrollHistoryNavigation,
    "0" },
  { IDS_OVERSCROLL_HISTORY_NAVIGATION_SIMPLE_UI,
    switches::kOverscrollHistoryNavigation,
    "2" }
};
#endif

const FeatureEntry::Choice kTouchTextSelectionStrategyChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", "" },
  { IDS_TOUCH_SELECTION_STRATEGY_CHARACTER,
    switches::kTouchTextSelectionStrategy,
    "character" },
  { IDS_TOUCH_SELECTION_STRATEGY_DIRECTION,
    switches::kTouchTextSelectionStrategy,
    "direction" }
};

const FeatureEntry::Choice kTraceUploadURL[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DISABLED, "", "" },
  { IDS_TRACE_UPLOAD_URL_CHOICE_OTHER, switches::kTraceUploadURL,
    "https://performance-insights.appspot.com/upload?tags=flags,Other"},
  { IDS_TRACE_UPLOAD_URL_CHOICE_EMLOADING, switches::kTraceUploadURL,
    "https://performance-insights.appspot.com/upload?tags=flags,emloading" },
  { IDS_TRACE_UPLOAD_URL_CHOICE_QA, switches::kTraceUploadURL,
    "https://performance-insights.appspot.com/upload?tags=flags,QA" },
  { IDS_TRACE_UPLOAD_URL_CHOICE_TESTING, switches::kTraceUploadURL,
    "https://performance-insights.appspot.com/upload?tags=flags,TestingTeam" }
};

#if !defined(DISABLE_NACL)
const FeatureEntry::Choice kNaClDebugMaskChoices[] = {
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

const FeatureEntry::Choice kMarkNonSecureAsChoices[] = {
    { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", "" },
    { IDS_MARK_NON_SECURE_AS_NEUTRAL,
        switches::kMarkNonSecureAs, switches::kMarkNonSecureAsNeutral},
    { IDS_MARK_NON_SECURE_AS_NON_SECURE,
        switches::kMarkNonSecureAs, switches::kMarkNonSecureAsNonSecure}
};

const FeatureEntry::Choice kDataReductionProxyLoFiChoices[] = {
    { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", "" },
    { IDS_FLAGS_DATA_REDUCTION_PROXY_LO_FI_ALWAYS_ON,
        data_reduction_proxy::switches::kDataReductionProxyLoFi,
        data_reduction_proxy::switches::kDataReductionProxyLoFiValueAlwaysOn},
    { IDS_FLAGS_DATA_REDUCTION_PROXY_LO_FI_CELLULAR_ONLY,
        data_reduction_proxy::switches::kDataReductionProxyLoFi,
        data_reduction_proxy::switches::
            kDataReductionProxyLoFiValueCellularOnly},
    { IDS_FLAGS_DATA_REDUCTION_PROXY_LO_FI_DISABLED,
        data_reduction_proxy::switches::kDataReductionProxyLoFi,
        data_reduction_proxy::switches::kDataReductionProxyLoFiValueDisabled},
    { IDS_FLAGS_DATA_REDUCTION_PROXY_LO_FI_SLOW_CONNECTIONS_ONLY,
        data_reduction_proxy::switches::kDataReductionProxyLoFi,
        data_reduction_proxy::switches::
            kDataReductionProxyLoFiValueSlowConnectionsOnly}
};

const FeatureEntry::Choice kShowSavedCopyChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", "" },
  { IDS_FLAGS_ENABLE_SHOW_SAVED_COPY_PRIMARY,
    switches::kShowSavedCopy, switches::kEnableShowSavedCopyPrimary },
  { IDS_FLAGS_ENABLE_SHOW_SAVED_COPY_SECONDARY,
    switches::kShowSavedCopy, switches::kEnableShowSavedCopySecondary },
  { IDS_FLAGS_DISABLE_SHOW_SAVED_COPY,
    switches::kShowSavedCopy, switches::kDisableShowSavedCopy }
};

const FeatureEntry::Choice kDefaultTileWidthChoices[] = {
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

const FeatureEntry::Choice kDefaultTileHeightChoices[] = {
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

const FeatureEntry::Choice kSimpleCacheBackendChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", "" },
  { IDS_GENERIC_EXPERIMENT_CHOICE_DISABLED,
    switches::kUseSimpleCacheBackend, "off" },
  { IDS_GENERIC_EXPERIMENT_CHOICE_ENABLED,
    switches::kUseSimpleCacheBackend, "on"}
};

#if defined(USE_AURA)
const FeatureEntry::Choice kTabCaptureUpscaleQualityChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", "" },
  { IDS_FLAGS_TAB_CAPTURE_SCALE_QUALITY_FAST,
    switches::kTabCaptureUpscaleQuality, "fast" },
  { IDS_FLAGS_TAB_CAPTURE_SCALE_QUALITY_GOOD,
    switches::kTabCaptureUpscaleQuality, "good" },
  { IDS_FLAGS_TAB_CAPTURE_SCALE_QUALITY_BEST,
    switches::kTabCaptureUpscaleQuality, "best" },
};

const FeatureEntry::Choice kTabCaptureDownscaleQualityChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", "" },
  { IDS_FLAGS_TAB_CAPTURE_SCALE_QUALITY_FAST,
    switches::kTabCaptureDownscaleQuality, "fast" },
  { IDS_FLAGS_TAB_CAPTURE_SCALE_QUALITY_GOOD,
    switches::kTabCaptureDownscaleQuality, "good" },
  { IDS_FLAGS_TAB_CAPTURE_SCALE_QUALITY_BEST,
    switches::kTabCaptureDownscaleQuality, "best" },
};
#endif

#if defined(OS_ANDROID)
const FeatureEntry::Choice kReaderModeHeuristicsChoices[] = {
    { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", ""},
    { IDS_FLAGS_READER_MODE_HEURISTICS_MARKUP,
      switches::kReaderModeHeuristics,
      switches::reader_mode_heuristics::kOGArticle },
    { IDS_FLAGS_READER_MODE_HEURISTICS_ADABOOST,
      switches::kReaderModeHeuristics,
      switches::reader_mode_heuristics::kAdaBoost },
    { IDS_FLAGS_READER_MODE_HEURISTICS_ALWAYS_ON,
      switches::kReaderModeHeuristics,
      switches::reader_mode_heuristics::kAlwaysTrue },
    { IDS_FLAGS_READER_MODE_HEURISTICS_ALWAYS_OFF,
      switches::kReaderModeHeuristics,
      switches::reader_mode_heuristics::kNone },
};
#endif

const FeatureEntry::Choice kNumRasterThreadsChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", "" },
  { IDS_FLAGS_NUM_RASTER_THREADS_ONE, switches::kNumRasterThreads, "1" },
  { IDS_FLAGS_NUM_RASTER_THREADS_TWO, switches::kNumRasterThreads, "2" },
  { IDS_FLAGS_NUM_RASTER_THREADS_THREE, switches::kNumRasterThreads, "3" },
  { IDS_FLAGS_NUM_RASTER_THREADS_FOUR, switches::kNumRasterThreads, "4" }
};

const FeatureEntry::Choice kGpuRasterizationMSAASampleCountChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT,
    "",
    "" },
  { IDS_FLAGS_GPU_RASTERIZATION_MSAA_SAMPLE_COUNT_ZERO,
    switches::kGpuRasterizationMSAASampleCount, "0" },
  { IDS_FLAGS_GPU_RASTERIZATION_MSAA_SAMPLE_COUNT_TWO,
    switches::kGpuRasterizationMSAASampleCount, "2" },
  { IDS_FLAGS_GPU_RASTERIZATION_MSAA_SAMPLE_COUNT_FOUR,
    switches::kGpuRasterizationMSAASampleCount, "4" },
  { IDS_FLAGS_GPU_RASTERIZATION_MSAA_SAMPLE_COUNT_EIGHT,
    switches::kGpuRasterizationMSAASampleCount, "8" },
  { IDS_FLAGS_GPU_RASTERIZATION_MSAA_SAMPLE_COUNT_SIXTEEN,
    switches::kGpuRasterizationMSAASampleCount, "16" },
};

const FeatureEntry::Choice kEnableGpuRasterizationChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", "" },
  { IDS_GENERIC_EXPERIMENT_CHOICE_ENABLED,
    switches::kEnableGpuRasterization, "" },
  { IDS_GENERIC_EXPERIMENT_CHOICE_DISABLED,
    switches::kDisableGpuRasterization, "" },
  { IDS_FLAGS_FORCE_GPU_RASTERIZATION,
    switches::kForceGpuRasterization, "" },
};

#if defined(OS_CHROMEOS)
const FeatureEntry::Choice kMemoryPressureThresholdChoices[] = {
    { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", "" },
    { IDS_FLAGS_CONSERVATIVE_THRESHOLDS,
      chromeos::switches::kMemoryPressureThresholds,
      chromeos::switches::kConservativeThreshold },
    { IDS_FLAGS_AGGRESSIVE_CACHE_DISCARD_THRESHOLDS,
      chromeos::switches::kMemoryPressureThresholds,
      chromeos::switches::kAggressiveCacheDiscardThreshold },
    { IDS_FLAGS_AGGRESSIVE_TAB_DISCARD_THRESHOLDS,
      chromeos::switches::kMemoryPressureThresholds,
      chromeos::switches::kAggressiveTabDiscardThreshold },
    { IDS_FLAGS_AGGRESSIVE_THRESHOLDS,
      chromeos::switches::kMemoryPressureThresholds,
      chromeos::switches::kAggressiveThreshold },
};
#endif

const FeatureEntry::Choice kExtensionContentVerificationChoices[] = {
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

const FeatureEntry::Choice kAutofillSyncCredentialChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", ""},
  { IDS_ALLOW_AUTOFILL_SYNC_CREDENTIAL,
    password_manager::switches::kAllowAutofillSyncCredential, ""},
  { IDS_DISALLOW_AUTOFILL_SYNC_CREDENTIAL_FOR_REAUTH,
    password_manager::switches::kDisallowAutofillSyncCredentialForReauth, ""},
  { IDS_DISALLOW_AUTOFILL_SYNC_CREDENTIAL,
    password_manager::switches::kDisallowAutofillSyncCredential, ""},
};

const FeatureEntry::Choice kFillOnAccountSelectChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", "" },
  { IDS_GENERIC_EXPERIMENT_CHOICE_DISABLED,
    autofill::switches::kDisableFillOnAccountSelect, "" },
  { IDS_FLAGS_FILL_ON_ACCOUNT_SELECT_ENABLE_HIGHLIGHTING,
    autofill::switches::kEnableFillOnAccountSelect, "" },
  { IDS_FLAGS_FILL_ON_ACCOUNT_SELECT_ENABLE_NO_HIGHLIGHTING,
    autofill::switches::kEnableFillOnAccountSelectNoHighlighting, "" },
};

#if defined(ENABLE_TOPCHROME_MD)
const FeatureEntry::Choice kTopChromeMaterialDesignChoices[] = {
    {IDS_FLAGS_TOP_CHROME_MD_NON_MATERIAL, "", ""},
    {IDS_FLAGS_TOP_CHROME_MD_MATERIAL,
     switches::kTopChromeMD,
     switches::kTopChromeMDMaterial},
    {IDS_FLAGS_TOP_CHROME_MD_MATERIAL_HYBRID,
     switches::kTopChromeMD,
     switches::kTopChromeMDMaterialHybrid}};
#endif

#if defined(OS_CHROMEOS)
const FeatureEntry::Choice kAshMaterialDesignInkDropAnimationSpeed[] = {
    {IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", ""},
    {IDS_FLAGS_MATERIAL_DESIGN_INK_DROP_ANIMATION_FAST,
     switches::kMaterialDesignInkDropAnimationSpeed,
     switches::kMaterialDesignInkDropAnimationSpeedFast},
    {IDS_FLAGS_MATERIAL_DESIGN_INK_DROP_ANIMATION_SLOW,
     switches::kMaterialDesignInkDropAnimationSpeed,
     switches::kMaterialDesignInkDropAnimationSpeedSlow}};

const FeatureEntry::Choice kDataSaverPromptChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", "" },
  { IDS_GENERIC_EXPERIMENT_CHOICE_ENABLED,
    chromeos::switches::kEnableDataSaverPrompt, "" },
  { IDS_GENERIC_EXPERIMENT_CHOICE_DISABLED,
    chromeos::switches::kDisableDataSaverPrompt, "" },
  { IDS_FLAGS_DATASAVER_PROMPT_DEMO_MODE,
    chromeos::switches::kEnableDataSaverPrompt,
    chromeos::switches::kDataSaverPromptDemoMode },
};

const FeatureEntry::Choice kFloatingVirtualKeyboardChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", "" },
  { IDS_GENERIC_EXPERIMENT_CHOICE_DISABLED,
    keyboard::switches::kFloatingVirtualKeyboard,
    keyboard::switches::kFloatingVirtualKeyboardDisabled},
  { IDS_GENERIC_EXPERIMENT_CHOICE_ENABLED,
    keyboard::switches::kFloatingVirtualKeyboard,
    keyboard::switches::kFloatingVirtualKeyboardEnabled},
};

const FeatureEntry::Choice kSmartVirtualKeyboardChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", "" },
  { IDS_GENERIC_EXPERIMENT_CHOICE_DISABLED,
    keyboard::switches::kSmartVirtualKeyboard,
    keyboard::switches::kSmartVirtualKeyboardDisabled},
  { IDS_GENERIC_EXPERIMENT_CHOICE_ENABLED,
    keyboard::switches::kSmartVirtualKeyboard,
    keyboard::switches::kSmartVirtualKeyboardEnabled},
};

const FeatureEntry::Choice kGestureTypingChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", "" },
  { IDS_GENERIC_EXPERIMENT_CHOICE_DISABLED,
    keyboard::switches::kGestureTyping,
    keyboard::switches::kGestureTypingDisabled},
  { IDS_GENERIC_EXPERIMENT_CHOICE_ENABLED,
    keyboard::switches::kGestureTyping,
    keyboard::switches::kGestureTypingEnabled},
};

const FeatureEntry::Choice kGestureEditingChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", "" },
  { IDS_GENERIC_EXPERIMENT_CHOICE_DISABLED,
    keyboard::switches::kGestureEditing,
    keyboard::switches::kGestureEditingDisabled},
  { IDS_GENERIC_EXPERIMENT_CHOICE_ENABLED,
    keyboard::switches::kGestureEditing,
    keyboard::switches::kGestureEditingEnabled},
};

const FeatureEntry::Choice kDownloadNotificationChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", "" },
  { IDS_GENERIC_EXPERIMENT_CHOICE_ENABLED,
    switches::kEnableDownloadNotification,
    "enabled" },
  { IDS_GENERIC_EXPERIMENT_CHOICE_DISABLED,
    switches::kEnableDownloadNotification,
    "disabled" }
};
#endif

const FeatureEntry::Choice kSupervisedUserSafeSitesChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", "" },
  { IDS_GENERIC_EXPERIMENT_CHOICE_ENABLED,
    switches::kSupervisedUserSafeSites,
    "enabled" },
  { IDS_GENERIC_EXPERIMENT_CHOICE_DISABLED,
    switches::kSupervisedUserSafeSites,
    "disabled" },
  { IDS_SUPERVISED_USER_SAFESITES_BLACKLIST_ONLY,
    switches::kSupervisedUserSafeSites,
    "blacklist-only" },
  { IDS_SUPERVISED_USER_SAFESITES_ONLINE_CHECK_ONLY,
    switches::kSupervisedUserSafeSites,
    "online-check-only" }
};

const FeatureEntry::Choice kV8CacheOptionsChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", "" },
  { IDS_GENERIC_EXPERIMENT_CHOICE_DISABLED, switches::kV8CacheOptions, "none" },
  { IDS_FLAGS_V8_CACHE_OPTIONS_PARSE, switches::kV8CacheOptions, "parse" },
  { IDS_FLAGS_V8_CACHE_OPTIONS_CODE, switches::kV8CacheOptions, "code" },
};

#if defined(OS_ANDROID)
const FeatureEntry::Choice kProgressBarAnimationChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", "" },
  { IDS_GENERIC_EXPERIMENT_CHOICE_DISABLED,
      switches::kProgressBarAnimation, "disabled" },
  { IDS_FLAGS_PROGRESS_BAR_ANIMATION_LINEAR,
        switches::kProgressBarAnimation, "linear" },
  { IDS_FLAGS_PROGRESS_BAR_ANIMATION_SMOOTH,
      switches::kProgressBarAnimation, "smooth" },
  { IDS_FLAGS_PROGRESS_BAR_ANIMATION_FAST_START,
      switches::kProgressBarAnimation, "fast-start" },
};
#endif  // defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
const FeatureEntry::Choice kCrosRegionsModeChoices[] = {
  { IDS_FLAGS_CROS_REGIONS_MODE_DEFAULT, "", "" },
  { IDS_FLAGS_CROS_REGIONS_MODE_OVERRIDE, chromeos::switches::kCrosRegionsMode,
        chromeos::switches::kCrosRegionsModeOverride },
  { IDS_FLAGS_CROS_REGIONS_MODE_HIDE, chromeos::switches::kCrosRegionsMode,
        chromeos::switches::kCrosRegionsModeHide },
};
#endif  // defined(OS_CHROMEOS)

#if defined(OS_WIN)
const FeatureEntry::Choice kPpapiWin32kLockdown[] = {
    {IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", ""},
    {IDS_GENERIC_EXPERIMENT_CHOICE_DISABLED,
     switches::kEnableWin32kLockDownMimeTypes, ""},
    {IDS_FLAGS_PPAPI_WIN32K_LOCKDOWN_FLASH_ONLY,
     switches::kEnableWin32kLockDownMimeTypes,
     "application/x-shockwave-flash,application/futuresplash"},
    {IDS_FLAGS_PPAPI_WIN32K_LOCKDOWN_PDF_ONLY,
     switches::kEnableWin32kLockDownMimeTypes,
     "application/x-google-chrome-pdf,application/pdf"},
    {IDS_FLAGS_PPAPI_WIN32K_LOCKDOWN_FLASH_AND_PDF,
     switches::kEnableWin32kLockDownMimeTypes,
     "application/x-shockwave-flash,application/futuresplash,"
     "application/x-google-chrome-pdf,application/pdf"},
    {IDS_FLAGS_PPAPI_WIN32K_LOCKDOWN_ALL,
     switches::kEnableWin32kLockDownMimeTypes, "*"},
};
#endif  // defined(OS_WIN)

// RECORDING USER METRICS FOR FLAGS:
// -----------------------------------------------------------------------------
// The first line of the entry is the internal name. If you'd like to gather
// statistics about the usage of your flag, you should append a marker comment
// to the end of the feature name, like so:
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
// Command-line switches must have entries in enum "LoginCustomFlags" in
// histograms.xml. See note in histograms.xml and don't forget to run
// AboutFlagsHistogramTest unit test to calculate and verify checksum.
//
// When adding a new choice, add it to the end of the list.
const FeatureEntry kFeatureEntries[] = {
    {"ignore-gpu-blacklist",
     IDS_FLAGS_IGNORE_GPU_BLACKLIST_NAME,
     IDS_FLAGS_IGNORE_GPU_BLACKLIST_DESCRIPTION,
     kOsAll,
     SINGLE_VALUE_TYPE(switches::kIgnoreGpuBlacklist)},
#if defined(OS_WIN)
    {"disable-direct-write",
     IDS_FLAGS_DISABLE_DIRECT_WRITE_NAME,
     IDS_FLAGS_DISABLE_DIRECT_WRITE_DESCRIPTION,
     kOsWin,
     SINGLE_VALUE_TYPE(switches::kDisableDirectWrite)},
#endif
    {"enable-experimental-canvas-features",
     IDS_FLAGS_ENABLE_EXPERIMENTAL_CANVAS_FEATURES_NAME,
     IDS_FLAGS_ENABLE_EXPERIMENTAL_CANVAS_FEATURES_DESCRIPTION,
     kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableExperimentalCanvasFeatures)},
    {"disable-accelerated-2d-canvas",
     IDS_FLAGS_DISABLE_ACCELERATED_2D_CANVAS_NAME,
     IDS_FLAGS_DISABLE_ACCELERATED_2D_CANVAS_DESCRIPTION,
     kOsAll,
     SINGLE_VALUE_TYPE(switches::kDisableAccelerated2dCanvas)},
    {"enable-display-list-2d-canvas",
     IDS_FLAGS_ENABLE_DISPLAY_LIST_2D_CANVAS_NAME,
     IDS_FLAGS_ENABLE_DISPLAY_LIST_2D_CANVAS_DESCRIPTION,
     kOsAll,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableDisplayList2dCanvas,
                               switches::kDisableDisplayList2dCanvas)},
    {"composited-layer-borders",
     IDS_FLAGS_COMPOSITED_LAYER_BORDERS,
     IDS_FLAGS_COMPOSITED_LAYER_BORDERS_DESCRIPTION,
     kOsAll,
     SINGLE_VALUE_TYPE(cc::switches::kShowCompositedLayerBorders)},
    {"show-fps-counter",
     IDS_FLAGS_SHOW_FPS_COUNTER,
     IDS_FLAGS_SHOW_FPS_COUNTER_DESCRIPTION,
     kOsAll,
     SINGLE_VALUE_TYPE(cc::switches::kShowFPSCounter)},
#if defined(ENABLE_WEBRTC)
    {"disable-webrtc-hw-decoding",
     IDS_FLAGS_DISABLE_WEBRTC_HW_DECODING_NAME,
     IDS_FLAGS_DISABLE_WEBRTC_HW_DECODING_DESCRIPTION,
     kOsAndroid | kOsCrOS,
     SINGLE_VALUE_TYPE(switches::kDisableWebRtcHWDecoding)},
    {"disable-webrtc-hw-encoding",
     IDS_FLAGS_DISABLE_WEBRTC_HW_ENCODING_NAME,
     IDS_FLAGS_DISABLE_WEBRTC_HW_ENCODING_DESCRIPTION,
     kOsAndroid | kOsCrOS,
     SINGLE_VALUE_TYPE(switches::kDisableWebRtcHWEncoding)},
    {"enable-webrtc-stun-origin",
     IDS_FLAGS_ENABLE_WEBRTC_STUN_ORIGIN_NAME,
     IDS_FLAGS_ENABLE_WEBRTC_STUN_ORIGIN_DESCRIPTION,
     kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableWebRtcStunOrigin)},
#endif
#if defined(OS_ANDROID)
    {"disable-webaudio",
     IDS_FLAGS_DISABLE_WEBAUDIO_NAME,
     IDS_FLAGS_DISABLE_WEBAUDIO_DESCRIPTION,
     kOsAndroid,
     SINGLE_VALUE_TYPE(switches::kDisableWebAudio)},
#endif
  // Native client is compiled out when DISABLE_NACL is defined.
#if !defined(DISABLE_NACL)
    {"enable-nacl",  // FLAGS:RECORD_UMA
     IDS_FLAGS_ENABLE_NACL_NAME,
     IDS_FLAGS_ENABLE_NACL_DESCRIPTION,
     kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableNaCl)},
    {"enable-nacl-debug",  // FLAGS:RECORD_UMA
     IDS_FLAGS_ENABLE_NACL_DEBUG_NAME,
     IDS_FLAGS_ENABLE_NACL_DEBUG_DESCRIPTION,
     kOsDesktop,
     SINGLE_VALUE_TYPE(switches::kEnableNaClDebug)},
    {"nacl-debug-mask",  // FLAGS:RECORD_UMA
     IDS_FLAGS_NACL_DEBUG_MASK_NAME,
     IDS_FLAGS_NACL_DEBUG_MASK_DESCRIPTION,
     kOsDesktop,
     MULTI_VALUE_TYPE(kNaClDebugMaskChoices)},
#endif
#if defined(ENABLE_EXTENSIONS)
    {"extension-apis",  // FLAGS:RECORD_UMA
     IDS_FLAGS_EXPERIMENTAL_EXTENSION_APIS_NAME,
     IDS_FLAGS_EXPERIMENTAL_EXTENSION_APIS_DESCRIPTION,
     kOsDesktop,
     SINGLE_VALUE_TYPE(extensions::switches::kEnableExperimentalExtensionApis)},
    {"extensions-on-chrome-urls",
     IDS_FLAGS_EXTENSIONS_ON_CHROME_URLS_NAME,
     IDS_FLAGS_EXTENSIONS_ON_CHROME_URLS_DESCRIPTION,
     kOsAll,
     SINGLE_VALUE_TYPE(extensions::switches::kExtensionsOnChromeURLs)},
#endif
    {"enable-fast-unload",
     IDS_FLAGS_ENABLE_FAST_UNLOAD_NAME,
     IDS_FLAGS_ENABLE_FAST_UNLOAD_DESCRIPTION,
     kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableFastUnload)},
#if defined(ENABLE_EXTENSIONS)
    {"enable-app-window-controls",
     IDS_FLAGS_ENABLE_APP_WINDOW_CONTROLS_NAME,
     IDS_FLAGS_ENABLE_APP_WINDOW_CONTROLS_DESCRIPTION,
     kOsDesktop,
     SINGLE_VALUE_TYPE(extensions::switches::kEnableAppWindowControls)},
#endif
    {"disable-hyperlink-auditing",
     IDS_FLAGS_DISABLE_HYPERLINK_AUDITING_NAME,
     IDS_FLAGS_DISABLE_HYPERLINK_AUDITING_DESCRIPTION,
     kOsAll,
     SINGLE_VALUE_TYPE(switches::kNoPings)},
#if defined(OS_ANDROID)
    {"contextual-search",
     IDS_FLAGS_ENABLE_CONTEXTUAL_SEARCH,
     IDS_FLAGS_ENABLE_CONTEXTUAL_SEARCH_DESCRIPTION,
     kOsAndroid,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableContextualSearch,
                               switches::kDisableContextualSearch)},
#endif
    {"show-autofill-type-predictions",
     IDS_FLAGS_SHOW_AUTOFILL_TYPE_PREDICTIONS_NAME,
     IDS_FLAGS_SHOW_AUTOFILL_TYPE_PREDICTIONS_DESCRIPTION,
     kOsAll,
     SINGLE_VALUE_TYPE(autofill::switches::kShowAutofillTypePredictions)},
    {"enable-smooth-scrolling",  // FLAGS:RECORD_UMA
     IDS_FLAGS_ENABLE_SMOOTH_SCROLLING_NAME,
     IDS_FLAGS_ENABLE_SMOOTH_SCROLLING_DESCRIPTION,
     // Can't expose the switch unless the code is compiled in.
     // On by default for the Mac (different implementation in WebKit).
     kOsLinux | kOsWin,
     SINGLE_VALUE_TYPE(switches::kEnableSmoothScrolling)},
#if defined(USE_AURA) || defined(OS_LINUX)
    {"overlay-scrollbars",
     IDS_FLAGS_ENABLE_OVERLAY_SCROLLBARS_NAME,
     IDS_FLAGS_ENABLE_OVERLAY_SCROLLBARS_DESCRIPTION,
     // Uses the system preference on Mac (a different implementation).
     // On Android, this is always enabled.
     kOsLinux | kOsCrOS | kOsWin,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableOverlayScrollbar,
                               switches::kDisableOverlayScrollbar)},
#endif
    {"enable-panels",
     IDS_FLAGS_ENABLE_PANELS_NAME,
     IDS_FLAGS_ENABLE_PANELS_DESCRIPTION,
     kOsDesktop,
     SINGLE_VALUE_TYPE(switches::kEnablePanels)},
    {// See http://crbug.com/120416 for how to remove this flag.
     "save-page-as-mhtml",  // FLAGS:RECORD_UMA
     IDS_FLAGS_SAVE_PAGE_AS_MHTML_NAME,
     IDS_FLAGS_SAVE_PAGE_AS_MHTML_DESCRIPTION,
     kOsMac | kOsWin | kOsLinux,
     SINGLE_VALUE_TYPE(switches::kSavePageAsMHTML)},
    {"enable-quic",
     IDS_FLAGS_ENABLE_QUIC_NAME,
     IDS_FLAGS_ENABLE_QUIC_DESCRIPTION,
     kOsAll,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableQuic, switches::kDisableQuic)},
    {"enable-alternative-services",
     IDS_FLAGS_ENABLE_ALTSVC_NAME,
     IDS_FLAGS_ENABLE_ALTSVC_DESCRIPTION,
     kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableAlternativeServices)},
    {"disable-media-source",
     IDS_FLAGS_DISABLE_MEDIA_SOURCE_NAME,
     IDS_FLAGS_DISABLE_MEDIA_SOURCE_DESCRIPTION,
     kOsAll,
     SINGLE_VALUE_TYPE(switches::kDisableMediaSource)},
    {"disable-encrypted-media",
     IDS_FLAGS_DISABLE_ENCRYPTED_MEDIA_NAME,
     IDS_FLAGS_DISABLE_ENCRYPTED_MEDIA_DESCRIPTION,
     kOsAll,
     SINGLE_VALUE_TYPE(switches::kDisableEncryptedMedia)},
    {"enable-prefixed-encrypted-media",
     IDS_FLAGS_ENABLE_PREFIXED_ENCRYPTED_MEDIA_NAME,
     IDS_FLAGS_ENABLE_PREFIXED_ENCRYPTED_MEDIA_DESCRIPTION,
     kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnablePrefixedEncryptedMedia)},
    {"disable-javascript-harmony-shipping",
     IDS_FLAGS_DISABLE_JAVASCRIPT_HARMONY_SHIPPING_NAME,
     IDS_FLAGS_DISABLE_JAVASCRIPT_HARMONY_SHIPPING_DESCRIPTION,
     kOsAll,
     SINGLE_VALUE_TYPE(switches::kDisableJavaScriptHarmonyShipping)},
    {"enable-javascript-harmony",
     IDS_FLAGS_ENABLE_JAVASCRIPT_HARMONY_NAME,
     IDS_FLAGS_ENABLE_JAVASCRIPT_HARMONY_DESCRIPTION,
     kOsAll,
     SINGLE_VALUE_TYPE(switches::kJavaScriptHarmony)},
    {"disable-software-rasterizer",
     IDS_FLAGS_DISABLE_SOFTWARE_RASTERIZER_NAME,
     IDS_FLAGS_DISABLE_SOFTWARE_RASTERIZER_DESCRIPTION,
#if defined(ENABLE_SWIFTSHADER)
     kOsAll,
#else
     0,
#endif
     SINGLE_VALUE_TYPE(switches::kDisableSoftwareRasterizer)},
    {"enable-gpu-rasterization",
     IDS_FLAGS_ENABLE_GPU_RASTERIZATION_NAME,
     IDS_FLAGS_ENABLE_GPU_RASTERIZATION_DESCRIPTION,
     kOsAll,
     MULTI_VALUE_TYPE(kEnableGpuRasterizationChoices)},
    {"gpu-rasterization-msaa-sample-count",
     IDS_FLAGS_GPU_RASTERIZATION_MSAA_SAMPLE_COUNT_NAME,
     IDS_FLAGS_GPU_RASTERIZATION_MSAA_SAMPLE_COUNT_DESCRIPTION,
     kOsAll,
     MULTI_VALUE_TYPE(kGpuRasterizationMSAASampleCountChoices)},
     {"enable-slimming-paint-v2",
      IDS_FLAGS_ENABLE_SLIMMING_PAINT_V2_NAME,
      IDS_FLAGS_ENABLE_SLIMMING_PAINT_V2_DESCRIPTION,
      kOsAll,
      SINGLE_VALUE_TYPE(switches::kEnableSlimmingPaintV2)},
    {"enable-experimental-web-platform-features",
     IDS_FLAGS_EXPERIMENTAL_WEB_PLATFORM_FEATURES_NAME,
     IDS_FLAGS_EXPERIMENTAL_WEB_PLATFORM_FEATURES_DESCRIPTION,
     kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableExperimentalWebPlatformFeatures)},
    {"enable-web-bluetooth",
     IDS_FLAGS_WEB_BLUETOOTH_NAME,
     IDS_FLAGS_WEB_BLUETOOTH_DESCRIPTION,
     kOsCrOS | kOsMac | kOsAndroid,
     SINGLE_VALUE_TYPE(switches::kEnableWebBluetooth)},
#if defined(ENABLE_EXTENSIONS)
    {"enable-ble-advertising-in-apps",
     IDS_FLAGS_BLE_ADVERTISING_IN_EXTENSIONS_NAME,
     IDS_FLAGS_BLE_ADVERTISING_IN_EXTENSIONS_DESCRIPTION,
     kOsCrOS,
     SINGLE_VALUE_TYPE(extensions::switches::kEnableBLEAdvertising)},
#endif
    {"enable-devtools-experiments",
     IDS_FLAGS_ENABLE_DEVTOOLS_EXPERIMENTS_NAME,
     IDS_FLAGS_ENABLE_DEVTOOLS_EXPERIMENTS_DESCRIPTION,
     kOsDesktop,
     SINGLE_VALUE_TYPE(switches::kEnableDevToolsExperiments)},
    {"silent-debugger-extension-api",
     IDS_FLAGS_SILENT_DEBUGGER_EXTENSION_API_NAME,
     IDS_FLAGS_SILENT_DEBUGGER_EXTENSION_API_DESCRIPTION,
     kOsDesktop,
     SINGLE_VALUE_TYPE(switches::kSilentDebuggerExtensionAPI)},
#if defined(ENABLE_SPELLCHECK)
#if defined(OS_ANDROID)
    {"enable-android-spellchecker",
     IDS_OPTIONS_ENABLE_SPELLCHECK,
     IDS_OPTIONS_ENABLE_ANDROID_SPELLCHECKER_DESCRIPTION,
     kOsAndroid,
     SINGLE_VALUE_TYPE(switches::kEnableAndroidSpellChecker)},
#endif
    {"spellcheck-autocorrect",
     IDS_FLAGS_SPELLCHECK_AUTOCORRECT,
     IDS_FLAGS_SPELLCHECK_AUTOCORRECT_DESCRIPTION,
     kOsWin | kOsLinux | kOsCrOS,
     SINGLE_VALUE_TYPE(switches::kEnableSpellingAutoCorrect)},
#endif
#if defined(ENABLE_SPELLCHECK) && \
    (defined(OS_LINUX) || defined(OS_WIN) || defined(OS_CHROMEOS))
    {"enable-multilingual-spellchecker",
     IDS_FLAGS_ENABLE_MULTILINGUAL_SPELLCHECKER_NAME,
     IDS_FLAGS_ENABLE_MULTILINGUAL_SPELLCHECKER_DESCRIPTION,
     kOsWin | kOsLinux | kOsCrOS,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableMultilingualSpellChecker,
                               switches::kDisableMultilingualSpellChecker)},
#endif
    {"enable-scroll-prediction",
     IDS_FLAGS_ENABLE_SCROLL_PREDICTION_NAME,
     IDS_FLAGS_ENABLE_SCROLL_PREDICTION_DESCRIPTION,
     kOsDesktop,
     SINGLE_VALUE_TYPE(switches::kEnableScrollPrediction)},
#if defined(ENABLE_TOPCHROME_MD)
    {"top-chrome-md",
     IDS_FLAGS_TOP_CHROME_MD,
     IDS_FLAGS_TOP_CHROME_MD_DESCRIPTION,
     kOsWin | kOsLinux | kOsCrOS,
     MULTI_VALUE_TYPE(kTopChromeMaterialDesignChoices)},
#endif
    {"touch-events",
     IDS_TOUCH_EVENTS_NAME,
     IDS_TOUCH_EVENTS_DESCRIPTION,
     kOsDesktop,
     MULTI_VALUE_TYPE(kTouchEventsChoices)},
    {"disable-touch-adjustment",
     IDS_DISABLE_TOUCH_ADJUSTMENT_NAME,
     IDS_DISABLE_TOUCH_ADJUSTMENT_DESCRIPTION,
     kOsWin | kOsLinux | kOsCrOS | kOsAndroid,
     SINGLE_VALUE_TYPE(switches::kDisableTouchAdjustment)},
#if defined(OS_CHROMEOS)
    {"network-portal-notification",
     IDS_FLAGS_NETWORK_PORTAL_NOTIFICATION_NAME,
     IDS_FLAGS_NETWORK_PORTAL_NOTIFICATION_DESCRIPTION,
     kOsCrOS,
     ENABLE_DISABLE_VALUE_TYPE(
         chromeos::switches::kEnableNetworkPortalNotification,
         chromeos::switches::kDisableNetworkPortalNotification)},
#endif
    {"enable-download-resumption",
     IDS_FLAGS_ENABLE_DOWNLOAD_RESUMPTION_NAME,
     IDS_FLAGS_ENABLE_DOWNLOAD_RESUMPTION_DESCRIPTION,
     kOsDesktop,
     SINGLE_VALUE_TYPE(switches::kEnableDownloadResumption)},
#if defined(OS_CHROMEOS)
    {"enable-download-notification",
     IDS_FLAGS_ENABLE_DOWNLOAD_NOTIFICATION_NAME,
     IDS_FLAGS_ENABLE_DOWNLOAD_NOTIFICATION_DESCRIPTION,
     kOsCrOS,
     MULTI_VALUE_TYPE(kDownloadNotificationChoices)},
#endif
#if defined(ENABLE_PLUGINS)
    {"allow-nacl-socket-api",
     IDS_FLAGS_ALLOW_NACL_SOCKET_API_NAME,
     IDS_FLAGS_ALLOW_NACL_SOCKET_API_DESCRIPTION,
     kOsDesktop,
     SINGLE_VALUE_TYPE_AND_VALUE(switches::kAllowNaClSocketAPI, "*")},
#endif
#if defined(OS_CHROMEOS)
    {"allow-touchpad-three-finger-click",
     IDS_FLAGS_ALLOW_TOUCHPAD_THREE_FINGER_CLICK_NAME,
     IDS_FLAGS_ALLOW_TOUCHPAD_THREE_FINGER_CLICK_DESCRIPTION,
     kOsCrOS,
     SINGLE_VALUE_TYPE(chromeos::switches::kEnableTouchpadThreeFingerClick)},
    {"ash-enable-unified-desktop",
     IDS_FLAGS_ASH_ENABLE_UNIFIED_DESKTOP_NAME,
     IDS_FLAGS_ASH_ENABLE_UNIFIED_DESKTOP_DESCRIPTION,
     kOsCrOS,
     SINGLE_VALUE_TYPE(ash::switches::kAshEnableUnifiedDesktop)},
    {"enable-easy-unlock-proximity-detection",
     IDS_FLAGS_ENABLE_EASY_UNLOCK_PROXIMITY_DETECTION_NAME,
     IDS_FLAGS_ENABLE_EASY_UNLOCK_PROXIMITY_DETECTION_DESCRIPTION,
     kOsCrOS,
     SINGLE_VALUE_TYPE(proximity_auth::switches::kEnableProximityDetection)},
    {"enable-easy-unlock-bluetooth-low-energy-detection",
     IDS_FLAGS_ENABLE_EASY_UNLOCK_BLUETOOTH_LOW_ENERGY_DISCOVERY_NAME,
     IDS_FLAGS_ENABLE_EASY_UNLOCK_BLUETOOTH_LOW_ENERGY_DISCOVERY_DESCRIPTION,
     kOsCrOS,
     SINGLE_VALUE_TYPE(
         proximity_auth::switches::kEnableBluetoothLowEnergyDiscovery)},
#endif
#if defined(USE_ASH)
    {"disable-minimize-on-second-launcher-item-click",
     IDS_FLAGS_DISABLE_MINIMIZE_ON_SECOND_LAUNCHER_ITEM_CLICK_NAME,
     IDS_FLAGS_DISABLE_MINIMIZE_ON_SECOND_LAUNCHER_ITEM_CLICK_DESCRIPTION,
     kOsAll,
     SINGLE_VALUE_TYPE(switches::kDisableMinimizeOnSecondLauncherItemClick)},
    {"show-touch-hud",
     IDS_FLAGS_SHOW_TOUCH_HUD_NAME,
     IDS_FLAGS_SHOW_TOUCH_HUD_DESCRIPTION,
     kOsAll,
     SINGLE_VALUE_TYPE(ash::switches::kAshTouchHud)},
    {
     "enable-pinch",
     IDS_FLAGS_ENABLE_PINCH_SCALE_NAME,
     IDS_FLAGS_ENABLE_PINCH_SCALE_DESCRIPTION,
     kOsLinux | kOsWin | kOsCrOS,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnablePinch, switches::kDisablePinch),
    },
#endif  // defined(USE_ASH)
#if defined(OS_CHROMEOS)
    {
     "disable-boot-animation",
     IDS_FLAGS_DISABLE_BOOT_ANIMATION,
     IDS_FLAGS_DISABLE_BOOT_ANIMATION_DESCRIPTION,
     kOsCrOSOwnerOnly,
     SINGLE_VALUE_TYPE(chromeos::switches::kDisableBootAnimation),
    },
    {"enable-video-player-chromecast-support",
     IDS_FLAGS_ENABLE_VIDEO_PLAYER_CHROMECAST_SUPPORT_NAME,
     IDS_FLAGS_ENABLE_VIDEO_PLAYER_CHROMECAST_SUPPORT_DESCRIPTION,
     kOsCrOS,
     SINGLE_VALUE_TYPE(
         chromeos::switches::kEnableVideoPlayerChromecastSupport)},
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
    {
     "ash-disable-screen-orientation-lock",
     IDS_FLAGS_ASH_DISABLE_SCREEN_ORIENTATION_LOCK_NAME,
     IDS_FLAGS_ASH_DISABLE_SCREEN_ORIENTATION_LOCK_DESCRIPTION,
     kOsCrOS,
     SINGLE_VALUE_TYPE(ash::switches::kAshDisableScreenOrientationLock),
    },
#endif  // defined(OS_CHROMEOS)
    {
     "disable-accelerated-video-decode",
     IDS_FLAGS_DISABLE_ACCELERATED_VIDEO_DECODE_NAME,
     IDS_FLAGS_DISABLE_ACCELERATED_VIDEO_DECODE_DESCRIPTION,
     kOsMac | kOsWin | kOsCrOS,
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
    {
     "ash-disable-maximize-mode-window-backdrop",
     IDS_FLAGS_ASH_DISABLE_MAXIMIZE_MODE_WINDOW_BACKDROP_NAME,
     IDS_FLAGS_ASH_DISABLE_MAXIMIZE_MODE_WINDOW_BACKDROP_DESCRIPTION,
     kOsCrOS,
     SINGLE_VALUE_TYPE(ash::switches::kAshDisableMaximizeModeWindowBackdrop),
    },
    {
     "ash-enable-touch-view-testing",
     IDS_FLAGS_ASH_ENABLE_TOUCH_VIEW_TESTING_NAME,
     IDS_FLAGS_ASH_ENABLE_TOUCH_VIEW_TESTING_DESCRIPTION,
     kOsCrOS,
     SINGLE_VALUE_TYPE(ash::switches::kAshEnableTouchViewTesting),
    },
    {
     "disable-touch-feedback",
     IDS_FLAGS_DISABLE_TOUCH_FEEDBACK_NAME,
     IDS_FLAGS_DISABLE_TOUCH_FEEDBACK_DESCRIPTION,
     kOsCrOS,
     SINGLE_VALUE_TYPE(switches::kDisableTouchFeedback),
    },
    {
     "ash-enable-mirrored-screen",
     IDS_FLAGS_ASH_ENABLE_MIRRORED_SCREEN_NAME,
     IDS_FLAGS_ASH_ENABLE_MIRRORED_SCREEN_DESCRIPTION,
     kOsCrOS,
     SINGLE_VALUE_TYPE(ash::switches::kAshEnableMirroredScreen),
    },
    {
     "ash-stable-overview-order",
     IDS_FLAGS_ASH_STABLE_OVERVIEW_ORDER_NAME,
     IDS_FLAGS_ASH_STABLE_OVERVIEW_ORDER_DESCRIPTION,
     kOsCrOS,
     ENABLE_DISABLE_VALUE_TYPE(ash::switches::kAshEnableStableOverviewOrder,
                               ash::switches::kAshDisableStableOverviewOrder),
    },
#endif  // defined(USE_ASH)
#if defined(OS_CHROMEOS)
    {"material-design-ink-drop-animation-speed",
     IDS_FLAGS_MATERIAL_DESIGN_INK_DROP_ANIMATION_SPEED_NAME,
     IDS_FLAGS_MATERIAL_DESIGN_INK_DROP_ANIMATION_SPEED_DESCRIPTION,
     kOsCrOS,
     MULTI_VALUE_TYPE(kAshMaterialDesignInkDropAnimationSpeed)},
    {"disable-cloud-import",
     IDS_FLAGS_DISABLE_CLOUD_IMPORT,
     IDS_FLAGS_DISABLE_CLOUD_IMPORT_DESCRIPTION,
     kOsCrOS,
     SINGLE_VALUE_TYPE(chromeos::switches::kDisableCloudImport)},
    {"enable-request-tablet-site",
     IDS_FLAGS_ENABLE_REQUEST_TABLET_SITE_NAME,
     IDS_FLAGS_ENABLE_REQUEST_TABLET_SITE_DESCRIPTION,
     kOsCrOS,
     SINGLE_VALUE_TYPE(chromeos::switches::kEnableRequestTabletSite)},
#endif
    {"debug-packed-apps",
     IDS_FLAGS_DEBUG_PACKED_APP_NAME,
     IDS_FLAGS_DEBUG_PACKED_APP_DESCRIPTION,
     kOsDesktop,
     SINGLE_VALUE_TYPE(switches::kDebugPackedApps)},
    {"enable-password-generation",
     IDS_FLAGS_ENABLE_PASSWORD_GENERATION_NAME,
     IDS_FLAGS_ENABLE_PASSWORD_GENERATION_DESCRIPTION,
     kOsWin | kOsLinux | kOsCrOS | kOsMac | kOsAndroid,
     ENABLE_DISABLE_VALUE_TYPE(autofill::switches::kEnablePasswordGeneration,
                               autofill::switches::kDisablePasswordGeneration)},
    {"enable-automatic-password-saving",
     IDS_FLAGS_ENABLE_AUTOMATIC_PASSWORD_SAVING_NAME,
     IDS_FLAGS_ENABLE_AUTOMATIC_PASSWORD_SAVING_DESCRIPTION,
     kOsDesktop,
     SINGLE_VALUE_TYPE(
         password_manager::switches::kEnableAutomaticPasswordSaving)},
    {"password-manager-reauthentication",
     IDS_FLAGS_PASSWORD_MANAGER_REAUTHENTICATION_NAME,
     IDS_FLAGS_PASSWORD_MANAGER_REAUTHENTICATION_DESCRIPTION,
     kOsMac | kOsWin,
     SINGLE_VALUE_TYPE(switches::kDisablePasswordManagerReauthentication)},
    {"enable-password-change-support",
     IDS_FLAGS_ENABLE_PASSWORD_CHANGE_SUPPORT_NAME,
     IDS_FLAGS_ENABLE_PASSWORD_CHANGE_SUPPORT_DESCRIPTION,
     kOsMac,
     SINGLE_VALUE_TYPE(
         password_manager::switches::kEnablePasswordChangeSupport)},
    {"enable-password-force-saving",
     IDS_FLAGS_ENABLE_PASSWORD_FORCE_SAVING_NAME,
     IDS_FLAGS_ENABLE_PASSWORD_FORCE_SAVING_DESCRIPTION,
     kOsAll,
     SINGLE_VALUE_TYPE(password_manager::switches::kEnablePasswordForceSaving)},
    {"enable-affiliation-based-matching",
     IDS_FLAGS_ENABLE_AFFILIATION_BASED_MATCHING_NAME,
     IDS_FLAGS_ENABLE_AFFILIATION_BASED_MATCHING_DESCRIPTION,
     kOsWin | kOsLinux | kOsCrOS | kOsMac | kOsAndroid,
     ENABLE_DISABLE_VALUE_TYPE(
         password_manager::switches::kEnableAffiliationBasedMatching,
         password_manager::switches::kDisableAffiliationBasedMatching)},
    {"wallet-service-use-sandbox",
     IDS_FLAGS_WALLET_SERVICE_USE_SANDBOX_NAME,
     IDS_FLAGS_WALLET_SERVICE_USE_SANDBOX_DESCRIPTION,
     kOsAndroid | kOsDesktop,
     ENABLE_DISABLE_VALUE_TYPE_AND_VALUE(
         autofill::switches::kWalletServiceUseSandbox,
         "1",
         autofill::switches::kWalletServiceUseSandbox,
         "0")},
#if defined(USE_AURA)
    {"overscroll-history-navigation",
     IDS_FLAGS_OVERSCROLL_HISTORY_NAVIGATION_NAME,
     IDS_FLAGS_OVERSCROLL_HISTORY_NAVIGATION_DESCRIPTION,
     kOsAll,
     MULTI_VALUE_TYPE(kOverscrollHistoryNavigationChoices)},
#endif
    {"scroll-end-effect",
     IDS_FLAGS_SCROLL_END_EFFECT_NAME,
     IDS_FLAGS_SCROLL_END_EFFECT_DESCRIPTION,
     kOsCrOS,
     ENABLE_DISABLE_VALUE_TYPE_AND_VALUE(switches::kScrollEndEffect,
                                         "1",
                                         switches::kScrollEndEffect,
                                         "0")},
    {"enable-icon-ntp",
     IDS_FLAGS_ENABLE_ICON_NTP_NAME,
     IDS_FLAGS_ENABLE_ICON_NTP_DESCRIPTION,
     kOsAndroid | kOsMac | kOsWin | kOsLinux | kOsCrOS,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableIconNtp,
                               switches::kDisableIconNtp)},
    {"enable-touch-drag-drop",
     IDS_FLAGS_ENABLE_TOUCH_DRAG_DROP_NAME,
     IDS_FLAGS_ENABLE_TOUCH_DRAG_DROP_DESCRIPTION,
     kOsWin | kOsCrOS,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableTouchDragDrop,
                               switches::kDisableTouchDragDrop)},
    {"touch-selection-strategy",
     IDS_FLAGS_TOUCH_SELECTION_STRATEGY_NAME,
     IDS_FLAGS_TOUCH_SELECTION_STRATEGY_DESCRIPTION,
     kOsAndroid,  // TODO(mfomitchev): Add CrOS/Win/Linux support soon.
     MULTI_VALUE_TYPE(kTouchTextSelectionStrategyChoices)},
    {"enable-navigation-tracing",
     IDS_FLAGS_ENABLE_NAVIGATION_TRACING,
     IDS_FLAGS_ENABLE_NAVIGATION_TRACING_DESCRIPTION,
     kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableNavigationTracing)},
    {"trace-upload-url",
     IDS_FLAGS_TRACE_UPLOAD_URL,
     IDS_FLAGS_TRACE_UPLOAD_URL_DESCRIPTION,
     kOsAll,
     MULTI_VALUE_TYPE(kTraceUploadURL)},
    {"enable-suggestions-service",
     IDS_FLAGS_ENABLE_SUGGESTIONS_SERVICE_NAME,
     IDS_FLAGS_ENABLE_SUGGESTIONS_SERVICE_DESCRIPTION,
     kOsAndroid | kOsCrOS,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableSuggestionsService,
                               switches::kDisableSuggestionsService)},
    {"enable-suggestions-with-substring-match",
     IDS_FLAGS_ENABLE_SUGGESTIONS_WITH_SUB_STRING_MATCH_NAME,
     IDS_FLAGS_ENABLE_SUGGESTIONS_WITH_SUB_STRING_MATCH_DESCRIPTION,
     kOsAll,
     SINGLE_VALUE_TYPE(
         autofill::switches::kEnableSuggestionsWithSubstringMatch)},
    {"enable-supervised-user-managed-bookmarks-folder",
     IDS_FLAGS_ENABLE_SUPERVISED_USER_MANAGED_BOOKMARKS_FOLDER_NAME,
     IDS_FLAGS_ENABLE_SUPERVISED_USER_MANAGED_BOOKMARKS_FOLDER_DESCRIPTION,
     kOsAndroid | kOsMac | kOsWin | kOsLinux | kOsCrOS,
     SINGLE_VALUE_TYPE(switches::kEnableSupervisedUserManagedBookmarksFolder)},
#if defined(ENABLE_APP_LIST)
    {"enable-sync-app-list",
     IDS_FLAGS_ENABLE_SYNC_APP_LIST_NAME,
     IDS_FLAGS_ENABLE_SYNC_APP_LIST_DESCRIPTION,
     kOsDesktop,
     ENABLE_DISABLE_VALUE_TYPE(app_list::switches::kEnableSyncAppList,
                               app_list::switches::kDisableSyncAppList)},
#endif
#if defined(OS_MACOSX)
    {"enable-avfoundation",
     IDS_FLAGS_ENABLE_AVFOUNDATION_NAME,
     IDS_FLAGS_ENABLE_AVFOUNDATION_DESCRIPTION,
     kOsMac,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableAVFoundation,
                               switches::kForceQTKit)},
#endif
    {"lcd-text-aa",
     IDS_FLAGS_LCD_TEXT_NAME,
     IDS_FLAGS_LCD_TEXT_DESCRIPTION,
     kOsDesktop,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableLCDText,
                               switches::kDisableLCDText)},
    {"enable-offer-store-unmasked-wallet-cards",
     IDS_FLAGS_ENABLE_OFFER_STORE_UNMASKED_WALLET_CARDS,
     IDS_FLAGS_ENABLE_OFFER_STORE_UNMASKED_WALLET_CARDS_DESCRIPTION,
     kOsAll,
     ENABLE_DISABLE_VALUE_TYPE(
         autofill::switches::kEnableOfferStoreUnmaskedWalletCards,
         autofill::switches::kDisableOfferStoreUnmaskedWalletCards)},
    {"enable-offline-auto-reload",
     IDS_FLAGS_ENABLE_OFFLINE_AUTO_RELOAD_NAME,
     IDS_FLAGS_ENABLE_OFFLINE_AUTO_RELOAD_DESCRIPTION,
     kOsAll,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableOfflineAutoReload,
                               switches::kDisableOfflineAutoReload)},
    {"enable-offline-auto-reload-visible-only",
     IDS_FLAGS_ENABLE_OFFLINE_AUTO_RELOAD_VISIBLE_ONLY_NAME,
     IDS_FLAGS_ENABLE_OFFLINE_AUTO_RELOAD_VISIBLE_ONLY_DESCRIPTION,
     kOsAll,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableOfflineAutoReloadVisibleOnly,
                               switches::kDisableOfflineAutoReloadVisibleOnly)},
    {"show-saved-copy",
     IDS_FLAGS_SHOW_SAVED_COPY_NAME,
     IDS_FLAGS_SHOW_SAVED_COPY_DESCRIPTION,
     kOsAll,
     MULTI_VALUE_TYPE(kShowSavedCopyChoices)},
    {"default-tile-width",
     IDS_FLAGS_DEFAULT_TILE_WIDTH_NAME,
     IDS_FLAGS_DEFAULT_TILE_WIDTH_DESCRIPTION,
     kOsAll,
     MULTI_VALUE_TYPE(kDefaultTileWidthChoices)},
    {"default-tile-height",
     IDS_FLAGS_DEFAULT_TILE_HEIGHT_NAME,
     IDS_FLAGS_DEFAULT_TILE_HEIGHT_DESCRIPTION,
     kOsAll,
     MULTI_VALUE_TYPE(kDefaultTileHeightChoices)},
#if defined(OS_ANDROID)
    {"disable-gesture-requirement-for-media-playback",
     IDS_FLAGS_DISABLE_GESTURE_REQUIREMENT_FOR_MEDIA_PLAYBACK_NAME,
     IDS_FLAGS_DISABLE_GESTURE_REQUIREMENT_FOR_MEDIA_PLAYBACK_DESCRIPTION,
     kOsAndroid,
     SINGLE_VALUE_TYPE(switches::kDisableGestureRequirementForMediaPlayback)},
#endif
#if defined(OS_CHROMEOS)
    {"enable-virtual-keyboard",
     IDS_FLAGS_ENABLE_VIRTUAL_KEYBOARD_NAME,
     IDS_FLAGS_ENABLE_VIRTUAL_KEYBOARD_DESCRIPTION,
     kOsCrOS,
     SINGLE_VALUE_TYPE(keyboard::switches::kEnableVirtualKeyboard)},
    {"enable-virtual-keyboard-overscroll",
     IDS_FLAGS_ENABLE_VIRTUAL_KEYBOARD_OVERSCROLL_NAME,
     IDS_FLAGS_ENABLE_VIRTUAL_KEYBOARD_OVERSCROLL_DESCRIPTION,
     kOsCrOS,
     ENABLE_DISABLE_VALUE_TYPE(
         keyboard::switches::kEnableVirtualKeyboardOverscroll,
         keyboard::switches::kDisableVirtualKeyboardOverscroll)},
    {"enable-input-view",
     IDS_FLAGS_ENABLE_INPUT_VIEW_NAME,
     IDS_FLAGS_ENABLE_INPUT_VIEW_DESCRIPTION,
     kOsCrOS,
     ENABLE_DISABLE_VALUE_TYPE(keyboard::switches::kEnableInputView,
                               keyboard::switches::kDisableInputView)},
    {"disable-new-korean-ime",
     IDS_FLAGS_DISABLE_NEW_KOREAN_IME_NAME,
     IDS_FLAGS_DISABLE_NEW_KOREAN_IME_DESCRIPTION,
     kOsCrOS,
     SINGLE_VALUE_TYPE(chromeos::switches::kDisableNewKoreanIme)},
    {"enable-physical-keyboard-autocorrect",
     IDS_FLAGS_ENABLE_PHYSICAL_KEYBOARD_AUTOCORRECT_NAME,
     IDS_FLAGS_ENABLE_PHYSICAL_KEYBOARD_AUTOCORRECT_DESCRIPTION,
     kOsCrOS,
     ENABLE_DISABLE_VALUE_TYPE(
         chromeos::switches::kEnablePhysicalKeyboardAutocorrect,
         chromeos::switches::kDisablePhysicalKeyboardAutocorrect)},
    {"disable-voice-input",
     IDS_FLAGS_DISABLE_VOICE_INPUT_NAME,
     IDS_FLAGS_DISABLE_VOICE_INPUT_DESCRIPTION,
     kOsCrOS,
     SINGLE_VALUE_TYPE(keyboard::switches::kDisableVoiceInput)},
    {"enable-experimental-input-view-features",
     IDS_FLAGS_ENABLE_EXPERIMENTAL_INPUT_VIEW_FEATURES_NAME,
     IDS_FLAGS_ENABLE_EXPERIMENTAL_INPUT_VIEW_FEATURES_DESCRIPTION,
     kOsCrOS,
     SINGLE_VALUE_TYPE(
         keyboard::switches::kEnableExperimentalInputViewFeatures)},
    {"floating-virtual-keyboard",
     IDS_FLAGS_FLOATING_VIRTUAL_KEYBOARD_NAME,
     IDS_FLAGS_FLOATING_VIRTUAL_KEYBOARD_DESCRIPTION,
     kOsCrOS,
     MULTI_VALUE_TYPE(kFloatingVirtualKeyboardChoices)},
    {"smart-virtual-keyboard",
     IDS_FLAGS_SMART_VIRTUAL_KEYBOARD_NAME,
     IDS_FLAGS_SMART_VIRTUAL_KEYBOARD_DESCRIPTION,
     kOsCrOS,
     MULTI_VALUE_TYPE(kSmartVirtualKeyboardChoices)},
    {"gesture-typing",
     IDS_FLAGS_GESTURE_TYPING_NAME,
     IDS_FLAGS_GESTURE_TYPING_DESCRIPTION,
     kOsCrOS,
     MULTI_VALUE_TYPE(kGestureTypingChoices)},
    {"gesture-editing",
     IDS_FLAGS_GESTURE_EDITING_NAME,
     IDS_FLAGS_GESTURE_EDITING_DESCRIPTION,
     kOsCrOS,
     MULTI_VALUE_TYPE(kGestureEditingChoices)},
    {"enable-fullscreen-app-list",
     IDS_FLAGS_FULLSCREEN_APP_LIST_NAME,
     IDS_FLAGS_FULLSCREEN_APP_LIST_DESCRIPTION,
     kOsCrOS,
     SINGLE_VALUE_TYPE(ash::switches::kAshEnableFullscreenAppList)},
#endif
    {"enable-simple-cache-backend",
     IDS_FLAGS_ENABLE_SIMPLE_CACHE_BACKEND_NAME,
     IDS_FLAGS_ENABLE_SIMPLE_CACHE_BACKEND_DESCRIPTION,
     kOsWin | kOsMac | kOsLinux | kOsCrOS,
     MULTI_VALUE_TYPE(kSimpleCacheBackendChoices)},
    {"enable-tcp-fast-open",
     IDS_FLAGS_ENABLE_TCP_FAST_OPEN_NAME,
     IDS_FLAGS_ENABLE_TCP_FAST_OPEN_DESCRIPTION,
     kOsLinux | kOsCrOS | kOsAndroid,
     SINGLE_VALUE_TYPE(switches::kEnableTcpFastOpen)},
#if defined(ENABLE_SERVICE_DISCOVERY)
    {"device-discovery-notifications",
     IDS_FLAGS_DEVICE_DISCOVERY_NOTIFICATIONS_NAME,
     IDS_FLAGS_DEVICE_DISCOVERY_NOTIFICATIONS_DESCRIPTION,
     kOsDesktop,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableDeviceDiscoveryNotifications,
                               switches::kDisableDeviceDiscoveryNotifications)},
    {"enable-print-preview-register-promos",
     IDS_FLAGS_ENABLE_PRINT_PREVIEW_REGISTER_PROMOS_NAME,
     IDS_FLAGS_ENABLE_PRINT_PREVIEW_REGISTER_PROMOS_DESCRIPTION,
     kOsDesktop,
     SINGLE_VALUE_TYPE(switches::kEnablePrintPreviewRegisterPromos)},
#endif  // ENABLE_SERVICE_DISCOVERY
#if defined(OS_WIN)
    {"enable-cloud-print-xps",
     IDS_FLAGS_ENABLE_CLOUD_PRINT_XPS_NAME,
     IDS_FLAGS_ENABLE_CLOUD_PRINT_XPS_DESCRIPTION,
     kOsWin,
     SINGLE_VALUE_TYPE(switches::kEnableCloudPrintXps)},
#endif
#if defined(USE_AURA)
    {"tab-capture-upscale-quality",
     IDS_FLAGS_TAB_CAPTURE_UPSCALE_QUALITY_NAME,
     IDS_FLAGS_TAB_CAPTURE_UPSCALE_QUALITY_DESCRIPTION,
     kOsAll,
     MULTI_VALUE_TYPE(kTabCaptureUpscaleQualityChoices)},
    {"tab-capture-downscale-quality",
     IDS_FLAGS_TAB_CAPTURE_DOWNSCALE_QUALITY_NAME,
     IDS_FLAGS_TAB_CAPTURE_DOWNSCALE_QUALITY_DESCRIPTION,
     kOsAll,
     MULTI_VALUE_TYPE(kTabCaptureDownscaleQualityChoices)},
#endif
#if defined(TOOLKIT_VIEWS)
    {"disable-hide-inactive-stacked-tab-close-buttons",
     IDS_FLAGS_DISABLE_HIDE_INACTIVE_STACKED_TAB_CLOSE_BUTTONS_NAME,
     IDS_FLAGS_DISABLE_HIDE_INACTIVE_STACKED_TAB_CLOSE_BUTTONS_DESCRIPTION,
     kOsCrOS | kOsWin | kOsLinux,
     SINGLE_VALUE_TYPE(switches::kDisableHideInactiveStackedTabCloseButtons)},
#endif
#if defined(ENABLE_SPELLCHECK)
    {"enable-spelling-feedback-field-trial",
     IDS_FLAGS_ENABLE_SPELLING_FEEDBACK_FIELD_TRIAL_NAME,
     IDS_FLAGS_ENABLE_SPELLING_FEEDBACK_FIELD_TRIAL_DESCRIPTION,
     kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableSpellingFeedbackFieldTrial)},
#endif
    {"enable-webgl-draft-extensions",
     IDS_FLAGS_ENABLE_WEBGL_DRAFT_EXTENSIONS_NAME,
     IDS_FLAGS_ENABLE_WEBGL_DRAFT_EXTENSIONS_DESCRIPTION,
     kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableWebGLDraftExtensions)},
    {"enable-new-profile-management",
     IDS_FLAGS_ENABLE_NEW_PROFILE_MANAGEMENT_NAME,
     IDS_FLAGS_ENABLE_NEW_PROFILE_MANAGEMENT_DESCRIPTION,
     kOsAndroid | kOsMac | kOsWin | kOsLinux | kOsCrOS,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableNewProfileManagement,
                               switches::kDisableNewProfileManagement)},
    {"enable-account-consistency",
     IDS_FLAGS_ENABLE_ACCOUNT_CONSISTENCY_NAME,
     IDS_FLAGS_ENABLE_ACCOUNT_CONSISTENCY_DESCRIPTION,
     kOsAndroid | kOsMac | kOsWin | kOsLinux | kOsCrOS,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableAccountConsistency,
                               switches::kDisableAccountConsistency)},
    {"enable-iframe-based-signin",
     IDS_FLAGS_ENABLE_IFRAME_BASED_SIGNIN_NAME,
     IDS_FLAGS_ENABLE_IFRAME_BASED_SIGNIN_DESCRIPTION,
     kOsMac | kOsWin | kOsLinux,
     SINGLE_VALUE_TYPE(switches::kEnableIframeBasedSignin)},
    {"enable-password-separated-signin-flow",
     IDS_FLAGS_ENABLE_PASSWORD_SEPARATED_SIGNIN_FLOW_NAME,
     IDS_FLAGS_ENABLE_PASSWORD_SEPARATED_SIGNIN_FLOW_DESCRIPTION,
     kOsWin | kOsLinux,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnablePasswordSeparatedSigninFlow,
                               switches::kDisablePasswordSeparatedSigninFlow)},
    {"enable-google-profile-info",
     IDS_FLAGS_ENABLE_GOOGLE_PROFILE_INFO_NAME,
     IDS_FLAGS_ENABLE_GOOGLE_PROFILE_INFO_DESCRIPTION,
     kOsMac | kOsWin | kOsLinux,
     SINGLE_VALUE_TYPE(switches::kGoogleProfileInfo)},
#if defined(ENABLE_APP_LIST)
    {"reset-app-list-install-state",
     IDS_FLAGS_RESET_APP_LIST_INSTALL_STATE_NAME,
     IDS_FLAGS_RESET_APP_LIST_INSTALL_STATE_DESCRIPTION,
     kOsMac | kOsWin | kOsLinux,
     SINGLE_VALUE_TYPE(app_list::switches::kResetAppListInstallState)},
#endif
#if defined(OS_ANDROID)
    {"enable-accessibility-tab-switcher",
     IDS_FLAGS_ENABLE_ACCESSIBILITY_TAB_SWITCHER_NAME,
     IDS_FLAGS_ENABLE_ACCESSIBILITY_TAB_SWITCHER_DESCRIPTION,
     kOsAndroid,
     SINGLE_VALUE_TYPE(switches::kEnableAccessibilityTabSwitcher)},
    {"enable-physical-web",
     IDS_FLAGS_ENABLE_PHYSICAL_WEB_NAME,
     IDS_FLAGS_ENABLE_PHYSICAL_WEB_DESCRIPTION,
     kOsAndroid,
     SINGLE_VALUE_TYPE(switches::kEnablePhysicalWeb)},
#endif
    {"enable-zero-copy",
     IDS_FLAGS_ENABLE_ZERO_COPY_NAME,
     IDS_FLAGS_ENABLE_ZERO_COPY_DESCRIPTION,
     kOsAll,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableZeroCopy,
                               switches::kDisableZeroCopy)},
#if defined(OS_CHROMEOS)
    {"enable-first-run-ui-transitions",
     IDS_FLAGS_ENABLE_FIRST_RUN_UI_TRANSITIONS_NAME,
     IDS_FLAGS_ENABLE_FIRST_RUN_UI_TRANSITIONS_DESCRIPTION,
     kOsCrOS,
     SINGLE_VALUE_TYPE(chromeos::switches::kEnableFirstRunUITransitions)},
#endif
    {"disable-new-bookmark-apps",
     IDS_FLAGS_NEW_BOOKMARK_APPS_NAME,
     IDS_FLAGS_NEW_BOOKMARK_APPS_DESCRIPTION,
     kOsWin | kOsCrOS | kOsLinux | kOsMac,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableNewBookmarkApps,
                               switches::kDisableNewBookmarkApps)},
#if defined(OS_MACOSX)
    {"disable-hosted-apps-in-windows",
     IDS_FLAGS_HOSTED_APPS_IN_WINDOWS_NAME,
     IDS_FLAGS_HOSTED_APPS_IN_WINDOWS_DESCRIPTION,
     kOsMac,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableHostedAppsInWindows,
                               switches::kDisableHostedAppsInWindows)},
    {"disable-hosted-app-shim-creation",
     IDS_FLAGS_DISABLE_HOSTED_APP_SHIM_CREATION_NAME,
     IDS_FLAGS_DISABLE_HOSTED_APP_SHIM_CREATION_DESCRIPTION,
     kOsMac,
     SINGLE_VALUE_TYPE(switches::kDisableHostedAppShimCreation)},
    {"enable-hosted-app-quit-notification",
     IDS_FLAGS_ENABLE_HOSTED_APP_QUIT_NOTIFICATION_NAME,
     IDS_FLAGS_ENABLE_HOSTED_APP_QUIT_NOTIFICATION_DESCRIPTION,
     kOsMac,
     SINGLE_VALUE_TYPE(switches::kHostedAppQuitNotification)},
#endif
#if defined(OS_ANDROID)
    {"disable-pull-to-refresh-effect",
     IDS_FLAGS_DISABLE_PULL_TO_REFRESH_EFFECT_NAME,
     IDS_FLAGS_DISABLE_PULL_TO_REFRESH_EFFECT_DESCRIPTION,
     kOsAndroid,
     SINGLE_VALUE_TYPE(switches::kDisablePullToRefreshEffect)},
#endif
#if defined(OS_MACOSX)
    {"enable-translate-new-ux",
     IDS_FLAGS_ENABLE_TRANSLATE_NEW_UX_NAME,
     IDS_FLAGS_ENABLE_TRANSLATE_NEW_UX_DESCRIPTION,
     kOsMac,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableTranslateNewUX,
                               switches::kDisableTranslateNewUX)},
#endif
#if defined(TOOLKIT_VIEWS)
    {"disable-views-rect-based-targeting",  // FLAGS:RECORD_UMA
     IDS_FLAGS_DISABLE_VIEWS_RECT_BASED_TARGETING_NAME,
     IDS_FLAGS_DISABLE_VIEWS_RECT_BASED_TARGETING_DESCRIPTION,
     kOsCrOS | kOsWin | kOsLinux,
     SINGLE_VALUE_TYPE(views::switches::kDisableViewsRectBasedTargeting)},
    {"enable-link-disambiguation-popup",
     IDS_FLAGS_ENABLE_LINK_DISAMBIGUATION_POPUP_NAME,
     IDS_FLAGS_ENABLE_LINK_DISAMBIGUATION_POPUP_DESCRIPTION,
     kOsCrOS | kOsWin,
     SINGLE_VALUE_TYPE(switches::kEnableLinkDisambiguationPopup)},
#endif
#if defined(ENABLE_EXTENSIONS)
    {"enable-apps-show-on-first-paint",
     IDS_FLAGS_ENABLE_APPS_SHOW_ON_FIRST_PAINT_NAME,
     IDS_FLAGS_ENABLE_APPS_SHOW_ON_FIRST_PAINT_DESCRIPTION,
     kOsDesktop,
     SINGLE_VALUE_TYPE(extensions::switches::kEnableAppsShowOnFirstPaint)},
#endif
    {"enhanced-bookmarks-experiment",
     IDS_FLAGS_ENABLE_ENHANCED_BOOKMARKS_NAME,
     IDS_FLAGS_ENABLE_ENHANCED_BOOKMARKS_DESCRIPTION,
     kOsDesktop | kOsAndroid,
     ENABLE_DISABLE_VALUE_TYPE_AND_VALUE(switches::kEnhancedBookmarksExperiment,
                                         "1",
                                         switches::kEnhancedBookmarksExperiment,
                                         "0")},
#if defined(OS_ANDROID)
    {"reader-mode-heuristics",
     IDS_FLAGS_READER_MODE_HEURISTICS_NAME,
     IDS_FLAGS_READER_MODE_HEURISTICS_DESCRIPTION,
     kOsAndroid,
     MULTI_VALUE_TYPE(kReaderModeHeuristicsChoices)},
    {"enable-dom-distiller-button-animation",
     IDS_FLAGS_READER_MODE_BUTTON_ANIMATION,
     IDS_FLAGS_READER_MODE_BUTTON_ANIMATION_DESCRIPTION,
     kOsAndroid,
     SINGLE_VALUE_TYPE(switches::kEnableDomDistillerButtonAnimation)},
#endif
    {"num-raster-threads",
     IDS_FLAGS_NUM_RASTER_THREADS_NAME,
     IDS_FLAGS_NUM_RASTER_THREADS_DESCRIPTION,
     kOsAll,
     MULTI_VALUE_TYPE(kNumRasterThreadsChoices)},
    {"enable-single-click-autofill",
     IDS_FLAGS_ENABLE_SINGLE_CLICK_AUTOFILL_NAME,
     IDS_FLAGS_ENABLE_SINGLE_CLICK_AUTOFILL_DESCRIPTION,
     kOsCrOS | kOsMac | kOsWin | kOsLinux | kOsAndroid,
     ENABLE_DISABLE_VALUE_TYPE(
         autofill::switches::kEnableSingleClickAutofill,
         autofill::switches::kDisableSingleClickAutofill)},
    {"enable-site-engagement-service",
     IDS_FLAGS_ENABLE_SITE_ENGAGEMENT_SERVICE_NAME,
     IDS_FLAGS_ENABLE_SITE_ENGAGEMENT_SERVICE_DESCRIPTION,
     kOsAll,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableSiteEngagementService,
                               switches::kDisableSiteEngagementService)},
    {"enable-session-crashed-bubble",
     IDS_FLAGS_ENABLE_SESSION_CRASHED_BUBBLE_NAME,
     IDS_FLAGS_ENABLE_SESSION_CRASHED_BUBBLE_DESCRIPTION,
     kOsWin | kOsLinux,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableSessionCrashedBubble,
                               switches::kDisableSessionCrashedBubble)},
    {"enable-pdf-material-ui",
     IDS_FLAGS_PDF_MATERIAL_UI_NAME,
     IDS_FLAGS_PDF_MATERIAL_UI_DESCRIPTION,
     kOsDesktop,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnablePdfMaterialUI,
                               switches::kDisablePdfMaterialUI)},
    {"disable-cast-streaming-hw-encoding",
     IDS_FLAGS_DISABLE_CAST_STREAMING_HW_ENCODING_NAME,
     IDS_FLAGS_DISABLE_CAST_STREAMING_HW_ENCODING_DESCRIPTION,
     kOsAll,
     SINGLE_VALUE_TYPE(switches::kDisableCastStreamingHWEncoding)},
#if defined(OS_ANDROID)
    {"prefetch-search-results",
     IDS_FLAGS_PREFETCH_SEARCH_RESULTS_NAME,
     IDS_FLAGS_PREFETCH_SEARCH_RESULTS_DESCRIPTION,
     kOsAndroid,
     SINGLE_VALUE_TYPE(switches::kPrefetchSearchResults)},
#endif
#if defined(ENABLE_APP_LIST)
    {"enable-experimental-app-list",
     IDS_FLAGS_ENABLE_EXPERIMENTAL_APP_LIST_NAME,
     IDS_FLAGS_ENABLE_EXPERIMENTAL_APP_LIST_DESCRIPTION,
     kOsWin | kOsLinux | kOsCrOS,
     ENABLE_DISABLE_VALUE_TYPE(
         app_list::switches::kEnableExperimentalAppList,
         app_list::switches::kDisableExperimentalAppList)},
    {"enable-centered-app-list",
     IDS_FLAGS_ENABLE_CENTERED_APP_LIST_NAME,
     IDS_FLAGS_ENABLE_CENTERED_APP_LIST_DESCRIPTION,
     kOsWin | kOsLinux | kOsCrOS,
     SINGLE_VALUE_TYPE(app_list::switches::kEnableCenteredAppList)},
    {"enable-new-app-list-mixer",
     IDS_FLAGS_ENABLE_NEW_APP_LIST_MIXER_NAME,
     IDS_FLAGS_ENABLE_NEW_APP_LIST_MIXER_DESCRIPTION,
     kOsWin | kOsLinux | kOsCrOS | kOsMac,
     ENABLE_DISABLE_VALUE_TYPE(app_list::switches::kEnableNewAppListMixer,
                               app_list::switches::kDisableNewAppListMixer)},
#endif
    {"disable-threaded-scrolling",
     IDS_FLAGS_DISABLE_THREADED_SCROLLING_NAME,
     IDS_FLAGS_DISABLE_THREADED_SCROLLING_DESCRIPTION,
     kOsWin | kOsLinux | kOsCrOS | kOsAndroid | kOsMac,
     SINGLE_VALUE_TYPE(switches::kDisableThreadedScrolling)},
    {"inert-visual-viewport",
     IDS_FLAGS_INERT_VISUAL_VIEWPORT_NAME,
     IDS_FLAGS_INERT_VISUAL_VIEWPORT_DESCRIPTION,
     kOsAll,
     SINGLE_VALUE_TYPE(switches::kInertVisualViewport)},
    {"enable-settings-window",
     IDS_FLAGS_ENABLE_SETTINGS_WINDOW_NAME,
     IDS_FLAGS_ENABLE_SETTINGS_WINDOW_DESCRIPTION,
     kOsDesktop,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableSettingsWindow,
                               switches::kDisableSettingsWindow)},
#if defined(OS_MACOSX)
    {"enable-save-password-bubble",
     IDS_FLAGS_ENABLE_SAVE_PASSWORD_BUBBLE_NAME,
     IDS_FLAGS_ENABLE_SAVE_PASSWORD_BUBBLE_DESCRIPTION,
     kOsMac,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableSavePasswordBubble,
                               switches::kDisableSavePasswordBubble)},
#endif
    {"enable-apps-file-associations",
     IDS_FLAGS_ENABLE_APPS_FILE_ASSOCIATIONS_NAME,
     IDS_FLAGS_ENABLE_APPS_FILE_ASSOCIATIONS_DESCRIPTION,
     kOsMac,
     SINGLE_VALUE_TYPE(switches::kEnableAppsFileAssociations)},
#if defined(OS_ANDROID)
    {"enable-embeddedsearch-api",
     IDS_FLAGS_ENABLE_EMBEDDEDSEARCH_API_NAME,
     IDS_FLAGS_ENABLE_EMBEDDEDSEARCH_API_DESCRIPTION,
     kOsAndroid,
     SINGLE_VALUE_TYPE(switches::kEnableEmbeddedSearchAPI)},
#endif
    {"distance-field-text",
     IDS_FLAGS_DISTANCE_FIELD_TEXT_NAME,
     IDS_FLAGS_DISTANCE_FIELD_TEXT_DESCRIPTION,
     kOsAll,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableDistanceFieldText,
                               switches::kDisableDistanceFieldText)},
    {"extension-content-verification",
     IDS_FLAGS_EXTENSION_CONTENT_VERIFICATION_NAME,
     IDS_FLAGS_EXTENSION_CONTENT_VERIFICATION_DESCRIPTION,
     kOsDesktop,
     MULTI_VALUE_TYPE(kExtensionContentVerificationChoices)},
#if defined(ENABLE_EXTENSIONS)
    {"extension-active-script-permission",
     IDS_FLAGS_USER_CONSENT_FOR_EXTENSION_SCRIPTS_NAME,
     IDS_FLAGS_USER_CONSENT_FOR_EXTENSION_SCRIPTS_DESCRIPTION,
     kOsAll,
     SINGLE_VALUE_TYPE(extensions::switches::kEnableScriptsRequireAction)},
#endif
#if defined(OS_ANDROID)
    {"enable-data-reduction-proxy-dev",
     IDS_FLAGS_ENABLE_DATA_REDUCTION_PROXY_DEV_NAME,
     IDS_FLAGS_ENABLE_DATA_REDUCTION_PROXY_DEV_DESCRIPTION,
     kOsAndroid,
     ENABLE_DISABLE_VALUE_TYPE(
         data_reduction_proxy::switches::kEnableDataReductionProxyDev,
         data_reduction_proxy::switches::kDisableDataReductionProxyDev)},
    {"enable-data-reduction-proxy-carrier-test",
     IDS_FLAGS_ENABLE_DATA_REDUCTION_PROXY_CARRIER_TEST_NAME,
     IDS_FLAGS_ENABLE_DATA_REDUCTION_PROXY_CARRIER_TEST_DESCRIPTION,
     kOsAndroid,
     SINGLE_VALUE_TYPE(
         data_reduction_proxy::switches::kEnableDataReductionProxyCarrierTest)},
#endif
    {"enable-hotword-hardware",
     IDS_FLAGS_ENABLE_EXPERIMENTAL_HOTWORD_HARDWARE_NAME,
     IDS_FLAGS_ENABLE_EXPERIMENTAL_HOTWORD_HARDWARE_DESCRIPTION,
     kOsCrOS,
     SINGLE_VALUE_TYPE(switches::kEnableExperimentalHotwordHardware)},
#if defined(ENABLE_EXTENSIONS)
    {"enable-embedded-extension-options",
     IDS_FLAGS_ENABLE_EMBEDDED_EXTENSION_OPTIONS_NAME,
     IDS_FLAGS_ENABLE_EMBEDDED_EXTENSION_OPTIONS_DESCRIPTION,
     kOsDesktop,
     SINGLE_VALUE_TYPE(extensions::switches::kEnableEmbeddedExtensionOptions)},
#endif
#if defined(USE_ASH)
    {"enable-web-app-frame",
     IDS_FLAGS_ENABLE_WEB_APP_FRAME_NAME,
     IDS_FLAGS_ENABLE_WEB_APP_FRAME_DESCRIPTION,
     kOsWin | kOsCrOS,
     SINGLE_VALUE_TYPE(switches::kEnableWebAppFrame)},
#endif
    {"enable-drop-sync-credential",
     IDS_FLAGS_ENABLE_DROP_SYNC_CREDENTIAL_NAME,
     IDS_FLAGS_ENABLE_DROP_SYNC_CREDENTIAL_DESCRIPTION,
     kOsAll,
     ENABLE_DISABLE_VALUE_TYPE(
         password_manager::switches::kEnableDropSyncCredential,
         password_manager::switches::kDisableDropSyncCredential)},
#if defined(ENABLE_EXTENSIONS)
    {"enable-extension-action-redesign",
     IDS_FLAGS_ENABLE_EXTENSION_ACTION_REDESIGN_NAME,
     IDS_FLAGS_ENABLE_EXTENSION_ACTION_REDESIGN_DESCRIPTION,
     kOsDesktop,
     SINGLE_VALUE_TYPE(extensions::switches::kEnableExtensionActionRedesign)},
#endif
    {"autofill-sync-credential",
     IDS_FLAGS_AUTOFILL_SYNC_CREDENTIAL_NAME,
     IDS_FLAGS_AUTOFILL_SYNC_CREDENTIAL_DESCRIPTION,
     kOsAll,
     MULTI_VALUE_TYPE(kAutofillSyncCredentialChoices)},
#if !defined(OS_ANDROID)
    {"enable-message-center-always-scroll-up-upon-notification-removal",
     IDS_FLAGS_ENABLE_MESSAGE_CENTER_ALWAYS_SCROLL_UP_UPON_REMOVAL_NAME,
     IDS_FLAGS_ENABLE_MESSAGE_CENTER_ALWAYS_SCROLL_UP_UPON_REMOVAL_DESCRIPTION,
     kOsDesktop,
     SINGLE_VALUE_TYPE(
         switches::kEnableMessageCenterAlwaysScrollUpUponNotificationRemoval)},
#endif
    {"enable-md-policy-page",
     IDS_FLAGS_ENABLE_MATERIAL_DESIGN_POLICY_PAGE_NAME,
     IDS_FLAGS_ENABLE_MATERIAL_DESIGN_POLICY_PAGE_DESCRIPTION,
     kOsDesktop,
     SINGLE_VALUE_TYPE(switches::kEnableMaterialDesignPolicyPage)},
#if defined(OS_CHROMEOS)
    {"memory-pressure-thresholds",
     IDS_FLAGS_MEMORY_PRESSURE_THRESHOLD_NAME,
     IDS_FLAGS_MEMORY_PRESSURE_THRESHOLD_DESCRIPTION,
     kOsCrOS,
     MULTI_VALUE_TYPE(kMemoryPressureThresholdChoices)},
    {"wake-on-packets",
     IDS_FLAGS_WAKE_ON_PACKETS_NAME,
     IDS_FLAGS_WAKE_ON_PACKETS_DESCRIPTION,
     kOsCrOSOwnerOnly,
     SINGLE_VALUE_TYPE(chromeos::switches::kWakeOnPackets)},
#endif  // OS_CHROMEOS
    {"enable-tab-audio-muting",
     IDS_FLAGS_ENABLE_TAB_AUDIO_MUTING_NAME,
     IDS_FLAGS_ENABLE_TAB_AUDIO_MUTING_DESCRIPTION,
     kOsDesktop,
     SINGLE_VALUE_TYPE(switches::kEnableTabAudioMuting)},
    {"enable-credential-manager-api",
     IDS_FLAGS_CREDENTIAL_MANAGER_API_NAME,
     IDS_FLAGS_CREDENTIAL_MANAGER_API_DESCRIPTION,
     kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableCredentialManagerAPI)},
    {"reduced-referrer-granularity",
     IDS_FLAGS_REDUCED_REFERRER_GRANULARITY_NAME,
     IDS_FLAGS_REDUCED_REFERRER_GRANULARITY_DESCRIPTION,
     kOsAll,
     SINGLE_VALUE_TYPE(switches::kReducedReferrerGranularity)},
#if defined(ENABLE_PLUGINS)
    {"enable-plugin-power-saver",
     IDS_FLAGS_ENABLE_PLUGIN_POWER_SAVER_NAME,
     IDS_FLAGS_ENABLE_PLUGIN_POWER_SAVER_DESCRIPTION,
     kOsDesktop,
     ENABLE_DISABLE_VALUE_TYPE(plugins::switches::kEnablePluginPowerSaver,
                               plugins::switches::kDisablePluginPowerSaver)},
#endif
#if defined(OS_CHROMEOS)
    {"disable-new-zip-unpacker",
     IDS_FLAGS_DISABLE_NEW_ZIP_UNPACKER_NAME,
     IDS_FLAGS_DISABLE_NEW_ZIP_UNPACKER_DESCRIPTION,
     kOsCrOS,
     SINGLE_VALUE_TYPE(chromeos::switches::kDisableNewZIPUnpacker)},
#endif  // defined(OS_CHROMEOS)
    {"enable-credit-card-scan",
     IDS_FLAGS_ENABLE_CREDIT_CARD_SCAN_NAME,
     IDS_FLAGS_ENABLE_CREDIT_CARD_SCAN_DESCRIPTION,
     kOsAndroid,
     ENABLE_DISABLE_VALUE_TYPE(autofill::switches::kEnableCreditCardScan,
                               autofill::switches::kDisableCreditCardScan)},
#if defined(OS_CHROMEOS)
    {"disable-captive-portal-bypass-proxy",
     IDS_FLAGS_DISABLE_CAPTIVE_PORTAL_BYPASS_PROXY_NAME,
     IDS_FLAGS_DISABLE_CAPTIVE_PORTAL_BYPASS_PROXY_DESCRIPTION,
     kOsCrOS,
     SINGLE_VALUE_TYPE(chromeos::switches::kDisableCaptivePortalBypassProxy)},
#endif  // defined(OS_CHROMEOS)
#if defined(OS_ANDROID)
    {"enable-seccomp-filter-sandbox",
     IDS_FLAGS_ENABLE_SECCOMP_FILTER_SANDBOX_ANDROID_NAME,
     IDS_FLAGS_ENABLE_SECCOMP_FILTER_SANDBOX_ANDROID_DESCRIPTION,
     kOsAndroid,
     SINGLE_VALUE_TYPE(switches::kEnableSeccompFilterSandbox)},
#endif
    {"enable-touch-hover",
     IDS_FLAGS_ENABLE_TOUCH_HOVER_NAME,
     IDS_FLAGS_ENABLE_TOUCH_HOVER_DESCRIPTION,
     kOsAndroid,
     SINGLE_VALUE_TYPE("enable-touch-hover")},
    {"enable-fill-on-account-select",
     IDS_FILL_ON_ACCOUNT_SELECT_NAME,
     IDS_FILL_ON_ACCOUNT_SELECT_DESCRIPTION,
     kOsAll,
     MULTI_VALUE_TYPE(kFillOnAccountSelectChoices)},
#if defined(OS_CHROMEOS)
    {"enable-wifi-credential-sync",  // FLAGS:RECORD_UMA
     IDS_FLAGS_ENABLE_WIFI_CREDENTIAL_SYNC_NAME,
     IDS_FLAGS_ENABLE_WIFI_CREDENTIAL_SYNC_DESCRIPTION,
     kOsCrOS,
     SINGLE_VALUE_TYPE(switches::kEnableWifiCredentialSync)},
    {"enable-potentially-annoying-security-features",
     IDS_FLAGS_EXPERIMENTAL_SECURITY_FEATURES_NAME,
     IDS_FLAGS_EXPERIMENTAL_SECURITY_FEATURES_DESCRIPTION,
     kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnablePotentiallyAnnoyingSecurityFeatures)},
#endif
    {"disable-delay-agnostic-aec",
     IDS_FLAGS_DISABLE_DELAY_AGNOSTIC_AEC_NAME,
     IDS_FLAGS_DISABLE_DELAY_AGNOSTIC_AEC_DESCRIPTION,
     kOsWin | kOsLinux | kOsCrOS | kOsMac,
     SINGLE_VALUE_TYPE(switches::kDisableDelayAgnosticAec)},
    {"mark-non-secure-as",  // FLAGS:RECORD_UMA
     IDS_MARK_NON_SECURE_AS_NAME,
     IDS_MARK_NON_SECURE_AS_DESCRIPTION,
     kOsAll,
     MULTI_VALUE_TYPE(kMarkNonSecureAsChoices)},
    {"enable-site-per-process",
     IDS_FLAGS_ENABLE_SITE_PER_PROCESS_NAME,
     IDS_FLAGS_ENABLE_SITE_PER_PROCESS_DESCRIPTION,
     kOsAll,
     SINGLE_VALUE_TYPE(switches::kSitePerProcess)},
#if defined(OS_MACOSX)
    {"enable-harfbuzz-rendertext",
     IDS_FLAGS_ENABLE_HARFBUZZ_RENDERTEXT_NAME,
     IDS_FLAGS_ENABLE_HARFBUZZ_RENDERTEXT_DESCRIPTION,
     kOsMac,
     SINGLE_VALUE_TYPE(switches::kEnableHarfBuzzRenderText)},
#endif  // defined(OS_MACOSX)
#if defined(OS_CHROMEOS)
    {"disable-timezone-tracking",
     IDS_FLAGS_DISABLE_RESOLVE_TIMEZONE_BY_GEOLOCATION_NAME,
     IDS_FLAGS_DISABLE_RESOLVE_TIMEZONE_BY_GEOLOCATION_DESCRIPTION,
     kOsCrOS,
     SINGLE_VALUE_TYPE(chromeos::switches::kDisableTimeZoneTrackingOption)},
#endif  // defined(OS_CHROMEOS)
    {"data-reduction-proxy-lo-fi",
     IDS_FLAGS_DATA_REDUCTION_PROXY_LO_FI_NAME,
     IDS_FLAGS_DATA_REDUCTION_PROXY_LO_FI_DESCRIPTION,
     kOsAll,
     MULTI_VALUE_TYPE(kDataReductionProxyLoFiChoices)},
    {"clear-data-reduction-proxy-data-savings",
     IDS_FLAGS_DATA_REDUCTION_PROXY_RESET_SAVINGS_NAME,
     IDS_FLAGS_DATA_REDUCTION_PROXY_RESET_SAVINGS_DESCRIPTION,
     kOsAll,
     SINGLE_VALUE_TYPE(
         data_reduction_proxy::switches::kClearDataReductionProxyDataSavings)},
    {"enable-data-reduction-proxy-config-client",
     IDS_FLAGS_DATA_REDUCTION_PROXY_CONFIG_CLIENT_NAME,
     IDS_FLAGS_DATA_REDUCTION_PROXY_CONFIG_CLIENT_DESCRIPTION,
     kOsAll,
     SINGLE_VALUE_TYPE(data_reduction_proxy::switches::
                           kEnableDataReductionProxyConfigClient)},
#if defined(ENABLE_DATA_REDUCTION_PROXY_DEBUGGING)
    {"enable-data-reduction-proxy-bypass-warnings",
     IDS_FLAGS_ENABLE_DATA_REDUCTION_PROXY_BYPASS_WARNING_NAME,
     IDS_FLAGS_ENABLE_DATA_REDUCTION_PROXY_BYPASS_WARNING_DESCRIPTION,
     kOsAndroid,
     SINGLE_VALUE_TYPE(data_reduction_proxy::switches::
                           kEnableDataReductionProxyBypassWarning)},
#endif
    {"allow-insecure-localhost",
     IDS_ALLOW_INSECURE_LOCALHOST,
     IDS_ALLOW_INSECURE_LOCALHOST_DESCRIPTION,
     kOsAll,
     SINGLE_VALUE_TYPE(switches::kAllowInsecureLocalhost)},
    {"enable-add-to-shelf",
     IDS_FLAGS_ENABLE_ADD_TO_SHELF_NAME,
     IDS_FLAGS_ENABLE_ADD_TO_SHELF_DESCRIPTION,
     kOsDesktop,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableAddToShelf,
                               switches::kDisableAddToShelf)},
    {"bypass-app-banner-engagement-checks",
     IDS_FLAGS_BYPASS_APP_BANNER_ENGAGEMENT_CHECKS_NAME,
     IDS_FLAGS_BYPASS_APP_BANNER_ENGAGEMENT_CHECKS_DESCRIPTION,
     kOsAll,
     SINGLE_VALUE_TYPE(switches::kBypassAppBannerEngagementChecks)},
    {"use-sync-sandbox",
     IDS_FLAGS_SYNC_SANDBOX_NAME,
     IDS_FLAGS_SYNC_SANDBOX_DESCRIPTION,
     kOsAll,
     SINGLE_VALUE_TYPE_AND_VALUE(
         switches::kSyncServiceURL,
         "https://chrome-sync.sandbox.google.com/chrome-sync/alpha")},
    {"enable-child-account-detection",
     IDS_FLAGS_CHILD_ACCOUNT_DETECTION_NAME,
     IDS_FLAGS_CHILD_ACCOUNT_DETECTION_DESCRIPTION,
     kOsAndroid | kOsMac | kOsWin | kOsLinux | kOsCrOS,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableChildAccountDetection,
                               switches::kDisableChildAccountDetection)},
#if defined(OS_CHROMEOS) && defined(USE_OZONE)
    {"ozone-test-single-overlay-support",
     IDS_FLAGS_OZONE_TEST_SINGLE_HARDWARE_OVERLAY,
     IDS_FLAGS_OZONE_TEST_SINGLE_HARDWARE_OVERLAY_DESCRIPTION,
     kOsCrOS,
     SINGLE_VALUE_TYPE(switches::kOzoneTestSingleOverlaySupport)},
#endif  // defined(OS_CHROMEOS) && defined(USE_OZONE)
    {"v8-pac-mojo-out-of-process",
     IDS_FLAGS_V8_PAC_MOJO_OUT_OF_PROCESS_NAME,
     IDS_FLAGS_V8_PAC_MOJO_OUT_OF_PROCESS_DESCRIPTION,
     kOsDesktop,
     ENABLE_DISABLE_VALUE_TYPE(switches::kV8PacMojoOutOfProcess,
                               switches::kDisableOutOfProcessPac)},
#if defined(ENABLE_MEDIA_ROUTER) && !defined(OS_ANDROID)
    {"enable-media-router",
     IDS_FLAGS_ENABLE_MEDIA_ROUTER_NAME,
     IDS_FLAGS_ENABLE_MEDIA_ROUTER_DESCRIPTION,
     kOsDesktop,
     SINGLE_VALUE_TYPE(switches::kEnableMediaRouter)},
#endif  // defined(ENABLE_MEDIA_ROUTER) && !defined(OS_ANDROID)
// Since Drive Search is not available when app list is disabled, flag guard
// enable-drive-search-in-chrome-launcher flag.
#if defined(ENABLE_APP_LIST)
    {"enable-drive-search-in-app-launcher",
     IDS_FLAGS_ENABLE_DRIVE_SEARCH_IN_CHROME_LAUNCHER,
     IDS_FLAGS_ENABLE_DRIVE_SEARCH_IN_CHROME_LAUNCHER_DESCRIPTION,
     kOsCrOS,
     ENABLE_DISABLE_VALUE_TYPE(
         app_list::switches::kEnableDriveSearchInChromeLauncher,
         app_list::switches::kDisableDriveSearchInChromeLauncher)},
#endif  // defined(ENABLE_APP_LIST)
#if defined(OS_CHROMEOS)
    {"disable-mtp-write-support",
     IDS_FLAG_DISABLE_MTP_WRITE_SUPPORT_NAME,
     IDS_FLAG_DISABLE_MTP_WRITE_SUPPORT_DESCRIPTION,
     kOsCrOS,
     SINGLE_VALUE_TYPE(chromeos::switches::kDisableMtpWriteSupport)},
#endif  // defined(OS_CHROMEOS)
#if defined(OS_CHROMEOS)
    {"enable-datasaver-prompt",
     IDS_FLAGS_DATASAVER_PROMPT_NAME,
     IDS_FLAGS_DATASAVER_PROMPT_DESCRIPTION,
     kOsCrOS,
     MULTI_VALUE_TYPE(kDataSaverPromptChoices)},
#endif  // defined(OS_CHROMEOS)
    {"supervised-user-safesites",
     IDS_FLAGS_SUPERVISED_USER_SAFESITES_NAME,
     IDS_FLAGS_SUPERVISED_USER_SAFESITES_DESCRIPTION,
     kOsAndroid | kOsMac | kOsWin | kOsLinux | kOsCrOS,
     MULTI_VALUE_TYPE(kSupervisedUserSafeSitesChoices)},
#if defined(OS_ANDROID)
    {"enable-autofill-keyboard-accessory-view",
     IDS_FLAGS_AUTOFILL_ACCESSORY_VIEW_NAME,
     IDS_FLAGS_AUTOFILL_ACCESSORY_VIEW_DESCRIPTION,
     kOsAndroid,
     ENABLE_DISABLE_VALUE_TYPE(
         autofill::switches::kEnableAccessorySuggestionView,
         autofill::switches::kDisableAccessorySuggestionView)},
#endif  // defined(OS_ANDROID)
    // Temporary flag to ease the transition to standard-compliant scrollTop
    // behavior.  Will be removed shortly after http://crbug.com/157855 ships.
    {"scroll-top-left-interop",
     IDS_FLAGS_SCROLL_TOP_LEFT_INTEROP_NAME,
     IDS_FLAGS_SCROLL_TOP_LEFT_INTEROP_DESCRIPTION,
     kOsAll,
     ENABLE_DISABLE_VALUE_TYPE_AND_VALUE(switches::kEnableBlinkFeatures,
                                         "ScrollTopLeftInterop",
                                         switches::kDisableBlinkFeatures,
                                         "ScrollTopLeftInterop")},
#if defined(OS_WIN)
    {"try-supported-channel-layouts",
     IDS_FLAGS_TRY_SUPPORTED_CHANNEL_LAYOUTS_NAME,
     IDS_FLAGS_TRY_SUPPORTED_CHANNEL_LAYOUTS_DESCRIPTION,
     kOsWin,
     SINGLE_VALUE_TYPE(switches::kTrySupportedChannelLayouts)},
#endif
    {"emphasize-titles-in-omnibox-dropdown",
     IDS_FLAGS_EMPHASIZE_TITLES_IN_OMNIBOX_DROPDOWN_NAME,
     IDS_FLAGS_EMPHASIZE_TITLES_IN_OMNIBOX_DROPDOWN_DESCRIPTION,
     kOsAll,
     SINGLE_VALUE_TYPE(switches::kEmphasizeTitlesInOmniboxDropdown)},
#if defined(ENABLE_WEBRTC)
    {"enable-webrtc-dtls12",
     IDS_FLAGS_ENABLE_WEBRTC_DTLS12_NAME,
     IDS_FLAGS_ENABLE_WEBRTC_DTLS12_DESCRIPTION,
     kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableWebRtcDtls12)},
#endif
#if defined(OS_MACOSX)
    {"app-info-dialog",
     IDS_FLAGS_APP_INFO_DIALOG_NAME,
     IDS_FLAGS_APP_INFO_DIALOG_DESCRIPTION,
     kOsMac,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableAppInfoDialogMac,
                               switches::kDisableAppInfoDialogMac)},
    {"mac-views-native-app-windows",
     IDS_FLAGS_MAC_VIEWS_NATIVE_APP_WINDOWS_NAME,
     IDS_FLAGS_MAC_VIEWS_NATIVE_APP_WINDOWS_DESCRIPTION,
     kOsMac,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableMacViewsNativeAppWindows,
                               switches::kDisableMacViewsNativeAppWindows)},
    {"app-window-cycling",
     IDS_FLAGS_APP_WINDOW_CYCLING_NAME,
     IDS_FLAGS_APP_WINDOW_CYCLING_DESCRIPTION,
     kOsMac,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableAppWindowCycling,
                               switches::kDisableAppWindowCycling)},
    {"mac-views-dialogs",
     IDS_FLAGS_MAC_VIEWS_DIALOGS_NAME,
     IDS_FLAGS_MAC_VIEWS_DIALOGS_DESCRIPTION,
     kOsMac,
     SINGLE_VALUE_TYPE(switches::kEnableMacViewsDialogs)},
#endif
#if defined(ENABLE_WEBVR)
    {"enable-webvr",
     IDS_FLAGS_ENABLE_WEBVR_NAME,
     IDS_FLAGS_ENABLE_WEBVR_DESCRIPTION,
     kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableWebVR)},
#endif
#if defined(OS_CHROMEOS)
    {"disable-accelerated-mjpeg-decode",
     IDS_FLAGS_DISABLE_ACCELERATED_MJPEG_DECODE_NAME,
     IDS_FLAGS_DISABLE_ACCELERATED_MJPEG_DECODE_DESCRIPTION,
     kOsCrOS,
     SINGLE_VALUE_TYPE(switches::kDisableAcceleratedMjpegDecode)},
#endif  // OS_CHROMEOS
    {"v8-cache-options",
     IDS_FLAGS_V8_CACHE_OPTIONS_NAME,
     IDS_FLAGS_V8_CACHE_OPTIONS_DESCRIPTION,
     kOsAll,
     MULTI_VALUE_TYPE(kV8CacheOptionsChoices)},
#if !defined(OS_ANDROID) && !defined(OS_IOS)
    {"enable-md-downloads",
     IDS_FLAGS_ENABLE_MATERIAL_DESIGN_DOWNLOADS_NAME,
     IDS_FLAGS_ENABLE_MATERIAL_DESIGN_DOWNLOADS_DESCRIPTION,
     kOsDesktop,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableMaterialDesignDownloads,
                               switches::kDisableMaterialDesignDownloads)},
#endif
#if defined(OS_WIN) || defined(OS_MACOSX)
    {"enable-tab-discarding",
     IDS_FLAGS_ENABLE_TAB_DISCARDING_NAME,
     IDS_FLAGS_ENABLE_TAB_DISCARDING_DESCRIPTION,
     kOsWin | kOsMac,
     SINGLE_VALUE_TYPE(switches::kEnableTabDiscarding)},
#endif
    {"enable-clear-browsing-data-counters",
     IDS_FLAGS_ENABLE_CLEAR_BROWSING_DATA_COUNTERS_NAME,
     IDS_FLAGS_ENABLE_CLEAR_BROWSING_DATA_COUNTERS_DESCRIPTION,
     kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableClearBrowsingDataCounters)
    },
#if defined(ENABLE_TASK_MANAGER)
    {"disable-new-task-manager",
     IDS_FLAGS_DISABLE_NEW_TASK_MANAGER_NAME,
     IDS_FLAGS_DISABLE_NEW_TASK_MANAGER_DESCRIPTION,
     kOsDesktop,
     SINGLE_VALUE_TYPE(switches::kDisableNewTaskManager)
    },
#endif  // defined(ENABLE_TASK_MANAGER)
    {"simplified-fullscreen-ui",
     IDS_FLAGS_SIMPLIFIED_FULLSCREEN_UI_NAME,
     IDS_FLAGS_SIMPLIFIED_FULLSCREEN_UI_DESCRIPTION,
     kOsDesktop,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableSimplifiedFullscreenUI,
                               switches::kDisableSimplifiedFullscreenUI)
    },
#if defined(OS_ANDROID)
    {"progress-bar-animation",
     IDS_FLAGS_PROGRESS_BAR_ANIMATION_NAME,
     IDS_FLAGS_PROGRESS_BAR_ANIMATION_DESCRIPTION,
     kOsAndroid,
     MULTI_VALUE_TYPE(kProgressBarAnimationChoices)},
#endif  // defined(OS_ANDROID)
#if defined(OS_ANDROID)
    {"enable-theme-color-in-tabbed-mode",
     IDS_FLAGS_THEME_COLOR_IN_TABBED_MODE_NAME,
     IDS_FLAGS_THEME_COLOR_IN_TABBED_MODE_DESCRIPTION,
     kOsAndroid,
     SINGLE_VALUE_TYPE(switches::kEnableThemeColorInTabbedMode)
    },
#endif  // defined(OS_ANDROID)
#if defined(OS_ANDROID)
    {"offline-pages",
     IDS_FLAGS_OFFLINE_PAGES_NAME,
     IDS_FLAGS_OFFLINE_PAGES_DESCRIPTION,
     kOsAndroid,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableOfflinePages,
                               switches::kDisableOfflinePages)},
#endif  // defined(OS_ANDROID)
    {"low-priority-iframes",
     IDS_FLAGS_LOW_PRIORITY_IFRAMES_UI_NAME,
     IDS_FLAGS_LOW_PRIORITY_IFRAMES_UI_DESCRIPTION,
     kOsAll,
     // NOTE: if we want to add additional experiment entries for other
     // features controlled by kBlinkSettings, we'll need to add logic to
     // merge the flag values.
     SINGLE_VALUE_TYPE_AND_VALUE(
         switches::kBlinkSettings, "lowPriorityIframes=true")},
#if defined(OS_ANDROID)
    {"enable-ntp-popular-sites",
     IDS_FLAGS_NTP_POPULAR_SITES_NAME,
     IDS_FLAGS_NTP_POPULAR_SITES_DESCRIPTION,
     kOsAndroid,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableNTPPopularSites,
                               switches::kDisableNTPPopularSites)},
    {"use-android-midi-api",
     IDS_FLAGS_USE_ANDROID_MIDI_API_NAME,
     IDS_FLAGS_USE_ANDROID_MIDI_API_DESCRIPTION,
     kOsAndroid,
     SINGLE_VALUE_TYPE(switches::kUseAndroidMidiApi)},
#endif  // defined(OS_ANDROID)
#if defined(OS_WIN)
     {"trace-export-events-to-etw",
      IDS_FLAGS_TRACE_EXPORT_EVENTS_TO_ETW_NAME,
      IDS_FLAGS_TRACE_EXPORT_EVENTS_TO_ETW_DESRIPTION,
      kOsWin,
      SINGLE_VALUE_TYPE(switches::kTraceExportEventsToETW)},
    {"merge-key-char-events",
     IDS_FLAGS_MERGE_KEY_CHAR_EVENTS_NAME,
     IDS_FLAGS_MERGE_KEY_CHAR_EVENTS_DESCRIPTION,
     kOsWin,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableMergeKeyCharEvents,
                               switches::kDisableMergeKeyCharEvents)},
#endif  // defined(OS_WIN)
#if defined(ENABLE_BACKGROUND)
    {"enable-push-api-background-mode",
     IDS_FLAGS_ENABLE_PUSH_API_BACKGROUND_MODE_NAME,
     IDS_FLAGS_ENABLE_PUSH_API_BACKGROUND_MODE_DESCRIPTION,
     kOsMac | kOsWin | kOsLinux,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnablePushApiBackgroundMode,
                               switches::kDisablePushApiBackgroundMode)},
#endif  // defined(ENABLE_BACKGROUND)
#if !defined(OS_ANDROID) && !defined(OS_IOS)
     {"enable-webusb-notifications",
      IDS_FLAGS_ENABLE_WEBUSB_NOTIFICATIONS_NAME,
      IDS_FLAGS_ENABLE_WEBUSB_NOTIFICATIONS_DESCRIPTION,
      kOsDesktop,
      SINGLE_VALUE_TYPE(switches::kEnableWebUsbNotifications)},
     // TODO(reillyg): Remove this flag when the permission granting UI is
     // available. crbug.com/529950
     {"enable-webusb-on-any-origin",
      IDS_FLAGS_ENABLE_WEBUSB_ON_ANY_ORIGIN_NAME,
      IDS_FLAGS_ENABLE_WEBUSB_ON_ANY_ORIGIN_DESCRIPTION,
      kOsDesktop,
      SINGLE_VALUE_TYPE(switches::kEnableWebUsbOnAnyOrigin)},
#endif
#if defined(OS_CHROMEOS)
    {"cros-regions-mode",
     IDS_FLAGS_CROS_REGIONS_MODE_NAME,
     IDS_FLAGS_CROS_REGIONS_MODE_DESCRIPTION,
     kOsCrOS,
     MULTI_VALUE_TYPE(kCrosRegionsModeChoices)},
#endif  // OS_CHROMEOS
#if defined(OS_WIN)
    {"enable-ppapi-win32k-lockdown",
     IDS_FLAGS_PPAPI_WIN32K_LOCKDOWN_NAME,
     IDS_FLAGS_PPAPI_WIN32K_LOCKDOWN_DESCRIPTION, kOsWin,
     MULTI_VALUE_TYPE(kPpapiWin32kLockdown)},
#endif  // defined(OS_WIN)
#if defined(ENABLE_NOTIFICATIONS) && defined(OS_ANDROID)
    {"enable-web-notification-custom-layouts",
     IDS_FLAGS_ENABLE_WEB_NOTIFICATION_CUSTOM_LAYOUTS_NAME,
     IDS_FLAGS_ENABLE_WEB_NOTIFICATION_CUSTOM_LAYOUTS_DESCRIPTION,
     kOsAndroid,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableWebNotificationCustomLayouts,
                               switches::kDisableWebNotificationCustomLayouts)},
#endif  // defined(ENABLE_NOTIFICATIONS) && defined(OS_ANDROID)
    // NOTE: Adding new command-line switches requires adding corresponding
    // entries to enum "LoginCustomFlags" in histograms.xml. See note in
    // histograms.xml and don't forget to run AboutFlagsHistogramTest unit test.
};

// Stores and encapsulates the little state that about:flags has.
class FlagsState {
 public:
  FlagsState()
      : feature_entries(kFeatureEntries),
        num_feature_entries(arraysize(kFeatureEntries)),
        needs_restart_(false) {}
  void ConvertFlagsToSwitches(flags_ui::FlagsStorage* flags_storage,
                              base::CommandLine* command_line,
                              SentinelsMode sentinels);
  bool IsRestartNeededToCommitChanges();
  void SetFeatureEntryEnabled(flags_ui::FlagsStorage* flags_storage,
                            const std::string& internal_name,
                            bool enable);
  void RemoveFlagsSwitches(
      std::map<std::string, base::CommandLine::StringType>* switch_list);
  void ResetAllFlags(flags_ui::FlagsStorage* flags_storage);
  void Reset();

  // Gets the list of feature entries. Entries that are available for the
  // current platform are appended to |supported_entries|; all other entries are
  // appended to |unsupported_entries|.
  void GetFlagFeatureEntries(flags_ui::FlagsStorage* flags_storage,
                             FlagAccess access,
                             base::ListValue* supported_entries,
                             base::ListValue* unsupported_entries);

  void SetFeatureEntries(const FeatureEntry* entries, size_t count);
  const FeatureEntry* GetFeatureEntries(size_t* count);

  // Returns the singleton instance of this class
  static FlagsState* GetInstance() {
    return base::Singleton<FlagsState>::get();
  }

 private:
  // Keeps track of affected switches for each FeatureEntry, based on which
  // choice is selected for it.
  struct SwitchEntry {
    // Corresponding base::Feature to toggle.
    std::string feature_name;

    // If |feature_name| is not empty, the state (enable/disabled) to set.
    bool feature_state;

    // The name of the switch to add.
    std::string switch_name;

    // If |switch_name| is not empty, the value of the switch to set.
    std::string switch_value;

    SwitchEntry() : feature_state(false) {}
  };

  // Adds mapping to |name_to_switch_map| to set the given switch name/value.
  void AddSwitchMapping(const std::string& key,
                        const std::string& switch_name,
                        const std::string& switch_value,
                        std::map<std::string, SwitchEntry>* name_to_switch_map);

  // Adds mapping to |name_to_switch_map| to toggle base::Feature |feature_name|
  // to state |feature_state|.
  void AddFeatureMapping(
      const std::string& key,
      const std::string& feature_name,
      bool feature_state,
      std::map<std::string, SwitchEntry>* name_to_switch_map);

  // Updates the switches in |command_line| by applying the modifications
  // specified in |name_to_switch_map| for each entry in |enabled_entries|.
  void AddSwitchesToCommandLine(
      const std::set<std::string>& enabled_entries,
      const std::map<std::string, SwitchEntry>& name_to_switch_map,
      SentinelsMode sentinels,
      base::CommandLine* command_line);

  // Updates |command_line| by merging the value of the --enable-features= or
  // --disable-features= list (per the |switch_name| param) with corresponding
  // entries in |feature_switches| that have value |feature_state|. Keeps track
  // of the changes by updating |appended_switches|.
  void MergeFeatureCommandLineSwitch(
      const std::map<std::string, bool>& feature_switches,
      const char* switch_name,
      bool feature_state,
      base::CommandLine* command_line);

  // Removes all entries from prefs::kEnabledLabsExperiments that are unknown,
  // to prevent this list to become very long as entries are added and removed.
  void SanitizeList(flags_ui::FlagsStorage* flags_storage);

  void GetSanitizedEnabledFlags(flags_ui::FlagsStorage* flags_storage,
                                std::set<std::string>* result);

  // Variant of GetSanitizedEnabledFlags that also removes any flags that aren't
  // enabled on the current platform.
  void GetSanitizedEnabledFlagsForCurrentPlatform(
      flags_ui::FlagsStorage* flags_storage,
      std::set<std::string>* result);

  const FeatureEntry* feature_entries;
  size_t num_feature_entries;

  bool needs_restart_;
  std::map<std::string, std::string> flags_switches_;

  // Map from switch name to a set of string, that keeps track which strings
  // were appended to existing (list value) switches.
  std::map<std::string, std::set<std::string>> appended_switches_;

  DISALLOW_COPY_AND_ASSIGN(FlagsState);
};

// Adds the internal names for the specified entry to |names|.
void AddInternalName(const FeatureEntry& e, std::set<std::string>* names) {
  switch (e.type) {
    case FeatureEntry::SINGLE_VALUE:
    case FeatureEntry::SINGLE_DISABLE_VALUE:
      names->insert(e.internal_name);
      break;
    case FeatureEntry::MULTI_VALUE:
    case FeatureEntry::ENABLE_DISABLE_VALUE:
    case FeatureEntry::FEATURE_VALUE:
      for (int i = 0; i < e.num_choices; ++i)
        names->insert(e.NameForChoice(i));
      break;
  }
}

// Confirms that an entry is valid, used in a DCHECK in
// SanitizeList below.
bool ValidateFeatureEntry(const FeatureEntry& e) {
  switch (e.type) {
    case FeatureEntry::SINGLE_VALUE:
    case FeatureEntry::SINGLE_DISABLE_VALUE:
      DCHECK_EQ(0, e.num_choices);
      DCHECK(!e.choices);
      return true;
    case FeatureEntry::MULTI_VALUE:
      DCHECK_GT(e.num_choices, 0);
      DCHECK(e.choices);
      DCHECK(e.choices[0].command_line_switch);
      DCHECK_EQ('\0', e.choices[0].command_line_switch[0]);
      return true;
    case FeatureEntry::ENABLE_DISABLE_VALUE:
      DCHECK_EQ(3, e.num_choices);
      DCHECK(!e.choices);
      DCHECK(e.command_line_switch);
      DCHECK(e.command_line_value);
      DCHECK(e.disable_command_line_switch);
      DCHECK(e.disable_command_line_value);
      return true;
    case FeatureEntry::FEATURE_VALUE:
      DCHECK_EQ(3, e.num_choices);
      DCHECK(!e.choices);
      DCHECK(e.feature_name);
      return true;
  }
  NOTREACHED();
  return false;
}

void FlagsState::SanitizeList(flags_ui::FlagsStorage* flags_storage) {
  std::set<std::string> known_entries;
  for (size_t i = 0; i < num_feature_entries; ++i) {
    DCHECK(ValidateFeatureEntry(feature_entries[i]));
    AddInternalName(feature_entries[i], &known_entries);
  }

  std::set<std::string> enabled_entries = flags_storage->GetFlags();

  std::set<std::string> new_enabled_entries =
      base::STLSetIntersection<std::set<std::string> >(
          known_entries, enabled_entries);

  if (new_enabled_entries != enabled_entries)
    flags_storage->SetFlags(new_enabled_entries);
}

void FlagsState::GetSanitizedEnabledFlags(flags_ui::FlagsStorage* flags_storage,
                                          std::set<std::string>* result) {
  SanitizeList(flags_storage);
  *result = flags_storage->GetFlags();
}

bool SkipConditionalFeatureEntry(const FeatureEntry& entry) {
  version_info::Channel channel = chrome::GetChannel();

#if defined(OS_ANDROID)
  // enable-data-reduction-proxy-dev is only available for the Dev/Beta channel.
  if (!strcmp("enable-data-reduction-proxy-dev", entry.internal_name) &&
      channel != version_info::Channel::BETA &&
      channel != version_info::Channel::DEV) {
    return true;
  }
  // enable-data-reduction-proxy-alt is only available for the Dev channel.
  if (!strcmp("enable-data-reduction-proxy-alt", entry.internal_name) &&
      channel != version_info::Channel::DEV) {
    return true;
  }
  // enable-data-reduction-proxy-carrier-test is only available for Chromium
  // builds and the Canary/Dev channel.
  if (!strcmp("enable-data-reduction-proxy-carrier-test",
              entry.internal_name) &&
      channel != version_info::Channel::DEV &&
      channel != version_info::Channel::CANARY &&
      channel != version_info::Channel::UNKNOWN) {
    return true;
  }
#endif

  // data-reduction-proxy-lo-fi is only available for Chromium builds and
  // the Canary/Dev/Beta channels.
  if (!strcmp("data-reduction-proxy-lo-fi", entry.internal_name) &&
      channel != version_info::Channel::BETA &&
      channel != version_info::Channel::DEV &&
      channel != version_info::Channel::CANARY &&
      channel != version_info::Channel::UNKNOWN) {
    return true;
  }

  // enable-data-reduction-proxy-config-client is only available for Chromium
  // builds and the Canary/Dev channels.
  if (!strcmp("enable-data-reduction-proxy-config-client",
              entry.internal_name) &&
      channel != version_info::Channel::DEV &&
      channel != version_info::Channel::CANARY &&
      channel != version_info::Channel::UNKNOWN) {
    return true;
  }

#if defined(ENABLE_DATA_REDUCTION_PROXY_DEBUGGING)
  // enable-data-reduction-proxy-bypass-warning is only available for Chromium
  // builds and Canary/Dev channel.
  if (!strcmp("enable-data-reduction-proxy-bypass-warnings",
              entry.internal_name) &&
      channel != version_info::Channel::UNKNOWN &&
      channel != version_info::Channel::CANARY &&
      channel != version_info::Channel::DEV) {
    return true;
  }
#endif

  return false;
}

void FlagsState::GetSanitizedEnabledFlagsForCurrentPlatform(
    flags_ui::FlagsStorage* flags_storage,
    std::set<std::string>* result) {
  GetSanitizedEnabledFlags(flags_storage, result);

  // Filter out any entries that aren't enabled on the current platform.  We
  // don't remove these from prefs else syncing to a platform with a different
  // set of entries would be lossy.
  std::set<std::string> platform_entries;
  int current_platform = GetCurrentPlatform();
  for (size_t i = 0; i < num_feature_entries; ++i) {
    const FeatureEntry& entry = feature_entries[i];
    if (entry.supported_platforms & current_platform)
      AddInternalName(entry, &platform_entries);
#if defined(OS_CHROMEOS)
    if (feature_entries[i].supported_platforms & kOsCrOSOwnerOnly)
      AddInternalName(entry, &platform_entries);
#endif
  }

  std::set<std::string> new_enabled_entries =
      base::STLSetIntersection<std::set<std::string> >(
          platform_entries, *result);

  result->swap(new_enabled_entries);
}

// Returns true if none of this entry's options have been enabled.
bool IsDefaultValue(
    const FeatureEntry& entry,
    const std::set<std::string>& enabled_entries) {
  switch (entry.type) {
    case FeatureEntry::SINGLE_VALUE:
    case FeatureEntry::SINGLE_DISABLE_VALUE:
      return enabled_entries.count(entry.internal_name) == 0;
    case FeatureEntry::MULTI_VALUE:
    case FeatureEntry::ENABLE_DISABLE_VALUE:
    case FeatureEntry::FEATURE_VALUE:
      for (int i = 0; i < entry.num_choices; ++i) {
        if (enabled_entries.count(entry.NameForChoice(i)) > 0)
          return false;
      }
      return true;
  }
  NOTREACHED();
  return true;
}

// Returns the Value representing the choice data in the specified entry.
base::Value* CreateChoiceData(
    const FeatureEntry& entry,
    const std::set<std::string>& enabled_entries) {
  DCHECK(entry.type == FeatureEntry::MULTI_VALUE ||
         entry.type == FeatureEntry::ENABLE_DISABLE_VALUE ||
         entry.type == FeatureEntry::FEATURE_VALUE);
  base::ListValue* result = new base::ListValue;
  for (int i = 0; i < entry.num_choices; ++i) {
    base::DictionaryValue* value = new base::DictionaryValue;
    const std::string name = entry.NameForChoice(i);
    value->SetString("internal_name", name);
    value->SetString("description", entry.DescriptionForChoice(i));
    value->SetBoolean("selected", enabled_entries.count(name) > 0);
    result->Append(value);
  }
  return result;
}

}  // namespace

std::string FeatureEntry::NameForChoice(int index) const {
  DCHECK(type == FeatureEntry::MULTI_VALUE ||
         type == FeatureEntry::ENABLE_DISABLE_VALUE ||
         type == FeatureEntry::FEATURE_VALUE);
  DCHECK_LT(index, num_choices);
  return std::string(internal_name) + testing::kMultiSeparator +
         base::IntToString(index);
}

base::string16 FeatureEntry::DescriptionForChoice(int index) const {
  DCHECK(type == FeatureEntry::MULTI_VALUE ||
         type == FeatureEntry::ENABLE_DISABLE_VALUE ||
         type == FeatureEntry::FEATURE_VALUE);
  DCHECK_LT(index, num_choices);
  int description_id;
  if (type == FeatureEntry::ENABLE_DISABLE_VALUE ||
      type == FeatureEntry::FEATURE_VALUE) {
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

void ConvertFlagsToSwitches(flags_ui::FlagsStorage* flags_storage,
                            base::CommandLine* command_line,
                            SentinelsMode sentinels) {
  FlagsState::GetInstance()->ConvertFlagsToSwitches(flags_storage,
                                                    command_line,
                                                    sentinels);
}

bool AreSwitchesIdenticalToCurrentCommandLine(
    const base::CommandLine& new_cmdline,
    const base::CommandLine& active_cmdline,
    std::set<base::CommandLine::StringType>* out_difference) {
  std::set<base::CommandLine::StringType> new_flags =
      ExtractFlagsFromCommandLine(new_cmdline);
  std::set<base::CommandLine::StringType> active_flags =
      ExtractFlagsFromCommandLine(active_cmdline);

  bool result = false;
  // Needed because std::equal doesn't check if the 2nd set is empty.
  if (new_flags.size() == active_flags.size()) {
    result =
        std::equal(new_flags.begin(), new_flags.end(), active_flags.begin());
  }

  if (out_difference && !result) {
    std::set_symmetric_difference(
        new_flags.begin(),
        new_flags.end(),
        active_flags.begin(),
        active_flags.end(),
        std::inserter(*out_difference, out_difference->begin()));
  }

  return result;
}

void FlagsState::GetFlagFeatureEntries(flags_ui::FlagsStorage* flags_storage,
                                       FlagAccess access,
                                       base::ListValue* supported_entries,
                                       base::ListValue* unsupported_entries) {
  std::set<std::string> enabled_entries;
  GetSanitizedEnabledFlags(flags_storage, &enabled_entries);

  int current_platform = GetCurrentPlatform();

  for (size_t i = 0; i < num_feature_entries; ++i) {
    const FeatureEntry& entry = feature_entries[i];
    if (SkipConditionalFeatureEntry(entry))
      continue;

    base::DictionaryValue* data = new base::DictionaryValue();
    data->SetString("internal_name", entry.internal_name);
    data->SetString("name",
                    l10n_util::GetStringUTF16(entry.visible_name_id));
    data->SetString("description",
                    l10n_util::GetStringUTF16(
                        entry.visible_description_id));

    base::ListValue* supported_platforms = new base::ListValue();
    AddOsStrings(entry.supported_platforms, supported_platforms);
    data->Set("supported_platforms", supported_platforms);
    // True if the switch is not currently passed.
    bool is_default_value = IsDefaultValue(entry, enabled_entries);
    data->SetBoolean("is_default", is_default_value);

    switch (entry.type) {
      case FeatureEntry::SINGLE_VALUE:
      case FeatureEntry::SINGLE_DISABLE_VALUE:
        data->SetBoolean(
            "enabled",
            (!is_default_value &&
             entry.type == FeatureEntry::SINGLE_VALUE) ||
            (is_default_value &&
             entry.type == FeatureEntry::SINGLE_DISABLE_VALUE));
        break;
      case FeatureEntry::MULTI_VALUE:
      case FeatureEntry::ENABLE_DISABLE_VALUE:
        data->Set("choices", CreateChoiceData(entry, enabled_entries));
        break;
      default:
        NOTREACHED();
    }

    bool supported = (entry.supported_platforms & current_platform) != 0;
#if defined(OS_CHROMEOS)
    if (access == kOwnerAccessToFlags &&
        (entry.supported_platforms & kOsCrOSOwnerOnly) != 0) {
      supported = true;
    }
#endif
    if (supported)
      supported_entries->Append(data);
    else
      unsupported_entries->Append(data);
  }
}

void GetFlagFeatureEntries(flags_ui::FlagsStorage* flags_storage,
                           FlagAccess access,
                           base::ListValue* supported_entries,
                           base::ListValue* unsupported_entries) {
  FlagsState::GetInstance()->GetFlagFeatureEntries(flags_storage, access,
                                                   supported_entries,
                                                   unsupported_entries);
}

bool IsRestartNeededToCommitChanges() {
  return FlagsState::GetInstance()->IsRestartNeededToCommitChanges();
}

void SetFeatureEntryEnabled(flags_ui::FlagsStorage* flags_storage,
                            const std::string& internal_name,
                            bool enable) {
  FlagsState::GetInstance()->SetFeatureEntryEnabled(flags_storage,
                                                  internal_name, enable);
}

void RemoveFlagsSwitches(
    std::map<std::string, base::CommandLine::StringType>* switch_list) {
  FlagsState::GetInstance()->RemoveFlagsSwitches(switch_list);
}

void ResetAllFlags(flags_ui::FlagsStorage* flags_storage) {
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

void RecordUMAStatistics(flags_ui::FlagsStorage* flags_storage) {
  std::set<std::string> flags = flags_storage->GetFlags();
  for (const std::string& flag : flags) {
    std::string action("AboutFlags_");
    action += flag;
    content::RecordComputedAction(action);
  }
  // Since flag metrics are recorded every startup, add a tick so that the
  // stats can be made meaningful.
  if (flags.size())
    content::RecordAction(base::UserMetricsAction("AboutFlags_StartupTick"));
  content::RecordAction(base::UserMetricsAction("StartupTick"));
}

base::HistogramBase::Sample GetSwitchUMAId(const std::string& switch_name) {
  return static_cast<base::HistogramBase::Sample>(
      metrics::HashMetricName(switch_name));
}

void ReportCustomFlags(const std::string& uma_histogram_hame,
                       const std::set<std::string>& command_line_difference) {
  for (const std::string& flag : command_line_difference) {
    int uma_id = about_flags::testing::kBadSwitchFormatHistogramId;
    if (base::StartsWith(flag, "--", base::CompareCase::SENSITIVE)) {
      // Skip '--' before switch name.
      std::string switch_name(flag.substr(2));

      // Kill value, if any.
      const size_t value_pos = switch_name.find('=');
      if (value_pos != std::string::npos)
        switch_name.resize(value_pos);

      uma_id = GetSwitchUMAId(switch_name);
    } else {
      NOTREACHED() << "ReportCustomFlags(): flag '" << flag
                   << "' has incorrect format.";
    }
    DVLOG(1) << "ReportCustomFlags(): histogram='" << uma_histogram_hame
             << "' '" << flag << "', uma_id=" << uma_id;

    // Sparse histogram macro does not cache the histogram, so it's safe
    // to use macro with non-static histogram name here.
    UMA_HISTOGRAM_SPARSE_SLOWLY(uma_histogram_hame, uma_id);
  }
}

//////////////////////////////////////////////////////////////////////////////
// FlagsState implementation.

namespace {

void FlagsState::ConvertFlagsToSwitches(flags_ui::FlagsStorage* flags_storage,
                                        base::CommandLine* command_line,
                                        SentinelsMode sentinels) {
  if (command_line->HasSwitch(switches::kNoExperiments))
    return;

  std::set<std::string> enabled_entries;

  GetSanitizedEnabledFlagsForCurrentPlatform(flags_storage, &enabled_entries);

  std::map<std::string, SwitchEntry> name_to_switch_map;
  for (size_t i = 0; i < num_feature_entries; ++i) {
    const FeatureEntry& e = feature_entries[i];
    switch (e.type) {
      case FeatureEntry::SINGLE_VALUE:
      case FeatureEntry::SINGLE_DISABLE_VALUE:
        AddSwitchMapping(e.internal_name, e.command_line_switch,
                         e.command_line_value, &name_to_switch_map);
        break;
      case FeatureEntry::MULTI_VALUE:
        for (int j = 0; j < e.num_choices; ++j) {
          AddSwitchMapping(e.NameForChoice(j), e.choices[j].command_line_switch,
                           e.choices[j].command_line_value,
                           &name_to_switch_map);
        }
        break;
      case FeatureEntry::ENABLE_DISABLE_VALUE:
        AddSwitchMapping(e.NameForChoice(0), std::string(), std::string(),
                         &name_to_switch_map);
        AddSwitchMapping(e.NameForChoice(1), e.command_line_switch,
                         e.command_line_value, &name_to_switch_map);
        AddSwitchMapping(e.NameForChoice(2), e.disable_command_line_switch,
                         e.disable_command_line_value, &name_to_switch_map);
        break;
      case FeatureEntry::FEATURE_VALUE:
        AddFeatureMapping(e.NameForChoice(0), std::string(), false,
                          &name_to_switch_map);
        AddFeatureMapping(e.NameForChoice(1), e.feature_name, true,
                          &name_to_switch_map);
        AddFeatureMapping(e.NameForChoice(2), e.feature_name, false,
                          &name_to_switch_map);
        break;
    }
  }

  AddSwitchesToCommandLine(enabled_entries, name_to_switch_map, sentinels,
                           command_line);
}

bool FlagsState::IsRestartNeededToCommitChanges() {
  return needs_restart_;
}

void FlagsState::SetFeatureEntryEnabled(flags_ui::FlagsStorage* flags_storage,
                                        const std::string& internal_name,
                                        bool enable) {
  size_t at_index = internal_name.find(testing::kMultiSeparator);
  if (at_index != std::string::npos) {
    DCHECK(enable);
    // We're being asked to enable a multi-choice entry. Disable the
    // currently selected choice.
    DCHECK_NE(at_index, 0u);
    const std::string entry_name = internal_name.substr(0, at_index);
    SetFeatureEntryEnabled(flags_storage, entry_name, false);

    // And enable the new choice, if it is not the default first choice.
    if (internal_name != entry_name + "@0") {
      std::set<std::string> enabled_entries;
      GetSanitizedEnabledFlags(flags_storage, &enabled_entries);
      needs_restart_ |= enabled_entries.insert(internal_name).second;
      flags_storage->SetFlags(enabled_entries);
    }
    return;
  }

  std::set<std::string> enabled_entries;
  GetSanitizedEnabledFlags(flags_storage, &enabled_entries);

  const FeatureEntry* e = nullptr;
  for (size_t i = 0; i < num_feature_entries; ++i) {
    if (feature_entries[i].internal_name == internal_name) {
      e = feature_entries + i;
      break;
    }
  }
  DCHECK(e);

  if (e->type == FeatureEntry::SINGLE_VALUE) {
    if (enable)
      needs_restart_ |= enabled_entries.insert(internal_name).second;
    else
      needs_restart_ |= (enabled_entries.erase(internal_name) > 0);
  } else if (e->type == FeatureEntry::SINGLE_DISABLE_VALUE) {
    if (!enable)
      needs_restart_ |= enabled_entries.insert(internal_name).second;
    else
      needs_restart_ |= (enabled_entries.erase(internal_name) > 0);
  } else {
    if (enable) {
      // Enable the first choice.
      needs_restart_ |= enabled_entries.insert(e->NameForChoice(0)).second;
    } else {
      // Find the currently enabled choice and disable it.
      for (int i = 0; i < e->num_choices; ++i) {
        std::string choice_name = e->NameForChoice(i);
        if (enabled_entries.find(choice_name) !=
            enabled_entries.end()) {
          needs_restart_ = true;
          enabled_entries.erase(choice_name);
          // Continue on just in case there's a bug and more than one
          // entry for this choice was enabled.
        }
      }
    }
  }

  flags_storage->SetFlags(enabled_entries);
}

void FlagsState::RemoveFlagsSwitches(
    std::map<std::string, base::CommandLine::StringType>* switch_list) {
  for (const auto& entry : flags_switches_)
    switch_list->erase(entry.first);

  // If feature entries were added to --enable-features= or --disable-features=
  // lists, remove them here while preserving existing values.
  for (const auto& entry : appended_switches_) {
    const auto& switch_name = entry.first;
    const auto& switch_added_values = entry.second;

    // The below is either a std::string or a base::string16 based on platform.
    const auto& existing_value = (*switch_list)[switch_name];
#if defined(OS_WIN)
    const std::string existing_value_utf8 = base::UTF16ToUTF8(existing_value);
#else
    const std::string& existing_value_utf8 = existing_value;
#endif

    std::vector<std::string> features =
        base::FeatureList::SplitFeatureListString(existing_value_utf8);
    std::vector<std::string> remaining_features;
    // For any featrue name in |features| that is not in |switch_added_values| -
    // i.e. it wasn't added by about_flags code, add it to |remaining_features|.
    for (const std::string& feature : features) {
      if (!ContainsKey(switch_added_values, feature))
        remaining_features.push_back(feature);
    }

    // Either remove the flag entirely if |remaining_features| is empty, or set
    // the new list.
    if (remaining_features.empty()) {
      switch_list->erase(switch_name);
    } else {
      std::string switch_value = base::JoinString(remaining_features, ",");
#if defined(OS_WIN)
      (*switch_list)[switch_name] = base::UTF8ToUTF16(switch_value);
#else
      (*switch_list)[switch_name] = switch_value;
#endif
    }
  }
}

void FlagsState::ResetAllFlags(flags_ui::FlagsStorage* flags_storage) {
  needs_restart_ = true;

  std::set<std::string> no_entries;
  flags_storage->SetFlags(no_entries);
}

void FlagsState::Reset() {
  needs_restart_ = false;
  flags_switches_.clear();
  appended_switches_.clear();
}

void FlagsState::SetFeatureEntries(const FeatureEntry* entries, size_t count) {
  feature_entries = entries;
  num_feature_entries = count;
}

const FeatureEntry* FlagsState::GetFeatureEntries(size_t* count) {
  *count = num_feature_entries;
  return feature_entries;
}

void FlagsState::AddSwitchMapping(
    const std::string& key,
    const std::string& switch_name,
    const std::string& switch_value,
    std::map<std::string, SwitchEntry>* name_to_switch_map) {
  DCHECK(!ContainsKey(*name_to_switch_map, key));

  SwitchEntry* entry = &(*name_to_switch_map)[key];
  entry->switch_name = switch_name;
  entry->switch_value = switch_value;
}

void FlagsState::AddFeatureMapping(
    const std::string& key,
    const std::string& feature_name,
    bool feature_state,
    std::map<std::string, SwitchEntry>* name_to_switch_map) {
  DCHECK(!ContainsKey(*name_to_switch_map, key));

  SwitchEntry* entry = &(*name_to_switch_map)[key];
  entry->feature_name = feature_name;
  entry->feature_state = feature_state;
}

void FlagsState::AddSwitchesToCommandLine(
    const std::set<std::string>& enabled_entries,
    const std::map<std::string, SwitchEntry>& name_to_switch_map,
    SentinelsMode sentinels,
    base::CommandLine* command_line) {
  std::map<std::string, bool> feature_switches;
  if (sentinels == kAddSentinels) {
    command_line->AppendSwitch(switches::kFlagSwitchesBegin);
    flags_switches_[switches::kFlagSwitchesBegin] = std::string();
  }

  for (const std::string& entry_name : enabled_entries) {
    const auto& entry_it = name_to_switch_map.find(entry_name);
    if (entry_it == name_to_switch_map.end()) {
      NOTREACHED();
      continue;
    }

    const SwitchEntry& entry = entry_it->second;
    if (!entry.feature_name.empty()) {
      feature_switches[entry.feature_name] = entry.feature_state;
    } else if (!entry.switch_name.empty()) {
      command_line->AppendSwitchASCII(entry.switch_name, entry.switch_value);
      flags_switches_[entry.switch_name] = entry.switch_value;
    }
    // If an entry doesn't match either of the above, then it is likely the
    // default entry for a FEATURE_VALUE entry. Safe to ignore.
  }

  if (!feature_switches.empty()) {
    MergeFeatureCommandLineSwitch(feature_switches, switches::kEnableFeatures,
                                  true, command_line);
    MergeFeatureCommandLineSwitch(feature_switches, switches::kDisableFeatures,
                                  false, command_line);
  }

  if (sentinels == kAddSentinels) {
    command_line->AppendSwitch(switches::kFlagSwitchesEnd);
    flags_switches_[switches::kFlagSwitchesEnd] = std::string();
  }
}

void FlagsState::MergeFeatureCommandLineSwitch(
    const std::map<std::string, bool>& feature_switches,
    const char* switch_name,
    bool feature_state,
    base::CommandLine* command_line) {
  std::string original_switch_value =
      command_line->GetSwitchValueASCII(switch_name);
  std::vector<std::string> features =
      base::FeatureList::SplitFeatureListString(original_switch_value);
  // Only add features that don't already exist in the lists.
  // Note: The ContainsValue() call results in O(n^2) performance, but in
  // practice n should be very small.
  for (const auto& entry : feature_switches) {
    if (entry.second == feature_state &&
        !ContainsValue(features, entry.first)) {
      features.push_back(entry.first);
      appended_switches_[switch_name].insert(entry.first);
    }
  }
  // Update the switch value only if it didn't change. This avoids setting an
  // empty list or duplicating the same list (since AppendSwitch() adds the
  // switch to the end but doesn't remove previous ones).
  std::string switch_value = base::JoinString(features, ",");
  if (switch_value != original_switch_value)
    command_line->AppendSwitchASCII(switch_name, switch_value);
}

}  // namespace

namespace testing {

// WARNING: '@' is also used in the html file. If you update this constant you
// also need to update the html file.
const char kMultiSeparator[] = "@";

const base::HistogramBase::Sample kBadSwitchFormatHistogramId = 0;

void ClearState() {
  FlagsState::GetInstance()->Reset();
}

void SetFeatureEntries(const FeatureEntry* entries, size_t count) {
  if (!entries) {
    entries = kFeatureEntries;
    count = arraysize(kFeatureEntries);
  }
  FlagsState::GetInstance()->SetFeatureEntries(entries, count);
}

const FeatureEntry* GetFeatureEntries(size_t* count) {
  return FlagsState::GetInstance()->GetFeatureEntries(count);
}

}  // namespace testing

}  // namespace about_flags
