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
#include "base/metrics/metrics_hashes.h"
#include "base/metrics/sparse_histogram.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
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
#include "chrome/grit/google_chrome_strings.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/browser_sync/common/browser_sync_switches.h"
#include "components/cloud_devices/common/cloud_devices_switches.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/dom_distiller/core/dom_distiller_switches.h"
#include "components/error_page/common/error_page_switches.h"
#include "components/flags_ui/feature_entry_macros.h"
#include "components/flags_ui/flags_storage.h"
#include "components/flags_ui/flags_ui_switches.h"
#include "components/nacl/common/nacl_switches.h"
#include "components/offline_pages/offline_page_feature.h"
#include "components/offline_pages/offline_page_switches.h"
#include "components/omnibox/browser/omnibox_switches.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/proximity_auth/switches.h"
#include "components/quirks/switches.h"
#include "components/search/search_switches.h"
#include "components/security_state/switches.h"
#include "components/signin/core/common/signin_switches.h"
#include "components/sync_driver/sync_driver_switches.h"
#include "components/tracing/tracing_switches.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/feature_h264_with_openh264_ffmpeg.h"
#include "content/public/common/features.h"
#include "gin/public/gin_features.h"
#include "grit/components_strings.h"
#include "media/base/media_switches.h"
#include "media/midi/midi_switches.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_switches.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/display/display_switches.h"
#include "ui/events/event_switches.h"
#include "ui/gfx/switches.h"
#include "ui/gl/gl_switches.h"
#include "ui/keyboard/keyboard_switches.h"
#include "ui/native_theme/native_theme_switches.h"
#include "ui/views/views_switches.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/chrome_feature_list.h"
#else
#include "ui/message_center/message_center_switches.h"
#endif

#if defined(OS_CHROMEOS)
#include "chromeos/chromeos_switches.h"
#include "third_party/cros_system_api/switches/chrome_switches.h"
#endif

#if defined(OS_WIN)
#include "components/search_engines/desktop_search_utils.h"
#endif  // defined(OS_WIN)

#if defined(ENABLE_APP_LIST)
#include "ui/app_list/app_list_switches.h"
#endif

#if defined(ENABLE_EXTENSIONS)
#include "extensions/common/switches.h"
#endif

#if defined(ENABLE_PRINT_PREVIEW)
#include "chrome/browser/ui/webui/print_preview/print_preview_distiller.h"
#endif

#if defined(USE_ASH)
#include "ash/ash_switches.h"
#endif

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_switches.h"
#endif

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
    {IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", ""},
    {IDS_MARK_NON_SECURE_AS_NEUTRAL, security_state::switches::kMarkNonSecureAs,
     security_state::switches::kMarkNonSecureAsNeutral},
    {IDS_MARK_NON_SECURE_AS_NON_SECURE,
     security_state::switches::kMarkNonSecureAs,
     security_state::switches::kMarkNonSecureAsNonSecure}};

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
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", "" },
  { IDS_FLAGS_TOP_CHROME_MD_NON_MATERIAL,
    switches::kTopChromeMD,
    switches::kTopChromeMDNonMaterial },
  { IDS_FLAGS_TOP_CHROME_MD_MATERIAL,
    switches::kTopChromeMD,
    switches::kTopChromeMDMaterial },
  { IDS_FLAGS_TOP_CHROME_MD_MATERIAL_HYBRID,
    switches::kTopChromeMD,
    switches::kTopChromeMDMaterialHybrid },
};
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
  { IDS_FLAGS_PROGRESS_BAR_ANIMATION_SMOOTH_INDETERMINATE,
      switches::kProgressBarAnimation, "smooth-indeterminate" },
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
#endif  // defined(OS_ANDROID)

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
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
const FeatureEntry::Choice kEnableOfflinePagesChoices[] = {
    {IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", ""},
    {IDS_FLAGS_ENABLE_OFFLINE_PAGES_AS_BOOKMARKS,
     switches::kEnableOfflinePagesAsBookmarks, ""},
    {IDS_FLAGS_ENABLE_OFFLINE_PAGES_AS_SAVED_PAGES,
     switches::kEnableOfflinePagesAsSavedPages, ""},
    {IDS_GENERIC_EXPERIMENT_CHOICE_DISABLED, switches::kDisableOfflinePages,
     ""},
};

const FeatureEntry::Choice kHerbPrototypeChoices[] = {
    {IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", ""},
    {IDS_GENERIC_EXPERIMENT_CHOICE_DISABLED,
     switches::kTabManagementExperimentTypeDisabled, ""},
    {IDS_FLAGS_HERB_PROTOTYPE_FLAVOR_ANISE,
     switches::kTabManagementExperimentTypeAnise, ""},
    {IDS_FLAGS_HERB_PROTOTYPE_FLAVOR_BASIL,
     switches::kTabManagementExperimentTypeBasil, ""},
    {IDS_FLAGS_HERB_PROTOTYPE_FLAVOR_CHIVE,
     switches::kTabManagementExperimentTypeChive, ""},
    {IDS_FLAGS_HERB_PROTOTYPE_FLAVOR_DILL,
     switches::kTabManagementExperimentTypeDill, ""},
    {IDS_FLAGS_HERB_PROTOTYPE_FLAVOR_ELDERBERRY,
     switches::kTabManagementExperimentTypeElderberry, ""},
};
#endif  // defined(OS_ANDROID)

const FeatureEntry::Choice kEnableUseZoomForDSFChoices[] = {
  { IDS_FLAGS_ENABLE_USE_ZOOM_FOR_DSF_CHOICE_DEFAULT, "", ""},
  { IDS_FLAGS_ENABLE_USE_ZOOM_FOR_DSF_CHOICE_ENABLED,
    switches::kEnableUseZoomForDSF, "true" },
  { IDS_FLAGS_ENABLE_USE_ZOOM_FOR_DSF_CHOICE_DISABLED,
    switches::kEnableUseZoomForDSF, "false" },
};

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
    {"ignore-gpu-blacklist", IDS_FLAGS_IGNORE_GPU_BLACKLIST_NAME,
     IDS_FLAGS_IGNORE_GPU_BLACKLIST_DESCRIPTION, kOsAll,
     SINGLE_VALUE_TYPE(switches::kIgnoreGpuBlacklist)},
#if defined(OS_WIN)
    {"disable-direct-write", IDS_FLAGS_DIRECT_WRITE_NAME,
     IDS_FLAGS_DIRECT_WRITE_DESCRIPTION, kOsWin,
     SINGLE_DISABLE_VALUE_TYPE(switches::kDisableDirectWrite)},
#endif
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
    {"composited-layer-borders", IDS_FLAGS_COMPOSITED_LAYER_BORDERS,
     IDS_FLAGS_COMPOSITED_LAYER_BORDERS_DESCRIPTION, kOsAll,
     SINGLE_VALUE_TYPE(cc::switches::kShowCompositedLayerBorders)},
#if defined(ENABLE_WEBRTC)
    {"disable-webrtc-hw-decoding", IDS_FLAGS_WEBRTC_HW_DECODING_NAME,
     IDS_FLAGS_WEBRTC_HW_DECODING_DESCRIPTION, kOsAndroid | kOsCrOS,
     SINGLE_DISABLE_VALUE_TYPE(switches::kDisableWebRtcHWDecoding)},
    {"disable-webrtc-hw-encoding", IDS_FLAGS_WEBRTC_HW_ENCODING_NAME,
     IDS_FLAGS_WEBRTC_HW_ENCODING_DESCRIPTION, kOsAndroid | kOsCrOS,
     SINGLE_DISABLE_VALUE_TYPE(switches::kDisableWebRtcHWEncoding)},
    {"enable-webrtc-stun-origin", IDS_FLAGS_WEBRTC_STUN_ORIGIN_NAME,
     IDS_FLAGS_WEBRTC_STUN_ORIGIN_DESCRIPTION, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableWebRtcStunOrigin)},
#endif
#if defined(OS_ANDROID)
    {"enable-osk-overscroll", IDS_FLAGS_ENABLE_OSK_OVERSCROLL_NAME,
     IDS_FLAGS_ENABLE_OSK_OVERSCROLL_DESCRIPTION, kOsAndroid,
     SINGLE_VALUE_TYPE(switches::kEnableOSKOverscroll)},
#endif
  // Native client is compiled out when DISABLE_NACL is defined.
#if !defined(DISABLE_NACL)
    {"enable-nacl",  // FLAGS:RECORD_UMA
     IDS_FLAGS_NACL_NAME, IDS_FLAGS_NACL_DESCRIPTION, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableNaCl)},
    {"enable-nacl-debug",  // FLAGS:RECORD_UMA
     IDS_FLAGS_NACL_DEBUG_NAME, IDS_FLAGS_NACL_DEBUG_DESCRIPTION, kOsDesktop,
     SINGLE_VALUE_TYPE(switches::kEnableNaClDebug)},
    {"force-pnacl-subzero", IDS_FLAGS_PNACL_SUBZERO_NAME,
     IDS_FLAGS_PNACL_SUBZERO_DESCRIPTION, kOsDesktop,
     SINGLE_VALUE_TYPE(switches::kForcePNaClSubzero)},
    {"nacl-debug-mask",  // FLAGS:RECORD_UMA
     IDS_FLAGS_NACL_DEBUG_MASK_NAME, IDS_FLAGS_NACL_DEBUG_MASK_DESCRIPTION,
     kOsDesktop, MULTI_VALUE_TYPE(kNaClDebugMaskChoices)},
#endif
#if defined(ENABLE_EXTENSIONS)
    {"extension-apis",  // FLAGS:RECORD_UMA
     IDS_FLAGS_EXPERIMENTAL_EXTENSION_APIS_NAME,
     IDS_FLAGS_EXPERIMENTAL_EXTENSION_APIS_DESCRIPTION, kOsDesktop,
     SINGLE_VALUE_TYPE(extensions::switches::kEnableExperimentalExtensionApis)},
    {"extensions-on-chrome-urls", IDS_FLAGS_EXTENSIONS_ON_CHROME_URLS_NAME,
     IDS_FLAGS_EXTENSIONS_ON_CHROME_URLS_DESCRIPTION, kOsAll,
     SINGLE_VALUE_TYPE(extensions::switches::kExtensionsOnChromeURLs)},
#endif
    {"enable-fast-unload", IDS_FLAGS_FAST_UNLOAD_NAME,
     IDS_FLAGS_FAST_UNLOAD_DESCRIPTION, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableFastUnload)},
#if defined(ENABLE_EXTENSIONS)
    {"enable-app-window-controls", IDS_FLAGS_APP_WINDOW_CONTROLS_NAME,
     IDS_FLAGS_APP_WINDOW_CONTROLS_DESCRIPTION, kOsDesktop,
     SINGLE_VALUE_TYPE(extensions::switches::kEnableAppWindowControls)},
#endif
    {"disable-hyperlink-auditing", IDS_FLAGS_HYPERLINK_AUDITING_NAME,
     IDS_FLAGS_HYPERLINK_AUDITING_DESCRIPTION, kOsAll,
     SINGLE_DISABLE_VALUE_TYPE(switches::kNoPings)},
#if defined(OS_ANDROID)
    {"contextual-search", IDS_FLAGS_CONTEXTUAL_SEARCH,
     IDS_FLAGS_CONTEXTUAL_SEARCH_DESCRIPTION, kOsAndroid,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableContextualSearch,
                               switches::kDisableContextualSearch)},
#endif
    {"show-autofill-type-predictions",
     IDS_FLAGS_SHOW_AUTOFILL_TYPE_PREDICTIONS_NAME,
     IDS_FLAGS_SHOW_AUTOFILL_TYPE_PREDICTIONS_DESCRIPTION, kOsAll,
     SINGLE_VALUE_TYPE(autofill::switches::kShowAutofillTypePredictions)},
    {"smooth-scrolling",  // FLAGS:RECORD_UMA
     IDS_FLAGS_SMOOTH_SCROLLING_NAME, IDS_FLAGS_SMOOTH_SCROLLING_DESCRIPTION,
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
#endif
    {"enable-panels", IDS_FLAGS_PANELS_NAME, IDS_FLAGS_PANELS_DESCRIPTION,
     kOsDesktop, ENABLE_DISABLE_VALUE_TYPE(switches::kEnablePanels,
                                           switches::kDisablePanels)},
    {// See http://crbug.com/120416 for how to remove this flag.
     "save-page-as-mhtml",  // FLAGS:RECORD_UMA
     IDS_FLAGS_SAVE_PAGE_AS_MHTML_NAME,
     IDS_FLAGS_SAVE_PAGE_AS_MHTML_DESCRIPTION, kOsMac | kOsWin | kOsLinux,
     SINGLE_VALUE_TYPE(switches::kSavePageAsMHTML)},
    {"enable-quic", IDS_FLAGS_QUIC_NAME, IDS_FLAGS_QUIC_DESCRIPTION, kOsAll,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableQuic, switches::kDisableQuic)},
    {"enable-alternative-services", IDS_FLAGS_ALTSVC_NAME,
     IDS_FLAGS_ALTSVC_DESCRIPTION, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableAlternativeServices)},
    {"disable-javascript-harmony-shipping",
     IDS_FLAGS_JAVASCRIPT_HARMONY_SHIPPING_NAME,
     IDS_FLAGS_JAVASCRIPT_HARMONY_SHIPPING_DESCRIPTION, kOsAll,
     SINGLE_DISABLE_VALUE_TYPE(switches::kDisableJavaScriptHarmonyShipping)},
    {"enable-javascript-harmony", IDS_FLAGS_JAVASCRIPT_HARMONY_NAME,
     IDS_FLAGS_JAVASCRIPT_HARMONY_DESCRIPTION, kOsAll,
     SINGLE_VALUE_TYPE(switches::kJavaScriptHarmony)},
    {"enable-webassembly", IDS_FLAGS_ENABLE_WASM_NAME,
     IDS_FLAGS_ENABLE_WASM_DESCRIPTION, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableWasm)},
    {"enable-ignition", IDS_FLAGS_V8_IGNITION_NAME,
     IDS_FLAGS_V8_IGNITION_DESCRIPTION, kOsAll,
     FEATURE_VALUE_TYPE(features::kV8Ignition)},
    {"disable-software-rasterizer", IDS_FLAGS_SOFTWARE_RASTERIZER_NAME,
     IDS_FLAGS_SOFTWARE_RASTERIZER_DESCRIPTION,
#if defined(ENABLE_SWIFTSHADER)
     kOsAll,
#else
     0,
#endif
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
    {"enable-web-bluetooth", IDS_FLAGS_WEB_BLUETOOTH_NAME,
     IDS_FLAGS_WEB_BLUETOOTH_DESCRIPTION,
     kOsCrOS | kOsMac | kOsAndroid | kOsLinux,
     SINGLE_VALUE_TYPE(switches::kEnableWebBluetooth)},
#if defined(ENABLE_EXTENSIONS)
    {"enable-ble-advertising-in-apps",
     IDS_FLAGS_BLE_ADVERTISING_IN_EXTENSIONS_NAME,
     IDS_FLAGS_BLE_ADVERTISING_IN_EXTENSIONS_DESCRIPTION, kOsCrOS,
     SINGLE_VALUE_TYPE(extensions::switches::kEnableBLEAdvertising)},
#endif
    {"enable-devtools-experiments", IDS_FLAGS_DEVTOOLS_EXPERIMENTS_NAME,
     IDS_FLAGS_DEVTOOLS_EXPERIMENTS_DESCRIPTION, kOsDesktop,
     SINGLE_VALUE_TYPE(switches::kEnableDevToolsExperiments)},
    {"silent-debugger-extension-api",
     IDS_FLAGS_SILENT_DEBUGGER_EXTENSION_API_NAME,
     IDS_FLAGS_SILENT_DEBUGGER_EXTENSION_API_DESCRIPTION, kOsDesktop,
     SINGLE_VALUE_TYPE(switches::kSilentDebuggerExtensionAPI)},
#if defined(ENABLE_SPELLCHECK) && defined(OS_ANDROID)
    {"enable-android-spellchecker", IDS_OPTIONS_ENABLE_SPELLCHECK,
     IDS_OPTIONS_ENABLE_ANDROID_SPELLCHECKER_DESCRIPTION, kOsAndroid,
     SINGLE_VALUE_TYPE(switches::kEnableAndroidSpellChecker)},
#endif
    {"enable-scroll-prediction", IDS_FLAGS_SCROLL_PREDICTION_NAME,
     IDS_FLAGS_SCROLL_PREDICTION_DESCRIPTION, kOsDesktop,
     SINGLE_VALUE_TYPE(switches::kEnableScrollPrediction)},
#if defined(ENABLE_TOPCHROME_MD)
    {"top-chrome-md", IDS_FLAGS_TOP_CHROME_MD,
     IDS_FLAGS_TOP_CHROME_MD_DESCRIPTION, kOsWin | kOsLinux | kOsCrOS | kOsMac,
     MULTI_VALUE_TYPE(kTopChromeMaterialDesignChoices)},
#endif
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
#endif
    {"enable-download-resumption", IDS_FLAGS_DOWNLOAD_RESUMPTION_NAME,
     IDS_FLAGS_DOWNLOAD_RESUMPTION_DESCRIPTION, kOsAll,
     FEATURE_VALUE_TYPE(features::kDownloadResumption)},
#if defined(OS_ANDROID)
    {"enable-system-download-manager",
     IDS_FLAGS_ENABLE_SYSTEM_DOWNLOAD_MANAGER_NAME,
     IDS_FLAGS_ENABLE_SYSTEM_DOWNLOAD_MANAGER_DESCRIPTION, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kSystemDownloadManager)},
    {"enable-media-document-download-button",
     IDS_FLAGS_MEDIA_DOCUMENT_DOWNLOAD_BUTTON_NAME,
     IDS_FLAGS_MEDIA_DOCUMENT_DOWNLOAD_BUTTON_DESCRIPTION, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kMediaDocumentDownloadButton)},
#endif
#if defined(OS_CHROMEOS)
    {"download-notification", IDS_FLAGS_DOWNLOAD_NOTIFICATION_NAME,
     IDS_FLAGS_DOWNLOAD_NOTIFICATION_DESCRIPTION, kOsCrOS,
     SINGLE_DISABLE_VALUE_TYPE(switches::kDisableDownloadNotification)},
#endif
#if defined(ENABLE_PLUGINS)
    {"allow-nacl-socket-api", IDS_FLAGS_ALLOW_NACL_SOCKET_API_NAME,
     IDS_FLAGS_ALLOW_NACL_SOCKET_API_DESCRIPTION, kOsDesktop,
     SINGLE_VALUE_TYPE_AND_VALUE(switches::kAllowNaClSocketAPI, "*")},
#endif
#if defined(OS_CHROMEOS)
    {"allow-touchpad-three-finger-click",
     IDS_FLAGS_ALLOW_TOUCHPAD_THREE_FINGER_CLICK_NAME,
     IDS_FLAGS_ALLOW_TOUCHPAD_THREE_FINGER_CLICK_DESCRIPTION, kOsCrOS,
     SINGLE_VALUE_TYPE(chromeos::switches::kEnableTouchpadThreeFingerClick)},
    {"ash-enable-unified-desktop", IDS_FLAGS_ASH_ENABLE_UNIFIED_DESKTOP_NAME,
     IDS_FLAGS_ASH_ENABLE_UNIFIED_DESKTOP_DESCRIPTION, kOsCrOS,
     SINGLE_VALUE_TYPE(ash::switches::kAshEnableUnifiedDesktop)},
    {"enable-easy-unlock-proximity-detection",
     IDS_FLAGS_EASY_UNLOCK_PROXIMITY_DETECTION_NAME,
     IDS_FLAGS_EASY_UNLOCK_PROXIMITY_DETECTION_DESCRIPTION, kOsCrOS,
     SINGLE_VALUE_TYPE(proximity_auth::switches::kEnableProximityDetection)},
    {"enable-easy-unlock-bluetooth-low-energy-detection",
     IDS_FLAGS_EASY_UNLOCK_BLUETOOTH_LOW_ENERGY_DISCOVERY_NAME,
     IDS_FLAGS_EASY_UNLOCK_BLUETOOTH_LOW_ENERGY_DISCOVERY_DESCRIPTION, kOsCrOS,
     SINGLE_VALUE_TYPE(
         proximity_auth::switches::kEnableBluetoothLowEnergyDiscovery)},
#endif
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
#endif  // defined(USE_ASH)
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
        SINGLE_DISABLE_VALUE_TYPE(
            ui::switches::kDisableDisplayColorCalibration),
    },
    {
        "disable-quirks-client", IDS_FLAGS_DISABLE_QUIRKS_CLIENT_NAME,
        IDS_FLAGS_DISABLE_QUIRKS_CLIENT_DESCRIPTION, kOsCrOS,
        SINGLE_VALUE_TYPE(quirks::switches::kDisableQuirksClient),
    },
    {
        "ash-disable-screen-orientation-lock",
        IDS_FLAGS_ASH_SCREEN_ORIENTATION_LOCK_NAME,
        IDS_FLAGS_ASH_SCREEN_ORIENTATION_LOCK_DESCRIPTION, kOsCrOS,
        SINGLE_DISABLE_VALUE_TYPE(
            ash::switches::kAshDisableScreenOrientationLock),
    },
#endif  // defined(OS_CHROMEOS)
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
        "disable-touch-feedback", IDS_FLAGS_TOUCH_FEEDBACK_NAME,
        IDS_FLAGS_TOUCH_FEEDBACK_DESCRIPTION, kOsCrOS,
        SINGLE_DISABLE_VALUE_TYPE(switches::kDisableTouchFeedback),
    },
    {
        "ash-enable-mirrored-screen", IDS_FLAGS_ASH_ENABLE_MIRRORED_SCREEN_NAME,
        IDS_FLAGS_ASH_ENABLE_MIRRORED_SCREEN_DESCRIPTION, kOsCrOS,
        SINGLE_VALUE_TYPE(ash::switches::kAshEnableMirroredScreen),
    },
    {
        "ash-stable-overview-order", IDS_FLAGS_ASH_STABLE_OVERVIEW_ORDER_NAME,
        IDS_FLAGS_ASH_STABLE_OVERVIEW_ORDER_DESCRIPTION, kOsCrOS,
        ENABLE_DISABLE_VALUE_TYPE(
            ash::switches::kAshEnableStableOverviewOrder,
            ash::switches::kAshDisableStableOverviewOrder),
    },
#endif  // defined(USE_ASH)
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
#endif
    {"debug-packed-apps", IDS_FLAGS_DEBUG_PACKED_APP_NAME,
     IDS_FLAGS_DEBUG_PACKED_APP_DESCRIPTION, kOsDesktop,
     SINGLE_VALUE_TYPE(switches::kDebugPackedApps)},
    {"enable-password-generation", IDS_FLAGS_PASSWORD_GENERATION_NAME,
     IDS_FLAGS_PASSWORD_GENERATION_DESCRIPTION,
     kOsWin | kOsLinux | kOsCrOS | kOsMac | kOsAndroid,
     ENABLE_DISABLE_VALUE_TYPE(autofill::switches::kEnablePasswordGeneration,
                               autofill::switches::kDisablePasswordGeneration)},
    {"enable-automatic-password-saving",
     IDS_FLAGS_AUTOMATIC_PASSWORD_SAVING_NAME,
     IDS_FLAGS_AUTOMATIC_PASSWORD_SAVING_DESCRIPTION, kOsDesktop,
     FEATURE_VALUE_TYPE(
         password_manager::features::kEnableAutomaticPasswordSaving)},
    {"enable-password-change-support", IDS_FLAGS_PASSWORD_CHANGE_SUPPORT_NAME,
     IDS_FLAGS_PASSWORD_CHANGE_SUPPORT_DESCRIPTION, kOsAndroid,
     FEATURE_VALUE_TYPE(
         password_manager::features::kEnablePasswordChangeSupport)},
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
     IDS_FLAGS_AFFILIATION_BASED_MATCHING_DESCRIPTION,
     kOsWin | kOsLinux | kOsCrOS | kOsMac | kOsAndroid,
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
#endif
    {"scroll-end-effect", IDS_FLAGS_SCROLL_END_EFFECT_NAME,
     IDS_FLAGS_SCROLL_END_EFFECT_DESCRIPTION, kOsCrOS,
     ENABLE_DISABLE_VALUE_TYPE_AND_VALUE(switches::kScrollEndEffect,
                                         "1",
                                         switches::kScrollEndEffect,
                                         "0")},
    {"enable-icon-ntp", IDS_FLAGS_ICON_NTP_NAME, IDS_FLAGS_ICON_NTP_DESCRIPTION,
     kOsAndroid | kOsMac | kOsWin | kOsLinux | kOsCrOS,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableIconNtp,
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
    {"enable-non-validating-reload-on-refresh-content",
     IDS_FLAGS_ENABLE_NON_VALIDATING_RELOAD_ON_REFRESH_CONTENT_NAME,
     IDS_FLAGS_ENABLE_NON_VALIDATING_RELOAD_ON_REFRESH_CONTENT_DESCRIPTION,
     kOsAndroid,
     FEATURE_VALUE_TYPE(features::kNonValidatingReloadOnRefreshContent)},
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
    {"enable-supervised-user-managed-bookmarks-folder",
     IDS_FLAGS_SUPERVISED_USER_MANAGED_BOOKMARKS_FOLDER_NAME,
     IDS_FLAGS_SUPERVISED_USER_MANAGED_BOOKMARKS_FOLDER_DESCRIPTION,
     kOsAndroid | kOsMac | kOsWin | kOsLinux | kOsCrOS,
     SINGLE_VALUE_TYPE(switches::kEnableSupervisedUserManagedBookmarksFolder)},
#if defined(ENABLE_APP_LIST)
    {"enable-sync-app-list", IDS_FLAGS_SYNC_APP_LIST_NAME,
     IDS_FLAGS_SYNC_APP_LIST_DESCRIPTION, kOsDesktop,
     ENABLE_DISABLE_VALUE_TYPE(app_list::switches::kEnableSyncAppList,
                               app_list::switches::kDisableSyncAppList)},
#endif
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
#endif
    {"enable-simple-cache-backend", IDS_FLAGS_SIMPLE_CACHE_BACKEND_NAME,
     IDS_FLAGS_SIMPLE_CACHE_BACKEND_DESCRIPTION,
     kOsWin | kOsMac | kOsLinux | kOsCrOS,
     MULTI_VALUE_TYPE(kSimpleCacheBackendChoices)},
    {"enable-tcp-fast-open", IDS_FLAGS_TCP_FAST_OPEN_NAME,
     IDS_FLAGS_TCP_FAST_OPEN_DESCRIPTION, kOsLinux | kOsCrOS | kOsAndroid,
     SINGLE_VALUE_TYPE(switches::kEnableTcpFastOpen)},
#if defined(ENABLE_SERVICE_DISCOVERY)
    {"device-discovery-notifications",
     IDS_FLAGS_DEVICE_DISCOVERY_NOTIFICATIONS_NAME,
     IDS_FLAGS_DEVICE_DISCOVERY_NOTIFICATIONS_DESCRIPTION, kOsDesktop,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableDeviceDiscoveryNotifications,
                               switches::kDisableDeviceDiscoveryNotifications)},
    {"enable-print-preview-register-promos",
     IDS_FLAGS_PRINT_PREVIEW_REGISTER_PROMOS_NAME,
     IDS_FLAGS_PRINT_PREVIEW_REGISTER_PROMOS_DESCRIPTION, kOsDesktop,
     SINGLE_VALUE_TYPE(switches::kEnablePrintPreviewRegisterPromos)},
#endif  // ENABLE_SERVICE_DISCOVERY
#if defined(ENABLE_PRINT_PREVIEW)
    {"enable-print-preview-simplify", IDS_FLAGS_DISTILLER_IN_PRINT_PREVIEW_NAME,
     IDS_FLAGS_DISTILLER_IN_PRINT_PREVIEW_DESCRIPTION, kOsDesktop,
     FEATURE_VALUE_TYPE(PrintPreviewDistiller::kFeature)},
#endif
#if defined(OS_WIN)
    {"enable-cloud-print-xps", IDS_FLAGS_CLOUD_PRINT_XPS_NAME,
     IDS_FLAGS_CLOUD_PRINT_XPS_DESCRIPTION, kOsWin,
     SINGLE_VALUE_TYPE(switches::kEnableCloudPrintXps)},
#endif
#if defined(TOOLKIT_VIEWS)
    {"disable-hide-inactive-stacked-tab-close-buttons",
     IDS_FLAGS_HIDE_INACTIVE_STACKED_TAB_CLOSE_BUTTONS_NAME,
     IDS_FLAGS_HIDE_INACTIVE_STACKED_TAB_CLOSE_BUTTONS_DESCRIPTION,
     kOsCrOS | kOsWin | kOsLinux,
     SINGLE_DISABLE_VALUE_TYPE(
         switches::kDisableHideInactiveStackedTabCloseButtons)},
#endif
#if defined(ENABLE_SPELLCHECK)
    {"enable-spelling-feedback-field-trial",
     IDS_FLAGS_SPELLING_FEEDBACK_FIELD_TRIAL_NAME,
     IDS_FLAGS_SPELLING_FEEDBACK_FIELD_TRIAL_DESCRIPTION, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableSpellingFeedbackFieldTrial)},
#endif
    {"enable-webgl-draft-extensions", IDS_FLAGS_WEBGL_DRAFT_EXTENSIONS_NAME,
     IDS_FLAGS_WEBGL_DRAFT_EXTENSIONS_DESCRIPTION, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableWebGLDraftExtensions)},
    {"enable-new-profile-management", IDS_FLAGS_NEW_PROFILE_MANAGEMENT_NAME,
     IDS_FLAGS_NEW_PROFILE_MANAGEMENT_DESCRIPTION,
     kOsAndroid | kOsMac | kOsWin | kOsLinux | kOsCrOS,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableNewProfileManagement,
                               switches::kDisableNewProfileManagement)},
    {"enable-account-consistency", IDS_FLAGS_ACCOUNT_CONSISTENCY_NAME,
     IDS_FLAGS_ACCOUNT_CONSISTENCY_DESCRIPTION,
     kOsAndroid | kOsMac | kOsWin | kOsLinux | kOsCrOS,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableAccountConsistency,
                               switches::kDisableAccountConsistency)},
    {"enable-password-separated-signin-flow",
     IDS_FLAGS_ENABLE_PASSWORD_SEPARATED_SIGNIN_FLOW_NAME,
     IDS_FLAGS_ENABLE_PASSWORD_SEPARATED_SIGNIN_FLOW_DESCRIPTION,
     kOsMac | kOsWin | kOsLinux,
     FEATURE_VALUE_TYPE(switches::kUsePasswordSeparatedSigninFlow)},
    {"enable-material-design-user-manager",
     IDS_FLAGS_ENABLE_MATERIAL_DESIGN_USER_MANAGER_NAME,
     IDS_FLAGS_ENABLE_MATERIAL_DESIGN_USER_MANAGER_DESCRIPTION,
     kOsMac | kOsWin | kOsLinux,
     FEATURE_VALUE_TYPE(switches::kMaterialDesignUserManager)},
    {"enable-google-profile-info", IDS_FLAGS_GOOGLE_PROFILE_INFO_NAME,
     IDS_FLAGS_GOOGLE_PROFILE_INFO_DESCRIPTION, kOsMac | kOsWin | kOsLinux,
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
     IDS_FLAGS_ACCESSIBILITY_TAB_SWITCHER_NAME,
     IDS_FLAGS_ACCESSIBILITY_TAB_SWITCHER_DESCRIPTION, kOsAndroid,
     SINGLE_VALUE_TYPE(switches::kEnableAccessibilityTabSwitcher)},
    {"enable-physical-web", IDS_FLAGS_ENABLE_PHYSICAL_WEB_NAME,
     IDS_FLAGS_ENABLE_PHYSICAL_WEB_DESCRIPTION, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kPhysicalWebFeature)},
#endif
    {"enable-zero-copy", IDS_FLAGS_ZERO_COPY_NAME,
     IDS_FLAGS_ZERO_COPY_DESCRIPTION, kOsAll,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableZeroCopy,
                               switches::kDisableZeroCopy)},
#if defined(OS_CHROMEOS)
    {"enable-first-run-ui-transitions", IDS_FLAGS_FIRST_RUN_UI_TRANSITIONS_NAME,
     IDS_FLAGS_FIRST_RUN_UI_TRANSITIONS_DESCRIPTION, kOsCrOS,
     SINGLE_VALUE_TYPE(chromeos::switches::kEnableFirstRunUITransitions)},
#endif
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
#endif
#if defined(OS_ANDROID)
    {"disable-pull-to-refresh-effect", IDS_FLAGS_PULL_TO_REFRESH_EFFECT_NAME,
     IDS_FLAGS_PULL_TO_REFRESH_EFFECT_DESCRIPTION, kOsAndroid,
     SINGLE_DISABLE_VALUE_TYPE(switches::kDisablePullToRefreshEffect)},
#endif
#if defined(OS_MACOSX)
    {"enable-translate-new-ux", IDS_FLAGS_TRANSLATE_NEW_UX_NAME,
     IDS_FLAGS_TRANSLATE_NEW_UX_DESCRIPTION, kOsMac,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableTranslateNewUX,
                               switches::kDisableTranslateNewUX)},
#endif
#if defined(OS_MACOSX)
    {"enable-native-notifications", IDS_NOTIFICATIONS_NATIVE_FLAG,
     IDS_NOTIFICATIONS_NATIVE_FLAG_DESCRIPTION, kOsMac,
     SINGLE_VALUE_TYPE(switches::kEnableNativeNotifications)},
#endif
#if defined(TOOLKIT_VIEWS)
    {"disable-views-rect-based-targeting",  // FLAGS:RECORD_UMA
     IDS_FLAGS_VIEWS_RECT_BASED_TARGETING_NAME,
     IDS_FLAGS_VIEWS_RECT_BASED_TARGETING_DESCRIPTION,
     kOsCrOS | kOsWin | kOsLinux,
     SINGLE_DISABLE_VALUE_TYPE(
         views::switches::kDisableViewsRectBasedTargeting)},
    {"enable-link-disambiguation-popup",
     IDS_FLAGS_LINK_DISAMBIGUATION_POPUP_NAME,
     IDS_FLAGS_LINK_DISAMBIGUATION_POPUP_DESCRIPTION, kOsCrOS | kOsWin,
     SINGLE_VALUE_TYPE(switches::kEnableLinkDisambiguationPopup)},
#endif
#if defined(ENABLE_EXTENSIONS)
    {"enable-apps-show-on-first-paint", IDS_FLAGS_APPS_SHOW_ON_FIRST_PAINT_NAME,
     IDS_FLAGS_APPS_SHOW_ON_FIRST_PAINT_DESCRIPTION, kOsDesktop,
     SINGLE_VALUE_TYPE(extensions::switches::kEnableAppsShowOnFirstPaint)},
#endif
#if defined(OS_ANDROID)
    {"reader-mode-heuristics", IDS_FLAGS_READER_MODE_HEURISTICS_NAME,
     IDS_FLAGS_READER_MODE_HEURISTICS_DESCRIPTION, kOsAndroid,
     MULTI_VALUE_TYPE(kReaderModeHeuristicsChoices)},
#endif
    {"num-raster-threads", IDS_FLAGS_NUM_RASTER_THREADS_NAME,
     IDS_FLAGS_NUM_RASTER_THREADS_DESCRIPTION, kOsAll,
     MULTI_VALUE_TYPE(kNumRasterThreadsChoices)},
    {"enable-permissions-blacklist", IDS_FLAGS_PERMISSIONS_BLACKLIST_NAME,
     IDS_FLAGS_PERMISSIONS_BLACKLIST_DESCRIPTION, kOsAll,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnablePermissionsBlacklist,
                               switches::kDisablePermissionsBlacklist)},
    {"enable-single-click-autofill", IDS_FLAGS_SINGLE_CLICK_AUTOFILL_NAME,
     IDS_FLAGS_SINGLE_CLICK_AUTOFILL_DESCRIPTION,
     kOsCrOS | kOsMac | kOsWin | kOsLinux | kOsAndroid,
     ENABLE_DISABLE_VALUE_TYPE(
         autofill::switches::kEnableSingleClickAutofill,
         autofill::switches::kDisableSingleClickAutofill)},
    {"enable-site-engagement-service", IDS_FLAGS_SITE_ENGAGEMENT_SERVICE_NAME,
     IDS_FLAGS_SITE_ENGAGEMENT_SERVICE_DESCRIPTION, kOsAll,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableSiteEngagementService,
                               switches::kDisableSiteEngagementService)},
    {"enable-session-crashed-bubble", IDS_FLAGS_SESSION_CRASHED_BUBBLE_NAME,
     IDS_FLAGS_SESSION_CRASHED_BUBBLE_DESCRIPTION, kOsWin | kOsLinux,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableSessionCrashedBubble,
                               switches::kDisableSessionCrashedBubble)},
    {"disable-cast-streaming-hw-encoding",
     IDS_FLAGS_CAST_STREAMING_HW_ENCODING_NAME,
     IDS_FLAGS_CAST_STREAMING_HW_ENCODING_DESCRIPTION, kOsAll,
     SINGLE_DISABLE_VALUE_TYPE(switches::kDisableCastStreamingHWEncoding)},
#if defined(OS_ANDROID)
    {"prefetch-search-results", IDS_FLAGS_PREFETCH_SEARCH_RESULTS_NAME,
     IDS_FLAGS_PREFETCH_SEARCH_RESULTS_DESCRIPTION, kOsAndroid,
     SINGLE_VALUE_TYPE(switches::kPrefetchSearchResults)},
#endif
#if defined(ENABLE_APP_LIST)
    {"enable-experimental-app-list", IDS_FLAGS_EXPERIMENTAL_APP_LIST_NAME,
     IDS_FLAGS_EXPERIMENTAL_APP_LIST_DESCRIPTION, kOsWin | kOsLinux | kOsCrOS,
     ENABLE_DISABLE_VALUE_TYPE(
         app_list::switches::kEnableExperimentalAppList,
         app_list::switches::kDisableExperimentalAppList)},
    {"enable-centered-app-list", IDS_FLAGS_CENTERED_APP_LIST_NAME,
     IDS_FLAGS_CENTERED_APP_LIST_DESCRIPTION, kOsWin | kOsLinux | kOsCrOS,
     SINGLE_VALUE_TYPE(app_list::switches::kEnableCenteredAppList)},
    {"enable-new-app-list-mixer", IDS_FLAGS_NEW_APP_LIST_MIXER_NAME,
     IDS_FLAGS_NEW_APP_LIST_MIXER_DESCRIPTION,
     kOsWin | kOsLinux | kOsCrOS | kOsMac,
     ENABLE_DISABLE_VALUE_TYPE(app_list::switches::kEnableNewAppListMixer,
                               app_list::switches::kDisableNewAppListMixer)},
#endif
    {"disable-threaded-scrolling", IDS_FLAGS_THREADED_SCROLLING_NAME,
     IDS_FLAGS_THREADED_SCROLLING_DESCRIPTION,
     kOsWin | kOsLinux | kOsCrOS | kOsAndroid | kOsMac,
     SINGLE_DISABLE_VALUE_TYPE(switches::kDisableThreadedScrolling)},
    {"enable-settings-window", IDS_FLAGS_SETTINGS_WINDOW_NAME,
     IDS_FLAGS_SETTINGS_WINDOW_DESCRIPTION, kOsDesktop,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableSettingsWindow,
                               switches::kDisableSettingsWindow)},
    {"inert-visual-viewport", IDS_FLAGS_INERT_VISUAL_VIEWPORT_NAME,
     IDS_FLAGS_INERT_VISUAL_VIEWPORT_DESCRIPTION, kOsAll,
     SINGLE_VALUE_TYPE(switches::kInertVisualViewport)},
#if defined(OS_MACOSX)
    {"enable-save-password-bubble", IDS_FLAGS_SAVE_PASSWORD_BUBBLE_NAME,
     IDS_FLAGS_SAVE_PASSWORD_BUBBLE_DESCRIPTION, kOsMac,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableSavePasswordBubble,
                               switches::kDisableSavePasswordBubble)},
#endif
    {"enable-apps-file-associations", IDS_FLAGS_APPS_FILE_ASSOCIATIONS_NAME,
     IDS_FLAGS_APPS_FILE_ASSOCIATIONS_DESCRIPTION, kOsMac,
     SINGLE_VALUE_TYPE(switches::kEnableAppsFileAssociations)},
#if defined(OS_ANDROID)
    {"enable-embeddedsearch-api", IDS_FLAGS_EMBEDDEDSEARCH_API_NAME,
     IDS_FLAGS_EMBEDDEDSEARCH_API_DESCRIPTION, kOsAndroid,
     SINGLE_VALUE_TYPE(switches::kEnableEmbeddedSearchAPI)},
#endif
    {"distance-field-text", IDS_FLAGS_DISTANCE_FIELD_TEXT_NAME,
     IDS_FLAGS_DISTANCE_FIELD_TEXT_DESCRIPTION, kOsAll,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableDistanceFieldText,
                               switches::kDisableDistanceFieldText)},
    {"extension-content-verification",
     IDS_FLAGS_EXTENSION_CONTENT_VERIFICATION_NAME,
     IDS_FLAGS_EXTENSION_CONTENT_VERIFICATION_DESCRIPTION, kOsDesktop,
     MULTI_VALUE_TYPE(kExtensionContentVerificationChoices)},
#if defined(ENABLE_EXTENSIONS)
    {"extension-active-script-permission",
     IDS_FLAGS_USER_CONSENT_FOR_EXTENSION_SCRIPTS_NAME,
     IDS_FLAGS_USER_CONSENT_FOR_EXTENSION_SCRIPTS_DESCRIPTION, kOsAll,
     SINGLE_VALUE_TYPE(extensions::switches::kEnableScriptsRequireAction)},
#endif
#if defined(OS_ANDROID)
    {"enable-data-reduction-proxy-carrier-test",
     IDS_FLAGS_DATA_REDUCTION_PROXY_CARRIER_TEST_NAME,
     IDS_FLAGS_DATA_REDUCTION_PROXY_CARRIER_TEST_DESCRIPTION, kOsAndroid,
     SINGLE_VALUE_TYPE(
         data_reduction_proxy::switches::kEnableDataReductionProxyCarrierTest)},
#endif
    {"enable-hotword-hardware", IDS_FLAGS_EXPERIMENTAL_HOTWORD_HARDWARE_NAME,
     IDS_FLAGS_EXPERIMENTAL_HOTWORD_HARDWARE_DESCRIPTION, kOsCrOS,
     SINGLE_VALUE_TYPE(switches::kEnableExperimentalHotwordHardware)},
#if defined(ENABLE_EXTENSIONS)
    {"enable-embedded-extension-options",
     IDS_FLAGS_EMBEDDED_EXTENSION_OPTIONS_NAME,
     IDS_FLAGS_EMBEDDED_EXTENSION_OPTIONS_DESCRIPTION, kOsDesktop,
     SINGLE_VALUE_TYPE(extensions::switches::kEnableEmbeddedExtensionOptions)},
#endif
#if defined(USE_ASH)
    {"enable-web-app-frame", IDS_FLAGS_WEB_APP_FRAME_NAME,
     IDS_FLAGS_WEB_APP_FRAME_DESCRIPTION, kOsWin | kOsCrOS,
     SINGLE_VALUE_TYPE(switches::kEnableWebAppFrame)},
#endif
    {"drop-sync-credential", IDS_FLAGS_DROP_SYNC_CREDENTIAL_NAME,
     IDS_FLAGS_DROP_SYNC_CREDENTIAL_DESCRIPTION, kOsAll,
     FEATURE_VALUE_TYPE(password_manager::features::kDropSyncCredential)},
#if defined(ENABLE_EXTENSIONS)
    {"enable-extension-action-redesign",
     IDS_FLAGS_EXTENSION_ACTION_REDESIGN_NAME,
     IDS_FLAGS_EXTENSION_ACTION_REDESIGN_DESCRIPTION, kOsDesktop,
     SINGLE_VALUE_TYPE(extensions::switches::kEnableExtensionActionRedesign)},
#endif
#if !defined(OS_ANDROID)
    {"enable-message-center-always-scroll-up-upon-notification-removal",
     IDS_FLAGS_MESSAGE_CENTER_ALWAYS_SCROLL_UP_UPON_REMOVAL_NAME,
     IDS_FLAGS_MESSAGE_CENTER_ALWAYS_SCROLL_UP_UPON_REMOVAL_DESCRIPTION,
     kOsDesktop,
     SINGLE_VALUE_TYPE(
         switches::kEnableMessageCenterAlwaysScrollUpUponNotificationRemoval)},
#endif
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
#endif  // defined(OS_CHROMEOS)
    {"enable-credit-card-scan", IDS_FLAGS_CREDIT_CARD_SCAN_NAME,
     IDS_FLAGS_CREDIT_CARD_SCAN_DESCRIPTION, kOsAndroid,
     ENABLE_DISABLE_VALUE_TYPE(autofill::switches::kEnableCreditCardScan,
                               autofill::switches::kDisableCreditCardScan)},
#if defined(OS_CHROMEOS)
    {"disable-captive-portal-bypass-proxy",
     IDS_FLAGS_CAPTIVE_PORTAL_BYPASS_PROXY_NAME,
     IDS_FLAGS_CAPTIVE_PORTAL_BYPASS_PROXY_DESCRIPTION, kOsCrOS,
     SINGLE_DISABLE_VALUE_TYPE(
         chromeos::switches::kDisableCaptivePortalBypassProxy)},
#endif  // defined(OS_CHROMEOS)
#if defined(OS_ANDROID)
    {"enable-seccomp-sandbox-android",
     IDS_FLAGS_SECCOMP_FILTER_SANDBOX_ANDROID_NAME,
     IDS_FLAGS_SECCOMP_FILTER_SANDBOX_ANDROID_DESCRIPTION, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kSeccompSandboxAndroid)},
#endif
    {"enable-touch-hover", IDS_FLAGS_TOUCH_HOVER_NAME,
     IDS_FLAGS_TOUCH_HOVER_DESCRIPTION, kOsAndroid,
     SINGLE_VALUE_TYPE("enable-touch-hover")},
    {"enable-fill-on-account-select", IDS_FILL_ON_ACCOUNT_SELECT_NAME,
     IDS_FILL_ON_ACCOUNT_SELECT_DESCRIPTION, kOsAll,
     MULTI_VALUE_TYPE(kFillOnAccountSelectChoices)},
#if defined(OS_CHROMEOS)
    {"enable-wifi-credential-sync",  // FLAGS:RECORD_UMA
     IDS_FLAGS_WIFI_CREDENTIAL_SYNC_NAME,
     IDS_FLAGS_WIFI_CREDENTIAL_SYNC_DESCRIPTION, kOsCrOS,
     SINGLE_VALUE_TYPE(switches::kEnableWifiCredentialSync)},
    {"enable-potentially-annoying-security-features",
     IDS_FLAGS_EXPERIMENTAL_SECURITY_FEATURES_NAME,
     IDS_FLAGS_EXPERIMENTAL_SECURITY_FEATURES_DESCRIPTION, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnablePotentiallyAnnoyingSecurityFeatures)},
#endif
    {"mark-non-secure-as",  // FLAGS:RECORD_UMA
     IDS_MARK_NON_SECURE_AS_NAME, IDS_MARK_NON_SECURE_AS_DESCRIPTION, kOsAll,
     MULTI_VALUE_TYPE(kMarkNonSecureAsChoices)},
    {"enable-site-per-process", IDS_FLAGS_SITE_PER_PROCESS_NAME,
     IDS_FLAGS_SITE_PER_PROCESS_DESCRIPTION, kOsAll,
     SINGLE_VALUE_TYPE(switches::kSitePerProcess)},
    {"enable-top-document-isolation", IDS_FLAGS_TOP_DOCUMENT_ISOLATION_NAME,
     IDS_FLAGS_TOP_DOCUMENT_ISOLATION_DESCRIPTION, kOsAll,
     SINGLE_VALUE_TYPE(switches::kTopDocumentIsolation)},
    {"enable-use-zoom-for-dsf", IDS_FLAGS_ENABLE_USE_ZOOM_FOR_DSF_NAME,
     IDS_FLAGS_ENABLE_USE_ZOOM_FOR_DSF_DESCRIPTION, kOsDesktop,
     MULTI_VALUE_TYPE(kEnableUseZoomForDSFChoices)},
#if defined(OS_MACOSX)
    {"enable-harfbuzz-rendertext", IDS_FLAGS_HARFBUZZ_RENDERTEXT_NAME,
     IDS_FLAGS_HARFBUZZ_RENDERTEXT_DESCRIPTION, kOsMac,
     SINGLE_VALUE_TYPE(switches::kEnableHarfBuzzRenderText)},
#endif  // defined(OS_MACOSX)
#if defined(OS_CHROMEOS)
    {"disable-timezone-tracking",
     IDS_FLAGS_RESOLVE_TIMEZONE_BY_GEOLOCATION_NAME,
     IDS_FLAGS_RESOLVE_TIMEZONE_BY_GEOLOCATION_DESCRIPTION, kOsCrOS,
     SINGLE_DISABLE_VALUE_TYPE(
         chromeos::switches::kDisableTimeZoneTrackingOption)},
#endif  // defined(OS_CHROMEOS)
    {"data-reduction-proxy-lo-fi", IDS_FLAGS_DATA_REDUCTION_PROXY_LO_FI_NAME,
     IDS_FLAGS_DATA_REDUCTION_PROXY_LO_FI_DESCRIPTION, kOsAll,
     MULTI_VALUE_TYPE(kDataReductionProxyLoFiChoices)},
    {"enable-data-reduction-proxy-lo-fi-preview",
     IDS_FLAGS_ENABLE_DATA_REDUCTION_PROXY_LO_FI_PREVIEW_NAME,
     IDS_FLAGS_ENABLE_DATA_REDUCTION_PROXY_LO_FI_PREVIEW_DESCRIPTION, kOsAll,
     SINGLE_VALUE_TYPE(
         data_reduction_proxy::switches::kEnableDataReductionProxyLoFiPreview)},
    {"clear-data-reduction-proxy-data-savings",
     IDS_FLAGS_DATA_REDUCTION_PROXY_RESET_SAVINGS_NAME,
     IDS_FLAGS_DATA_REDUCTION_PROXY_RESET_SAVINGS_DESCRIPTION, kOsAll,
     SINGLE_VALUE_TYPE(
         data_reduction_proxy::switches::kClearDataReductionProxyDataSavings)},
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
    {"enable-child-account-detection", IDS_FLAGS_CHILD_ACCOUNT_DETECTION_NAME,
     IDS_FLAGS_CHILD_ACCOUNT_DETECTION_DESCRIPTION,
     kOsAndroid | kOsMac | kOsWin | kOsLinux | kOsCrOS,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableChildAccountDetection,
                               switches::kDisableChildAccountDetection)},
#if defined(OS_CHROMEOS) && defined(USE_OZONE)
    {"ozone-test-single-overlay-support",
     IDS_FLAGS_OZONE_TEST_SINGLE_HARDWARE_OVERLAY,
     IDS_FLAGS_OZONE_TEST_SINGLE_HARDWARE_OVERLAY_DESCRIPTION, kOsCrOS,
     SINGLE_VALUE_TYPE(switches::kOzoneTestSingleOverlaySupport)},
#endif  // defined(OS_CHROMEOS) && defined(USE_OZONE)
    {"v8-pac-mojo-out-of-process", IDS_FLAGS_V8_PAC_MOJO_OUT_OF_PROCESS_NAME,
     IDS_FLAGS_V8_PAC_MOJO_OUT_OF_PROCESS_DESCRIPTION, kOsDesktop,
     ENABLE_DISABLE_VALUE_TYPE(switches::kV8PacMojoOutOfProcess,
                               switches::kDisableOutOfProcessPac)},
#if defined(ENABLE_MEDIA_ROUTER) && !defined(OS_ANDROID)
    {"media-router", IDS_FLAGS_MEDIA_ROUTER_NAME,
     IDS_FLAGS_MEDIA_ROUTER_DESCRIPTION, kOsDesktop,
     ENABLE_DISABLE_VALUE_TYPE_AND_VALUE(switches::kMediaRouter,
                                         "1",
                                         switches::kMediaRouter,
                                         "0")},
#endif  // defined(ENABLE_MEDIA_ROUTER) && !defined(OS_ANDROID)
// Since Drive Search is not available when app list is disabled, flag guard
// enable-drive-search-in-chrome-launcher flag.
#if defined(ENABLE_APP_LIST)
    {"enable-drive-search-in-app-launcher",
     IDS_FLAGS_DRIVE_SEARCH_IN_CHROME_LAUNCHER,
     IDS_FLAGS_DRIVE_SEARCH_IN_CHROME_LAUNCHER_DESCRIPTION, kOsCrOS,
     ENABLE_DISABLE_VALUE_TYPE(
         app_list::switches::kEnableDriveSearchInChromeLauncher,
         app_list::switches::kDisableDriveSearchInChromeLauncher)},
#endif  // defined(ENABLE_APP_LIST)
#if defined(OS_CHROMEOS)
    {"disable-mtp-write-support", IDS_FLAGS_MTP_WRITE_SUPPORT_NAME,
     IDS_FLAGS_MTP_WRITE_SUPPORT_DESCRIPTION, kOsCrOS,
     SINGLE_DISABLE_VALUE_TYPE(chromeos::switches::kDisableMtpWriteSupport)},
#endif  // defined(OS_CHROMEOS)
#if defined(OS_CHROMEOS)
    {"enable-datasaver-prompt", IDS_FLAGS_DATASAVER_PROMPT_NAME,
     IDS_FLAGS_DATASAVER_PROMPT_DESCRIPTION, kOsCrOS,
     MULTI_VALUE_TYPE(kDataSaverPromptChoices)},
#endif  // defined(OS_CHROMEOS)
    {"supervised-user-safesites", IDS_FLAGS_SUPERVISED_USER_SAFESITES_NAME,
     IDS_FLAGS_SUPERVISED_USER_SAFESITES_DESCRIPTION,
     kOsAndroid | kOsMac | kOsWin | kOsLinux | kOsCrOS,
     MULTI_VALUE_TYPE(kSupervisedUserSafeSitesChoices)},
#if defined(OS_ANDROID)
    {"enable-autofill-keyboard-accessory-view",
     IDS_FLAGS_AUTOFILL_ACCESSORY_VIEW_NAME,
     IDS_FLAGS_AUTOFILL_ACCESSORY_VIEW_DESCRIPTION, kOsAndroid,
     ENABLE_DISABLE_VALUE_TYPE(
         autofill::switches::kEnableAccessorySuggestionView,
         autofill::switches::kDisableAccessorySuggestionView)},
#endif  // defined(OS_ANDROID)
#if defined(OS_WIN)
    {"try-supported-channel-layouts",
     IDS_FLAGS_TRY_SUPPORTED_CHANNEL_LAYOUTS_NAME,
     IDS_FLAGS_TRY_SUPPORTED_CHANNEL_LAYOUTS_DESCRIPTION, kOsWin,
     SINGLE_VALUE_TYPE(switches::kTrySupportedChannelLayouts)},
#endif
#if defined(ENABLE_WEBRTC)
    {"enable-webrtc-dtls12", IDS_FLAGS_WEBRTC_DTLS12_NAME,
     IDS_FLAGS_WEBRTC_DTLS12_DESCRIPTION, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableWebRtcDtls12)},
#endif
#if defined(ENABLE_WEBRTC)
    {"enable-webrtc-ecdsa",
     IDS_FLAGS_WEBRTC_ECDSA_NAME,
     IDS_FLAGS_WEBRTC_ECDSA_DESCRIPTION, kOsAll,
     FEATURE_VALUE_TYPE(features::kWebRtcEcdsaDefault)},
#endif
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
    {"mac-views-dialogs", IDS_FLAGS_MAC_VIEWS_DIALOGS_NAME,
     IDS_FLAGS_MAC_VIEWS_DIALOGS_DESCRIPTION, kOsMac,
     SINGLE_VALUE_TYPE(switches::kEnableMacViewsDialogs)},
#endif
#if defined(ENABLE_WEBVR)
    {"enable-webvr", IDS_FLAGS_WEBVR_NAME, IDS_FLAGS_WEBVR_DESCRIPTION, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableWebVR)},
#endif
#if defined(OS_CHROMEOS)
    {"disable-accelerated-mjpeg-decode",
     IDS_FLAGS_ACCELERATED_MJPEG_DECODE_NAME,
     IDS_FLAGS_ACCELERATED_MJPEG_DECODE_DESCRIPTION, kOsCrOS,
     SINGLE_DISABLE_VALUE_TYPE(switches::kDisableAcceleratedMjpegDecode)},
#endif  // OS_CHROMEOS
    {"v8-cache-options", IDS_FLAGS_V8_CACHE_OPTIONS_NAME,
     IDS_FLAGS_V8_CACHE_OPTIONS_DESCRIPTION, kOsAll,
     MULTI_VALUE_TYPE(kV8CacheOptionsChoices)},
    {"enable-clear-browsing-data-counters",
     IDS_FLAGS_ENABLE_CLEAR_BROWSING_DATA_COUNTERS_NAME,
     IDS_FLAGS_ENABLE_CLEAR_BROWSING_DATA_COUNTERS_DESCRIPTION, kOsAll,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableClearBrowsingDataCounters,
                               switches::kDisableClearBrowsingDataCounters)},
#if defined(ENABLE_TASK_MANAGER)
    {"disable-new-task-manager", IDS_FLAGS_NEW_TASK_MANAGER_NAME,
     IDS_FLAGS_NEW_TASK_MANAGER_DESCRIPTION, kOsDesktop,
     SINGLE_DISABLE_VALUE_TYPE(switches::kDisableNewTaskManager)},
#endif  // defined(ENABLE_TASK_MANAGER)
    {"simplified-fullscreen-ui", IDS_FLAGS_SIMPLIFIED_FULLSCREEN_UI_NAME,
     IDS_FLAGS_SIMPLIFIED_FULLSCREEN_UI_DESCRIPTION, kOsAll,
     FEATURE_VALUE_TYPE(features::kSimplifiedFullscreenUI)},
    {"experimental-keyboard-lock-ui",
     IDS_FLAGS_EXPERIMENTAL_KEYBOARD_LOCK_UI_NAME,
     IDS_FLAGS_EXPERIMENTAL_KEYBOARD_LOCK_UI_DESCRIPTION, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kExperimentalKeyboardLockUI)},
#if defined(OS_ANDROID)
    {"progress-bar-animation", IDS_FLAGS_PROGRESS_BAR_ANIMATION_NAME,
     IDS_FLAGS_PROGRESS_BAR_ANIMATION_DESCRIPTION, kOsAndroid,
     MULTI_VALUE_TYPE(kProgressBarAnimationChoices)},
#endif  // defined(OS_ANDROID)
#if defined(OS_ANDROID)
    {"offline-pages-mode", IDS_FLAGS_OFFLINE_PAGES_NAME,
     IDS_FLAGS_OFFLINE_PAGES_DESCRIPTION, kOsAndroid,
     MULTI_VALUE_TYPE(kEnableOfflinePagesChoices)},
    {"offline-pages-background-loading",
     IDS_FLAGS_OFFLINE_PAGES_BACKGROUND_LOADING_NAME,
     IDS_FLAGS_OFFLINE_PAGES_BACKGROUND_LOADING_DESCRIPTION, kOsAndroid,
     FEATURE_VALUE_TYPE(offline_pages::kOfflinePagesBackgroundLoadingFeature)},
#endif  // defined(OS_ANDROID)
    {"disallow-doc-written-script-loads",
     IDS_FLAGS_DISALLOW_DOC_WRITTEN_SCRIPTS_UI_NAME,
     IDS_FLAGS_DISALLOW_DOC_WRITTEN_SCRIPTS_UI_DESCRIPTION, kOsAll,
     // NOTE: if we want to add additional experiment entries for other
     // features controlled by kBlinkSettings, we'll need to add logic to
     // merge the flag values.
     SINGLE_VALUE_TYPE_AND_VALUE(
         switches::kBlinkSettings,
         "disallowFetchForDocWrittenScriptsInMainFrame=true")},
#if defined(OS_ANDROID)
    {"enable-ntp-popular-sites", IDS_FLAGS_NTP_POPULAR_SITES_NAME,
     IDS_FLAGS_NTP_POPULAR_SITES_DESCRIPTION, kOsAndroid,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableNTPPopularSites,
                               switches::kDisableNTPPopularSites)},
    {"ntp-switch-to-existing-tab", IDS_FLAGS_NTP_SWITCH_TO_EXISTING_TAB_NAME,
     IDS_FLAGS_NTP_SWITCH_TO_EXISTING_TAB_DESCRIPTION, kOsAndroid,
     MULTI_VALUE_TYPE(kNtpSwitchToExistingTabChoices)},
    {"use-android-midi-api", IDS_FLAGS_USE_ANDROID_MIDI_API_NAME,
     IDS_FLAGS_USE_ANDROID_MIDI_API_DESCRIPTION, kOsAndroid,
     SINGLE_VALUE_TYPE(switches::kUseAndroidMidiApi)},
#endif  // defined(OS_ANDROID)
#if defined(OS_WIN)
    {"trace-export-events-to-etw", IDS_FLAGS_TRACE_EXPORT_EVENTS_TO_ETW_NAME,
     IDS_FLAGS_TRACE_EXPORT_EVENTS_TO_ETW_DESRIPTION, kOsWin,
     SINGLE_VALUE_TYPE(switches::kTraceExportEventsToETW)},
    {"merge-key-char-events", IDS_FLAGS_MERGE_KEY_CHAR_EVENTS_NAME,
     IDS_FLAGS_MERGE_KEY_CHAR_EVENTS_DESCRIPTION, kOsWin,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableMergeKeyCharEvents,
                               switches::kDisableMergeKeyCharEvents)},
#endif  // defined(OS_WIN)
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
#if defined(OS_WIN)
    {"enable-ppapi-win32k-lockdown", IDS_FLAGS_PPAPI_WIN32K_LOCKDOWN_NAME,
     IDS_FLAGS_PPAPI_WIN32K_LOCKDOWN_DESCRIPTION, kOsWin,
     MULTI_VALUE_TYPE(kPpapiWin32kLockdown)},
#endif  // defined(OS_WIN)
#if defined(ENABLE_NOTIFICATIONS) && defined(OS_ANDROID)
    {"enable-web-notification-custom-layouts",
     IDS_FLAGS_ENABLE_WEB_NOTIFICATION_CUSTOM_LAYOUTS_NAME,
     IDS_FLAGS_ENABLE_WEB_NOTIFICATION_CUSTOM_LAYOUTS_DESCRIPTION, kOsAndroid,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableWebNotificationCustomLayouts,
                               switches::kDisableWebNotificationCustomLayouts)},
#endif  // defined(ENABLE_NOTIFICATIONS) && defined(OS_ANDROID)
#if defined(OS_WIN)
    {"enable-appcontainer", IDS_FLAGS_ENABLE_APPCONTAINER_NAME,
     IDS_FLAGS_ENABLE_APPCONTAINER_DESCRIPTION, kOsWin,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableAppContainer,
                               switches::kDisableAppContainer)},
#endif  // defined(OS_WIN)
#if defined(TOOLKIT_VIEWS) || defined(OS_ANDROID)
    {"enable-autofill-credit-card-upload",
     IDS_FLAGS_AUTOFILL_CREDIT_CARD_UPLOAD_NAME,
     IDS_FLAGS_AUTOFILL_CREDIT_CARD_UPLOAD_DESCRIPTION,
     kOsCrOS | kOsWin | kOsLinux | kOsAndroid | kOsMac,
     ENABLE_DISABLE_VALUE_TYPE(
         autofill::switches::kEnableOfferUploadCreditCards,
         autofill::switches::kDisableOfferUploadCreditCards)},
#endif  // defined(TOOLKIT_VIEWS) || defined(OS_ANDROID)
#if defined(OS_ANDROID)
    {"tab-management-experiment-type", IDS_FLAGS_HERB_PROTOTYPE_CHOICES_NAME,
     IDS_FLAGS_HERB_PROTOTYPE_CHOICES_DESCRIPTION, kOsAndroid,
     MULTI_VALUE_TYPE(kHerbPrototypeChoices)},
    {"enable-tab-switcher-in-document-mode",
     IDS_FLAGS_TAB_SWITCHER_IN_DOCUMENT_MODE_NAME,
     IDS_FLAGS_TAB_SWITCHER_IN_DOCUMENT_MODE_DESCRIPTION, kOsAndroid,
     SINGLE_VALUE_TYPE(switches::kEnableTabSwitcherInDocumentMode)},
#endif  // OS_ANDROID
    {"enable-md-history", IDS_FLAGS_ENABLE_MATERIAL_DESIGN_HISTORY_NAME,
     IDS_FLAGS_ENABLE_MATERIAL_DESIGN_HISTORY_DESCRIPTION, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kMaterialDesignHistoryFeature)},
    {"safe-search-url-reporting", IDS_FLAGS_SAFE_SEARCH_URL_REPORTING_NAME,
     IDS_FLAGS_SAFE_SEARCH_URL_REPORTING_DESCRIPTION, kOsAll,
     FEATURE_VALUE_TYPE(features::kSafeSearchUrlReporting)},
#if defined(OS_WIN)
    {"enable-windows-desktop-search-redirection",
     IDS_FLAGS_WINDOWS_DESKTOP_SEARCH_REDIRECTION_NAME,
     IDS_FLAGS_WINDOWS_DESKTOP_SEARCH_REDIRECTION_DESCRIPTION, kOsWin,
     FEATURE_VALUE_TYPE(kDesktopSearchRedirectionFeature)},
#endif  // defined(OS_WIN)
    {"force-ui-direction", IDS_FLAGS_FORCE_UI_DIRECTION_NAME,
     IDS_FLAGS_FORCE_UI_DIRECTION_DESCRIPTION, kOsAll,
     MULTI_VALUE_TYPE(kForceUIDirectionChoices)},
#if defined(ENABLE_EXTENSIONS)
    {"enable-md-extensions", IDS_FLAGS_ENABLE_MATERIAL_DESIGN_EXTENSIONS_NAME,
     IDS_FLAGS_ENABLE_MATERIAL_DESIGN_EXTENSIONS_DESCRIPTION, kOsDesktop,
     SINGLE_VALUE_TYPE(switches::kEnableMaterialDesignExtensions)},
#endif
#if defined(OS_WIN) || defined(OS_LINUX)
    {"enable-input-ime-api", IDS_FLAGS_ENABLE_INPUT_IME_API_NAME,
     IDS_FLAGS_ENABLE_INPUT_IME_API_DESCRIPTION, kOsWin | kOsLinux,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableInputImeAPI,
                               switches::kDisableInputImeAPI)},
#endif  // defined(OS_WIN) || defined(OS_LINUX)
    {"enable-experimental-framework", IDS_FLAGS_EXPERIMENTAL_FRAMEWORK_NAME,
     IDS_FLAGS_EXPERIMENTAL_FRAMEWORK_DESCRIPTION, kOsAll,
     FEATURE_VALUE_TYPE(features::kExperimentalFramework)},
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
    {"enable-unsafe-es3-apis", IDS_FLAGS_WEBGL2_NAME,
     IDS_FLAGS_WEBGL2_DESCRIPTION, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableUnsafeES3APIs)},
    {"enable-webfonts-intervention-trigger",
     IDS_FLAGS_ENABLE_WEBFONTS_INTERVENTION_TRIGGER_NAME,
     IDS_FLAGS_ENABLE_WEBFONTS_INTERVENTION_TRIGGER_DESCRIPTION, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableWebFontsInterventionTrigger)},
    {"enable-grouped-history", IDS_FLAGS_ENABLE_GROUPED_HISTORY_NAME,
     IDS_FLAGS_ENABLE_GROUPED_HISTORY_DESCRIPTION, kOsDesktop,
     SINGLE_VALUE_TYPE(switches::kHistoryEnableGroupByDomain)},
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
#if defined(ENABLE_EXTENSIONS)
    {"tab-for-desktop-share", IDS_FLAG_DISABLE_TAB_FOR_DESKTOP_SHARE,
     IDS_FLAG_DISABLE_TAB_FOR_DESKTOP_SHARE_DESCRIPTION, kOsAll,
     ENABLE_DISABLE_VALUE_TYPE(
         extensions::switches::kEnableTabForDesktopShare,
         extensions::switches::kDisableTabForDesktopShare)},
#endif
#if defined(OS_ANDROID)
    {"enable-ntp-snippets", IDS_FLAGS_ENABLE_NTP_SNIPPETS_NAME,
     IDS_FLAGS_ENABLE_NTP_SNIPPETS_DESCRIPTION, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kNTPSnippetsFeature)},
#endif  // defined(OS_ANDROID)
#if defined(ENABLE_WEBRTC) && BUILDFLAG(RTC_USE_H264)
    {"enable-webrtc-h264-with-openh264-ffmpeg",
     IDS_FLAGS_WEBRTC_H264_WITH_OPENH264_FFMPEG_NAME,
     IDS_FLAGS_WEBRTC_H264_WITH_OPENH264_FFMPEG_DESCRIPTION, kOsDesktop,
     FEATURE_VALUE_TYPE(content::kWebRtcH264WithOpenH264FFmpeg)},
#endif  // defined(ENABLE_WEBRTC) && BUILDFLAG(RTC_USE_H264)
#if defined(OS_ANDROID)
    {"ime-thread", IDS_FLAGS_IME_THREAD_NAME, IDS_FLAGS_IME_THREAD_DESCRIPTION,
     kOsAndroid, FEATURE_VALUE_TYPE(features::kImeThread)},
#endif  // defined(OS_ANDROID)
#if defined(OS_ANDROID)
    {"offline-pages-ntp", IDS_FLAGS_NTP_OFFLINE_PAGES_NAME,
     IDS_FLAGS_NTP_OFFLINE_PAGES_DESCRIPTION, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kNTPOfflinePagesFeature)},
    {"offlining-recent-pages", IDS_FLAGS_OFFLINING_RECENT_PAGES_NAME,
     IDS_FLAGS_OFFLINING_RECENT_PAGES_DESCRIPTION, kOsAndroid,
     FEATURE_VALUE_TYPE(offline_pages::kOffliningRecentPagesFeature)},
#endif  // defined(OS_ANDROID)
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
    {"enable-system-timezone-automatic-detection",
     IDS_FLAGS_ENABLE_SYSTEM_TIMEZONE_AUTOMATIC_DETECTION_NAME,
     IDS_FLAGS_ENABLE_SYSTEM_TIMEZONE_AUTOMATIC_DETECTION_DESCRIPTION, kOsCrOS,
     SINGLE_VALUE_TYPE(
         chromeos::switches::kEnableSystemTimezoneAutomaticDetectionPolicy)},
#endif
#if !defined(OS_ANDROID) && !defined(OS_IOS) && defined(GOOGLE_CHROME_BUILD)
    {"enable-google-branded-context-menu",
     IDS_FLAGS_GOOGLE_BRANDED_CONTEXT_MENU_NAME,
     IDS_FLAGS_GOOGLE_BRANDED_CONTEXT_MENU_DESCRIPTION, kOsDesktop,
     SINGLE_VALUE_TYPE(switches::kEnableGoogleBrandedContextMenu)},
#endif
#if defined(OS_MACOSX)
    {"enable-fullscreen-in-tab-detaching",
     IDS_FLAGS_TAB_DETACHING_IN_FULLSCREEN_NAME,
     IDS_FLAGS_TAB_DETACHING_IN_FULLSCREEN_DESCRIPTION, kOsMac,
     SINGLE_VALUE_TYPE(switches::kEnableFullscreenTabDetaching)},
#endif
#if defined(OS_ANDROID)
    {"media-style-notification",
     IDS_FLAGS_MEDIA_STYLE_NOTIFICATION_NAME,
     IDS_FLAGS_MEDIA_STYLE_NOTIFICATION_DESCRIPTION, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kMediaStyleNotification)},
#endif
    {"enable-pointer-events",  // FLAGS:RECORD_UMA
      IDS_FLAGS_EXPERIMENTAL_POINTER_EVENT_NAME,
      IDS_FLAGS_EXPERIMENTAL_POINTER_EVENT_DESCRIPTION,
      kOsAll,
      FEATURE_VALUE_TYPE(features::kPointerEvents)},
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
  // Tab management prototypes are only available for local, Canary, and Dev
  // channel builds.
  if (!strcmp("tab-management-experiment-type", entry.internal_name) &&
      channel != version_info::Channel::DEV &&
      channel != version_info::Channel::CANARY &&
      channel != version_info::Channel::UNKNOWN) {
    return true;
  }
  // enable-tab-switcher-in-document-mode is only available for Chromium
  // builds and the Canary channel.
  if (!strcmp("enable-tab-switcher-in-document-mode",
              entry.internal_name) &&
      channel != version_info::Channel::CANARY &&
      channel != version_info::Channel::UNKNOWN) {
    return true;
  }
#endif

  // data-reduction-proxy-lo-fi and enable-data-reduction-proxy-lo-fi-preview
  // are only available for Chromium builds and the Canary/Dev/Beta channels.
  if ((!strcmp("data-reduction-proxy-lo-fi", entry.internal_name) ||
       !strcmp("enable-data-reduction-proxy-lo-fi-preview",
               entry.internal_name)) &&
      channel != version_info::Channel::BETA &&
      channel != version_info::Channel::DEV &&
      channel != version_info::Channel::CANARY &&
      channel != version_info::Channel::UNKNOWN) {
    return true;
  }

  return false;
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
#endif
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
      base::HashMetricName(switch_name));
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

namespace testing {

const base::HistogramBase::Sample kBadSwitchFormatHistogramId = 0;

const FeatureEntry* GetFeatureEntries(size_t* count) {
  *count = arraysize(kFeatureEntries);
  return kFeatureEntries;
}

}  // namespace testing

}  // namespace about_flags
