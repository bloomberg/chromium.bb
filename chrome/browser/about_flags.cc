// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/about_flags.h"

#include <iterator>
#include <map>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/i18n/base_i18n_switches.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/metrics_hashes.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_scheduler/switches.h"
#include "base/values.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/features.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/browser_sync/browser_sync_switches.h"
#include "components/cloud_devices/common/cloud_devices_switches.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/dom_distiller/core/dom_distiller_switches.h"
#include "components/error_page/common/error_page_switches.h"
#include "components/flags_ui/feature_entry_macros.h"
#include "components/flags_ui/flags_storage.h"
#include "components/flags_ui/flags_ui_switches.h"
#include "components/nacl/common/nacl_switches.h"
#include "components/ntp_snippets/features.h"
#include "components/ntp_snippets/ntp_snippets_constants.h"
#include "components/ntp_tiles/switches.h"
#include "components/offline_pages/offline_page_feature.h"
#include "components/omnibox/browser/omnibox_switches.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/proximity_auth/switches.h"
#include "components/security_state/core/switches.h"
#include "components/signin/core/common/signin_switches.h"
#include "components/spellcheck/common/spellcheck_features.h"
#include "components/spellcheck/common/spellcheck_switches.h"
#include "components/spellcheck/spellcheck_build_features.h"
#include "components/ssl_config/ssl_config_switches.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/tracing/common/tracing_switches.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/feature_h264_with_openh264_ffmpeg.h"
#include "content/public/common/features.h"
#include "extensions/features/features.h"
#include "gin/public/gin_features.h"
#include "media/audio/audio_features.h"
#include "media/base/media_switches.h"
#include "media/media_features.h"
#include "media/midi/midi_switches.h"
#include "ppapi/features/features.h"
#include "printing/features/features.h"
#include "ui/base/ui_base_switches.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/display/display_switches.h"
#include "ui/events/event_switches.h"
#include "ui/gfx/switches.h"
#include "ui/gl/gl_features.h"
#include "ui/gl/gl_switches.h"
#include "ui/keyboard/keyboard_switches.h"
#include "ui/native_theme/native_theme_switches.h"
#include "ui/views/views_switches.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/chrome_feature_list.h"
#else  // OS_ANDROID
#include "ui/message_center/message_center_switches.h"
#endif  // OS_ANDROID

#if defined(OS_CHROMEOS)
#include "chromeos/chromeos_switches.h"
#include "components/arc/arc_features.h"
#include "third_party/cros_system_api/switches/chrome_switches.h"
#endif  // OS_CHROMEOS

#if defined(OS_MACOSX)
#include "chrome/browser/ui/browser_dialogs.h"
#endif  // OS_MACOSX

#if BUILDFLAG(ENABLE_APP_LIST)
#include "ui/app_list/app_list_switches.h"
#endif  // ENABLE_APP_LIST

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/switches.h"
#endif  // ENABLE_EXTENSIONS

#if defined(USE_ASH)
#include "ash/common/ash_switches.h"
#endif  // USE_ASH

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_switches.h"
#endif  // USE_OZONE

using flags_ui::FeatureEntry;
using flags_ui::kOsMac;
using flags_ui::kOsWin;
using flags_ui::kOsLinux;
using flags_ui::kOsCrOS;
using flags_ui::kOsAndroid;
using flags_ui::kOsCrOSOwnerOnly;

namespace about_flags {

namespace {

const unsigned kOsAll = kOsMac | kOsWin | kOsLinux | kOsCrOS | kOsAndroid;
const unsigned kOsDesktop = kOsMac | kOsWin | kOsLinux | kOsCrOS;

const FeatureEntry::Choice kTouchEventsChoices[] = {
    { IDS_GENERIC_EXPERIMENT_CHOICE_ENABLED, "", "" },
    { IDS_GENERIC_EXPERIMENT_CHOICE_DISABLED, switches::kTouchEvents,
        switches::kTouchEventsDisabled },
    { IDS_GENERIC_EXPERIMENT_CHOICE_AUTOMATIC, switches::kTouchEvents,
        switches::kTouchEventsAuto }
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
#endif  // USE_AURA

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
#endif  // DISABLE_NACL

const FeatureEntry::Choice kPassiveListenersChoices[] = {
    {IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", ""},
    {IDS_FLAGS_PASSIVE_EVENT_LISTENER_TRUE, switches::kPassiveListenersDefault,
     "true"},
    {IDS_FLAGS_PASSIVE_EVENT_LISTENER_FORCE_ALL_TRUE,
     switches::kPassiveListenersDefault, "forcealltrue"},
};

const FeatureEntry::Choice kMarkHttpAsChoices[] = {
    {IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", ""},
    {IDS_MARK_HTTP_AS_NEUTRAL, security_state::switches::kMarkHttpAs,
     security_state::switches::kMarkHttpAsNeutral},
    {IDS_MARK_HTTP_AS_DANGEROUS, security_state::switches::kMarkHttpAs,
     security_state::switches::kMarkHttpAsDangerous},
    {IDS_MARK_HTTP_WITH_PASSWORDS_OR_CC_WITH_CHIP,
     security_state::switches::kMarkHttpAs,
     security_state::switches::kMarkHttpWithPasswordsOrCcWithChip},
    {IDS_MARK_HTTP_WITH_PASSWORDS_OR_CC_WITH_CHIP_AND_FORM_WARNING,
     security_state::switches::kMarkHttpAs,
     security_state::switches::
         kMarkHttpWithPasswordsOrCcWithChipAndFormWarning}};

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
    error_page::switches::kShowSavedCopy,
    error_page::switches::kEnableShowSavedCopyPrimary },
  { IDS_FLAGS_ENABLE_SHOW_SAVED_COPY_SECONDARY,
    error_page::switches::kShowSavedCopy,
    error_page::switches::kEnableShowSavedCopySecondary },
  { IDS_FLAGS_DISABLE_SHOW_SAVED_COPY,
    error_page::switches::kShowSavedCopy,
    error_page::switches::kDisableShowSavedCopy }
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
#endif  // OS_ANDROID

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

const FeatureEntry::Choice kMHTMLGeneratorOptionChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", "" },
  { IDS_FLAGS_MHTML_SKIP_NOSTORE_MAIN,
    switches::kMHTMLGeneratorOption, switches::kMHTMLSkipNostoreMain },
  { IDS_FLAGS_MHTML_SKIP_NOSTORE_ALL,
    switches::kMHTMLGeneratorOption, switches::kMHTMLSkipNostoreAll },
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

const FeatureEntry::Choice kEnableWebGL2Choices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", "" },
  { IDS_GENERIC_EXPERIMENT_CHOICE_ENABLED, switches::kEnableES3APIs, "" },
  { IDS_GENERIC_EXPERIMENT_CHOICE_DISABLED, switches::kDisableES3APIs, "" },
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
#endif  // OS_CHROMEOS

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

const FeatureEntry::Choice kTopChromeMaterialDesignChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", "" },
  { IDS_FLAGS_TOP_CHROME_MD_MATERIAL,
    switches::kTopChromeMD,
    switches::kTopChromeMDMaterial },
  { IDS_FLAGS_TOP_CHROME_MD_MATERIAL_HYBRID,
    switches::kTopChromeMD,
    switches::kTopChromeMDMaterialHybrid },
};

#if defined(USE_ASH)
const FeatureEntry::Choice kAshMaterialDesignChoices[] = {
    {IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", ""},
    {IDS_GENERIC_EXPERIMENT_CHOICE_DISABLED, ash::switches::kAshMaterialDesign,
     ash::switches::kAshMaterialDesignDisabled},
    {IDS_GENERIC_EXPERIMENT_CHOICE_ENABLED, ash::switches::kAshMaterialDesign,
     ash::switches::kAshMaterialDesignEnabled},
    {IDS_FLAGS_ASH_MD_EXPERIMENTAL, ash::switches::kAshMaterialDesign,
     ash::switches::kAshMaterialDesignExperimental},
};
#endif  // USE_ASH

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
#endif  // OS_CHROMEOS

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

const FeatureEntry::Choice kV8CacheStrategiesForCacheStorageChoices[] = {
    {IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", ""},
    {IDS_GENERIC_EXPERIMENT_CHOICE_DISABLED,
     switches::kV8CacheStrategiesForCacheStorage, "none"},
    {IDS_FLAGS_V8_CACHE_STRATEGIES_FOR_CACHE_STORAGE_NORMAL,
     switches::kV8CacheStrategiesForCacheStorage, "normal"},
    {IDS_FLAGS_V8_CACHE_STRATEGIES_FOR_CACHE_STORAGE_AGGRESSIVE,
     switches::kV8CacheStrategiesForCacheStorage, "aggressive"},
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
  { IDS_FLAGS_PROGRESS_BAR_ANIMATION_SMOOTH_INDETERMINATE,
      switches::kProgressBarAnimation, "smooth-indeterminate" },
  { IDS_FLAGS_PROGRESS_BAR_ANIMATION_FAST_START,
      switches::kProgressBarAnimation, "fast-start" },
};

const FeatureEntry::Choice kProgressBarCompletionChoices[] = {
    {IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", "" },
    {IDS_FLAGS_PROGRESS_BAR_COMPLETION_LOAD_EVENT,
     switches::kProgressBarCompletion, "loadEvent"},
    {IDS_FLAGS_PROGRESS_BAR_COMPLETION_RESOURCES_BEFORE_DCL,
     switches::kProgressBarCompletion, "resourcesBeforeDOMContentLoaded"},
    {IDS_FLAGS_PROGRESS_BAR_COMPLETION_DOM_CONTENT_LOADED,
     switches::kProgressBarCompletion, "domContentLoaded"},
    {IDS_FLAGS_PROGRESS_BAR_COMPLETION_RESOURCES_BEFORE_DCL_AND_SAME_ORIGIN_IFRAMES,
     switches::kProgressBarCompletion,
     "resourcesBeforeDOMContentLoadedAndSameOriginIFrames"},
};
#endif  // OS_ANDROID

#if defined(OS_CHROMEOS)
const FeatureEntry::Choice kCrosRegionsModeChoices[] = {
  { IDS_FLAGS_CROS_REGIONS_MODE_DEFAULT, "", "" },
  { IDS_FLAGS_CROS_REGIONS_MODE_OVERRIDE, chromeos::switches::kCrosRegionsMode,
        chromeos::switches::kCrosRegionsModeOverride },
  { IDS_FLAGS_CROS_REGIONS_MODE_HIDE, chromeos::switches::kCrosRegionsMode,
        chromeos::switches::kCrosRegionsModeHide },
};
#endif  // OS_CHROMEOS

const FeatureEntry::Choice kForceUIDirectionChoices[] = {
    {IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", ""},
    {IDS_FLAGS_FORCE_UI_DIRECTION_LTR, switches::kForceUIDirection,
     switches::kForceUIDirectionLTR},
    {IDS_FLAGS_FORCE_UI_DIRECTION_RTL, switches::kForceUIDirection,
     switches::kForceUIDirectionRTL},
};

#if defined(OS_ANDROID)
const FeatureEntry::Choice kNtpSwitchToExistingTabChoices[] = {
    {IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", ""},
    {IDS_GENERIC_EXPERIMENT_CHOICE_DISABLED, switches::kNtpSwitchToExistingTab,
     "disabled"},
    {IDS_FLAGS_NTP_SWITCH_TO_EXISTING_TAB_MATCH_URL,
     switches::kNtpSwitchToExistingTab, "url"},
    {IDS_FLAGS_NTP_SWITCH_TO_EXISTING_TAB_MATCH_HOST,
     switches::kNtpSwitchToExistingTab, "host"},
};
#endif  // OS_ANDROID

#if defined(OS_ANDROID)
const FeatureEntry::FeatureParam kNTPSnippetsFeatureVariationOnlyPersonal[] = {
    {"fetching_personalization", "personal"},
};

const FeatureEntry::FeatureParam kNTPSnippetsFeatureVariationServer[] = {
    {"content_suggestions_backend",
     ntp_snippets::kContentSuggestionsServer}};

const FeatureEntry::FeatureParam
    kNTPSnippetsFeatureVariationServerNonPersonalized[] = {
        {"content_suggestions_backend",
         ntp_snippets::kContentSuggestionsServer},
        {"fetching_personalization", "non_personal"}};

const FeatureEntry::FeatureVariation kNTPSnippetsFeatureVariations[] = {
    {"via ChromeReader (only personalized)",
     kNTPSnippetsFeatureVariationOnlyPersonal,
     arraysize(kNTPSnippetsFeatureVariationOnlyPersonal), nullptr},
    {"via content suggestion server (backed by ChromeReader)",
     kNTPSnippetsFeatureVariationServer,
     arraysize(kNTPSnippetsFeatureVariationServer), nullptr},
    {"via content suggestion server (backed by ChromeReader, non-personalized)",
     kNTPSnippetsFeatureVariationServerNonPersonalized,
     arraysize(kNTPSnippetsFeatureVariationServerNonPersonalized), nullptr},
    {"via content suggestion server (backed by Google Now)",
     kNTPSnippetsFeatureVariationServer,
     arraysize(kNTPSnippetsFeatureVariationServer), "3313279"},
    {"via content suggestion server (backed by Google Now, non-personalized)",
     kNTPSnippetsFeatureVariationServerNonPersonalized,
     arraysize(kNTPSnippetsFeatureVariationServerNonPersonalized), "3313279"}};
#endif  // OS_ANDROID

#if defined(OS_ANDROID)
const FeatureEntry::Choice kUpdateMenuItemSummaryChoices[] = {
    {IDS_FLAGS_UPDATE_MENU_ITEM_NO_SUMMARY, "", ""},
    {IDS_FLAGS_UPDATE_MENU_ITEM_DEFAULT_SUMMARY,
        switches::kForceShowUpdateMenuItemSummary, ""},
    {IDS_FLAGS_UPDATE_MENU_ITEM_NEW_FEATURES_SUMMARY,
        switches::kForceShowUpdateMenuItemNewFeaturesSummary, ""},
    {IDS_FLAGS_UPDATE_MENU_ITEM_CUSTOM_SUMMARY,
        switches::kForceShowUpdateMenuItemCustomSummary, "Custom summary"},
};
#endif  // OS_ANDROID

#if defined(OS_ANDROID)
const FeatureEntry::Choice kHerbPrototypeChoices[] = {
    {IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", ""},
    {IDS_GENERIC_EXPERIMENT_CHOICE_DISABLED,
     switches::kTabManagementExperimentTypeDisabled, ""},
    {IDS_FLAGS_HERB_PROTOTYPE_FLAVOR_ELDERBERRY,
     switches::kTabManagementExperimentTypeElderberry, ""},
};
#endif  // OS_ANDROID

const FeatureEntry::Choice kEnableUseZoomForDSFChoices[] = {
  { IDS_FLAGS_ENABLE_USE_ZOOM_FOR_DSF_CHOICE_DEFAULT, "", ""},
  { IDS_FLAGS_ENABLE_USE_ZOOM_FOR_DSF_CHOICE_ENABLED,
    switches::kEnableUseZoomForDSF, "true" },
  { IDS_FLAGS_ENABLE_USE_ZOOM_FOR_DSF_CHOICE_DISABLED,
    switches::kEnableUseZoomForDSF, "false" },
};

const FeatureEntry::Choice kEnableWebFontsInterventionV2Choices[] = {
    {IDS_FLAGS_ENABLE_WEBFONTS_INTERVENTION_V2_CHOICE_DEFAULT, "", ""},
    {IDS_FLAGS_ENABLE_WEBFONTS_INTERVENTION_V2_CHOICE_ENABLED_WITH_2G,
     switches::kEnableWebFontsInterventionV2,
     switches::kEnableWebFontsInterventionV2SwitchValueEnabledWith2G},
    {IDS_FLAGS_ENABLE_WEBFONTS_INTERVENTION_V2_CHOICE_ENABLED_WITH_3G,
     switches::kEnableWebFontsInterventionV2,
     switches::kEnableWebFontsInterventionV2SwitchValueEnabledWith3G},
    {IDS_FLAGS_ENABLE_WEBFONTS_INTERVENTION_V2_CHOICE_ENABLED_WITH_SLOW2G,
     switches::kEnableWebFontsInterventionV2,
     switches::kEnableWebFontsInterventionV2SwitchValueEnabledWithSlow2G},
    {IDS_FLAGS_ENABLE_WEBFONTS_INTERVENTION_V2_CHOICE_DISABLED,
     switches::kEnableWebFontsInterventionV2,
     switches::kEnableWebFontsInterventionV2SwitchValueDisabled},
};

const FeatureEntry::Choice kSSLVersionMaxChoices[] = {
    {IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", "" },
    {IDS_FLAGS_SSL_VERSION_MAX_TLS12, switches::kSSLVersionMax, "tls1.2"},
    {IDS_FLAGS_SSL_VERSION_MAX_TLS13, switches::kSSLVersionMax, "tls1.3"},
};

const FeatureEntry::Choice kSecurityChipChoices[] = {
    {IDS_FLAGS_SECURITY_CHIP_DEFAULT, "", ""},
    {IDS_FLAGS_SECURITY_CHIP_SHOW_NONSECURE_ONLY, switches::kSecurityChip,
     switches::kSecurityChipShowNonSecureOnly},
    {IDS_FLAGS_SECURITY_CHIP_SHOW_ALL, switches::kSecurityChip,
     switches::kSecurityChipShowAll},
};

const FeatureEntry::Choice kSecurityChipAnimationChoices[] = {
    {IDS_FLAGS_SECURITY_CHIP_ANIMATION_DEFAULT, "", ""},
    {IDS_FLAGS_SECURITY_CHIP_ANIMATION_NONE, switches::kSecurityChipAnimation,
     switches::kSecurityChipAnimationNone},
    {IDS_FLAGS_SECURITY_CHIP_ANIMATION_NONSECURE_ONLY,
     switches::kSecurityChipAnimation,
     switches::kSecurityChipAnimationNonSecureOnly},
    {IDS_FLAGS_SECURITY_CHIP_ANIMATION_ALL, switches::kSecurityChipAnimation,
     switches::kSecurityChipAnimationAll},
};

#if BUILDFLAG(ENABLE_WEBRTC)
const FeatureEntry::Choice kDisableWebRtcHWEncodingChoices[] = {
    {IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", ""},
    {IDS_FLAGS_WEBRTC_HW_ENCODING_ALL, switches::kDisableWebRtcHWEncoding, ""},
    {IDS_FLAGS_WEBRTC_HW_ENCODING_VPX, switches::kDisableWebRtcHWEncoding,
     switches::kDisableWebRtcHWEncodingVPx},
    {IDS_FLAGS_WEBRTC_HW_ENCODING_H264, switches::kDisableWebRtcHWEncoding,
     switches::kDisableWebRtcHWEncodingH264},
    {IDS_FLAGS_WEBRTC_HW_ENCODING_NONE, switches::kDisableWebRtcHWEncoding,
     switches::kDisableWebRtcHWEncodingNone},
};
#endif  // ENABLE_WEBRTC

#if !defined(OS_ANDROID)
const FeatureEntry::Choice kEnableDefaultMediaSessionChoices[] = {
    {IDS_FLAGS_ENABLE_DEFAULT_MEDIA_SESSION_DISABLED, "", ""},
    {IDS_FLAGS_ENABLE_DEFAULT_MEDIA_SESSION_ENABLED,
     switches::kEnableDefaultMediaSession, ""},
#if BUILDFLAG(ENABLE_PLUGINS)
    {IDS_FLAGS_ENABLE_DEFAULT_MEDIA_SESSION_ENABLED_DUCK_FLASH,
     switches::kEnableDefaultMediaSession,
     switches::kEnableDefaultMediaSessionDuckFlash},
#endif  // BUILDFLAG(ENABLE_PLUGINS)
};
#endif  // !defined(OS_ANDROID)

// RECORDING USER METRICS FOR FLAGS:
// -----------------------------------------------------------------------------
// The first line of the entry is the internal name.
//
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
// Usage of about:flags is logged on startup via the "Launch.FlagsAtStartup"
// UMA histogram. This histogram shows the number of startups with a given flag
// enabled. If you'd like to see user counts instead, make sure to switch to
// to "count users" view on the dashboard. When adding new entries, the enum
// "LoginCustomFlags" must be updated in histograms.xml. See note in
// histograms.xml and don't forget to run AboutFlagsHistogramTest unit test
// to calculate and verify checksum.
//
// When adding a new choice, add it to the end of the list.
const FeatureEntry kFeatureEntries[] = {
    {"ignore-gpu-blacklist", IDS_FLAGS_IGNORE_GPU_BLACKLIST_NAME,
     IDS_FLAGS_IGNORE_GPU_BLACKLIST_DESCRIPTION, kOsAll,
     SINGLE_VALUE_TYPE(switches::kIgnoreGpuBlacklist)},
    {"enable-experimental-canvas-features",
     IDS_FLAGS_EXPERIMENTAL_CANVAS_FEATURES_NAME,
     IDS_FLAGS_EXPERIMENTAL_CANVAS_FEATURES_DESCRIPTION, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableExperimentalCanvasFeatures)},
    {"disable-accelerated-2d-canvas", IDS_FLAGS_ACCELERATED_2D_CANVAS_NAME,
     IDS_FLAGS_ACCELERATED_2D_CANVAS_DESCRIPTION, kOsAll,
     SINGLE_DISABLE_VALUE_TYPE(switches::kDisableAccelerated2dCanvas)},
    {"enable-display-list-2d-canvas", IDS_FLAGS_DISPLAY_LIST_2D_CANVAS_NAME,
     IDS_FLAGS_DISPLAY_LIST_2D_CANVAS_DESCRIPTION, kOsAll,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableDisplayList2dCanvas,
                               switches::kDisableDisplayList2dCanvas)},
    {"enable-canvas-2d-dynamic-rendering-mode-switching",
     IDS_FLAGS_ENABLE_2D_CANVAS_DYNAMIC_RENDERING_MODE_SWITCHING_NAME,
     IDS_FLAGS_ENABLE_2D_CANVAS_DYNAMIC_RENDERING_MODE_SWITCHING_DESCRIPTION,
     kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableCanvas2dDynamicRenderingModeSwitching)},
    {"composited-layer-borders", IDS_FLAGS_COMPOSITED_LAYER_BORDERS,
     IDS_FLAGS_COMPOSITED_LAYER_BORDERS_DESCRIPTION, kOsAll,
     SINGLE_VALUE_TYPE(cc::switches::kShowCompositedLayerBorders)},
    {"gl-composited-texture-quad-borders",
     IDS_FLAGS_GL_COMPOSITED_TEXTURE_QUAD_BORDERS,
     IDS_FLAGS_GL_COMPOSITED_TEXTURE_QUAD_BORDERS_DESCRIPTION, kOsAll,
     SINGLE_VALUE_TYPE(cc::switches::kGlCompositedTextureQuadBorder)},
#if BUILDFLAG(ENABLE_WEBRTC)
    {"disable-webrtc-hw-decoding", IDS_FLAGS_WEBRTC_HW_DECODING_NAME,
     IDS_FLAGS_WEBRTC_HW_DECODING_DESCRIPTION, kOsAndroid | kOsCrOS,
     SINGLE_DISABLE_VALUE_TYPE(switches::kDisableWebRtcHWDecoding)},
    {"disable-webrtc-hw-encoding", IDS_FLAGS_WEBRTC_HW_ENCODING_NAME,
     IDS_FLAGS_WEBRTC_HW_ENCODING_DESCRIPTION, kOsAndroid | kOsCrOS,
     MULTI_VALUE_TYPE(kDisableWebRtcHWEncodingChoices)},
    {"enable-webrtc-stun-origin", IDS_FLAGS_WEBRTC_STUN_ORIGIN_NAME,
     IDS_FLAGS_WEBRTC_STUN_ORIGIN_DESCRIPTION, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableWebRtcStunOrigin)},
#endif  // ENABLE_WEBRTC
#if defined(OS_ANDROID)
    {"enable-osk-overscroll", IDS_FLAGS_ENABLE_OSK_OVERSCROLL_NAME,
     IDS_FLAGS_ENABLE_OSK_OVERSCROLL_DESCRIPTION, kOsAndroid,
     SINGLE_VALUE_TYPE(switches::kEnableOSKOverscroll)},
    {"enable-usermedia-screen-capturing", IDS_FLAGS_MEDIA_SCREEN_CAPTURE_NAME,
     IDS_FLAGS_MEDIA_SCREEN_CAPTURE_DESCRIPTION, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kUserMediaScreenCapturing)},
#endif  // OS_ANDROID
// Native client is compiled out when DISABLE_NACL is defined.
#if !defined(DISABLE_NACL)
    {"enable-nacl", IDS_FLAGS_NACL_NAME, IDS_FLAGS_NACL_DESCRIPTION, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableNaCl)},
    {"enable-nacl-debug", IDS_FLAGS_NACL_DEBUG_NAME,
     IDS_FLAGS_NACL_DEBUG_DESCRIPTION, kOsDesktop,
     SINGLE_VALUE_TYPE(switches::kEnableNaClDebug)},
    {"force-pnacl-subzero", IDS_FLAGS_PNACL_SUBZERO_NAME,
     IDS_FLAGS_PNACL_SUBZERO_DESCRIPTION, kOsDesktop,
     SINGLE_VALUE_TYPE(switches::kForcePNaClSubzero)},
    {"nacl-debug-mask", IDS_FLAGS_NACL_DEBUG_MASK_NAME,
     IDS_FLAGS_NACL_DEBUG_MASK_DESCRIPTION, kOsDesktop,
     MULTI_VALUE_TYPE(kNaClDebugMaskChoices)},
#endif  // DISABLE_NACL
#if BUILDFLAG(ENABLE_EXTENSIONS)
    {"extension-apis", IDS_FLAGS_EXPERIMENTAL_EXTENSION_APIS_NAME,
     IDS_FLAGS_EXPERIMENTAL_EXTENSION_APIS_DESCRIPTION, kOsDesktop,
     SINGLE_VALUE_TYPE(extensions::switches::kEnableExperimentalExtensionApis)},
    {"extensions-on-chrome-urls", IDS_FLAGS_EXTENSIONS_ON_CHROME_URLS_NAME,
     IDS_FLAGS_EXTENSIONS_ON_CHROME_URLS_DESCRIPTION, kOsAll,
     SINGLE_VALUE_TYPE(extensions::switches::kExtensionsOnChromeURLs)},
#endif  // ENABLE_EXTENSIONS
    {"enable-fast-unload", IDS_FLAGS_FAST_UNLOAD_NAME,
     IDS_FLAGS_FAST_UNLOAD_DESCRIPTION, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableFastUnload)},
#if BUILDFLAG(ENABLE_EXTENSIONS)
    {"enable-app-window-controls", IDS_FLAGS_APP_WINDOW_CONTROLS_NAME,
     IDS_FLAGS_APP_WINDOW_CONTROLS_DESCRIPTION, kOsDesktop,
     SINGLE_VALUE_TYPE(extensions::switches::kEnableAppWindowControls)},
#endif  // ENABLE_EXTENSIONS
    {"enable-history-entry-requires-user-gesture",
     IDS_FLAGS_HISTORY_REQUIRES_USER_GESTURE_NAME,
     IDS_FLAGS_HISTORY_REQUIRES_USER_GESTURE_DESCRIPTION, kOsAll,
     SINGLE_VALUE_TYPE(switches::kHistoryEntryRequiresUserGesture)},
    {"disable-hyperlink-auditing", IDS_FLAGS_HYPERLINK_AUDITING_NAME,
     IDS_FLAGS_HYPERLINK_AUDITING_DESCRIPTION, kOsAll,
     SINGLE_DISABLE_VALUE_TYPE(switches::kNoPings)},
#if defined(OS_ANDROID)
    {"contextual-search", IDS_FLAGS_CONTEXTUAL_SEARCH,
     IDS_FLAGS_CONTEXTUAL_SEARCH_DESCRIPTION, kOsAndroid,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableContextualSearch,
                               switches::kDisableContextualSearch)},
    {"cs-contextual-cards-bar-integration",
     IDS_FLAGS_CONTEXTUAL_SEARCH_CONTEXTUAL_CARDS_BAR_INTEGRATION,
     IDS_FLAGS_CONTEXTUAL_SEARCH_CONTEXTUAL_CARDS_BAR_INTEGRATION_DESCRIPTION,
     kOsAndroid,
     SINGLE_VALUE_TYPE(
         switches::kEnableContextualSearchContextualCardsBarIntegration)},
    {"cs-contextual-cards-single-actions",
     IDS_FLAGS_CONTEXTUAL_SEARCH_SINGLE_ACTIONS,
     IDS_FLAGS_CONTEXTUAL_SEARCH_SINGLE_ACTIONS_DESCRIPTION, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kContextualSearchSingleActions)},
#endif  // OS_ANDROID
    {"show-autofill-type-predictions",
     IDS_FLAGS_SHOW_AUTOFILL_TYPE_PREDICTIONS_NAME,
     IDS_FLAGS_SHOW_AUTOFILL_TYPE_PREDICTIONS_DESCRIPTION, kOsAll,
     SINGLE_VALUE_TYPE(autofill::switches::kShowAutofillTypePredictions)},
    {"enable-credit-card-signin-promo",
     IDS_FLAGS_ENABLE_AUTOFILL_CREDIT_CARD_SIGNIN_PROMO_NAME,
     IDS_FLAGS_ENABLE_AUTOFILL_CREDIT_CARD_SIGNIN_PROMO_DESCRIPTION, kOsAll,
     FEATURE_VALUE_TYPE(autofill::kAutofillCreditCardSigninPromo)},
    {"smooth-scrolling", IDS_FLAGS_SMOOTH_SCROLLING_NAME,
     IDS_FLAGS_SMOOTH_SCROLLING_DESCRIPTION,
     // Mac has a separate implementation with its own setting to disable.
     kOsLinux | kOsCrOS | kOsWin | kOsAndroid,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableSmoothScrolling,
                               switches::kDisableSmoothScrolling)},
#if defined(USE_AURA) || defined(OS_LINUX)
    {"overlay-scrollbars", IDS_FLAGS_OVERLAY_SCROLLBARS_NAME,
     IDS_FLAGS_OVERLAY_SCROLLBARS_DESCRIPTION,
     // Uses the system preference on Mac (a different implementation).
     // On Android, this is always enabled.
     kOsLinux | kOsCrOS | kOsWin,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableOverlayScrollbar,
                               switches::kDisableOverlayScrollbar)},
#endif  // USE_AURA || OS_LINUX
    {   // See http://crbug.com/120416 for how to remove this flag.
     "save-page-as-mhtml", IDS_FLAGS_SAVE_PAGE_AS_MHTML_NAME,
     IDS_FLAGS_SAVE_PAGE_AS_MHTML_DESCRIPTION, kOsMac | kOsWin | kOsLinux,
     SINGLE_VALUE_TYPE(switches::kSavePageAsMHTML)},
    {"mhtml-generator-option", IDS_FLAGS_MHTML_GENERATOR_OPTION_NAME,
     IDS_FLAGS_MHTML_GENERATOR_OPTION_DESCRIPTION, kOsMac | kOsWin | kOsLinux,
     MULTI_VALUE_TYPE(kMHTMLGeneratorOptionChoices)},
    {"enable-quic", IDS_FLAGS_QUIC_NAME, IDS_FLAGS_QUIC_DESCRIPTION, kOsAll,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableQuic, switches::kDisableQuic)},
    {"disable-javascript-harmony-shipping",
     IDS_FLAGS_JAVASCRIPT_HARMONY_SHIPPING_NAME,
     IDS_FLAGS_JAVASCRIPT_HARMONY_SHIPPING_DESCRIPTION, kOsAll,
     SINGLE_DISABLE_VALUE_TYPE(switches::kDisableJavaScriptHarmonyShipping)},
    {"enable-javascript-harmony", IDS_FLAGS_JAVASCRIPT_HARMONY_NAME,
     IDS_FLAGS_JAVASCRIPT_HARMONY_DESCRIPTION, kOsAll,
     SINGLE_VALUE_TYPE(switches::kJavaScriptHarmony)},
    {"enable-asm-webassembly", IDS_FLAGS_ENABLE_ASM_WASM_NAME,
     IDS_FLAGS_ENABLE_ASM_WASM_DESCRIPTION, kOsAll,
     FEATURE_VALUE_TYPE(features::kAsmJsToWebAssembly)},
    {"enable-webassembly", IDS_FLAGS_ENABLE_WASM_NAME,
     IDS_FLAGS_ENABLE_WASM_DESCRIPTION, kOsAll,
     FEATURE_VALUE_TYPE(features::kWebAssembly)},
    {"shared-array-buffer", IDS_FLAGS_ENABLE_SHARED_ARRAY_BUFFER_NAME,
     IDS_FLAGS_ENABLE_SHARED_ARRAY_BUFFER_DESCRIPTION, kOsAll,
     FEATURE_VALUE_TYPE(features::kSharedArrayBuffer)},
    {"enable-ignition", IDS_FLAGS_V8_IGNITION_NAME,
     IDS_FLAGS_V8_IGNITION_DESCRIPTION, kOsAll,
     FEATURE_VALUE_TYPE(features::kV8Ignition)},
    {"disable-software-rasterizer", IDS_FLAGS_SOFTWARE_RASTERIZER_NAME,
     IDS_FLAGS_SOFTWARE_RASTERIZER_DESCRIPTION,
#if BUILDFLAG(ENABLE_SWIFTSHADER)
     kOsAll,
#else   // BUILDFLAG(ENABLE_SWIFTSHADER)
     0,
#endif  // BUILDFLAG(ENABLE_SWIFTSHADER)
     SINGLE_DISABLE_VALUE_TYPE(switches::kDisableSoftwareRasterizer)},
    {"enable-gpu-rasterization", IDS_FLAGS_GPU_RASTERIZATION_NAME,
     IDS_FLAGS_GPU_RASTERIZATION_DESCRIPTION, kOsAll,
     MULTI_VALUE_TYPE(kEnableGpuRasterizationChoices)},
    {"gpu-rasterization-msaa-sample-count",
     IDS_FLAGS_GPU_RASTERIZATION_MSAA_SAMPLE_COUNT_NAME,
     IDS_FLAGS_GPU_RASTERIZATION_MSAA_SAMPLE_COUNT_DESCRIPTION, kOsAll,
     MULTI_VALUE_TYPE(kGpuRasterizationMSAASampleCountChoices)},
    {"enable-experimental-web-platform-features",
     IDS_FLAGS_EXPERIMENTAL_WEB_PLATFORM_FEATURES_NAME,
     IDS_FLAGS_EXPERIMENTAL_WEB_PLATFORM_FEATURES_DESCRIPTION, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableExperimentalWebPlatformFeatures)},
#if BUILDFLAG(ENABLE_EXTENSIONS)
    {"enable-ble-advertising-in-apps",
     IDS_FLAGS_BLE_ADVERTISING_IN_EXTENSIONS_NAME,
     IDS_FLAGS_BLE_ADVERTISING_IN_EXTENSIONS_DESCRIPTION, kOsCrOS,
     SINGLE_VALUE_TYPE(extensions::switches::kEnableBLEAdvertising)},
#endif  // ENABLE_EXTENSIONS
    {"enable-devtools-experiments", IDS_FLAGS_DEVTOOLS_EXPERIMENTS_NAME,
     IDS_FLAGS_DEVTOOLS_EXPERIMENTS_DESCRIPTION, kOsDesktop,
     SINGLE_VALUE_TYPE(switches::kEnableDevToolsExperiments)},
    {"silent-debugger-extension-api",
     IDS_FLAGS_SILENT_DEBUGGER_EXTENSION_API_NAME,
     IDS_FLAGS_SILENT_DEBUGGER_EXTENSION_API_DESCRIPTION, kOsDesktop,
     SINGLE_VALUE_TYPE(switches::kSilentDebuggerExtensionAPI)},
#if BUILDFLAG(ENABLE_SPELLCHECK) && defined(OS_ANDROID)
    {"enable-android-spellchecker", IDS_OPTIONS_ENABLE_SPELLCHECK,
     IDS_OPTIONS_ENABLE_ANDROID_SPELLCHECKER_DESCRIPTION, kOsAndroid,
     FEATURE_VALUE_TYPE(spellcheck::kAndroidSpellChecker)},
#endif  // ENABLE_SPELLCHECK && OS_ANDROID
    {"enable-scroll-prediction", IDS_FLAGS_SCROLL_PREDICTION_NAME,
     IDS_FLAGS_SCROLL_PREDICTION_DESCRIPTION, kOsDesktop,
     SINGLE_VALUE_TYPE(switches::kEnableScrollPrediction)},
    {"top-chrome-md", IDS_FLAGS_TOP_CHROME_MD,
     IDS_FLAGS_TOP_CHROME_MD_DESCRIPTION, kOsDesktop,
     MULTI_VALUE_TYPE(kTopChromeMaterialDesignChoices)},
    {"enable-site-settings", IDS_FLAGS_SITE_SETTINGS,
     IDS_FLAGS_SITE_SETTINGS_DESCRIPTION, kOsDesktop,
     SINGLE_VALUE_TYPE(switches::kEnableSiteSettings)},
    {"secondary-ui-md", IDS_FLAGS_SECONDARY_UI_MD,
     IDS_FLAGS_SECONDARY_UI_MD_DESCRIPTION, kOsDesktop,
     SINGLE_VALUE_TYPE(switches::kExtendMdToSecondaryUi)},
    {"touch-events", IDS_FLAGS_TOUCH_EVENTS_NAME,
     IDS_FLAGS_TOUCH_EVENTS_DESCRIPTION, kOsDesktop,
     MULTI_VALUE_TYPE(kTouchEventsChoices)},
    {"disable-touch-adjustment", IDS_FLAGS_TOUCH_ADJUSTMENT_NAME,
     IDS_FLAGS_TOUCH_ADJUSTMENT_DESCRIPTION,
     kOsWin | kOsLinux | kOsCrOS | kOsAndroid,
     SINGLE_DISABLE_VALUE_TYPE(switches::kDisableTouchAdjustment)},
#if defined(OS_CHROMEOS)
    {"network-portal-notification", IDS_FLAGS_NETWORK_PORTAL_NOTIFICATION_NAME,
     IDS_FLAGS_NETWORK_PORTAL_NOTIFICATION_DESCRIPTION, kOsCrOS,
     ENABLE_DISABLE_VALUE_TYPE(
         chromeos::switches::kEnableNetworkPortalNotification,
         chromeos::switches::kDisableNetworkPortalNotification)},
#endif  // OS_CHROMEOS
#if defined(OS_ANDROID)
    {"enable-media-document-download-button",
     IDS_FLAGS_MEDIA_DOCUMENT_DOWNLOAD_BUTTON_NAME,
     IDS_FLAGS_MEDIA_DOCUMENT_DOWNLOAD_BUTTON_DESCRIPTION, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kMediaDocumentDownloadButton)},
#endif  // OS_ANDROID
#if BUILDFLAG(ENABLE_PLUGINS)
    {"prefer-html-over-flash", IDS_FLAGS_PREFER_HTML_OVER_PLUGINS_NAME,
     IDS_FLAGS_PREFER_HTML_OVER_PLUGINS_DESCRIPTION, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kPreferHtmlOverPlugins)},
    {"allow-nacl-socket-api", IDS_FLAGS_ALLOW_NACL_SOCKET_API_NAME,
     IDS_FLAGS_ALLOW_NACL_SOCKET_API_DESCRIPTION, kOsDesktop,
     SINGLE_VALUE_TYPE_AND_VALUE(switches::kAllowNaClSocketAPI, "*")},
    {"run-all-flash-in-allow-mode", IDS_FLAGS_RUN_ALL_FLASH_IN_ALLOW_MODE_NAME,
     IDS_FLAGS_RUN_ALL_FLASH_IN_ALLOW_MODE_DESCRIPTION, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kRunAllFlashInAllowMode)},
#endif  // ENABLE_PLUGINS
#if defined(OS_CHROMEOS)
    {"mash", IDS_FLAGS_USE_MASH_NAME, IDS_FLAGS_USE_MASH_DESCRIPTION, kOsCrOS,
     SINGLE_VALUE_TYPE("mash")},
    {"allow-touchpad-three-finger-click",
     IDS_FLAGS_ALLOW_TOUCHPAD_THREE_FINGER_CLICK_NAME,
     IDS_FLAGS_ALLOW_TOUCHPAD_THREE_FINGER_CLICK_DESCRIPTION, kOsCrOS,
     SINGLE_VALUE_TYPE(chromeos::switches::kEnableTouchpadThreeFingerClick)},
    {"ash-enable-unified-desktop", IDS_FLAGS_ASH_ENABLE_UNIFIED_DESKTOP_NAME,
     IDS_FLAGS_ASH_ENABLE_UNIFIED_DESKTOP_DESCRIPTION, kOsCrOS,
     SINGLE_VALUE_TYPE(switches::kEnableUnifiedDesktop)},
    {"enable-easy-unlock-proximity-detection",
     IDS_FLAGS_EASY_UNLOCK_PROXIMITY_DETECTION_NAME,
     IDS_FLAGS_EASY_UNLOCK_PROXIMITY_DETECTION_DESCRIPTION, kOsCrOS,
     SINGLE_VALUE_TYPE(proximity_auth::switches::kEnableProximityDetection)},
    {"enable-easy-unlock-bluetooth-low-energy-detection",
     IDS_FLAGS_EASY_UNLOCK_BLUETOOTH_LOW_ENERGY_DISCOVERY_NAME,
     IDS_FLAGS_EASY_UNLOCK_BLUETOOTH_LOW_ENERGY_DISCOVERY_DESCRIPTION, kOsCrOS,
     SINGLE_VALUE_TYPE(
         proximity_auth::switches::kEnableBluetoothLowEnergyDiscovery)},
#endif  // OS_CHROMEOS
#if defined(USE_ASH)
    {"show-touch-hud", IDS_FLAGS_SHOW_TOUCH_HUD_NAME,
     IDS_FLAGS_SHOW_TOUCH_HUD_DESCRIPTION, kOsAll,
     SINGLE_VALUE_TYPE(ash::switches::kAshTouchHud)},
    {
        "enable-pinch", IDS_FLAGS_PINCH_SCALE_NAME,
        IDS_FLAGS_PINCH_SCALE_DESCRIPTION, kOsLinux | kOsWin | kOsCrOS,
        ENABLE_DISABLE_VALUE_TYPE(switches::kEnablePinch,
                                  switches::kDisablePinch),
    },
#endif  // USE_ASH
#if defined(OS_CHROMEOS)
    {
        "disable-boot-animation", IDS_FLAGS_BOOT_ANIMATION,
        IDS_FLAGS_BOOT_ANIMATION_DESCRIPTION, kOsCrOSOwnerOnly,
        SINGLE_DISABLE_VALUE_TYPE(chromeos::switches::kDisableBootAnimation),
    },
    {"enable-video-player-chromecast-support",
     IDS_FLAGS_VIDEO_PLAYER_CHROMECAST_SUPPORT_NAME,
     IDS_FLAGS_VIDEO_PLAYER_CHROMECAST_SUPPORT_DESCRIPTION, kOsCrOS,
     SINGLE_VALUE_TYPE(
         chromeos::switches::kEnableVideoPlayerChromecastSupport)},
    {
        "disable-office-editing-component-app",
        IDS_FLAGS_OFFICE_EDITING_COMPONENT_APP_NAME,
        IDS_FLAGS_OFFICE_EDITING_COMPONENT_APP_DESCRIPTION, kOsCrOS,
        SINGLE_DISABLE_VALUE_TYPE(
            chromeos::switches::kDisableOfficeEditingComponentApp),
    },
    {
        "disable-display-color-calibration",
        IDS_FLAGS_DISPLAY_COLOR_CALIBRATION_NAME,
        IDS_FLAGS_DISPLAY_COLOR_CALIBRATION_DESCRIPTION, kOsCrOS,
        SINGLE_DISABLE_VALUE_TYPE(::switches::kDisableDisplayColorCalibration),
    },
    {
        "ash-disable-screen-orientation-lock",
        IDS_FLAGS_ASH_SCREEN_ORIENTATION_LOCK_NAME,
        IDS_FLAGS_ASH_SCREEN_ORIENTATION_LOCK_DESCRIPTION, kOsCrOS,
        SINGLE_DISABLE_VALUE_TYPE(
            ash::switches::kAshDisableScreenOrientationLock),
    },
#endif  // OS_CHROMEOS
    {
        "disable-accelerated-video-decode",
        IDS_FLAGS_ACCELERATED_VIDEO_DECODE_NAME,
        IDS_FLAGS_ACCELERATED_VIDEO_DECODE_DESCRIPTION,
        kOsMac | kOsWin | kOsCrOS,
        SINGLE_DISABLE_VALUE_TYPE(switches::kDisableAcceleratedVideoDecode),
    },
#if defined(USE_ASH)
    {
        "ash-debug-shortcuts", IDS_FLAGS_DEBUG_SHORTCUTS_NAME,
        IDS_FLAGS_DEBUG_SHORTCUTS_DESCRIPTION, kOsAll,
        SINGLE_VALUE_TYPE(ash::switches::kAshDebugShortcuts),
    },
    {
        "ash-disable-maximize-mode-window-backdrop",
        IDS_FLAGS_ASH_MAXIMIZE_MODE_WINDOW_BACKDROP_NAME,
        IDS_FLAGS_ASH_MAXIMIZE_MODE_WINDOW_BACKDROP_DESCRIPTION, kOsCrOS,
        SINGLE_DISABLE_VALUE_TYPE(
            ash::switches::kAshDisableMaximizeModeWindowBackdrop),
    },
    {
        "ash-enable-touch-view-testing",
        IDS_FLAGS_ASH_ENABLE_TOUCH_VIEW_TESTING_NAME,
        IDS_FLAGS_ASH_ENABLE_TOUCH_VIEW_TESTING_DESCRIPTION, kOsCrOS,
        SINGLE_VALUE_TYPE(ash::switches::kAshEnableTouchViewTesting),
    },
    {
        "ash-enable-mirrored-screen", IDS_FLAGS_ASH_ENABLE_MIRRORED_SCREEN_NAME,
        IDS_FLAGS_ASH_ENABLE_MIRRORED_SCREEN_DESCRIPTION, kOsCrOS,
        SINGLE_VALUE_TYPE(ash::switches::kAshEnableMirroredScreen),
    },
    {"ash-md", IDS_FLAGS_ASH_MD, IDS_FLAGS_ASH_MD_DESCRIPTION, kOsCrOS,
     MULTI_VALUE_TYPE(kAshMaterialDesignChoices)},
#endif  // USE_ASH
#if defined(OS_CHROMEOS)
    {"material-design-ink-drop-animation-speed",
     IDS_FLAGS_MATERIAL_DESIGN_INK_DROP_ANIMATION_SPEED_NAME,
     IDS_FLAGS_MATERIAL_DESIGN_INK_DROP_ANIMATION_SPEED_DESCRIPTION, kOsCrOS,
     MULTI_VALUE_TYPE(kAshMaterialDesignInkDropAnimationSpeed)},
    {"disable-cloud-import", IDS_FLAGS_CLOUD_IMPORT,
     IDS_FLAGS_CLOUD_IMPORT_DESCRIPTION, kOsCrOS,
     SINGLE_DISABLE_VALUE_TYPE(chromeos::switches::kDisableCloudImport)},
    {"enable-request-tablet-site", IDS_FLAGS_REQUEST_TABLET_SITE_NAME,
     IDS_FLAGS_REQUEST_TABLET_SITE_DESCRIPTION, kOsCrOS,
     SINGLE_VALUE_TYPE(chromeos::switches::kEnableRequestTabletSite)},
#endif  // OS_CHROMEOS
    {"debug-packed-apps", IDS_FLAGS_DEBUG_PACKED_APP_NAME,
     IDS_FLAGS_DEBUG_PACKED_APP_DESCRIPTION, kOsDesktop,
     SINGLE_VALUE_TYPE(switches::kDebugPackedApps)},
    {"enable-password-generation", IDS_FLAGS_PASSWORD_GENERATION_NAME,
     IDS_FLAGS_PASSWORD_GENERATION_DESCRIPTION, kOsAll,
     ENABLE_DISABLE_VALUE_TYPE(autofill::switches::kEnablePasswordGeneration,
                               autofill::switches::kDisablePasswordGeneration)},
    {"enable-automatic-password-saving",
     IDS_FLAGS_AUTOMATIC_PASSWORD_SAVING_NAME,
     IDS_FLAGS_AUTOMATIC_PASSWORD_SAVING_DESCRIPTION, kOsDesktop,
     FEATURE_VALUE_TYPE(
         password_manager::features::kEnableAutomaticPasswordSaving)},
    {"enable-password-force-saving", IDS_FLAGS_PASSWORD_FORCE_SAVING_NAME,
     IDS_FLAGS_PASSWORD_FORCE_SAVING_DESCRIPTION, kOsAll,
     FEATURE_VALUE_TYPE(
         password_manager::features::kEnablePasswordForceSaving)},
    {"enable-manual-password-generation",
     IDS_FLAGS_MANUAL_PASSWORD_GENERATION_NAME,
     IDS_FLAGS_MANUAL_PASSWORD_GENERATION_DESCRIPTION, kOsAll,
     FEATURE_VALUE_TYPE(
         password_manager::features::kEnableManualPasswordGeneration)},
    {"affiliation-based-matching", IDS_FLAGS_AFFILIATION_BASED_MATCHING_NAME,
     IDS_FLAGS_AFFILIATION_BASED_MATCHING_DESCRIPTION, kOsAll,
     FEATURE_VALUE_TYPE(password_manager::features::kAffiliationBasedMatching)},
    {"wallet-service-use-sandbox", IDS_FLAGS_WALLET_SERVICE_USE_SANDBOX_NAME,
     IDS_FLAGS_WALLET_SERVICE_USE_SANDBOX_DESCRIPTION, kOsAndroid | kOsDesktop,
     ENABLE_DISABLE_VALUE_TYPE_AND_VALUE(
         autofill::switches::kWalletServiceUseSandbox,
         "1",
         autofill::switches::kWalletServiceUseSandbox,
         "0")},
#if defined(USE_AURA)
    {"overscroll-history-navigation",
     IDS_FLAGS_OVERSCROLL_HISTORY_NAVIGATION_NAME,
     IDS_FLAGS_OVERSCROLL_HISTORY_NAVIGATION_DESCRIPTION, kOsAll,
     MULTI_VALUE_TYPE(kOverscrollHistoryNavigationChoices)},
#endif  // USE_AURA
    {"scroll-end-effect", IDS_FLAGS_SCROLL_END_EFFECT_NAME,
     IDS_FLAGS_SCROLL_END_EFFECT_DESCRIPTION, kOsCrOS,
     ENABLE_DISABLE_VALUE_TYPE_AND_VALUE(switches::kScrollEndEffect,
                                         "1",
                                         switches::kScrollEndEffect,
                                         "0")},
    {"enable-icon-ntp", IDS_FLAGS_ICON_NTP_NAME, IDS_FLAGS_ICON_NTP_DESCRIPTION,
     kOsAll, ENABLE_DISABLE_VALUE_TYPE(switches::kEnableIconNtp,
                                       switches::kDisableIconNtp)},
    {"enable-touch-drag-drop", IDS_FLAGS_TOUCH_DRAG_DROP_NAME,
     IDS_FLAGS_TOUCH_DRAG_DROP_DESCRIPTION, kOsWin | kOsCrOS,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableTouchDragDrop,
                               switches::kDisableTouchDragDrop)},
    {"touch-selection-strategy", IDS_FLAGS_TOUCH_SELECTION_STRATEGY_NAME,
     IDS_FLAGS_TOUCH_SELECTION_STRATEGY_DESCRIPTION,
     kOsAndroid,  // TODO(mfomitchev): Add CrOS/Win/Linux support soon.
     MULTI_VALUE_TYPE(kTouchTextSelectionStrategyChoices)},
    {"enable-navigation-tracing", IDS_FLAGS_ENABLE_NAVIGATION_TRACING,
     IDS_FLAGS_ENABLE_NAVIGATION_TRACING_DESCRIPTION, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableNavigationTracing)},
    {"enable-non-validating-reload-on-normal-reload",
     IDS_FLAGS_ENABLE_NON_VALIDATING_RELOAD_ON_NORMAL_RELOAD_NAME,
     IDS_FLAGS_ENABLE_NON_VALIDATING_RELOAD_ON_NORMAL_RELOAD_DESCRIPTION,
     kOsAll, FEATURE_VALUE_TYPE(features::kNonValidatingReloadOnNormalReload)},
    {"trace-upload-url", IDS_FLAGS_TRACE_UPLOAD_URL,
     IDS_FLAGS_TRACE_UPLOAD_URL_DESCRIPTION, kOsAll,
     MULTI_VALUE_TYPE(kTraceUploadURL)},
    {"enable-stale-while-revalidate",
     IDS_FLAGS_ENABLE_STALE_WHILE_REVALIDATE_NAME,
     IDS_FLAGS_ENABLE_STALE_WHILE_REVALIDATE_DESCRIPTION, kOsAll,
     FEATURE_VALUE_TYPE(features::kStaleWhileRevalidate)},
    {"enable-suggestions-with-substring-match",
     IDS_FLAGS_SUGGESTIONS_WITH_SUB_STRING_MATCH_NAME,
     IDS_FLAGS_SUGGESTIONS_WITH_SUB_STRING_MATCH_DESCRIPTION, kOsAll,
     SINGLE_VALUE_TYPE(
         autofill::switches::kEnableSuggestionsWithSubstringMatch)},
    {"enable-speculative-launch-service-worker",
     IDS_FLAGS_SPECULATIVE_LAUNCH_SERVICE_WORKER_NAME,
     IDS_FLAGS_SPECULATIVE_LAUNCH_SERVICE_WORKER_DESCRIPTION, kOsAll,
     FEATURE_VALUE_TYPE(features::kSpeculativeLaunchServiceWorker)},
    {"enable-supervised-user-managed-bookmarks-folder",
     IDS_FLAGS_SUPERVISED_USER_MANAGED_BOOKMARKS_FOLDER_NAME,
     IDS_FLAGS_SUPERVISED_USER_MANAGED_BOOKMARKS_FOLDER_DESCRIPTION, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableSupervisedUserManagedBookmarksFolder)},
#if BUILDFLAG(ENABLE_APP_LIST)
    {"enable-sync-app-list", IDS_FLAGS_SYNC_APP_LIST_NAME,
     IDS_FLAGS_SYNC_APP_LIST_DESCRIPTION, kOsDesktop,
     ENABLE_DISABLE_VALUE_TYPE(app_list::switches::kEnableSyncAppList,
                               app_list::switches::kDisableSyncAppList)},
#endif  // ENABLE_APP_LIST
    {"lcd-text-aa", IDS_FLAGS_LCD_TEXT_NAME, IDS_FLAGS_LCD_TEXT_DESCRIPTION,
     kOsDesktop, ENABLE_DISABLE_VALUE_TYPE(switches::kEnableLCDText,
                                           switches::kDisableLCDText)},
    {"enable-offer-store-unmasked-wallet-cards",
     IDS_FLAGS_OFFER_STORE_UNMASKED_WALLET_CARDS,
     IDS_FLAGS_OFFER_STORE_UNMASKED_WALLET_CARDS_DESCRIPTION, kOsAll,
     ENABLE_DISABLE_VALUE_TYPE(
         autofill::switches::kEnableOfferStoreUnmaskedWalletCards,
         autofill::switches::kDisableOfferStoreUnmaskedWalletCards)},
    {"enable-offline-auto-reload", IDS_FLAGS_OFFLINE_AUTO_RELOAD_NAME,
     IDS_FLAGS_OFFLINE_AUTO_RELOAD_DESCRIPTION, kOsAll,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableOfflineAutoReload,
                               switches::kDisableOfflineAutoReload)},
    {"enable-offline-auto-reload-visible-only",
     IDS_FLAGS_OFFLINE_AUTO_RELOAD_VISIBLE_ONLY_NAME,
     IDS_FLAGS_OFFLINE_AUTO_RELOAD_VISIBLE_ONLY_DESCRIPTION, kOsAll,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableOfflineAutoReloadVisibleOnly,
                               switches::kDisableOfflineAutoReloadVisibleOnly)},
    {"show-saved-copy", IDS_FLAGS_SHOW_SAVED_COPY_NAME,
     IDS_FLAGS_SHOW_SAVED_COPY_DESCRIPTION, kOsAll,
     MULTI_VALUE_TYPE(kShowSavedCopyChoices)},
    {"default-tile-width", IDS_FLAGS_DEFAULT_TILE_WIDTH_NAME,
     IDS_FLAGS_DEFAULT_TILE_WIDTH_DESCRIPTION, kOsAll,
     MULTI_VALUE_TYPE(kDefaultTileWidthChoices)},
    {"default-tile-height", IDS_FLAGS_DEFAULT_TILE_HEIGHT_NAME,
     IDS_FLAGS_DEFAULT_TILE_HEIGHT_DESCRIPTION, kOsAll,
     MULTI_VALUE_TYPE(kDefaultTileHeightChoices)},
    {"disable-gesture-requirement-for-media-playback",
     IDS_FLAGS_GESTURE_REQUIREMENT_FOR_MEDIA_PLAYBACK_NAME,
     IDS_FLAGS_GESTURE_REQUIREMENT_FOR_MEDIA_PLAYBACK_DESCRIPTION, kOsAll,
     SINGLE_DISABLE_VALUE_TYPE(
         switches::kDisableGestureRequirementForMediaPlayback)},
#if defined(OS_CHROMEOS)
    {"enable-virtual-keyboard", IDS_FLAGS_VIRTUAL_KEYBOARD_NAME,
     IDS_FLAGS_VIRTUAL_KEYBOARD_DESCRIPTION, kOsCrOS,
     SINGLE_VALUE_TYPE(keyboard::switches::kEnableVirtualKeyboard)},
    {"virtual-keyboard-overscroll", IDS_FLAGS_VIRTUAL_KEYBOARD_OVERSCROLL_NAME,
     IDS_FLAGS_VIRTUAL_KEYBOARD_OVERSCROLL_DESCRIPTION, kOsCrOS,
     SINGLE_DISABLE_VALUE_TYPE(
         keyboard::switches::kDisableVirtualKeyboardOverscroll)},
    {"input-view", IDS_FLAGS_INPUT_VIEW_NAME, IDS_FLAGS_INPUT_VIEW_DESCRIPTION,
     kOsCrOS, SINGLE_DISABLE_VALUE_TYPE(keyboard::switches::kDisableInputView)},
    {"disable-new-korean-ime", IDS_FLAGS_NEW_KOREAN_IME_NAME,
     IDS_FLAGS_NEW_KOREAN_IME_DESCRIPTION, kOsCrOS,
     SINGLE_DISABLE_VALUE_TYPE(chromeos::switches::kDisableNewKoreanIme)},
    {"enable-physical-keyboard-autocorrect",
     IDS_FLAGS_PHYSICAL_KEYBOARD_AUTOCORRECT_NAME,
     IDS_FLAGS_PHYSICAL_KEYBOARD_AUTOCORRECT_DESCRIPTION, kOsCrOS,
     ENABLE_DISABLE_VALUE_TYPE(
         chromeos::switches::kEnablePhysicalKeyboardAutocorrect,
         chromeos::switches::kDisablePhysicalKeyboardAutocorrect)},
    {"disable-voice-input", IDS_FLAGS_VOICE_INPUT_NAME,
     IDS_FLAGS_VOICE_INPUT_DESCRIPTION, kOsCrOS,
     SINGLE_DISABLE_VALUE_TYPE(keyboard::switches::kDisableVoiceInput)},
    {"experimental-input-view-features",
     IDS_FLAGS_EXPERIMENTAL_INPUT_VIEW_FEATURES_NAME,
     IDS_FLAGS_EXPERIMENTAL_INPUT_VIEW_FEATURES_DESCRIPTION, kOsCrOS,
     SINGLE_VALUE_TYPE(
         keyboard::switches::kEnableExperimentalInputViewFeatures)},
    {"floating-virtual-keyboard", IDS_FLAGS_FLOATING_VIRTUAL_KEYBOARD_NAME,
     IDS_FLAGS_FLOATING_VIRTUAL_KEYBOARD_DESCRIPTION, kOsCrOS,
     SINGLE_VALUE_TYPE(keyboard::switches::kEnableFloatingVirtualKeyboard)},
    {"smart-virtual-keyboard", IDS_FLAGS_SMART_VIRTUAL_KEYBOARD_NAME,
     IDS_FLAGS_SMART_VIRTUAL_KEYBOARD_DESCRIPTION, kOsCrOS,
     SINGLE_DISABLE_VALUE_TYPE(
         keyboard::switches::kDisableSmartVirtualKeyboard)},
    {"gesture-typing", IDS_FLAGS_GESTURE_TYPING_NAME,
     IDS_FLAGS_GESTURE_TYPING_DESCRIPTION, kOsCrOS,
     SINGLE_DISABLE_VALUE_TYPE(keyboard::switches::kDisableGestureTyping)},
    {"gesture-editing", IDS_FLAGS_GESTURE_EDITING_NAME,
     IDS_FLAGS_GESTURE_EDITING_DESCRIPTION, kOsCrOS,
     SINGLE_DISABLE_VALUE_TYPE(keyboard::switches::kDisableGestureEditing)},
    {"enable-fullscreen-app-list", IDS_FLAGS_FULLSCREEN_APP_LIST_NAME,
     IDS_FLAGS_FULLSCREEN_APP_LIST_DESCRIPTION, kOsCrOS,
     SINGLE_VALUE_TYPE(ash::switches::kAshEnableFullscreenAppList)},
#endif  // OS_CHROMEOS
    {"enable-simple-cache-backend", IDS_FLAGS_SIMPLE_CACHE_BACKEND_NAME,
     IDS_FLAGS_SIMPLE_CACHE_BACKEND_DESCRIPTION,
     kOsWin | kOsMac | kOsLinux | kOsCrOS,
     MULTI_VALUE_TYPE(kSimpleCacheBackendChoices)},
    {"enable-tcp-fast-open", IDS_FLAGS_TCP_FAST_OPEN_NAME,
     IDS_FLAGS_TCP_FAST_OPEN_DESCRIPTION, kOsLinux | kOsCrOS | kOsAndroid,
     SINGLE_VALUE_TYPE(switches::kEnableTcpFastOpen)},
#if BUILDFLAG(ENABLE_SERVICE_DISCOVERY)
    {"device-discovery-notifications",
     IDS_FLAGS_DEVICE_DISCOVERY_NOTIFICATIONS_NAME,
     IDS_FLAGS_DEVICE_DISCOVERY_NOTIFICATIONS_DESCRIPTION, kOsDesktop,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableDeviceDiscoveryNotifications,
                               switches::kDisableDeviceDiscoveryNotifications)},
    {"enable-print-preview-register-promos",
     IDS_FLAGS_PRINT_PREVIEW_REGISTER_PROMOS_NAME,
     IDS_FLAGS_PRINT_PREVIEW_REGISTER_PROMOS_DESCRIPTION, kOsDesktop,
     SINGLE_VALUE_TYPE(switches::kEnablePrintPreviewRegisterPromos)},
#endif  // BUILDFLAG(ENABLE_SERVICE_DISCOVERY)
#if defined(OS_WIN)
    {"enable-cloud-print-xps", IDS_FLAGS_CLOUD_PRINT_XPS_NAME,
     IDS_FLAGS_CLOUD_PRINT_XPS_DESCRIPTION, kOsWin,
     SINGLE_VALUE_TYPE(switches::kEnableCloudPrintXps)},
#endif  // OS_WIN
#if defined(TOOLKIT_VIEWS)
    {"disable-hide-inactive-stacked-tab-close-buttons",
     IDS_FLAGS_HIDE_INACTIVE_STACKED_TAB_CLOSE_BUTTONS_NAME,
     IDS_FLAGS_HIDE_INACTIVE_STACKED_TAB_CLOSE_BUTTONS_DESCRIPTION,
     kOsCrOS | kOsWin | kOsLinux,
     SINGLE_DISABLE_VALUE_TYPE(
         switches::kDisableHideInactiveStackedTabCloseButtons)},
#endif  // TOOLKIT_VIEWS
#if BUILDFLAG(ENABLE_SPELLCHECK)
    {"enable-spelling-feedback-field-trial",
     IDS_FLAGS_SPELLING_FEEDBACK_FIELD_TRIAL_NAME,
     IDS_FLAGS_SPELLING_FEEDBACK_FIELD_TRIAL_DESCRIPTION, kOsAll,
     SINGLE_VALUE_TYPE(
         spellcheck::switches::kEnableSpellingFeedbackFieldTrial)},
#endif  // ENABLE_SPELLCHECK
    {"enable-webgl-draft-extensions", IDS_FLAGS_WEBGL_DRAFT_EXTENSIONS_NAME,
     IDS_FLAGS_WEBGL_DRAFT_EXTENSIONS_DESCRIPTION, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableWebGLDraftExtensions)},
    {"enable-new-profile-management", IDS_FLAGS_NEW_PROFILE_MANAGEMENT_NAME,
     IDS_FLAGS_NEW_PROFILE_MANAGEMENT_DESCRIPTION, kOsAll,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableNewProfileManagement,
                               switches::kDisableNewProfileManagement)},
    {"enable-account-consistency", IDS_FLAGS_ACCOUNT_CONSISTENCY_NAME,
     IDS_FLAGS_ACCOUNT_CONSISTENCY_DESCRIPTION, kOsAll,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableAccountConsistency,
                               switches::kDisableAccountConsistency)},
    {"enable-password-separated-signin-flow",
     IDS_FLAGS_ENABLE_PASSWORD_SEPARATED_SIGNIN_FLOW_NAME,
     IDS_FLAGS_ENABLE_PASSWORD_SEPARATED_SIGNIN_FLOW_DESCRIPTION,
     kOsMac | kOsWin | kOsLinux,
     FEATURE_VALUE_TYPE(switches::kUsePasswordSeparatedSigninFlow)},
    {"show-material-design-user-menu",
     IDS_FLAGS_SHOW_MATERIAL_DESIGN_USER_MENU_NAME,
     IDS_FLAGS_SHOW_MATERIAL_DESIGN_USER_MENU_DESCRIPTION,
     kOsMac | kOsWin | kOsLinux,
     FEATURE_VALUE_TYPE(switches::kMaterialDesignUserMenu)},
    {"enable-google-profile-info", IDS_FLAGS_GOOGLE_PROFILE_INFO_NAME,
     IDS_FLAGS_GOOGLE_PROFILE_INFO_DESCRIPTION, kOsMac | kOsWin | kOsLinux,
     SINGLE_VALUE_TYPE(switches::kGoogleProfileInfo)},
#if BUILDFLAG(ENABLE_APP_LIST)
    {"reset-app-list-install-state",
     IDS_FLAGS_RESET_APP_LIST_INSTALL_STATE_NAME,
     IDS_FLAGS_RESET_APP_LIST_INSTALL_STATE_DESCRIPTION,
     kOsMac | kOsWin | kOsLinux,
     SINGLE_VALUE_TYPE(app_list::switches::kResetAppListInstallState)},
#endif  // BUILDFLAG(ENABLE_APP_LIST)
#if defined(OS_ANDROID)
    {"enable-downloads-ui", IDS_FLAGS_ENABLE_DOWNLOADS_UI_NAME,
     IDS_FLAGS_ENABLE_DOWNLOADS_UI_DESCRIPTION, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kDownloadsUiFeature)},
    {"enable-special-locale", IDS_FLAGS_ENABLE_SPECIAL_LOCALE_NAME,
     IDS_FLAGS_ENABLE_SPECIAL_LOCALE_DESCRIPTION, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kSpecialLocaleFeature)},
    {"enable-accessibility-tab-switcher",
     IDS_FLAGS_ACCESSIBILITY_TAB_SWITCHER_NAME,
     IDS_FLAGS_ACCESSIBILITY_TAB_SWITCHER_DESCRIPTION, kOsAndroid,
     SINGLE_VALUE_TYPE(switches::kEnableAccessibilityTabSwitcher)},
    {"enable-physical-web", IDS_FLAGS_ENABLE_PHYSICAL_WEB_NAME,
     IDS_FLAGS_ENABLE_PHYSICAL_WEB_DESCRIPTION, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kPhysicalWebFeature)},
#endif  // OS_ANDROID
    {"enable-zero-copy", IDS_FLAGS_ZERO_COPY_NAME,
     IDS_FLAGS_ZERO_COPY_DESCRIPTION, kOsAll,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableZeroCopy,
                               switches::kDisableZeroCopy)},
#if defined(OS_CHROMEOS)
    {"enable-first-run-ui-transitions", IDS_FLAGS_FIRST_RUN_UI_TRANSITIONS_NAME,
     IDS_FLAGS_FIRST_RUN_UI_TRANSITIONS_DESCRIPTION, kOsCrOS,
     SINGLE_VALUE_TYPE(chromeos::switches::kEnableFirstRunUITransitions)},
#endif  // OS_CHROMEOS
    {"disable-new-bookmark-apps", IDS_FLAGS_NEW_BOOKMARK_APPS_NAME,
     IDS_FLAGS_NEW_BOOKMARK_APPS_DESCRIPTION,
     kOsWin | kOsCrOS | kOsLinux | kOsMac,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableNewBookmarkApps,
                               switches::kDisableNewBookmarkApps)},
#if defined(OS_MACOSX)
    {"disable-hosted-apps-in-windows", IDS_FLAGS_HOSTED_APPS_IN_WINDOWS_NAME,
     IDS_FLAGS_HOSTED_APPS_IN_WINDOWS_DESCRIPTION, kOsMac,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableHostedAppsInWindows,
                               switches::kDisableHostedAppsInWindows)},
    {"disable-hosted-app-shim-creation",
     IDS_FLAGS_HOSTED_APP_SHIM_CREATION_NAME,
     IDS_FLAGS_HOSTED_APP_SHIM_CREATION_DESCRIPTION, kOsMac,
     SINGLE_DISABLE_VALUE_TYPE(switches::kDisableHostedAppShimCreation)},
    {"enable-hosted-app-quit-notification",
     IDS_FLAGS_HOSTED_APP_QUIT_NOTIFICATION_NAME,
     IDS_FLAGS_HOSTED_APP_QUIT_NOTIFICATION_DESCRIPTION, kOsMac,
     SINGLE_VALUE_TYPE(switches::kHostedAppQuitNotification)},
#endif  // OS_MACOSX
#if defined(OS_ANDROID)
    {"disable-pull-to-refresh-effect", IDS_FLAGS_PULL_TO_REFRESH_EFFECT_NAME,
     IDS_FLAGS_PULL_TO_REFRESH_EFFECT_DESCRIPTION, kOsAndroid,
     SINGLE_DISABLE_VALUE_TYPE(switches::kDisablePullToRefreshEffect)},
#endif  // OS_ANDROID
#if defined(OS_MACOSX)
    {"enable-translate-new-ux", IDS_FLAGS_TRANSLATE_NEW_UX_NAME,
     IDS_FLAGS_TRANSLATE_NEW_UX_DESCRIPTION, kOsMac,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableTranslateNewUX,
                               switches::kDisableTranslateNewUX)},
#endif  // OS_MACOSX
#if defined(OS_LINUX) || defined(OS_WIN) || defined(OS_CHROMEOS)
    {"translate-2016q2-ui", IDS_FLAGS_TRANSLATE_2016Q2_UI_NAME,
     IDS_FLAGS_TRANSLATE_2016Q2_UI_DESCRIPTION, kOsCrOS | kOsWin | kOsLinux,
     FEATURE_VALUE_TYPE(translate::kTranslateUI2016Q2)},
#endif  // OS_LINUX || OS_WIN || OS_CHROMEOS
    {"translate-lang-by-ulp", IDS_FLAGS_TRANSLATE_LANGUAGE_BY_ULP_NAME,
     IDS_FLAGS_TRANSLATE_LANGUAGE_BY_ULP_DESCRIPTION, kOsAll,
     FEATURE_VALUE_TYPE(translate::kTranslateLanguageByULP)},
#if defined(OS_MACOSX)
    {"enable-native-notifications", IDS_NOTIFICATIONS_NATIVE_FLAG,
     IDS_NOTIFICATIONS_NATIVE_FLAG_DESCRIPTION, kOsMac,
     FEATURE_VALUE_TYPE(features::kNativeNotifications)},
#endif  // OS_MACOSX
#if defined(TOOLKIT_VIEWS)
    {"disable-views-rect-based-targeting",
     IDS_FLAGS_VIEWS_RECT_BASED_TARGETING_NAME,
     IDS_FLAGS_VIEWS_RECT_BASED_TARGETING_DESCRIPTION,
     kOsCrOS | kOsWin | kOsLinux,
     SINGLE_DISABLE_VALUE_TYPE(
         views::switches::kDisableViewsRectBasedTargeting)},
#endif  // TOOLKIT_VIEWS
#if BUILDFLAG(ENABLE_EXTENSIONS)
    {"enable-apps-show-on-first-paint", IDS_FLAGS_APPS_SHOW_ON_FIRST_PAINT_NAME,
     IDS_FLAGS_APPS_SHOW_ON_FIRST_PAINT_DESCRIPTION, kOsDesktop,
     SINGLE_VALUE_TYPE(extensions::switches::kEnableAppsShowOnFirstPaint)},
#endif  // ENABLE_EXTENSIONS
#if defined(OS_ANDROID)
    {"reader-mode-heuristics", IDS_FLAGS_READER_MODE_HEURISTICS_NAME,
     IDS_FLAGS_READER_MODE_HEURISTICS_DESCRIPTION, kOsAndroid,
     MULTI_VALUE_TYPE(kReaderModeHeuristicsChoices)},
#endif  // OS_ANDROID
#if defined(OS_ANDROID)
    {"enable-chrome-home", IDS_FLAGS_CHROME_HOME_NAME,
     IDS_FLAGS_CHROME_HOME_DESCRIPTION, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kChromeHomeFeature)},
#endif  // OS_ANDROID
    {"num-raster-threads", IDS_FLAGS_NUM_RASTER_THREADS_NAME,
     IDS_FLAGS_NUM_RASTER_THREADS_DESCRIPTION, kOsAll,
     MULTI_VALUE_TYPE(kNumRasterThreadsChoices)},
    {"enable-permission-action-reporting",
     IDS_FLAGS_PERMISSION_ACTION_REPORTING_NAME,
     IDS_FLAGS_PERMISSION_ACTION_REPORTING_DESCRIPTION, kOsAll,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnablePermissionActionReporting,
                               switches::kDisablePermissionActionReporting)},
    {"enable-permissions-blacklist", IDS_FLAGS_PERMISSIONS_BLACKLIST_NAME,
     IDS_FLAGS_PERMISSIONS_BLACKLIST_DESCRIPTION, kOsAll,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnablePermissionsBlacklist,
                               switches::kDisablePermissionsBlacklist)},
    {"enable-single-click-autofill", IDS_FLAGS_SINGLE_CLICK_AUTOFILL_NAME,
     IDS_FLAGS_SINGLE_CLICK_AUTOFILL_DESCRIPTION, kOsAll,
     ENABLE_DISABLE_VALUE_TYPE(
         autofill::switches::kEnableSingleClickAutofill,
         autofill::switches::kDisableSingleClickAutofill)},
    {"disable-cast-streaming-hw-encoding",
     IDS_FLAGS_CAST_STREAMING_HW_ENCODING_NAME,
     IDS_FLAGS_CAST_STREAMING_HW_ENCODING_DESCRIPTION, kOsAll,
     SINGLE_DISABLE_VALUE_TYPE(switches::kDisableCastStreamingHWEncoding)},
    {"disable-threaded-scrolling", IDS_FLAGS_THREADED_SCROLLING_NAME,
     IDS_FLAGS_THREADED_SCROLLING_DESCRIPTION, kOsAll,
     SINGLE_DISABLE_VALUE_TYPE(switches::kDisableThreadedScrolling)},
    {"enable-settings-window", IDS_FLAGS_SETTINGS_WINDOW_NAME,
     IDS_FLAGS_SETTINGS_WINDOW_DESCRIPTION, kOsDesktop,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableSettingsWindow,
                               switches::kDisableSettingsWindow)},
    {"inert-visual-viewport", IDS_FLAGS_INERT_VISUAL_VIEWPORT_NAME,
     IDS_FLAGS_INERT_VISUAL_VIEWPORT_DESCRIPTION, kOsAll,
     SINGLE_VALUE_TYPE(switches::kInertVisualViewport)},
    {"enable-apps-file-associations", IDS_FLAGS_APPS_FILE_ASSOCIATIONS_NAME,
     IDS_FLAGS_APPS_FILE_ASSOCIATIONS_DESCRIPTION, kOsMac,
     SINGLE_VALUE_TYPE(switches::kEnableAppsFileAssociations)},
    {"distance-field-text", IDS_FLAGS_DISTANCE_FIELD_TEXT_NAME,
     IDS_FLAGS_DISTANCE_FIELD_TEXT_DESCRIPTION, kOsAll,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableDistanceFieldText,
                               switches::kDisableDistanceFieldText)},
    {"extension-content-verification",
     IDS_FLAGS_EXTENSION_CONTENT_VERIFICATION_NAME,
     IDS_FLAGS_EXTENSION_CONTENT_VERIFICATION_DESCRIPTION, kOsDesktop,
     MULTI_VALUE_TYPE(kExtensionContentVerificationChoices)},
#if BUILDFLAG(ENABLE_EXTENSIONS)
    {"extension-active-script-permission",
     IDS_FLAGS_USER_CONSENT_FOR_EXTENSION_SCRIPTS_NAME,
     IDS_FLAGS_USER_CONSENT_FOR_EXTENSION_SCRIPTS_DESCRIPTION, kOsAll,
     SINGLE_VALUE_TYPE(extensions::switches::kEnableScriptsRequireAction)},
#endif  // ENABLE_EXTENSIONS
#if defined(OS_ANDROID)
    {"enable-data-reduction-proxy-carrier-test",
     IDS_FLAGS_DATA_REDUCTION_PROXY_CARRIER_TEST_NAME,
     IDS_FLAGS_DATA_REDUCTION_PROXY_CARRIER_TEST_DESCRIPTION, kOsAndroid,
     SINGLE_VALUE_TYPE(
         data_reduction_proxy::switches::kEnableDataReductionProxyCarrierTest)},
#endif  // OS_ANDROID
    {"enable-hotword-hardware", IDS_FLAGS_EXPERIMENTAL_HOTWORD_HARDWARE_NAME,
     IDS_FLAGS_EXPERIMENTAL_HOTWORD_HARDWARE_DESCRIPTION, kOsCrOS,
     SINGLE_VALUE_TYPE(switches::kEnableExperimentalHotwordHardware)},
#if BUILDFLAG(ENABLE_EXTENSIONS)
    {"enable-embedded-extension-options",
     IDS_FLAGS_EMBEDDED_EXTENSION_OPTIONS_NAME,
     IDS_FLAGS_EMBEDDED_EXTENSION_OPTIONS_DESCRIPTION, kOsDesktop,
     SINGLE_VALUE_TYPE(extensions::switches::kEnableEmbeddedExtensionOptions)},
#endif  // ENABLE_EXTENSIONS
#if defined(USE_ASH)
    {"enable-web-app-frame", IDS_FLAGS_WEB_APP_FRAME_NAME,
     IDS_FLAGS_WEB_APP_FRAME_DESCRIPTION, kOsWin | kOsCrOS,
     SINGLE_VALUE_TYPE(switches::kEnableWebAppFrame)},
#endif  // USE_ASH
    {"drop-sync-credential", IDS_FLAGS_DROP_SYNC_CREDENTIAL_NAME,
     IDS_FLAGS_DROP_SYNC_CREDENTIAL_DESCRIPTION, kOsAll,
     FEATURE_VALUE_TYPE(password_manager::features::kDropSyncCredential)},
#if BUILDFLAG(ENABLE_EXTENSIONS)
    {"enable-extension-action-redesign",
     IDS_FLAGS_EXTENSION_ACTION_REDESIGN_NAME,
     IDS_FLAGS_EXTENSION_ACTION_REDESIGN_DESCRIPTION, kOsDesktop,
     SINGLE_VALUE_TYPE(extensions::switches::kEnableExtensionActionRedesign)},
#endif  // ENABLE_EXTENSIONS
#if !defined(OS_ANDROID)
    {"enable-message-center-always-scroll-up-upon-notification-removal",
     IDS_FLAGS_MESSAGE_CENTER_ALWAYS_SCROLL_UP_UPON_REMOVAL_NAME,
     IDS_FLAGS_MESSAGE_CENTER_ALWAYS_SCROLL_UP_UPON_REMOVAL_DESCRIPTION,
     kOsDesktop,
     SINGLE_VALUE_TYPE(
         switches::kEnableMessageCenterAlwaysScrollUpUponNotificationRemoval)},
#endif  // !OS_ANDROID
    {"enable-md-policy-page", IDS_FLAGS_ENABLE_MATERIAL_DESIGN_POLICY_PAGE_NAME,
     IDS_FLAGS_ENABLE_MATERIAL_DESIGN_POLICY_PAGE_DESCRIPTION, kOsDesktop,
     SINGLE_VALUE_TYPE(switches::kEnableMaterialDesignPolicyPage)},
#if defined(OS_CHROMEOS)
    {"memory-pressure-thresholds", IDS_FLAGS_MEMORY_PRESSURE_THRESHOLD_NAME,
     IDS_FLAGS_MEMORY_PRESSURE_THRESHOLD_DESCRIPTION, kOsCrOS,
     MULTI_VALUE_TYPE(kMemoryPressureThresholdChoices)},
    {"wake-on-wifi-packet", IDS_FLAGS_WAKE_ON_PACKETS_NAME,
     IDS_FLAGS_WAKE_ON_PACKETS_DESCRIPTION, kOsCrOSOwnerOnly,
     SINGLE_VALUE_TYPE(chromeos::switches::kWakeOnWifiPacket)},
#endif  // OS_CHROMEOS
    {"enable-memory-coordinator", IDS_FLAGS_MEMORY_COORDINATOR_NAME,
     IDS_FLAGS_MEMORY_COORDINATOR_DESCRIPTION,
     kOsAndroid | kOsCrOS | kOsLinux | kOsWin,
     FEATURE_VALUE_TYPE(features::kMemoryCoordinator)},
    {"enable-tab-audio-muting", IDS_FLAGS_TAB_AUDIO_MUTING_NAME,
     IDS_FLAGS_TAB_AUDIO_MUTING_DESCRIPTION, kOsDesktop,
     SINGLE_VALUE_TYPE(switches::kEnableTabAudioMuting)},
    {"enable-credential-manager-api", IDS_FLAGS_CREDENTIAL_MANAGER_API_NAME,
     IDS_FLAGS_CREDENTIAL_MANAGER_API_DESCRIPTION, kOsAll,
     FEATURE_VALUE_TYPE(features::kCredentialManagementAPI)},
    {"reduced-referrer-granularity",
     IDS_FLAGS_REDUCED_REFERRER_GRANULARITY_NAME,
     IDS_FLAGS_REDUCED_REFERRER_GRANULARITY_DESCRIPTION, kOsAll,
     SINGLE_VALUE_TYPE(switches::kReducedReferrerGranularity)},
#if defined(OS_CHROMEOS)
    {"disable-new-zip-unpacker", IDS_FLAGS_NEW_ZIP_UNPACKER_NAME,
     IDS_FLAGS_NEW_ZIP_UNPACKER_DESCRIPTION, kOsCrOS,
     SINGLE_DISABLE_VALUE_TYPE(chromeos::switches::kDisableNewZIPUnpacker)},
#endif  // OS_CHROMEOS
#if defined(OS_ANDROID)
    {"enable-credit-card-assist", IDS_FLAGS_CREDIT_CARD_ASSIST_NAME,
     IDS_FLAGS_CREDIT_CARD_ASSIST_DESCRIPTION, kOsAndroid,
     FEATURE_VALUE_TYPE(autofill::kAutofillCreditCardAssist)},
#endif  // OS_ANDROID
#if defined(OS_CHROMEOS)
    {"disable-captive-portal-bypass-proxy",
     IDS_FLAGS_CAPTIVE_PORTAL_BYPASS_PROXY_NAME,
     IDS_FLAGS_CAPTIVE_PORTAL_BYPASS_PROXY_DESCRIPTION, kOsCrOS,
     SINGLE_DISABLE_VALUE_TYPE(
         chromeos::switches::kDisableCaptivePortalBypassProxy)},
#endif  // OS_CHROMEOS
#if defined(OS_ANDROID)
    {"enable-seccomp-sandbox-android",
     IDS_FLAGS_SECCOMP_FILTER_SANDBOX_ANDROID_NAME,
     IDS_FLAGS_SECCOMP_FILTER_SANDBOX_ANDROID_DESCRIPTION, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kSeccompSandboxAndroid)},
#endif  // OS_ANDROID
    {"enable-touch-hover", IDS_FLAGS_TOUCH_HOVER_NAME,
     IDS_FLAGS_TOUCH_HOVER_DESCRIPTION, kOsAndroid,
     SINGLE_VALUE_TYPE("enable-touch-hover")},
#if defined(OS_CHROMEOS)
    {"enable-wifi-credential-sync", IDS_FLAGS_WIFI_CREDENTIAL_SYNC_NAME,
     IDS_FLAGS_WIFI_CREDENTIAL_SYNC_DESCRIPTION, kOsCrOS,
     SINGLE_VALUE_TYPE(switches::kEnableWifiCredentialSync)},
    {"enable-potentially-annoying-security-features",
     IDS_FLAGS_EXPERIMENTAL_SECURITY_FEATURES_NAME,
     IDS_FLAGS_EXPERIMENTAL_SECURITY_FEATURES_DESCRIPTION, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnablePotentiallyAnnoyingSecurityFeatures)},
#endif  // OS_CHROMEOS
    {"mark-non-secure-as", IDS_MARK_HTTP_AS_NAME, IDS_MARK_HTTP_AS_DESCRIPTION,
     kOsAll, MULTI_VALUE_TYPE(kMarkHttpAsChoices)},
    {"enable-site-per-process", IDS_FLAGS_SITE_PER_PROCESS_NAME,
     IDS_FLAGS_SITE_PER_PROCESS_DESCRIPTION, kOsAll,
     SINGLE_VALUE_TYPE(switches::kSitePerProcess)},
    {"enable-top-document-isolation", IDS_FLAGS_TOP_DOCUMENT_ISOLATION_NAME,
     IDS_FLAGS_TOP_DOCUMENT_ISOLATION_DESCRIPTION, kOsAll,
     SINGLE_VALUE_TYPE(switches::kTopDocumentIsolation)},
    {"enable-use-zoom-for-dsf", IDS_FLAGS_ENABLE_USE_ZOOM_FOR_DSF_NAME,
     IDS_FLAGS_ENABLE_USE_ZOOM_FOR_DSF_DESCRIPTION, kOsAll,
     MULTI_VALUE_TYPE(kEnableUseZoomForDSFChoices)},
#if defined(OS_MACOSX)
    {"enable-harfbuzz-rendertext", IDS_FLAGS_HARFBUZZ_RENDERTEXT_NAME,
     IDS_FLAGS_HARFBUZZ_RENDERTEXT_DESCRIPTION, kOsMac,
     SINGLE_VALUE_TYPE(switches::kEnableHarfBuzzRenderText)},
#endif  // OS_MACOSX
    {"data-reduction-proxy-lo-fi", IDS_FLAGS_DATA_REDUCTION_PROXY_LO_FI_NAME,
     IDS_FLAGS_DATA_REDUCTION_PROXY_LO_FI_DESCRIPTION, kOsAll,
     MULTI_VALUE_TYPE(kDataReductionProxyLoFiChoices)},
    {"enable-data-reduction-proxy-lite-page",
     IDS_FLAGS_ENABLE_DATA_REDUCTION_PROXY_LITE_PAGE_NAME,
     IDS_FLAGS_ENABLE_DATA_REDUCTION_PROXY_LITE_PAGE_DESCRIPTION, kOsAll,
     SINGLE_VALUE_TYPE(
         data_reduction_proxy::switches::kEnableDataReductionProxyLitePage)},
    {"clear-data-reduction-proxy-data-savings",
     IDS_FLAGS_DATA_REDUCTION_PROXY_RESET_SAVINGS_NAME,
     IDS_FLAGS_DATA_REDUCTION_PROXY_RESET_SAVINGS_DESCRIPTION, kOsAll,
     SINGLE_VALUE_TYPE(
         data_reduction_proxy::switches::kClearDataReductionProxyDataSavings)},
#if defined(OS_ANDROID)
    {"enable-data-reduction-proxy-savings-promo",
     IDS_FLAGS_ENABLE_DATA_REDUCTION_PROXY_SAVINGS_PROMO_NAME,
     IDS_FLAGS_ENABLE_DATA_REDUCTION_PROXY_SAVINGS_PROMO_DESCRIPTION,
     kOsAndroid, SINGLE_VALUE_TYPE(data_reduction_proxy::switches::
                                       kEnableDataReductionProxySavingsPromo)},
#endif  // OS_ANDROID
    {"allow-insecure-localhost", IDS_ALLOW_INSECURE_LOCALHOST,
     IDS_ALLOW_INSECURE_LOCALHOST_DESCRIPTION, kOsAll,
     SINGLE_VALUE_TYPE(switches::kAllowInsecureLocalhost)},
    {"enable-add-to-shelf", IDS_FLAGS_ADD_TO_SHELF_NAME,
     IDS_FLAGS_ADD_TO_SHELF_DESCRIPTION, kOsDesktop,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableAddToShelf,
                               switches::kDisableAddToShelf)},
    {"bypass-app-banner-engagement-checks",
     IDS_FLAGS_BYPASS_APP_BANNER_ENGAGEMENT_CHECKS_NAME,
     IDS_FLAGS_BYPASS_APP_BANNER_ENGAGEMENT_CHECKS_DESCRIPTION, kOsAll,
     SINGLE_VALUE_TYPE(switches::kBypassAppBannerEngagementChecks)},
    {"use-sync-sandbox", IDS_FLAGS_SYNC_SANDBOX_NAME,
     IDS_FLAGS_SYNC_SANDBOX_DESCRIPTION, kOsAll,
     SINGLE_VALUE_TYPE_AND_VALUE(
         switches::kSyncServiceURL,
         "https://chrome-sync.sandbox.google.com/chrome-sync/alpha")},
    {"v8-pac-mojo-out-of-process", IDS_FLAGS_V8_PAC_MOJO_OUT_OF_PROCESS_NAME,
     IDS_FLAGS_V8_PAC_MOJO_OUT_OF_PROCESS_DESCRIPTION, kOsDesktop,
     ENABLE_DISABLE_VALUE_TYPE(switches::kV8PacMojoOutOfProcess,
                               switches::kDisableOutOfProcessPac)},
#if defined(ENABLE_MEDIA_ROUTER) && !defined(OS_ANDROID)
    {"load-media-router-component-extension",
     IDS_FLAGS_LOAD_MEDIA_ROUTER_COMPONENT_EXTENSION_NAME,
     IDS_FLAGS_LOAD_MEDIA_ROUTER_COMPONENT_EXTENSION_DESCRIPTION, kOsDesktop,
     ENABLE_DISABLE_VALUE_TYPE_AND_VALUE(
         switches::kLoadMediaRouterComponentExtension,
         "1",
         switches::kLoadMediaRouterComponentExtension,
         "0")},
#endif  // ENABLE_MEDIA_ROUTER && !OS_ANDROID
// Since Drive Search is not available when app list is disabled, flag guard
// enable-drive-search-in-chrome-launcher flag.
#if BUILDFLAG(ENABLE_APP_LIST)
    {"enable-drive-search-in-app-launcher",
     IDS_FLAGS_DRIVE_SEARCH_IN_CHROME_LAUNCHER,
     IDS_FLAGS_DRIVE_SEARCH_IN_CHROME_LAUNCHER_DESCRIPTION, kOsCrOS,
     ENABLE_DISABLE_VALUE_TYPE(
         app_list::switches::kEnableDriveSearchInChromeLauncher,
         app_list::switches::kDisableDriveSearchInChromeLauncher)},
#endif  // BUILDFLAG(ENABLE_APP_LIST)
#if defined(OS_CHROMEOS)
    {"disable-mtp-write-support", IDS_FLAGS_MTP_WRITE_SUPPORT_NAME,
     IDS_FLAGS_MTP_WRITE_SUPPORT_DESCRIPTION, kOsCrOS,
     SINGLE_DISABLE_VALUE_TYPE(chromeos::switches::kDisableMtpWriteSupport)},
#endif  // OS_CHROMEOS
#if defined(OS_CHROMEOS)
    {"enable-datasaver-prompt", IDS_FLAGS_DATASAVER_PROMPT_NAME,
     IDS_FLAGS_DATASAVER_PROMPT_DESCRIPTION, kOsCrOS,
     MULTI_VALUE_TYPE(kDataSaverPromptChoices)},
#endif  // OS_CHROMEOS
    {"supervised-user-safesites", IDS_FLAGS_SUPERVISED_USER_SAFESITES_NAME,
     IDS_FLAGS_SUPERVISED_USER_SAFESITES_DESCRIPTION, kOsAll,
     MULTI_VALUE_TYPE(kSupervisedUserSafeSitesChoices)},
#if defined(OS_ANDROID)
    {"enable-autofill-keyboard-accessory-view",
     IDS_FLAGS_AUTOFILL_ACCESSORY_VIEW_NAME,
     IDS_FLAGS_AUTOFILL_ACCESSORY_VIEW_DESCRIPTION, kOsAndroid,
     ENABLE_DISABLE_VALUE_TYPE(
         autofill::switches::kEnableAccessorySuggestionView,
         autofill::switches::kDisableAccessorySuggestionView)},
#endif  // OS_ANDROID
#if defined(OS_WIN)
    {"try-supported-channel-layouts",
     IDS_FLAGS_TRY_SUPPORTED_CHANNEL_LAYOUTS_NAME,
     IDS_FLAGS_TRY_SUPPORTED_CHANNEL_LAYOUTS_DESCRIPTION, kOsWin,
     SINGLE_VALUE_TYPE(switches::kTrySupportedChannelLayouts)},
#endif  // OS_WIN
#if defined(OS_MACOSX)
    {"app-info-dialog", IDS_FLAGS_APP_INFO_DIALOG_NAME,
     IDS_FLAGS_APP_INFO_DIALOG_DESCRIPTION, kOsMac,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableAppInfoDialogMac,
                               switches::kDisableAppInfoDialogMac)},
    {"mac-views-native-app-windows",
     IDS_FLAGS_MAC_VIEWS_NATIVE_APP_WINDOWS_NAME,
     IDS_FLAGS_MAC_VIEWS_NATIVE_APP_WINDOWS_DESCRIPTION, kOsMac,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableMacViewsNativeAppWindows,
                               switches::kDisableMacViewsNativeAppWindows)},
    {"app-window-cycling", IDS_FLAGS_APP_WINDOW_CYCLING_NAME,
     IDS_FLAGS_APP_WINDOW_CYCLING_DESCRIPTION, kOsMac,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableAppWindowCycling,
                               switches::kDisableAppWindowCycling)},
    {"mac-views-native-dialogs", IDS_FLAGS_MAC_VIEWS_NATIVE_DIALOGS_NAME,
     IDS_FLAGS_MAC_VIEWS_NATIVE_DIALOGS_DESCRIPTION, kOsMac,
     FEATURE_VALUE_TYPE(chrome::kMacViewsNativeDialogs)},
    {"mac-views-webui-dialogs", IDS_FLAGS_MAC_VIEWS_WEBUI_DIALOGS_NAME,
     IDS_FLAGS_MAC_VIEWS_WEBUI_DIALOGS_DESCRIPTION, kOsMac,
     FEATURE_VALUE_TYPE(chrome::kMacViewsWebUIDialogs)},
#endif  // OS_MACOSX
#if defined(ENABLE_WEBVR)
    {"enable-webvr", IDS_FLAGS_WEBVR_NAME, IDS_FLAGS_WEBVR_DESCRIPTION, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableWebVR)},
#endif  // ENABLE_WEBVR
#if defined(ENABLE_VR_SHELL)
    {"enable-vr-shell", IDS_FLAGS_ENABLE_VR_SHELL_NAME,
     IDS_FLAGS_ENABLE_VR_SHELL_DESCRIPTION, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kVrShell)},
#endif  // ENABLE_VR_SHELL
#if defined(OS_CHROMEOS)
    {"disable-accelerated-mjpeg-decode",
     IDS_FLAGS_ACCELERATED_MJPEG_DECODE_NAME,
     IDS_FLAGS_ACCELERATED_MJPEG_DECODE_DESCRIPTION, kOsCrOS,
     SINGLE_DISABLE_VALUE_TYPE(switches::kDisableAcceleratedMjpegDecode)},
#endif  // OS_CHROMEOS
    {"v8-cache-options", IDS_FLAGS_V8_CACHE_OPTIONS_NAME,
     IDS_FLAGS_V8_CACHE_OPTIONS_DESCRIPTION, kOsAll,
     MULTI_VALUE_TYPE(kV8CacheOptionsChoices)},
    {"v8-cache-strategies-for-cache-storage",
     IDS_FLAGS_V8_CACHE_STRATEGIES_FOR_CACHE_STORAGE_NAME,
     IDS_FLAGS_V8_CACHE_STRATEGIES_FOR_CACHE_STORAGE_DESCRIPTION, kOsAll,
     MULTI_VALUE_TYPE(kV8CacheStrategiesForCacheStorageChoices)},
    {"enable-clear-browsing-data-counters",
     IDS_FLAGS_ENABLE_CLEAR_BROWSING_DATA_COUNTERS_NAME,
     IDS_FLAGS_ENABLE_CLEAR_BROWSING_DATA_COUNTERS_DESCRIPTION, kOsAll,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableClearBrowsingDataCounters,
                               switches::kDisableClearBrowsingDataCounters)},
    {"simplified-fullscreen-ui", IDS_FLAGS_SIMPLIFIED_FULLSCREEN_UI_NAME,
     IDS_FLAGS_SIMPLIFIED_FULLSCREEN_UI_DESCRIPTION,
     kOsWin | kOsLinux | kOsCrOS,
     FEATURE_VALUE_TYPE(features::kSimplifiedFullscreenUI)},
    {"experimental-keyboard-lock-ui",
     IDS_FLAGS_EXPERIMENTAL_KEYBOARD_LOCK_UI_NAME,
     IDS_FLAGS_EXPERIMENTAL_KEYBOARD_LOCK_UI_DESCRIPTION, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kExperimentalKeyboardLockUI)},
#if defined(OS_ANDROID)
    {"progress-bar-animation", IDS_FLAGS_PROGRESS_BAR_ANIMATION_NAME,
     IDS_FLAGS_PROGRESS_BAR_ANIMATION_DESCRIPTION, kOsAndroid,
     MULTI_VALUE_TYPE(kProgressBarAnimationChoices)},
    {"progress-bar-completion", IDS_FLAGS_PROGRESS_BAR_COMPLETION_NAME,
     IDS_FLAGS_PROGRESS_BAR_COMPLETION_DESCRIPTION, kOsAndroid,
     MULTI_VALUE_TYPE(kProgressBarCompletionChoices)},
#endif  // OS_ANDROID
#if defined(OS_ANDROID)
    {"offline-bookmarks", IDS_FLAGS_OFFLINE_BOOKMARKS_NAME,
     IDS_FLAGS_OFFLINE_BOOKMARKS_DESCRIPTION, kOsAndroid,
     FEATURE_VALUE_TYPE(offline_pages::kOfflineBookmarksFeature)},
    {"offline-pages-async-download",
     IDS_FLAGS_OFFLINE_PAGES_ASYNC_DOWNLOAD_NAME,
     IDS_FLAGS_OFFLINE_PAGES_ASYNC_DOWNLOAD_DESCRIPTION, kOsAndroid,
     FEATURE_VALUE_TYPE(offline_pages::kOfflinePagesAsyncDownloadFeature)},
    {"offline-pages-sharing", IDS_FLAGS_OFFLINE_PAGES_SHARING_NAME,
     IDS_FLAGS_OFFLINE_PAGES_SHARING_DESCRIPTION, kOsAndroid,
     FEATURE_VALUE_TYPE(offline_pages::kOfflinePagesSharingFeature)},
    {"background-loader-for-downloads",
     IDS_FLAGS_BACKGROUND_LOADER_FOR_DOWNLOADS_NAME,
     IDS_FLAGS_BACKGROUND_LOADER_FOR_DOWNLOADS_DESCRIPTION, kOsAndroid,
     FEATURE_VALUE_TYPE(offline_pages::kBackgroundLoaderForDownloadsFeature)},
    {"background-loader", IDS_FLAGS_NEW_BACKGROUND_LOADER_NAME,
     IDS_FLAGS_NEW_BACKGROUND_LOADER_DESCRIPTION, kOsAndroid,
     FEATURE_VALUE_TYPE(offline_pages::kNewBackgroundLoaderFeature)},
#endif  // OS_ANDROID
    {"disallow-doc-written-script-loads",
     IDS_FLAGS_DISALLOW_DOC_WRITTEN_SCRIPTS_UI_NAME,
     IDS_FLAGS_DISALLOW_DOC_WRITTEN_SCRIPTS_UI_DESCRIPTION, kOsAll,
     // NOTE: if we want to add additional experiment entries for other
     // features controlled by kBlinkSettings, we'll need to add logic to
     // merge the flag values.
     ENABLE_DISABLE_VALUE_TYPE_AND_VALUE(
         switches::kBlinkSettings,
         "disallowFetchForDocWrittenScriptsInMainFrame=true",
         switches::kBlinkSettings,
         "disallowFetchForDocWrittenScriptsInMainFrame=false")},
#if defined(OS_ANDROID)
    {"enable-ntp-popular-sites", IDS_FLAGS_NTP_POPULAR_SITES_NAME,
     IDS_FLAGS_NTP_POPULAR_SITES_DESCRIPTION, kOsAndroid,
     ENABLE_DISABLE_VALUE_TYPE(ntp_tiles::switches::kEnableNTPPopularSites,
                               ntp_tiles::switches::kDisableNTPPopularSites)},
    {"ntp-switch-to-existing-tab", IDS_FLAGS_NTP_SWITCH_TO_EXISTING_TAB_NAME,
     IDS_FLAGS_NTP_SWITCH_TO_EXISTING_TAB_DESCRIPTION, kOsAndroid,
     MULTI_VALUE_TYPE(kNtpSwitchToExistingTabChoices)},
    {"use-android-midi-api", IDS_FLAGS_USE_ANDROID_MIDI_API_NAME,
     IDS_FLAGS_USE_ANDROID_MIDI_API_DESCRIPTION, kOsAndroid,
     FEATURE_VALUE_TYPE(midi::features::kMidiManagerAndroid)},
#endif  // OS_ANDROID
#if defined(OS_WIN)
    {"trace-export-events-to-etw", IDS_FLAGS_TRACE_EXPORT_EVENTS_TO_ETW_NAME,
     IDS_FLAGS_TRACE_EXPORT_EVENTS_TO_ETW_DESRIPTION, kOsWin,
     SINGLE_VALUE_TYPE(switches::kTraceExportEventsToETW)},
    {"merge-key-char-events", IDS_FLAGS_MERGE_KEY_CHAR_EVENTS_NAME,
     IDS_FLAGS_MERGE_KEY_CHAR_EVENTS_DESCRIPTION, kOsWin,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableMergeKeyCharEvents,
                               switches::kDisableMergeKeyCharEvents)},
    {"use-winrt-midi-api", IDS_FLAGS_USE_WINRT_MIDI_API_NAME,
     IDS_FLAGS_USE_WINRT_MIDI_API_DESCRIPTION, kOsWin,
     FEATURE_VALUE_TYPE(midi::features::kMidiManagerWinrt)},
#endif  // OS_WIN
#if BUILDFLAG(ENABLE_BACKGROUND)
    {"enable-push-api-background-mode", IDS_FLAGS_PUSH_API_BACKGROUND_MODE_NAME,
     IDS_FLAGS_PUSH_API_BACKGROUND_MODE_DESCRIPTION, kOsMac | kOsWin | kOsLinux,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnablePushApiBackgroundMode,
                               switches::kDisablePushApiBackgroundMode)},
#endif  // BUILDFLAG(ENABLE_BACKGROUND)
#if defined(OS_CHROMEOS)
    {"cros-regions-mode", IDS_FLAGS_CROS_REGIONS_MODE_NAME,
     IDS_FLAGS_CROS_REGIONS_MODE_DESCRIPTION, kOsCrOS,
     MULTI_VALUE_TYPE(kCrosRegionsModeChoices)},
#endif  // OS_CHROMEOS
#if defined(ENABLE_NOTIFICATIONS) && defined(OS_ANDROID)
    {"enable-web-notification-custom-layouts",
     IDS_FLAGS_ENABLE_WEB_NOTIFICATION_CUSTOM_LAYOUTS_NAME,
     IDS_FLAGS_ENABLE_WEB_NOTIFICATION_CUSTOM_LAYOUTS_DESCRIPTION, kOsAndroid,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableWebNotificationCustomLayouts,
                               switches::kDisableWebNotificationCustomLayouts)},
#endif  // ENABLE_NOTIFICATIONS && OS_ANDROID
#if defined(OS_WIN)
    {"enable-appcontainer", IDS_FLAGS_ENABLE_APPCONTAINER_NAME,
     IDS_FLAGS_ENABLE_APPCONTAINER_DESCRIPTION, kOsWin,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableAppContainer,
                               switches::kDisableAppContainer)},
#endif  // OS_WIN
#if defined(TOOLKIT_VIEWS) || defined(OS_ANDROID)
    {"enable-autofill-credit-card-upload",
     IDS_FLAGS_AUTOFILL_CREDIT_CARD_UPLOAD_NAME,
     IDS_FLAGS_AUTOFILL_CREDIT_CARD_UPLOAD_DESCRIPTION, kOsAll,
     ENABLE_DISABLE_VALUE_TYPE(
         autofill::switches::kEnableOfferUploadCreditCards,
         autofill::switches::kDisableOfferUploadCreditCards)},
#endif  // TOOLKIT_VIEWS || OS_ANDROID
#if defined(OS_ANDROID)
    {"tab-management-experiment-type", IDS_FLAGS_HERB_PROTOTYPE_CHOICES_NAME,
     IDS_FLAGS_HERB_PROTOTYPE_CHOICES_DESCRIPTION, kOsAndroid,
     MULTI_VALUE_TYPE(kHerbPrototypeChoices)},
    {"app-link", IDS_FLAGS_ENABLE_APP_LINK_NAME,
     IDS_FLAGS_ENABLE_APP_LINK_DESCRIPTION, kOsAndroid,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableAppLink,
                               switches::kDisableAppLink)},
#endif  // OS_ANDROID
    {"enable-md-bookmarks", IDS_FLAGS_ENABLE_MATERIAL_DESIGN_BOOKMARKS_NAME,
     IDS_FLAGS_ENABLE_MATERIAL_DESIGN_BOOKMARKS_DESCRIPTION, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kMaterialDesignBookmarks)},
    {"enable-md-feedback", IDS_FLAGS_ENABLE_MATERIAL_DESIGN_FEEDBACK_NAME,
     IDS_FLAGS_ENABLE_MATERIAL_DESIGN_FEEDBACK_DESCRIPTION, kOsDesktop,
     SINGLE_VALUE_TYPE(switches::kEnableMaterialDesignFeedback)},
    {"enable-md-history", IDS_FLAGS_ENABLE_MATERIAL_DESIGN_HISTORY_NAME,
     IDS_FLAGS_ENABLE_MATERIAL_DESIGN_HISTORY_DESCRIPTION, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kMaterialDesignHistory)},
    {"enable-md-settings", IDS_FLAGS_ENABLE_MATERIAL_DESIGN_SETTINGS_NAME,
     IDS_FLAGS_ENABLE_MATERIAL_DESIGN_SETTINGS_DESCRIPTION, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kMaterialDesignSettings)},
    {"safe-search-url-reporting", IDS_FLAGS_SAFE_SEARCH_URL_REPORTING_NAME,
     IDS_FLAGS_SAFE_SEARCH_URL_REPORTING_DESCRIPTION, kOsAll,
     FEATURE_VALUE_TYPE(features::kSafeSearchUrlReporting)},
    {"force-ui-direction", IDS_FLAGS_FORCE_UI_DIRECTION_NAME,
     IDS_FLAGS_FORCE_UI_DIRECTION_DESCRIPTION, kOsAll,
     MULTI_VALUE_TYPE(kForceUIDirectionChoices)},
#if BUILDFLAG(ENABLE_EXTENSIONS)
    {"enable-md-extensions", IDS_FLAGS_ENABLE_MATERIAL_DESIGN_EXTENSIONS_NAME,
     IDS_FLAGS_ENABLE_MATERIAL_DESIGN_EXTENSIONS_DESCRIPTION, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kMaterialDesignExtensions)},
#endif  // ENABLE_EXTENSIONS
#if defined(OS_WIN) || defined(OS_LINUX)
    {"enable-input-ime-api", IDS_FLAGS_ENABLE_INPUT_IME_API_NAME,
     IDS_FLAGS_ENABLE_INPUT_IME_API_DESCRIPTION, kOsWin | kOsLinux,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableInputImeAPI,
                               switches::kDisableInputImeAPI)},
#endif  // OS_WIN || OS_LINUX
    {"enable-origin-trials", IDS_FLAGS_ORIGIN_TRIALS_NAME,
     IDS_FLAGS_ORIGIN_TRIALS_DESCRIPTION, kOsAll,
     FEATURE_VALUE_TYPE(features::kOriginTrials)},
    {"enable-brotli", IDS_FLAGS_ENABLE_BROTLI_NAME,
     IDS_FLAGS_ENABLE_BROTLI_DESCRIPTION, kOsAll,
     FEATURE_VALUE_TYPE(features::kBrotliEncoding)},
    {"enable-webusb", IDS_FLAGS_ENABLE_WEB_USB_NAME,
     IDS_FLAGS_ENABLE_WEB_USB_DESCRIPTION, kOsAll,
     FEATURE_VALUE_TYPE(features::kWebUsb)},
#if defined(OS_ANDROID)
    {"disable-unified-media-pipeline",
     IDS_FLAGS_DISABLE_UNIFIED_MEDIA_PIPELINE_NAME,
     IDS_FLAGS_DISABLE_UNIFIED_MEDIA_PIPELINE_DESCRIPTION, kOsAndroid,
     SINGLE_VALUE_TYPE(switches::kDisableUnifiedMediaPipeline)},
#endif  // OS_ANDROID
#if defined(OS_ANDROID)
    {"force-show-update-menu-item", IDS_FLAGS_UPDATE_MENU_ITEM_NAME,
     IDS_FLAGS_UPDATE_MENU_ITEM_DESCRIPTION, kOsAndroid,
     SINGLE_VALUE_TYPE(switches::kForceShowUpdateMenuItem)},
    {"update-menu-item-summary", IDS_FLAGS_UPDATE_MENU_ITEM_SUMMARY_NAME,
     IDS_FLAGS_UPDATE_MENU_ITEM_SUMMARY_DESCRIPTION, kOsAndroid,
     MULTI_VALUE_TYPE(kUpdateMenuItemSummaryChoices)},
    {"force-show-update-menu-badge", IDS_FLAGS_UPDATE_MENU_BADGE_NAME,
     IDS_FLAGS_UPDATE_MENU_BADGE_DESCRIPTION, kOsAndroid,
     SINGLE_VALUE_TYPE(switches::kForceShowUpdateMenuBadge)},
    {"set-market-url-for-testing", IDS_FLAGS_SET_MARKET_URL_FOR_TESTING_NAME,
     IDS_FLAGS_SET_MARKET_URL_FOR_TESTING_DESCRIPTION, kOsAndroid,
     SINGLE_VALUE_TYPE_AND_VALUE(
         switches::kMarketUrlForTesting,
         "https://play.google.com/store/apps/details?id=com.android.chrome")},
#endif  // OS_ANDROID
#if defined(OS_WIN) || defined(OS_MACOSX)
    {"automatic-tab-discarding", IDS_FLAGS_AUTOMATIC_TAB_DISCARDING_NAME,
     IDS_FLAGS_AUTOMATIC_TAB_DISCARDING_DESCRIPTION, kOsWin | kOsMac,
     FEATURE_VALUE_TYPE(features::kAutomaticTabDiscarding)},
#endif  // OS_WIN || OS_MACOSX
    {"enable-es3-apis", IDS_FLAGS_WEBGL2_NAME, IDS_FLAGS_WEBGL2_DESCRIPTION,
     kOsAll, MULTI_VALUE_TYPE(kEnableWebGL2Choices)},
    {"enable-webfonts-intervention-v2",
     IDS_FLAGS_ENABLE_WEBFONTS_INTERVENTION_NAME,
     IDS_FLAGS_ENABLE_WEBFONTS_INTERVENTION_DESCRIPTION, kOsAll,
     MULTI_VALUE_TYPE(kEnableWebFontsInterventionV2Choices)},
    {"enable-webfonts-intervention-trigger",
     IDS_FLAGS_ENABLE_WEBFONTS_INTERVENTION_TRIGGER_NAME,
     IDS_FLAGS_ENABLE_WEBFONTS_INTERVENTION_TRIGGER_DESCRIPTION, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableWebFontsInterventionTrigger)},
    {"enable-grouped-history", IDS_FLAGS_ENABLE_GROUPED_HISTORY_NAME,
     IDS_FLAGS_ENABLE_GROUPED_HISTORY_DESCRIPTION, kOsDesktop,
     SINGLE_VALUE_TYPE(switches::kHistoryEnableGroupByDomain)},
    {"ssl-version-max", IDS_FLAGS_SSL_VERSION_MAX_NAME,
     IDS_FLAGS_SSL_VERSION_MAX_DESCRIPTION, kOsAll,
     MULTI_VALUE_TYPE(kSSLVersionMaxChoices)},
    {"enable-token-binding", IDS_FLAGS_ENABLE_TOKEN_BINDING_NAME,
     IDS_FLAGS_ENABLE_TOKEN_BINDING_DESCRIPTION, kOsAll,
     FEATURE_VALUE_TYPE(features::kTokenBinding)},
    {"enable-scroll-anchoring", IDS_FLAGS_ENABLE_SCROLL_ANCHORING_NAME,
     IDS_FLAGS_ENABLE_SCROLL_ANCHORING_DESCRIPTION, kOsAll,
     FEATURE_VALUE_TYPE(features::kScrollAnchoring)},
    {"disable-audio-support-for-desktop-share",
     IDS_FLAG_DISABLE_AUDIO_FOR_DESKTOP_SHARE,
     IDS_FLAG_DISABLE_AUDIO_FOR_DESKTOP_SHARE_DESCRIPTION, kOsAll,
     SINGLE_VALUE_TYPE(switches::kDisableAudioSupportForDesktopShare)},
#if BUILDFLAG(ENABLE_EXTENSIONS)
    {"tab-for-desktop-share", IDS_FLAG_DISABLE_TAB_FOR_DESKTOP_SHARE,
     IDS_FLAG_DISABLE_TAB_FOR_DESKTOP_SHARE_DESCRIPTION, kOsAll,
     SINGLE_VALUE_TYPE(extensions::switches::kDisableTabForDesktopShare)},
    {"disable-desktop-capture-picker-new-ui",
     IDS_FLAG_DISABLE_DESKTOP_CAPTURE_PICKER_NEW_UI,
     IDS_FLAG_DISABLE_DESKTOP_CAPTURE_PICKER_NEW_UI_DESCRIPTION, kOsAll,
     SINGLE_VALUE_TYPE(
         extensions::switches::kDisableDesktopCapturePickerNewUI)},
#endif  // ENABLE_EXTENSIONS
#if defined(OS_ANDROID)
    {"enable-ntp-snippets", IDS_FLAGS_ENABLE_NTP_SNIPPETS_NAME,
     IDS_FLAGS_ENABLE_NTP_SNIPPETS_DESCRIPTION, kOsAndroid,
     FEATURE_WITH_VARIATIONS_VALUE_TYPE(
         ntp_snippets::kContentSuggestionsFeature,
         kNTPSnippetsFeatureVariations,
         ntp_snippets::kStudyName)},
    {"enable-ntp-snippets-increased-visibility",
     IDS_FLAGS_ENABLE_NTP_SNIPPETS_VISIBILITY_NAME,
     IDS_FLAGS_ENABLE_NTP_SNIPPETS_VISIBILITY_DESCRIPTION, kOsAndroid,
     FEATURE_VALUE_TYPE(ntp_snippets::kIncreasedVisibility)},
    {"enable-ntp-save-to-offline", IDS_FLAGS_ENABLE_NTP_SAVE_TO_OFFLINE_NAME,
     IDS_FLAGS_ENABLE_NTP_SAVE_TO_OFFLINE_DESCRIPTION, kOsAndroid,
     FEATURE_VALUE_TYPE(ntp_snippets::kSaveToOfflineFeature)},
    {"enable-ntp-offline-badge", IDS_FLAGS_ENABLE_NTP_OFFLINE_BADGE_NAME,
     IDS_FLAGS_ENABLE_NTP_OFFLINE_BADGE_DESCRIPTION, kOsAndroid,
     FEATURE_VALUE_TYPE(ntp_snippets::kOfflineBadgeFeature)},
    {"enable-ntp-recent-offline-tab-suggestions",
     IDS_FLAGS_ENABLE_NTP_RECENT_OFFLINE_TAB_SUGGESTIONS_NAME,
     IDS_FLAGS_ENABLE_NTP_RECENT_OFFLINE_TAB_SUGGESTIONS_DESCRIPTION,
     kOsAndroid,
     FEATURE_VALUE_TYPE(ntp_snippets::kRecentOfflineTabSuggestionsFeature)},
    {"enable-ntp-asset-download-suggestions",
     IDS_FLAGS_ENABLE_NTP_ASSET_DOWNLOAD_SUGGESTIONS_NAME,
     IDS_FLAGS_ENABLE_NTP_ASSET_DOWNLOAD_SUGGESTIONS_DESCRIPTION, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kAssetDownloadSuggestionsFeature)},
    {"enable-ntp-offline-page-download-suggestions",
     IDS_FLAGS_ENABLE_NTP_OFFLINE_PAGE_DOWNLOAD_SUGGESTIONS_NAME,
     IDS_FLAGS_ENABLE_NTP_OFFLINE_PAGE_DOWNLOAD_SUGGESTIONS_DESCRIPTION,
     kOsAndroid,
     FEATURE_VALUE_TYPE(features::kOfflinePageDownloadSuggestionsFeature)},
    {"enable-ntp-bookmark-suggestions",
     IDS_FLAGS_ENABLE_NTP_BOOKMARK_SUGGESTIONS_NAME,
     IDS_FLAGS_ENABLE_NTP_BOOKMARK_SUGGESTIONS_DESCRIPTION, kOsAndroid,
     FEATURE_VALUE_TYPE(ntp_snippets::kBookmarkSuggestionsFeature)},
    {"enable-ntp-physical-web-page-suggestions",
     IDS_FLAGS_ENABLE_NTP_PHYSICAL_WEB_PAGE_SUGGESTIONS_NAME,
     IDS_FLAGS_ENABLE_NTP_PHYSICAL_WEB_PAGE_SUGGESTIONS_DESCRIPTION, kOsAndroid,
     FEATURE_VALUE_TYPE(ntp_snippets::kPhysicalWebPageSuggestionsFeature)},
    {"enable-ntp-foreign-sessions-suggestions",
     IDS_FLAGS_ENABLE_NTP_FOREIGN_SESSIONS_SUGGESTIONS_NAME,
     IDS_FLAGS_ENABLE_NTP_FOREIGN_SESSIONS_SUGGESTIONS_DESCRIPTION, kOsAndroid,
     FEATURE_VALUE_TYPE(ntp_snippets::kForeignSessionsSuggestionsFeature)},
#endif  // OS_ANDROID
#if BUILDFLAG(ENABLE_WEBRTC) && BUILDFLAG(RTC_USE_H264) && \
    !defined(MEDIA_DISABLE_FFMPEG)
    {"enable-webrtc-h264-with-openh264-ffmpeg",
     IDS_FLAGS_WEBRTC_H264_WITH_OPENH264_FFMPEG_NAME,
     IDS_FLAGS_WEBRTC_H264_WITH_OPENH264_FFMPEG_DESCRIPTION, kOsDesktop,
     FEATURE_VALUE_TYPE(content::kWebRtcH264WithOpenH264FFmpeg)},
#endif  // ENABLE_WEBRTC && BUILDFLAG(RTC_USE_H264) && !MEDIA_DISABLE_FFMPEG
#if defined(OS_ANDROID)
    {"ime-thread", IDS_FLAGS_IME_THREAD_NAME, IDS_FLAGS_IME_THREAD_DESCRIPTION,
     kOsAndroid, FEATURE_VALUE_TYPE(features::kImeThread)},
#endif  // OS_ANDROID
#if defined(OS_ANDROID)
    {"offline-pages-ntp", IDS_FLAGS_NTP_OFFLINE_PAGES_NAME,
     IDS_FLAGS_NTP_OFFLINE_PAGES_DESCRIPTION, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kNTPOfflinePagesFeature)},
    {"offlining-recent-pages", IDS_FLAGS_OFFLINING_RECENT_PAGES_NAME,
     IDS_FLAGS_OFFLINING_RECENT_PAGES_DESCRIPTION, kOsAndroid,
     FEATURE_VALUE_TYPE(offline_pages::kOffliningRecentPagesFeature)},
    {"offline-pages-ct", IDS_FLAGS_OFFLINE_PAGES_CT_NAME,
     IDS_FLAGS_OFFLINE_PAGES_CT_DESCRIPTION, kOsAndroid,
     FEATURE_VALUE_TYPE(offline_pages::kOfflinePagesCTFeature)},
#endif  // OS_ANDROID
    {"protect-sync-credential", IDS_FLAGS_PROTECT_SYNC_CREDENTIAL_NAME,
     IDS_FLAGS_PROTECT_SYNC_CREDENTIAL_DESCRIPTION, kOsAll,
     FEATURE_VALUE_TYPE(password_manager::features::kProtectSyncCredential)},
    {"protect-sync-credential-on-reauth",
     IDS_FLAGS_PROTECT_SYNC_CREDENTIAL_ON_REAUTH_NAME,
     IDS_FLAGS_PROTECT_SYNC_CREDENTIAL_ON_REAUTH_DESCRIPTION, kOsAll,
     FEATURE_VALUE_TYPE(
         password_manager::features::kProtectSyncCredentialOnReauth)},
    {"password-import-export", IDS_FLAGS_PASSWORD_IMPORT_EXPORT_NAME,
     IDS_FLAGS_PASSWORD_IMPORT_EXPORT_DESCRIPTION,
     kOsWin | kOsMac | kOsCrOS | kOsLinux,
     FEATURE_VALUE_TYPE(password_manager::features::kPasswordImportExport)},
#if defined(OS_CHROMEOS)
    {"enable-experimental-accessibility-features",
     IDS_FLAGS_EXPERIMENTAL_ACCESSIBILITY_FEATURES_NAME,
     IDS_FLAGS_EXPERIMENTAL_ACCESSIBILITY_FEATURES_DESCRIPTION, kOsCrOS,
     SINGLE_VALUE_TYPE(
         chromeos::switches::kEnableExperimentalAccessibilityFeatures)},
    {"opt-in-ime-menu", IDS_FLAGS_ENABLE_IME_MENU_NAME,
     IDS_FLAGS_ENABLE_IME_MENU_DESCRIPTION, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kOptInImeMenu)},
    {"disable-system-timezone-automatic-detection",
     IDS_FLAGS_DISABLE_SYSTEM_TIMEZONE_AUTOMATIC_DETECTION_NAME,
     IDS_FLAGS_DISABLE_SYSTEM_TIMEZONE_AUTOMATIC_DETECTION_DESCRIPTION, kOsCrOS,
     SINGLE_VALUE_TYPE(
         chromeos::switches::kDisableSystemTimezoneAutomaticDetectionPolicy)},
    {"enable-native-cups", IDS_FLAGS_ENABLE_NATIVE_CUPS_NAME,
     IDS_FLAGS_ENABLE_NATIVE_CUPS_DESCRIPTION, kOsCrOS,
     SINGLE_VALUE_TYPE(switches::kEnableNativeCups)},
    {"enable-files-details-panel", IDS_FLAGS_ENABLE_FILES_DETAILS_PANEL_NAME,
     IDS_FLAGS_ENABLE_FILES_DETAILS_PANEL_DESCRIPTION, kOsCrOS,
     SINGLE_VALUE_TYPE(chromeos::switches::kEnableFilesDetailsPanel)},
#endif  // OS_CHROMEOS
#if !defined(OS_ANDROID) && !defined(OS_IOS) && defined(GOOGLE_CHROME_BUILD)
    {"enable-google-branded-context-menu",
     IDS_FLAGS_GOOGLE_BRANDED_CONTEXT_MENU_NAME,
     IDS_FLAGS_GOOGLE_BRANDED_CONTEXT_MENU_DESCRIPTION, kOsDesktop,
     SINGLE_VALUE_TYPE(switches::kEnableGoogleBrandedContextMenu)},
#endif  // !OS_ANDROID && !OS_IOS && GOOGLE_CHROME_BUILD
#if defined(OS_MACOSX)
    {"enable-fullscreen-in-tab-detaching",
     IDS_FLAGS_TAB_DETACHING_IN_FULLSCREEN_NAME,
     IDS_FLAGS_TAB_DETACHING_IN_FULLSCREEN_DESCRIPTION, kOsMac,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableFullscreenTabDetaching,
                               switches::kDisableFullscreenTabDetaching)},
    {"enable-fullscreen-toolbar-reveal",
     IDS_FLAGS_FULLSCREEN_TOOLBAR_REVEAL_NAME,
     IDS_FLAGS_FULLSCREEN_TOOLBAR_REVEAL_DESCRIPTION, kOsMac,
     SINGLE_VALUE_TYPE(switches::kEnableFullscreenToolbarReveal)},
#endif  // OS_MACOSX
#if defined(OS_ANDROID)
    {"important-sites-in-cbd", IDS_FLAGS_IMPORTANT_SITES_IN_CBD_NAME,
     IDS_FLAGS_IMPORTANT_SITES_IN_CBD_DESCRIPTION, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kImportantSitesInCBD)},
#endif  // OS_ANDROID
    {"enable-pointer-events", IDS_FLAGS_EXPERIMENTAL_POINTER_EVENT_NAME,
     IDS_FLAGS_EXPERIMENTAL_POINTER_EVENT_DESCRIPTION, kOsAll,
     FEATURE_VALUE_TYPE(features::kPointerEvents)},
    {"passive-listener-default", IDS_FLAGS_PASSIVE_EVENT_LISTENER_DEFAULT_NAME,
     IDS_FLAGS_PASSIVE_EVENT_LISTENER_DEFAULT_DESCRIPTION, kOsAll,
     MULTI_VALUE_TYPE(kPassiveListenersChoices)},
    {"document-passive-event-listeners",
     IDS_FLAGS_PASSIVE_DOCUMENT_EVENT_LISTENERS_NAME,
     IDS_FLAGS_PASSIVE_DOCUMENT_EVENT_LISTENERS_DESCRIPTION, kOsAll,
     FEATURE_VALUE_TYPE(features::kPassiveDocumentEventListeners)},
    {"passive-event-listeners-due-to-fling",
     IDS_FLAGS_PASSIVE_EVENT_LISTENERS_DUE_TO_FLING_NAME,
     IDS_FLAGS_PASSIVE_EVENT_LISTENERS_DUE_TO_FLING_DESCRIPTION, kOsAll,
     FEATURE_VALUE_TYPE(features::kPassiveEventListenersDueToFling)},
    {"enable-loading-ipc-optimization-for-small-resources",
     IDS_FLAGS_OPTIMIZE_LOADING_IPC_FOR_SMALL_RESOURCES_NAME,
     IDS_FLAGS_OPTIMIZE_LOADING_IPC_FOR_SMALL_RESOURCES_DESCRIPTION, kOsAll,
     FEATURE_VALUE_TYPE(features::kOptimizeLoadingIPCForSmallResources)},
    {"enable-font-cache-scaling", IDS_FLAGS_FONT_CACHE_SCALING_NAME,
     IDS_FLAGS_FONT_CACHE_SCALING_DESCRIPTION, kOsAll,
     FEATURE_VALUE_TYPE(features::kFontCacheScaling)},
    {"enable-framebusting-needs-sameorigin-or-usergesture",
     IDS_FLAGS_FRAMEBUSTING_NAME, IDS_FLAGS_FRAMEBUSTING_DESCRIPTION, kOsAll,
     FEATURE_VALUE_TYPE(features::kFramebustingNeedsSameOriginOrUserGesture)},
#if defined(OS_ANDROID)
    {"enable-android-pay-integration-v1",
     IDS_FLAGS_ENABLE_ANDROID_PAY_INTEGRATION_V1_NAME,
     IDS_FLAGS_ENABLE_ANDROID_PAY_INTEGRATION_V1_DESCRIPTION, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kAndroidPayIntegrationV1)},
#endif  // OS_ANDROID
#if defined(OS_CHROMEOS)
    {"disable-eol-notification", IDS_FLAGS_EOL_NOTIFICATION_NAME,
     IDS_FLAGS_EOL_NOTIFICATION_DESCRIPTION, kOsCrOS,
     SINGLE_DISABLE_VALUE_TYPE(chromeos::switches::kDisableEolNotification)},
#endif  // OS_CHROMEOS
    {"fill-on-account-select", IDS_FILL_ON_ACCOUNT_SELECT_NAME,
     IDS_FILL_ON_ACCOUNT_SELECT_DESCRIPTION, kOsAll,
     FEATURE_VALUE_TYPE(password_manager::features::kFillOnAccountSelect)},
    {"new-audio-rendering-mixing-strategy",
     IDS_NEW_AUDIO_RENDERING_MIXING_STRATEGY_NAME,
     IDS_NEW_AUDIO_RENDERING_MIXING_STRATEGY_DESCRIPTION,
     kOsWin | kOsMac | kOsLinux | kOsAndroid,
     FEATURE_VALUE_TYPE(media::kNewAudioRenderingMixingStrategy)},
    {"disable-background-video-track",
     IDS_BACKGROUND_VIDEO_TRACK_OPTIMIZATION_NAME,
     IDS_BACKGROUND_VIDEO_TRACK_OPTIMIZATION_DESCRIPTION,
     kOsAll,
     FEATURE_VALUE_TYPE(media::kBackgroundVideoTrackOptimization)},
#if defined(OS_CHROMEOS)
    {"files-quick-view", IDS_FLAGS_FILES_QUICK_VIEW_NAME,
     IDS_FLAGS_FILES_QUICK_VIEW_DESCRIPTION, kOsCrOS,
     ENABLE_DISABLE_VALUE_TYPE(chromeos::switches::kEnableFilesQuickView,
                               chromeos::switches::kDisableFilesQuickView)},
    {"quick-unlock-pin", IDS_FLAGS_QUICK_UNLOCK_PIN,
     IDS_FLAGS_QUICK_UNLOCK_PIN_DESCRIPTION, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kQuickUnlockPin)},
#endif  // OS_CHROMEOS
    {"browser-task-scheduler", IDS_FLAGS_BROWSER_TASK_SCHEDULER_NAME,
     IDS_FLAGS_BROWSER_TASK_SCHEDULER_DESCRIPTION, kOsAll,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableBrowserTaskScheduler,
                               switches::kDisableBrowserTaskScheduler)},
#if defined(OS_ANDROID)
    {"enable-improved-a2hs", IDS_FLAGS_ENABLE_WEBAPK,
     IDS_FLAGS_ENABLE_WEBAPK_DESCRIPTION, kOsAndroid,
     SINGLE_VALUE_TYPE(switches::kEnableWebApk)},
    {"no-credit-card-abort", IDS_FLAGS_NO_CREDIT_CARD_ABORT,
     IDS_FLAGS_NO_CREDIT_CARD_ABORT_DESCRIPTION, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kNoCreditCardAbort)},
#endif  // OS_ANDROID
    {"enable-feature-policy", IDS_FLAGS_FEATURE_POLICY_NAME,
     IDS_FLAGS_FEATURE_POLICY_DESCRIPTION, kOsAll,
     FEATURE_VALUE_TYPE(features::kFeaturePolicy)},
#if defined(OS_CHROMEOS)
    {"enable-emoji-handwriting-voice-on-ime-menu",
     IDS_FLAGS_ENABLE_EHV_INPUT_NAME, IDS_FLAGS_ENABLE_EHV_INPUT_DESCRIPTION,
     kOsCrOS, FEATURE_VALUE_TYPE(features::kEHVInputOnImeMenu)},
#endif  // OS_CHROMEOS
    {"enable-gamepad-extensions", IDS_FLAGS_GAMEPAD_EXTENSIONS_NAME,
     IDS_FLAGS_GAMEPAD_EXTENSIONS_DESCRIPTION, kOsAll,
     FEATURE_VALUE_TYPE(features::kGamepadExtensions)},
#if defined(OS_CHROMEOS)
    {"arc-use-auth-endpoint", IDS_FLAGS_ARC_USE_AUTH_ENDPOINT_NAME,
     IDS_FLAGS_ARC_USE_AUTH_ENDPOINT_DESCRIPTION, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kArcUseAuthEndpointFeature)},
    {"arc-boot-completed-broadcast", IDS_FLAGS_ARC_BOOT_COMPLETED,
     IDS_FLAGS_ARC_BOOT_COMPLETED_DESCRIPTION, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kBootCompletedBroadcastFeature)},
#endif  // OS_CHROMEOS
    {"saveas-menu-text-experiment", IDS_FLAGS_SAVEAS_MENU_LABEL_EXPERIMENT_NAME,
     IDS_FLAGS_SAVEAS_MENU_LABEL_EXPERIMENT_DESCRIPTION, kOsDesktop,
     SINGLE_VALUE_TYPE(switches::kEnableSaveAsMenuLabelExperiment)},
    {"enable-generic-sensor", IDS_FLAGS_ENABLE_GENERIC_SENSOR_NAME,
     IDS_FLAGS_ENABLE_GENERIC_SENSOR_DESCRIPTION, kOsAll,
     FEATURE_VALUE_TYPE(features::kGenericSensor)},
    {"expensive-background-timer-throttling",
     IDS_FLAGS_EXPENSIVE_BACKGROUND_TIMER_THROTTLING_NAME,
     IDS_FLAGS_EXPENSIVE_BACKGROUND_TIMER_THROTTLING_DESCRIPTION, kOsAll,
     FEATURE_VALUE_TYPE(features::kExpensiveBackgroundTimerThrottling)},
    {"security-chip", IDS_FLAGS_SECURITY_CHIP_NAME,
     IDS_FLAGS_SECURITY_CHIP_DESCRIPTION, kOsDesktop,
     MULTI_VALUE_TYPE(kSecurityChipChoices)},
    {"security-chip-animation", IDS_FLAGS_SECURITY_CHIP_ANIMATION_NAME,
     IDS_FLAGS_SECURITY_CHIP_ANIMATION_DESCRIPTION, kOsDesktop,
     MULTI_VALUE_TYPE(kSecurityChipAnimationChoices)},
#if defined(OS_CHROMEOS)
    {"enumerate-audio-devices", IDS_FLAGS_ENABLE_ENUMERATING_AUDIO_DEVICES_NAME,
     IDS_FLAGS_ENABLE_ENUMERATING_AUDIO_DEVICES_DESCRIPTION, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kEnumerateAudioDevices)},
#endif  // OS_CHROMEOS
#if !defined(OS_ANDROID)
    {"enable-default-media-session",
     IDS_FLAGS_ENABLE_DEFAULT_MEDIA_SESSION_NAME,
     IDS_FLAGS_ENABLE_DEFAULT_MEDIA_SESSION_DESCRIPTION, kOsDesktop,
     MULTI_VALUE_TYPE(kEnableDefaultMediaSessionChoices)},
#endif  // !OS_ANDROID
#if defined(OS_ANDROID)
    {"modal-permission-prompts", IDS_FLAGS_MODAL_PERMISSION_PROMPTS_NAME,
     IDS_FLAGS_MODAL_PERMISSION_PROMPTS_DESCRIPTION, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kModalPermissionPrompts)},
#endif
#if !defined(OS_MACOSX)
    {"permission-prompt-persistence-toggle",
     IDS_FLAGS_PERMISSION_PROMPT_PERSISTENCE_TOGGLE_NAME,
     IDS_FLAGS_PERMISSION_PROMPT_PERSISTENCE_TOGGLE_DESCRIPTION,
     kOsWin | kOsCrOS | kOsLinux | kOsAndroid,
     FEATURE_VALUE_TYPE(
         features::kDisplayPersistenceToggleInPermissionPrompts)},
#endif
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
    {"print-scaling", IDS_FLAGS_PRINT_SCALING_NAME,
     IDS_FLAGS_PRINT_SCALING_DESCRIPTION, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kPrintScaling)},
#endif  // !defined(OS_ANDROID)
#if defined(OS_ANDROID)
    {"enable-consistent-omnibox-geolocation",
     IDS_FLAGS_ENABLE_CONSISTENT_OMNIBOX_GEOLOCATION_NAME,
     IDS_FLAGS_ENABLE_CONSISTENT_OMNIBOX_GEOLOCATION_DESCRIPTION, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kConsistentOmniboxGeolocation)},
    {"concurrent-background-loading-on-svelte",
     IDS_FLAGS_OFFLINE_PAGES_SVELTE_CONCURRENT_LOADING_NAME,
     IDS_FLAGS_OFFLINE_PAGES_SVELTE_CONCURRENT_LOADING_DESCRIPTION, kOsAndroid,
     FEATURE_VALUE_TYPE(
         offline_pages::kOfflinePagesSvelteConcurrentLoadingFeature)},
#endif
    {"cross-process-guests", IDS_FLAGS_CROSS_PROCESS_GUEST_VIEW_ISOLATION_NAME,
     IDS_FLAGS_CROSS_PROCESS_GUEST_VIEW_ISOLATION_DESCRIPTION, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kGuestViewCrossProcessFrames)},
#if !defined(OS_ANDROID) && !defined(OS_IOS)
    {"media-remoting", IDS_FLAGS_MEDIA_REMOTING_NAME,
     IDS_FLAGS_MEDIA_REMOTING_DESCRIPTION, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kMediaRemoting)},
    {"media-remoting-encrypted", IDS_FLAGS_MEDIA_REMOTING_ENCRYPTED_NAME,
     IDS_FLAGS_MEDIA_REMOTING_ENCRYPTED_DESCRIPTION, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kMediaRemotingEncrypted)},
#endif

    // NOTE: Adding new command-line switches requires adding corresponding
    // entries to enum "LoginCustomFlags" in histograms.xml. See note in
    // histograms.xml and don't forget to run AboutFlagsHistogramTest unit test.
};

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
#endif  // OS_ANDROID

  // data-reduction-proxy-lo-fi and enable-data-reduction-proxy-lite-page
  // are only available for Chromium builds and the Canary/Dev/Beta channels.
  if ((!strcmp("data-reduction-proxy-lo-fi", entry.internal_name) ||
       !strcmp("enable-data-reduction-proxy-lite-page",
               entry.internal_name)) &&
      channel != version_info::Channel::BETA &&
      channel != version_info::Channel::DEV &&
      channel != version_info::Channel::CANARY &&
      channel != version_info::Channel::UNKNOWN) {
    return true;
  }

  return false;
}

// Records a set of feature switches (prefixed with "--").
void ReportAboutFlagsHistogramSwitches(const std::string& uma_histogram_name,
                                       const std::set<std::string>& switches) {
  for (const std::string& flag : switches) {
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
      NOTREACHED() << "ReportAboutFlagsHistogram(): flag '" << flag
                   << "' has incorrect format.";
    }
    DVLOG(1) << "ReportAboutFlagsHistogram(): histogram='" << uma_histogram_name
             << "' '" << flag << "', uma_id=" << uma_id;

    // Sparse histogram macro does not cache the histogram, so it's safe
    // to use macro with non-static histogram name here.
    UMA_HISTOGRAM_SPARSE_SLOWLY(uma_histogram_name, uma_id);
  }
}

// Records a set of FEATURE_VALUE_TYPE features (suffixed with ":enabled" or
// "disabled", depending on their state).
void ReportAboutFlagsHistogramFeatures(const std::string& uma_histogram_name,
                                       const std::set<std::string>&features) {
  for (const std::string& feature : features) {
    int uma_id = GetSwitchUMAId(feature);
    DVLOG(1) << "ReportAboutFlagsHistogram(): histogram='" << uma_histogram_name
             << "' '" << feature << "', uma_id=" << uma_id;

    // Sparse histogram macro does not cache the histogram, so it's safe
    // to use macro with non-static histogram name here.
    UMA_HISTOGRAM_SPARSE_SLOWLY(uma_histogram_name, uma_id);
  }
}

}  // namespace

void ConvertFlagsToSwitches(flags_ui::FlagsStorage* flags_storage,
                            base::CommandLine* command_line,
                            flags_ui::SentinelsMode sentinels) {
  if (command_line->HasSwitch(switches::kNoExperiments))
    return;

  FlagsStateSingleton::GetFlagsState()->ConvertFlagsToSwitches(
      flags_storage, command_line, sentinels, switches::kEnableFeatures,
      switches::kDisableFeatures);
}

std::vector<std::string> RegisterAllFeatureVariationParameters(
    flags_ui::FlagsStorage* flags_storage,
    base::FeatureList* feature_list) {
  return FlagsStateSingleton::GetFlagsState()
      ->RegisterAllFeatureVariationParameters(flags_storage, feature_list);
}

bool AreSwitchesIdenticalToCurrentCommandLine(
    const base::CommandLine& new_cmdline,
    const base::CommandLine& active_cmdline,
    std::set<base::CommandLine::StringType>* out_difference) {
  const char* extra_flag_sentinel_begin_flag_name = nullptr;
  const char* extra_flag_sentinel_end_flag_name = nullptr;
#if defined(OS_CHROMEOS)
  // Put the flags between --policy-switches--begin and --policy-switches-end on
  // ChromeOS.
  extra_flag_sentinel_begin_flag_name =
      chromeos::switches::kPolicySwitchesBegin;
  extra_flag_sentinel_end_flag_name = chromeos::switches::kPolicySwitchesEnd;
#endif  // OS_CHROMEOS
  return flags_ui::FlagsState::AreSwitchesIdenticalToCurrentCommandLine(
      new_cmdline, active_cmdline, out_difference,
      extra_flag_sentinel_begin_flag_name, extra_flag_sentinel_end_flag_name);
}

void GetFlagFeatureEntries(flags_ui::FlagsStorage* flags_storage,
                           flags_ui::FlagAccess access,
                           base::ListValue* supported_entries,
                           base::ListValue* unsupported_entries) {
  FlagsStateSingleton::GetFlagsState()->GetFlagFeatureEntries(
      flags_storage, access, supported_entries, unsupported_entries,
      base::Bind(&SkipConditionalFeatureEntry));
}

bool IsRestartNeededToCommitChanges() {
  return FlagsStateSingleton::GetFlagsState()->IsRestartNeededToCommitChanges();
}

void SetFeatureEntryEnabled(flags_ui::FlagsStorage* flags_storage,
                            const std::string& internal_name,
                            bool enable) {
  FlagsStateSingleton::GetFlagsState()->SetFeatureEntryEnabled(
      flags_storage, internal_name, enable);
}

void RemoveFlagsSwitches(
    std::map<std::string, base::CommandLine::StringType>* switch_list) {
  FlagsStateSingleton::GetFlagsState()->RemoveFlagsSwitches(switch_list);
}

void ResetAllFlags(flags_ui::FlagsStorage* flags_storage) {
  FlagsStateSingleton::GetFlagsState()->ResetAllFlags(flags_storage);
}

void RecordUMAStatistics(flags_ui::FlagsStorage* flags_storage) {
  const std::set<std::string> switches =
      FlagsStateSingleton::GetFlagsState()->GetSwitchesFromFlags(flags_storage);
  const std::set<std::string> features =
      FlagsStateSingleton::GetFlagsState()->GetFeaturesFromFlags(flags_storage);
  ReportAboutFlagsHistogram("Launch.FlagsAtStartup", switches, features);
}

base::HistogramBase::Sample GetSwitchUMAId(const std::string& switch_name) {
  return static_cast<base::HistogramBase::Sample>(
      base::HashMetricName(switch_name));
}

void ReportAboutFlagsHistogram(
    const std::string& uma_histogram_name,
    const std::set<std::string>& switches,
    const std::set<std::string>& features) {
  ReportAboutFlagsHistogramSwitches(uma_histogram_name, switches);
  ReportAboutFlagsHistogramFeatures(uma_histogram_name, features);
}

namespace testing {

const base::HistogramBase::Sample kBadSwitchFormatHistogramId = 0;

const FeatureEntry* GetFeatureEntries(size_t* count) {
  *count = arraysize(kFeatureEntries);
  return kFeatureEntries;
}

}  // namespace testing

}  // namespace about_flags
