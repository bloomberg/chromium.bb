// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/about_flags.h"

#include <algorithm>
#include <iterator>
#include <map>
#include <set>

#include "base/command_line.h"
#include "base/memory/singleton.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "cc/switches.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/user_metrics.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/google_chrome_strings.h"
#include "media/base/media_switches.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/switches.h"
#include "ui/gl/gl_switches.h"

#if defined(USE_ASH)
#include "ash/ash_switches.h"
#endif

#if defined(USE_AURA)
#include "ui/aura/aura_switches.h"
#endif

#if defined(OS_CHROMEOS)
#include "chromeos/chromeos_switches.h"
#endif

using content::UserMetricsAction;

namespace about_flags {

// Macros to simplify specifying the type.
#define SINGLE_VALUE_TYPE_AND_VALUE(command_line_switch, switch_value) \
    Experiment::SINGLE_VALUE, command_line_switch, switch_value, NULL, 0
#define SINGLE_VALUE_TYPE(command_line_switch) \
    SINGLE_VALUE_TYPE_AND_VALUE(command_line_switch, "")
#define MULTI_VALUE_TYPE(choices) \
    Experiment::MULTI_VALUE, "", "", choices, arraysize(choices)

namespace {

const unsigned kOsAll = kOsMac | kOsWin | kOsLinux | kOsCrOS | kOsAndroid;

// Adds a |StringValue| to |list| for each platform where |bitmask| indicates
// whether the experiment is available on that platform.
void AddOsStrings(unsigned bitmask, ListValue* list) {
  struct {
    unsigned bit;
    const char* const name;
  } kBitsToOs[] = {
    {kOsMac, "Mac"},
    {kOsWin, "Windows"},
    {kOsLinux, "Linux"},
    {kOsCrOS, "Chrome OS"},
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kBitsToOs); ++i)
    if (bitmask & kBitsToOs[i].bit)
      list->Append(new StringValue(kBitsToOs[i].name));
}

const Experiment::Choice kOmniboxHistoryQuickProviderNewScoringChoices[] = {
  { IDS_FLAGS_OMNIBOX_HISTORY_QUICK_PROVIDER_NEW_SCORING_AUTOMATIC, "", "" },
  { IDS_FLAGS_OMNIBOX_HISTORY_QUICK_PROVIDER_NEW_SCORING_ENABLED,
    switches::kOmniboxHistoryQuickProviderNewScoring,
    switches::kOmniboxHistoryQuickProviderNewScoringEnabled },
  { IDS_FLAGS_OMNIBOX_HISTORY_QUICK_PROVIDER_NEW_SCORING_DISABLED,
    switches::kOmniboxHistoryQuickProviderNewScoring,
    switches::kOmniboxHistoryQuickProviderNewScoringDisabled }
};

const Experiment::Choice
    kOmniboxHistoryQuickProviderReorderForInliningChoices[] = {
  { IDS_FLAGS_OMNIBOX_HISTORY_QUICK_PROVIDER_REORDER_FOR_INLINING_AUTOMATIC,
    "",
    "" },
  { IDS_FLAGS_OMNIBOX_HISTORY_QUICK_PROVIDER_REORDER_FOR_INLINING_ENABLED,
    switches::kOmniboxHistoryQuickProviderReorderForInlining,
    switches::kOmniboxHistoryQuickProviderReorderForInliningEnabled },
  { IDS_FLAGS_OMNIBOX_HISTORY_QUICK_PROVIDER_REORDER_FOR_INLINING_DISABLED,
    switches::kOmniboxHistoryQuickProviderReorderForInlining,
    switches::kOmniboxHistoryQuickProviderReorderForInliningDisabled }
};

const Experiment::Choice kOmniboxInlineHistoryQuickProviderChoices[] = {
  { IDS_FLAGS_OMNIBOX_INLINE_HISTORY_QUICK_PROVIDER_AUTOMATIC, "", "" },
  { IDS_FLAGS_OMNIBOX_INLINE_HISTORY_QUICK_PROVIDER_ALLOWED,
    switches::kOmniboxInlineHistoryQuickProvider,
    switches::kOmniboxInlineHistoryQuickProviderAllowed },
  { IDS_FLAGS_OMNIBOX_INLINE_HISTORY_QUICK_PROVIDER_PROHIBITED,
    switches::kOmniboxInlineHistoryQuickProvider,
    switches::kOmniboxInlineHistoryQuickProviderProhibited }
};

const Experiment::Choice kFixedPositionCreatesStackingContextChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", "" },
  { IDS_GENERIC_EXPERIMENT_CHOICE_ENABLED,
    switches::kEnableFixedPositionCreatesStackingContext, ""},
  { IDS_GENERIC_EXPERIMENT_CHOICE_DISABLED,
    switches::kDisableFixedPositionCreatesStackingContext, ""}
};

const Experiment::Choice kForceCompositingModeChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", "" },
  { IDS_GENERIC_EXPERIMENT_CHOICE_ENABLED,
    switches::kForceCompositingMode, ""},
  { IDS_GENERIC_EXPERIMENT_CHOICE_DISABLED,
    switches::kDisableForceCompositingMode, ""}
};

const Experiment::Choice kThreadedCompositingModeChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", "" },
  { IDS_GENERIC_EXPERIMENT_CHOICE_DISABLED,
    switches::kDisableThreadedCompositing, ""},
  { IDS_GENERIC_EXPERIMENT_CHOICE_ENABLED,
    switches::kEnableThreadedCompositing, ""}
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

const Experiment::Choice kTouchOptimizedUIChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_AUTOMATIC, "", "" },
  { IDS_GENERIC_EXPERIMENT_CHOICE_ENABLED,
    switches::kTouchOptimizedUI,
    switches::kTouchOptimizedUIEnabled },
  { IDS_GENERIC_EXPERIMENT_CHOICE_DISABLED,
    switches::kTouchOptimizedUI,
    switches::kTouchOptimizedUIDisabled }
};

const Experiment::Choice kAsyncDnsChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", "" },
  { IDS_GENERIC_EXPERIMENT_CHOICE_DISABLED,
    switches::kDisableAsyncDns, ""},
  { IDS_GENERIC_EXPERIMENT_CHOICE_ENABLED,
    switches::kEnableAsyncDns, ""}
};

const Experiment::Choice kNaClDebugMaskChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", "" },
  // Secure shell can be used on ChromeOS for forwarding the TCP port opened by
  // debug stub to a remote machine. Since secure shell uses NaCl, we provide
  // an option to switch off its debugging.
  { IDS_NACL_DEBUG_MASK_CHOICE_EXCLUDE_UTILS,
      switches::kNaClDebugMask, "!*://*/*ssh_client.nmf" },
  { IDS_NACL_DEBUG_MASK_CHOICE_INCLUDE_DEBUG,
      switches::kNaClDebugMask, "*://*/*debug.nmf" }
};

const Experiment::Choice kActionBoxChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", "" },
  { IDS_GENERIC_EXPERIMENT_CHOICE_DISABLED,
    switches::kActionBox, "0"},
  { IDS_GENERIC_EXPERIMENT_CHOICE_ENABLED,
    switches::kActionBox, "1"}
};

const Experiment::Choice kScriptBubbleChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", "" },
  { IDS_GENERIC_EXPERIMENT_CHOICE_DISABLED,
    switches::kScriptBubble, "0"},
  { IDS_GENERIC_EXPERIMENT_CHOICE_ENABLED,
    switches::kScriptBubble, "1"}
};

const Experiment::Choice kTabCaptureChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", "" },
  { IDS_GENERIC_EXPERIMENT_CHOICE_DISABLED,
    switches::kTabCapture, "0"},
  { IDS_GENERIC_EXPERIMENT_CHOICE_ENABLED,
    switches::kTabCapture, "1"}
};

#if defined(OS_CHROMEOS)
const Experiment::Choice kAshBootAnimationFunction[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", "" },
  { IDS_FLAGS_ASH_BOOT_ANIMATION_FUNCTION2,
    ash::switches::kAshBootAnimationFunction2, ""},
  { IDS_FLAGS_ASH_BOOT_ANIMATION_FUNCTION3,
    ash::switches::kAshBootAnimationFunction3, ""}
};

const Experiment::Choice kChromeCaptivePortalDetectionChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", ""},
  { IDS_FLAGS_CHROME_CAPTIVE_PORTAL_DETECTOR,
    switches::kEnableChromeCaptivePortalDetector, ""},
  { IDS_FLAGS_SHILL_CAPTIVE_PORTAL_DETECTOR,
    switches::kDisableChromeCaptivePortalDetector, ""}
};
#endif

#if defined(USE_ASH)
const Experiment::Choice kAshImmersiveModeChoices[] = {
  { IDS_GENERIC_EXPERIMENT_CHOICE_DEFAULT, "", ""},
  { IDS_FLAGS_ASH_IMMERSIVE_HIDE_TAB_INDICATORS,
    ash::switches::kAshImmersiveHideTabIndicators, ""}
};
#endif

// RECORDING USER METRICS FOR FLAGS:
// -----------------------------------------------------------------------------
// The first line of the experiment is the internal name. If you'd like to
// gather statistics about the usage of your flag, you should append a marker
// comment to the end of the feature name, like so:
//   "my-special-feature",  // FLAGS:RECORD_UMA
//
// After doing that, run //chrome/tools/extract_actions.py (see instructions at
// the top of that file for details) to update the chromeactions.txt file, which
// will enable UMA to record your feature flag.
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
    "expose-for-tabs",  // FLAGS:RECORD_UMA
    IDS_FLAGS_TABPOSE_NAME,
    IDS_FLAGS_TABPOSE_DESCRIPTION,
    kOsMac,
#if defined(OS_MACOSX)
    // The switch exists only on OS X.
    SINGLE_VALUE_TYPE(switches::kEnableExposeForTabs)
#else
    SINGLE_VALUE_TYPE("")
#endif
  },
  {
    "conflicting-modules-check",  // FLAGS:RECORD_UMA
    IDS_FLAGS_CONFLICTS_CHECK_NAME,
    IDS_FLAGS_CONFLICTS_CHECK_DESCRIPTION,
    kOsWin,
    SINGLE_VALUE_TYPE(switches::kConflictingModulesCheck)
  },
  {
    "cloud-print-proxy",  // FLAGS:RECORD_UMA
    IDS_FLAGS_CLOUD_PRINT_CONNECTOR_NAME,
    IDS_FLAGS_CLOUD_PRINT_CONNECTOR_DESCRIPTION,
    // For a Chrome build, we know we have a PDF plug-in on Windows, so it's
    // fully enabled.
    // Otherwise, where we know Windows could be working if a viable PDF
    // plug-in could be supplied, we'll keep the lab enabled. Mac and Linux
    // always have PDF rasterization available, so no flag needed there.
#if !defined(GOOGLE_CHROME_BUILD)
    kOsWin,
#else
    0,
#endif
    SINGLE_VALUE_TYPE(switches::kEnableCloudPrintProxy)
  },
#if defined(OS_WIN)
  {
    "print-raster",
    IDS_FLAGS_PRINT_RASTER_NAME,
    IDS_FLAGS_PRINT_RASTER_DESCRIPTION,
    kOsWin,
    SINGLE_VALUE_TYPE(switches::kPrintRaster)
  },
#endif  // OS_WIN
  {
    "crxless-web-apps",
    IDS_FLAGS_CRXLESS_WEB_APPS_NAME,
    IDS_FLAGS_CRXLESS_WEB_APPS_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kEnableCrxlessWebApps)
  },
  {
    "ignore-gpu-blacklist",
    IDS_FLAGS_IGNORE_GPU_BLACKLIST_NAME,
    IDS_FLAGS_IGNORE_GPU_BLACKLIST_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kIgnoreGpuBlacklist)
  },
  {
    "force-compositing-mode-2",
    IDS_FLAGS_FORCE_COMPOSITING_MODE_NAME,
    IDS_FLAGS_FORCE_COMPOSITING_MODE_DESCRIPTION,
    kOsMac | kOsWin | kOsLinux,
    MULTI_VALUE_TYPE(kForceCompositingModeChoices)
  },
  {
    "threaded-compositing-mode",
    IDS_FLAGS_THREADED_COMPOSITING_MODE_NAME,
    IDS_FLAGS_THREADED_COMPOSITING_MODE_DESCRIPTION,
    kOsAll,
    MULTI_VALUE_TYPE(kThreadedCompositingModeChoices)
  },
  {
    "disable-accelerated-2d-canvas",
    IDS_FLAGS_DISABLE_ACCELERATED_2D_CANVAS_NAME,
    IDS_FLAGS_DISABLE_ACCELERATED_2D_CANVAS_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kDisableAccelerated2dCanvas)
  },
  {
    "disable-deferred-2d-canvas",
    IDS_FLAGS_DISABLE_DEFERRED_2D_CANVAS_NAME,
    IDS_FLAGS_DISABLE_DEFERRED_2D_CANVAS_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kDisableDeferred2dCanvas)
  },
  {
    "disable-threaded-animation",
    IDS_FLAGS_DISABLE_THREADED_ANIMATION_NAME,
    IDS_FLAGS_DISABLE_THREADED_ANIMATION_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(cc::switches::kDisableThreadedAnimation)
  },
  {
    "composited-layer-borders",
    IDS_FLAGS_COMPOSITED_LAYER_BORDERS,
    IDS_FLAGS_COMPOSITED_LAYER_BORDERS_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kShowCompositedLayerBorders)
  },
  {
    "show-fps-counter",
    IDS_FLAGS_SHOW_FPS_COUNTER,
    IDS_FLAGS_SHOW_FPS_COUNTER_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kShowFPSCounter)
  },
  {
    "accelerated-filters",
    IDS_FLAGS_ACCELERATED_FILTERS,
    IDS_FLAGS_ACCELERATED_FILTERS_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kEnableAcceleratedFilters)
  },
  {
    "disable-gpu-vsync",
    IDS_FLAGS_DISABLE_GPU_VSYNC_NAME,
    IDS_FLAGS_DISABLE_GPU_VSYNC_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kDisableGpuVsync)
  },
  {
    "disable-webgl",
    IDS_FLAGS_DISABLE_WEBGL_NAME,
    IDS_FLAGS_DISABLE_WEBGL_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kDisableExperimentalWebGL)
  },
  {
    "fixed-position-creates-stacking-context",
    IDS_FLAGS_FIXED_POSITION_CREATES_STACKING_CONTEXT_NAME,
    IDS_FLAGS_FIXED_POSITION_CREATES_STACKING_CONTEXT_DESCRIPTION,
    kOsAll,
    MULTI_VALUE_TYPE(kFixedPositionCreatesStackingContextChoices)
  },
  // TODO(bbudge): When NaCl is on by default, remove this flag entry.
  {
    "enable-nacl",  // FLAGS:RECORD_UMA
    IDS_FLAGS_ENABLE_NACL_NAME,
    IDS_FLAGS_ENABLE_NACL_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kEnableNaCl)
  },
  // TODO(halyavin): When exception handling is on by default, replace this
  // flag with disable-nacl-exception-handling.
  {
    "enable-nacl-exception-handling",  // FLAGS:RECORD_UMA
    IDS_FLAGS_ENABLE_NACL_EXCEPTION_HANDLING_NAME,
    IDS_FLAGS_ENABLE_NACL_EXCEPTION_HANDLING_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kEnableNaClExceptionHandling)
  },
  {
    "enable-nacl-debug",  // FLAGS:RECORD_UMA
    IDS_FLAGS_ENABLE_NACL_DEBUG_NAME,
    IDS_FLAGS_ENABLE_NACL_DEBUG_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kEnableNaClDebug)
  },
  {
    "nacl-debug-mask",  // FLAGS:RECORD_UMA
    IDS_FLAGS_NACL_DEBUG_MASK_NAME,
    IDS_FLAGS_NACL_DEBUG_MASK_DESCRIPTION,
    kOsAll,
    MULTI_VALUE_TYPE(kNaClDebugMaskChoices)
  },
  {
    "enable-pnacl",  // FLAGS:RECORD_UMA
    IDS_FLAGS_ENABLE_PNACL_NAME,
    IDS_FLAGS_ENABLE_PNACL_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kEnablePnacl)
  },
  {
    "extension-apis",  // FLAGS:RECORD_UMA
    IDS_FLAGS_EXPERIMENTAL_EXTENSION_APIS_NAME,
    IDS_FLAGS_EXPERIMENTAL_EXTENSION_APIS_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kEnableExperimentalExtensionApis)
  },
  {
    "action-box",
    IDS_FLAGS_ACTION_BOX_NAME,
    IDS_FLAGS_ACTION_BOX_DESCRIPTION,
    kOsAll,
    MULTI_VALUE_TYPE(kActionBoxChoices),
  },
  {
    "script-badges",
    IDS_FLAGS_SCRIPT_BADGES_NAME,
    IDS_FLAGS_SCRIPT_BADGES_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kScriptBadges)
  },
  {
    "script-bubble",
    IDS_FLAGS_SCRIPT_BUBBLE_NAME,
    IDS_FLAGS_SCRIPT_BUBBLE_DESCRIPTION,
    kOsAll,
    MULTI_VALUE_TYPE(kScriptBubbleChoices),
  },
  {
    "apps-new-install-bubble",
    IDS_FLAGS_APPS_NEW_INSTALL_BUBBLE_NAME,
    IDS_FLAGS_APPS_NEW_INSTALL_BUBBLE_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kAppsNewInstallBubble)
  },
  {
    "disable-hyperlink-auditing",
    IDS_FLAGS_DISABLE_HYPERLINK_AUDITING_NAME,
    IDS_FLAGS_DISABLE_HYPERLINK_AUDITING_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kNoPings)
  },
  {
    "experimental-location-features",  // FLAGS:RECORD_UMA
    IDS_FLAGS_EXPERIMENTAL_LOCATION_FEATURES_NAME,
    IDS_FLAGS_EXPERIMENTAL_LOCATION_FEATURES_DESCRIPTION,
    kOsMac | kOsWin | kOsLinux,  // Currently does nothing on CrOS.
    SINGLE_VALUE_TYPE(switches::kExperimentalLocationFeatures)
  },
  {
    "tab-groups-context-menu",
    IDS_FLAGS_TAB_GROUPS_CONTEXT_MENU_NAME,
    IDS_FLAGS_TAB_GROUPS_CONTEXT_MENU_DESCRIPTION,
    kOsWin,
    SINGLE_VALUE_TYPE(switches::kEnableTabGroupsContextMenu)
  },
  {
    "enable-instant-extended-api",
    IDS_FLAGS_ENABLE_INSTANT_EXTENDED_API,
    IDS_FLAGS_ENABLE_INSTANT_EXTENDED_API_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kEnableInstantExtendedAPI)
  },
  {
    "static-ip-config",
    IDS_FLAGS_STATIC_IP_CONFIG_NAME,
    IDS_FLAGS_STATIC_IP_CONFIG_DESCRIPTION,
    kOsCrOS,
#if defined(OS_CHROMEOS)
    // This switch exists only on Chrome OS.
    SINGLE_VALUE_TYPE(switches::kEnableStaticIPConfig)
#else
    SINGLE_VALUE_TYPE("")
#endif
  },
  {
    "show-autofill-type-predictions",
    IDS_FLAGS_SHOW_AUTOFILL_TYPE_PREDICTIONS_NAME,
    IDS_FLAGS_SHOW_AUTOFILL_TYPE_PREDICTIONS_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kShowAutofillTypePredictions)
  },
  {
    "sync-tab-favicons",
    IDS_FLAGS_SYNC_TAB_FAVICONS_NAME,
    IDS_FLAGS_SYNC_TAB_FAVICONS_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kSyncTabFavicons)
  },
  {
    "sync-app-notifications",
    IDS_FLAGS_SYNC_APP_NOTIFICATIONS_NAME,
    IDS_FLAGS_SYNC_APP_NOTIFICATIONS_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kDisableSyncAppNotifications)
  },
  {
    "sync-keystore-encryption",
    IDS_FLAGS_SYNC_KEYSTORE_ENCRYPTION_NAME,
    IDS_FLAGS_SYNC_KEYSTORE_ENCRYPTION_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kSyncKeystoreEncryption)
  },
  {
    "enable-gesture-tap-highlight",
    IDS_FLAGS_ENABLE_GESTURE_TAP_HIGHLIGHTING_NAME,
    IDS_FLAGS_ENABLE_GESTURE_TAP_HIGHLIGHTING_DESCRIPTION,
    kOsLinux | kOsCrOS,
    SINGLE_VALUE_TYPE(switches::kEnableGestureTapHighlight)
  },
  {
    "enable-smooth-scrolling",  // FLAGS:RECORD_UMA
    IDS_FLAGS_ENABLE_SMOOTH_SCROLLING_NAME,
    IDS_FLAGS_ENABLE_SMOOTH_SCROLLING_DESCRIPTION,
    // Can't expose the switch unless the code is compiled in.
    // On by default for the Mac (different implementation in WebKit).
    kOsWin | kOsLinux | kOsCrOS,
    SINGLE_VALUE_TYPE(switches::kEnableSmoothScrolling)
  },
  {
    "omnibox-history-quick-provider-new-scoring",
    IDS_FLAGS_OMNIBOX_HISTORY_QUICK_PROVIDER_NEW_SCORING_NAME,
    IDS_FLAGS_OMNIBOX_HISTORY_QUICK_PROVIDER_NEW_SCORING_DESCRIPTION,
    kOsAll,
    MULTI_VALUE_TYPE(kOmniboxHistoryQuickProviderNewScoringChoices)
  },
  {
    "omnibox-history-quick-provider-reorder-for-inlining",
    IDS_FLAGS_OMNIBOX_HISTORY_QUICK_PROVIDER_REORDER_FOR_INLINING_NAME,
    IDS_FLAGS_OMNIBOX_HISTORY_QUICK_PROVIDER_REORDER_FOR_INLINING_DESCRIPTION,
    kOsAll,
    MULTI_VALUE_TYPE(kOmniboxHistoryQuickProviderReorderForInliningChoices)
  },
  {
    "omnibox-inline-history-quick-provider",
    IDS_FLAGS_OMNIBOX_INLINE_HISTORY_QUICK_PROVIDER_NAME,
    IDS_FLAGS_OMNIBOX_INLINE_HISTORY_QUICK_PROVIDER_DESCRIPTION,
    kOsAll,
    MULTI_VALUE_TYPE(kOmniboxInlineHistoryQuickProviderChoices)
  },
  {
    "enable-panels",
    IDS_FLAGS_ENABLE_PANELS_NAME,
    IDS_FLAGS_ENABLE_PANELS_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kEnablePanels)
  },
  {
    "enable-panel-stacking",
    IDS_FLAGS_ENABLE_PANEL_STACKING_NAME,
    IDS_FLAGS_ENABLE_PANEL_STACKING_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kEnablePanelStacking)
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
    "enable-autologin",
    IDS_FLAGS_ENABLE_AUTOLOGIN_NAME,
    IDS_FLAGS_ENABLE_AUTOLOGIN_DESCRIPTION,
    kOsMac | kOsWin | kOsLinux,
    SINGLE_VALUE_TYPE(switches::kEnableAutologin)
  },
  {
    "enable-http-pipelining",
    IDS_FLAGS_ENABLE_HTTP_PIPELINING_NAME,
    IDS_FLAGS_ENABLE_HTTP_PIPELINING_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kEnableHttpPipelining)
  },
  {
    "enable-spdy3",
    IDS_FLAGS_ENABLE_SPDY3_NAME,
    IDS_FLAGS_ENABLE_SPDY3_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kEnableSpdy3)
  },
  {
    "enable-async-dns",
    IDS_FLAGS_ENABLE_ASYNC_DNS_NAME,
    IDS_FLAGS_ENABLE_ASYNC_DNS_DESCRIPTION,
    kOsWin | kOsMac | kOsLinux | kOsCrOS,
    MULTI_VALUE_TYPE(kAsyncDnsChoices)
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
    "enable-opus-playback",
    IDS_FLAGS_ENABLE_OPUS_PLAYBACK_NAME,
    IDS_FLAGS_ENABLE_OPUS_PLAYBACK_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kEnableOpusPlayback)
  },
#if defined(USE_ASH)
  {
    "ash-disable-auto-window-placement",
    IDS_FLAGS_ASH_AUTO_WINDOW_PLACEMENT_NAME,
    IDS_FLAGS_ASH_AUTO_WINDOW_PLACEMENT_DESCRIPTION,
    kOsWin | kOsLinux | kOsCrOS,
    SINGLE_VALUE_TYPE(ash::switches::kAshDisableAutoWindowPlacement)
  },
  {
    "ash-enable-per-app-launcher",
    IDS_FLAGS_ASH_ENABLE_PER_APP_LAUNCHER_NAME,
    IDS_FLAGS_ASH_ENABLE_PER_APP_LAUNCHER_DESCRIPTION,
    kOsWin | kOsLinux | kOsCrOS,
    SINGLE_VALUE_TYPE(ash::switches::kAshEnablePerAppLauncher)
  },
#endif
  {
    "per-tile-painting",
    IDS_FLAGS_PER_TILE_PAINTING_NAME,
    IDS_FLAGS_PER_TILE_PAINTING_DESCRIPTION,
#if defined(USE_SKIA)
    kOsMac | kOsLinux | kOsCrOS,
#else
    0,
#endif
    SINGLE_VALUE_TYPE(cc::switches::kEnablePerTilePainting)
  },
  {
    "enable-javascript-harmony",
    IDS_FLAGS_ENABLE_JAVASCRIPT_HARMONY_NAME,
    IDS_FLAGS_ENABLE_JAVASCRIPT_HARMONY_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE_AND_VALUE(switches::kJavaScriptFlags, "--harmony")
  },
  {
    "enable-tab-browser-dragging",
    IDS_FLAGS_ENABLE_TAB_BROWSER_DRAGGING_NAME,
    IDS_FLAGS_ENABLE_TAB_BROWSER_DRAGGING_DESCRIPTION,
    kOsWin,
    SINGLE_VALUE_TYPE(switches::kTabBrowserDragging)
  },
  {
    "disable-restore-session-state",
    IDS_FLAGS_DISABLE_RESTORE_SESSION_STATE_NAME,
    IDS_FLAGS_DISABLE_RESTORE_SESSION_STATE_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kDisableRestoreSessionState)
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
    "enable-experimental-webkit-features",
    IDS_FLAGS_EXPERIMENTAL_WEBKIT_FEATURES_NAME,
    IDS_FLAGS_EXPERIMENTAL_WEBKIT_FEATURES_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kEnableExperimentalWebKitFeatures)
  },
  {
    "enable-css-shaders",
    IDS_FLAGS_CSS_SHADERS_NAME,
    IDS_FLAGS_CSS_SHADERS_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kEnableCssShaders)
  },
  {
    "enable-extension-activity-ui",
    IDS_FLAGS_ENABLE_EXTENSION_ACTIVITY_UI_NAME,
    IDS_FLAGS_ENABLE_EXTENSION_ACTIVITY_UI_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kEnableExtensionActivityUI)
  },
  {
    "disable-ntp-other-sessions-menu",
    IDS_FLAGS_NTP_OTHER_SESSIONS_MENU_NAME,
    IDS_FLAGS_NTP_OTHER_SESSIONS_MENU_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kDisableNTPOtherSessionsMenu)
  },
#if defined(USE_ASH)
  {
    "enable-ash-oak",
    IDS_FLAGS_ENABLE_ASH_OAK_NAME,
    IDS_FLAGS_ENABLE_ASH_OAK_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(ash::switches::kAshEnableOak),
  },
#endif
  {
    "enable-devtools-experiments",
    IDS_FLAGS_ENABLE_DEVTOOLS_EXPERIMENTS_NAME,
    IDS_FLAGS_ENABLE_DEVTOOLS_EXPERIMENTS_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kEnableDevToolsExperiments)
  },
  {
    "enable-suggestions-ntp",
    IDS_FLAGS_NTP_SUGGESTIONS_PAGE_NAME,
    IDS_FLAGS_NTP_SUGGESTIONS_PAGE_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kEnableSuggestionsTabPage)
  },
  {
    "spellcheck-autocorrect",
    IDS_FLAGS_SPELLCHECK_AUTOCORRECT,
    IDS_FLAGS_SPELLCHECK_AUTOCORRECT_DESCRIPTION,
    kOsWin | kOsLinux | kOsCrOS,
    SINGLE_VALUE_TYPE(switches::kEnableSpellingAutoCorrect)
  },
  {
    "touch-events",
    IDS_TOUCH_EVENTS_NAME,
    IDS_TOUCH_EVENTS_DESCRIPTION,
    kOsAll,
    MULTI_VALUE_TYPE(kTouchEventsChoices)
  },
  {
    "touch-optimized-ui",
    IDS_FLAGS_TOUCH_OPTIMIZED_UI_NAME,
    IDS_FLAGS_TOUCH_OPTIMIZED_UI_DESCRIPTION,
    kOsWin,
    MULTI_VALUE_TYPE(kTouchOptimizedUIChoices)
  },
  {
    "enable-scaling-in-image-skia-operations",
    IDS_FLAGS_ENABLE_SCALING_IN_IMAGE_SKIA_OPERATIONS_NAME,
    IDS_FLAGS_ENABLE_SCALING_IN_IMAGE_SKIA_OPERATIONS_DESCRIPTION,
    kOsCrOS | kOsWin,
    SINGLE_VALUE_TYPE(gfx::switches::kEnableScalingInImageSkiaOperations)
  },
  {
    "enable-webkit-text-subpixel-positioning",
    IDS_FLAGS_ENABLE_WEBKIT_TEXT_SUBPIXEL_POSITIONING_NAME,
    IDS_FLAGS_ENABLE_WEBKIT_TEXT_SUBPIXEL_POSITIONING_DESCRIPTION,
    kOsCrOS,
    SINGLE_VALUE_TYPE(gfx::switches::kEnableWebkitTextSubpixelPositioning)
  },
  {
    "disable-touch-adjustment",
    IDS_DISABLE_TOUCH_ADJUSTMENT_NAME,
    IDS_DISABLE_TOUCH_ADJUSTMENT_DESCRIPTION,
    kOsWin | kOsLinux | kOsCrOS,
    SINGLE_VALUE_TYPE(switches::kDisableTouchAdjustment)
  },
  {
    "enable-tab-capture",
    IDS_ENABLE_TAB_CAPTURE_NAME,
    IDS_ENABLE_TAB_CAPTURE_DESCRIPTION,
    kOsWin | kOsMac | kOsLinux | kOsCrOS,
    MULTI_VALUE_TYPE(kTabCaptureChoices)
  },
#if defined(OS_CHROMEOS)
  {
    "enable-background-loader",
    IDS_ENABLE_BACKLOADER_NAME,
    IDS_ENABLE_BACKLOADER_DESCRIPTION,
    kOsCrOS,
    SINGLE_VALUE_TYPE(switches::kEnableBackgroundLoader)
  },
  {
    "enable-bezel-touch",
    IDS_ENABLE_BEZEL_TOUCH_NAME,
    IDS_ENABLE_BEZEL_TOUCH_DESCRIPTION,
    kOsCrOS,
    SINGLE_VALUE_TYPE(switches::kEnableBezelTouch)
  },
  {
    "no-discard-tabs",
    IDS_FLAGS_NO_DISCARD_TABS_NAME,
    IDS_FLAGS_NO_DISCARD_TABS_DESCRIPTION,
    kOsCrOS,
    SINGLE_VALUE_TYPE(switches::kNoDiscardTabs)
  },
#endif
  {
    "enable-download-resumption",
    IDS_FLAGS_ENABLE_DOWNLOAD_RESUMPTION_NAME,
    IDS_FLAGS_ENABLE_DOWNLOAD_RESUMPTION_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kEnableDownloadResumption)
  },
  {
    "allow-nacl-socket-api",
    IDS_FLAGS_ALLOW_NACL_SOCKET_API_NAME,
    IDS_FLAGS_ALLOW_NACL_SOCKET_API_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE_AND_VALUE(switches::kAllowNaClSocketAPI, "*")
  },
  {
    "stacked-tab-strip",
    IDS_FLAGS_STACKED_TAB_STRIP_NAME,
    IDS_FLAGS_STACKED_TAB_STRIP_DESCRIPTION,
    kOsWin,
    SINGLE_VALUE_TYPE(switches::kEnableStackedTabStrip)
  },
  {
    "force-device-scale-factor",
    IDS_FLAGS_FORCE_HIGH_DPI_NAME,
    IDS_FLAGS_FORCE_HIGH_DPI_DESCRIPTION,
    kOsCrOS,
    SINGLE_VALUE_TYPE_AND_VALUE(switches::kForceDeviceScaleFactor, "2")
  },
#if defined(OS_CHROMEOS)
  {
    "allow-touchpad-three-finger-click",
    IDS_FLAGS_ALLOW_TOUCHPAD_THREE_FINGER_CLICK_NAME,
    IDS_FLAGS_ALLOW_TOUCHPAD_THREE_FINGER_CLICK_DESCRIPTION,
    kOsCrOS,
    SINGLE_VALUE_TYPE(switches::kEnableTouchpadThreeFingerClick)
  },
  {
    "allow-touchpad-three-finger-swipe",
    IDS_FLAGS_ALLOW_TOUCHPAD_THREE_FINGER_SWIPE_NAME,
    IDS_FLAGS_ALLOW_TOUCHPAD_THREE_FINGER_SWIPE_DESCRIPTION,
    kOsCrOS,
    SINGLE_VALUE_TYPE(switches::kEnableTouchpadThreeFingerSwipe)
  },
#endif
  {
    "use-web-based-signin-flow",
    IDS_FLAGS_USE_WEB_BASED_SIGNIN_FLOW_NAME,
    IDS_FLAGS_USE_WEB_BASED_SIGNIN_FLOW_DESCRIPTION,
    kOsMac | kOsWin | kOsLinux,
    SINGLE_VALUE_TYPE(switches::kUseWebBasedSigninFlow)
  },
  {
    "enable-desktop-guest-mode",
    IDS_FLAGS_DESKTOP_GUEST_MODE_NAME,
    IDS_FLAGS_DESKTOP_GUEST_MODE_DESCRIPTION,
    kOsMac | kOsWin | kOsLinux,
    SINGLE_VALUE_TYPE(switches::kEnableDesktopGuestMode)
  },
#if defined(USE_ASH)
  {
    "show-launcher-alignment-menu",
    IDS_FLAGS_SHOW_LAUNCHER_ALIGNMENT_MENU_NAME,
    IDS_FLAGS_SHOW_LAUNCHER_ALIGNMENT_MENU_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kShowLauncherAlignmentMenu)
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
    SINGLE_VALUE_TYPE(switches::kEnablePinch),
  },
  {
    "app-list-show-apps-only",
    IDS_FLAGS_ENABLE_APP_LIST_SHOW_APPS_ONLY_NAME,
    IDS_FLAGS_ENABLE_APP_LIST_SHOW_APPS_ONLY_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(app_list::switches::kAppListShowAppsOnly),
  },
#endif  // defined(USE_ASH)
#if defined(OS_CHROMEOS)
  {
    "disable-new-oobe",
    IDS_FLAGS_DISABLE_NEW_OOBE,
    IDS_FLAGS_DISABLE_NEW_OOBE_DESCRIPTION,
    kOsCrOS,
    SINGLE_VALUE_TYPE(switches::kDisableNewOobe),
  },
  {
      "disable-new-password-changed-dialog",
      IDS_FLAGS_DISABLE_NEW_PASSWORD_CHANGED_DIALOG,
      IDS_FLAGS_DISABLE_NEW_PASSWORD_CHANGED_DIALOG_DESCRIPTION,
      kOsCrOS,
      SINGLE_VALUE_TYPE(switches::kDisableNewPasswordChangedDialog),
  },
  {
    "disable-boot-animation",
    IDS_FLAGS_DISABLE_BOOT_ANIMATION,
    IDS_FLAGS_DISABLE_BOOT_ANIMATION_DESCRIPTION,
    kOsCrOS,
    SINGLE_VALUE_TYPE(switches::kDisableBootAnimation),
  },
  {
    "disable-boot-animation2",
    IDS_FLAGS_DISABLE_BOOT_ANIMATION2,
    IDS_FLAGS_DISABLE_BOOT_ANIMATION2_DESCRIPTION,
    kOsCrOS,
    SINGLE_VALUE_TYPE(ash::switches::kAshDisableBootAnimation2),
  },
  {
    "boot-animation-fucntion",
    IDS_FLAGS_ASH_BOOT_ANIMATION_FUNCTION,
    IDS_FLAGS_ASH_BOOT_ANIMATION_FUNCTION_DESCRIPTION,
    kOsCrOS,
    MULTI_VALUE_TYPE(kAshBootAnimationFunction),
  },
  {
    "captive-portal-detector",
    IDS_FLAGS_CAPTIVE_PORTAL_DETECTOR_NAME,
    IDS_FLAGS_CAPTIVE_PORTAL_DETECTOR_DESCRIPTION,
    kOsCrOS,
    MULTI_VALUE_TYPE(kChromeCaptivePortalDetectionChoices),
  },
  {
    "disable-new-lock-animations",
    IDS_FLAGS_ASH_NEW_LOCK_ANIMATIONS,
    IDS_FLAGS_ASH_NEW_LOCK_ANIMATIONS_DESCRIPTION,
    kOsCrOS,
    SINGLE_VALUE_TYPE(ash::switches::kAshDisableNewLockAnimations),
  },
  {
    "file-manager-packaged",
    IDS_FLAGS_FILE_MANAGER_PACKAGED_NAME,
    IDS_FLAGS_FILE_MANAGER_PACKAGED_DESCRIPTION,
    kOsCrOS,
    SINGLE_VALUE_TYPE(switches::kFileManagerPackaged),
  },
  {
    "enable-launcher-per-display",
    IDS_FLAGS_ENABLE_LAUNCHER_PER_DISPLAY_NAME,
    IDS_FLAGS_ENABLE_LAUNCHER_PER_DISPLAY_DESCRIPTION,
    kOsCrOS,
    SINGLE_VALUE_TYPE(ash::switches::kAshLauncherPerDisplay),
  },
#endif  // defined(OS_CHROMEOS)
  {
    "enable-views-textfield",
    IDS_FLAGS_ENABLE_VIEWS_TEXTFIELD_NAME,
    IDS_FLAGS_ENABLE_VIEWS_TEXTFIELD_DESCRIPTION,
    kOsWin,
    SINGLE_VALUE_TYPE(switches::kEnableViewsTextfield),
  },
  {
    "old-checkbox-style",
    IDS_FLAGS_OLD_CHECKBOX_STYLE,
    IDS_FLAGS_OLD_CHECKBOX_STYLE_DESCRIPTION,
    kOsLinux | kOsCrOS,
    SINGLE_VALUE_TYPE(switches::kOldCheckboxStyle),
  },
  {
    "enable-new-dialog-style",
    IDS_FLAGS_ENABLE_NEW_DIALOG_STYLE_NAME,
    IDS_FLAGS_ENABLE_NEW_DIALOG_STYLE_DESCRIPTION,
    kOsWin | kOsCrOS,
    SINGLE_VALUE_TYPE(switches::kEnableNewDialogStyle),
  },
  { "disable-accelerated-video-decode",
    IDS_FLAGS_DISABLE_ACCELERATED_VIDEO_DECODE_NAME,
    IDS_FLAGS_DISABLE_ACCELERATED_VIDEO_DECODE_DESCRIPTION,
    kOsAll,
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
#endif
  {
    "disable-website-settings",  // FLAGS:RECORD_UMA
    IDS_FLAGS_DISABLE_WEBSITE_SETTINGS_NAME,
    IDS_FLAGS_DISABLE_WEBSITE_SETTINGS_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kDisableWebsiteSettings),
  },
  {
    "enable-webaudio-input",
    IDS_FLAGS_ENABLE_WEBAUDIO_INPUT_NAME,
    IDS_FLAGS_ENABLE_WEBAUDIO_INPUT_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kEnableWebAudioInput),
  },
  {
    "enable-contacts",
    IDS_FLAGS_ENABLE_CONTACTS_NAME,
    IDS_FLAGS_ENABLE_CONTACTS_DESCRIPTION,
    kOsCrOS,
    SINGLE_VALUE_TYPE(switches::kEnableContacts)
  },
#if defined(USE_ASH)
  { "ash-enable-advanced-gestures",
    IDS_FLAGS_ENABLE_ADVANCED_GESTURES_NAME,
    IDS_FLAGS_ENABLE_ADVANCED_GESTURES_DESCRIPTION,
    kOsCrOS,
    SINGLE_VALUE_TYPE(ash::switches::kAshEnableAdvancedGestures),
  },
  { "ash-enable-tab-scrubbing",
    IDS_FLAGS_ENABLE_TAB_SCRUBBING_NAME,
    IDS_FLAGS_ENABLE_TAB_SCRUBBING_DESCRIPTION,
    kOsCrOS,
    SINGLE_VALUE_TYPE(switches::kAshEnableTabScrubbing),
  },
  { "ash-enable-workspace-scrubbing",
    IDS_FLAGS_ENABLE_WORKSPACE_SCRUBBING_NAME,
    IDS_FLAGS_ENABLE_WORKSPACE_SCRUBBING_DESCRIPTION,
    kOsCrOS,
    SINGLE_VALUE_TYPE(ash::switches::kAshEnableWorkspaceScrubbing),
  },
  { "ash-immersive-mode",
    IDS_FLAGS_ASH_IMMERSIVE_MODE_NAME,
    IDS_FLAGS_ASH_IMMERSIVE_MODE_DESCRIPTION,
    kOsCrOS,
    MULTI_VALUE_TYPE(kAshImmersiveModeChoices),
  },
#if defined(OS_LINUX)
  { "ash-enable-memory-monitor",
      IDS_FLAGS_ENABLE_MEMORY_MONITOR_NAME,
      IDS_FLAGS_ENABLE_MEMORY_MONITOR_DESCRIPTION,
      kOsCrOS,
      SINGLE_VALUE_TYPE(ash::switches::kAshEnableMemoryMonitor),
  },
#endif
#endif
#if defined(OS_CHROMEOS)
  { "enable-new-network-handlers",
    IDS_FLAGS_ENABLE_NEW_NETWORK_HANDLERS_NAME,
    IDS_FLAGS_ENABLE_NEW_NETWORK_HANDLERS_DESCRIPTION,
    kOsCrOS,
    SINGLE_VALUE_TYPE(chromeos::switches::kEnableNewNetworkHandlers),
  },
  {
    "enable-carrier-switching",
    IDS_FLAGS_ENABLE_CARRIER_SWITCHING,
    IDS_FLAGS_ENABLE_CARRIER_SWITCHING_DESCRIPTION,
    kOsCrOS,
    SINGLE_VALUE_TYPE(switches::kEnableCarrierSwitching)
  },
  {
    "enable-request-tablet-site",
    IDS_FLAGS_ENABLE_REQUEST_TABLET_SITE_NAME,
    IDS_FLAGS_ENABLE_REQUEST_TABLET_SITE_DESCRIPTION,
    kOsCrOS,
    SINGLE_VALUE_TYPE(switches::kEnableRequestTabletSite)
  },
#endif
  {
    "debug-packed-apps",
    IDS_FLAGS_DEBUG_PACKED_APP_NAME,
    IDS_FLAGS_DEBUG_PACKED_APP_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kDebugPackedApps)
  },
  {
    "enable-password-generation",
    IDS_FLAGS_ENABLE_PASSWORD_GENERATION_NAME,
    IDS_FLAGS_ENABLE_PASSWORD_GENERATION_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kEnablePasswordGeneration)
  },
  {
    "crash-on-gpu-hang",
    IDS_FLAGS_CRASH_ON_GPU_HANG_NAME,
    IDS_FLAGS_CRASH_ON_GPU_HANG_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kCrashOnGpuHang)
  },
  {
    "enable-deferred-image-decoding",
    IDS_FLAGS_ENABLE_DEFERRED_IMAGE_DECODING_NAME,
    IDS_FLAGS_ENABLE_DEFERRED_IMAGE_DECODING_DESCRIPTION,
#if defined(USE_SKIA)
    kOsMac | kOsLinux | kOsCrOS,
#else
    0,
#endif
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
    "enable-new-autofill-ui",
    IDS_FLAGS_ENABLE_NEW_AUTOFILL_UI_NAME,
    IDS_FLAGS_ENABLE_NEW_AUTOFILL_UI_DESCRIPTION,
    kOsMac | kOsWin | kOsLinux | kOsCrOS,
    SINGLE_VALUE_TYPE(switches::kEnableNewAutofillUi)
  },
  {
    "enable-new-autofill-heuristics",
    IDS_FLAGS_ENABLE_NEW_AUTOFILL_HEURISTICS_NAME,
    IDS_FLAGS_ENABLE_NEW_AUTOFILL_HEURISTICS_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kEnableNewAutofillHeuristics)
  },
  {
    "show-app-list-shortcut",
    IDS_FLAGS_SHOW_APP_LIST_SHORTCUT_NAME,
    IDS_FLAGS_SHOW_APP_LIST_SHORTCUT_DESCRIPTION,
    kOsWin,
    SINGLE_VALUE_TYPE(switches::kShowAppListShortcut)
  },
  {
    "enable-experimental-form-filling",
    IDS_FLAGS_ENABLE_EXPERIMENTAL_FORM_FILLING_NAME,
    IDS_FLAGS_ENABLE_EXPERIMENTAL_FORM_FILLING_DESCRIPTION,
    kOsWin | kOsCrOS,
    SINGLE_VALUE_TYPE(switches::kEnableExperimentalFormFilling)
  },
  {
    "enable-interactive-autocomplete",
    IDS_FLAGS_ENABLE_INTERACTIVE_AUTOCOMPLETE_NAME,
    IDS_FLAGS_ENABLE_INTERACTIVE_AUTOCOMPLETE_DESCRIPTION,
    kOsWin | kOsCrOS,
    SINGLE_VALUE_TYPE(switches::kEnableInteractiveAutocomplete)
  },
#if defined(USE_AURA)
  {
    "enable-overscroll-history-navigation",
    IDS_FLAGS_ENABLE_OVERSCROLL_HISTORY_NAVIGATION_NAME,
    IDS_FLAGS_ENABLE_OVERSCROLL_HISTORY_NAVIGATION_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kEnableOverscrollHistoryNavigation)
  },
#endif

  {
    "enable-touch-drag-drop",
    IDS_FLAGS_ENABLE_TOUCH_DRAG_DROP_NAME,
    IDS_FLAGS_ENABLE_TOUCH_DRAG_DROP_DESCRIPTION,
    kOsWin | kOsCrOS,
    SINGLE_VALUE_TYPE(switches::kEnableTouchDragDrop)
  },
  {
    "load-cloud-policy-on-signin",
    IDS_FLAGS_DESKTOP_CLOUD_POLICY_NAME,
    IDS_FLAGS_DESKTOP_CLOUD_POLICY_DESCRIPTION,
    kOsWin | kOsMac | kOsLinux,
    SINGLE_VALUE_TYPE(switches::kLoadCloudPolicyOnSignin)
  },
  {
    "enable-rich-notifications",
    IDS_FLAGS_ENABLE_RICH_NOTIFICATIONS_NAME,
    IDS_FLAGS_ENABLE_RICH_NOTIFICATIONS_DESCRIPTION,
    kOsWin | kOsCrOS,
    SINGLE_VALUE_TYPE(switches::kEnableRichNotifications)
  },
  {
    "full-history-sync",
    IDS_FLAGS_FULL_HISTORY_SYNC_NAME,
    IDS_FLAGS_FULL_HISTORY_SYNC_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kHistoryEnableFullHistorySync)
  },
  {
    "enable-data-channels",
    IDS_FLAGS_RTC_DATACHANNELS,
    IDS_FLAGS_RTC_DATACHANNELS_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kEnableDataChannels)
  },
};

const Experiment* experiments = kExperiments;
size_t num_experiments = arraysize(kExperiments);

// Stores and encapsulates the little state that about:flags has.
class FlagsState {
 public:
  FlagsState() : needs_restart_(false) {}
  void ConvertFlagsToSwitches(PrefService* prefs, CommandLine* command_line);
  bool IsRestartNeededToCommitChanges();
  void SetExperimentEnabled(
      PrefService* prefs, const std::string& internal_name, bool enable);
  void RemoveFlagsSwitches(
      std::map<std::string, CommandLine::StringType>* switch_list);
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

// Extracts the list of enabled lab experiments from preferences and stores them
// in a set.
void GetEnabledFlags(const PrefService* prefs, std::set<std::string>* result) {
  const ListValue* enabled_experiments = prefs->GetList(
      prefs::kEnabledLabsExperiments);
  if (!enabled_experiments)
    return;

  for (ListValue::const_iterator it = enabled_experiments->begin();
       it != enabled_experiments->end();
       ++it) {
    std::string experiment_name;
    if (!(*it)->GetAsString(&experiment_name)) {
      LOG(WARNING) << "Invalid entry in " << prefs::kEnabledLabsExperiments;
      continue;
    }
    result->insert(experiment_name);
  }
}

// Takes a set of enabled lab experiments
void SetEnabledFlags(
    PrefService* prefs, const std::set<std::string>& enabled_experiments) {
  ListPrefUpdate update(prefs, prefs::kEnabledLabsExperiments);
  ListValue* experiments_list = update.Get();

  experiments_list->Clear();
  for (std::set<std::string>::const_iterator it = enabled_experiments.begin();
       it != enabled_experiments.end();
       ++it) {
    experiments_list->Append(new StringValue(*it));
  }
}

// Returns the name used in prefs for the choice at the specified index.
std::string NameForChoice(const Experiment& e, int index) {
  DCHECK_EQ(Experiment::MULTI_VALUE, e.type);
  DCHECK_LT(index, e.num_choices);
  return std::string(e.internal_name) + about_flags::testing::kMultiSeparator +
      base::IntToString(index);
}

// Adds the internal names for the specified experiment to |names|.
void AddInternalName(const Experiment& e, std::set<std::string>* names) {
  if (e.type == Experiment::SINGLE_VALUE) {
    names->insert(e.internal_name);
  } else {
    DCHECK_EQ(Experiment::MULTI_VALUE, e.type);
    for (int i = 0; i < e.num_choices; ++i)
      names->insert(NameForChoice(e, i));
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
    default:
      NOTREACHED();
  }
  return true;
}

// Removes all experiments from prefs::kEnabledLabsExperiments that are
// unknown, to prevent this list to become very long as experiments are added
// and removed.
void SanitizeList(PrefService* prefs) {
  std::set<std::string> known_experiments;
  for (size_t i = 0; i < num_experiments; ++i) {
    DCHECK(ValidateExperiment(experiments[i]));
    AddInternalName(experiments[i], &known_experiments);
  }

  std::set<std::string> enabled_experiments;
  GetEnabledFlags(prefs, &enabled_experiments);

  std::set<std::string> new_enabled_experiments;
  std::set_intersection(
      known_experiments.begin(), known_experiments.end(),
      enabled_experiments.begin(), enabled_experiments.end(),
      std::inserter(new_enabled_experiments, new_enabled_experiments.begin()));

  if (new_enabled_experiments != enabled_experiments)
    SetEnabledFlags(prefs, new_enabled_experiments);
}

void GetSanitizedEnabledFlags(
    PrefService* prefs, std::set<std::string>* result) {
  SanitizeList(prefs);
  GetEnabledFlags(prefs, result);
}

// Variant of GetSanitizedEnabledFlags that also removes any flags that aren't
// enabled on the current platform.
void GetSanitizedEnabledFlagsForCurrentPlatform(
    PrefService* prefs, std::set<std::string>* result) {
  GetSanitizedEnabledFlags(prefs, result);

  // Filter out any experiments that aren't enabled on the current platform.  We
  // don't remove these from prefs else syncing to a platform with a different
  // set of experiments would be lossy.
  std::set<std::string> platform_experiments;
  int current_platform = GetCurrentPlatform();
  for (size_t i = 0; i < num_experiments; ++i) {
    if (experiments[i].supported_platforms & current_platform)
      AddInternalName(experiments[i], &platform_experiments);
  }

  std::set<std::string> new_enabled_experiments;
  std::set_intersection(
      platform_experiments.begin(), platform_experiments.end(),
      result->begin(), result->end(),
      std::inserter(new_enabled_experiments, new_enabled_experiments.begin()));

  result->swap(new_enabled_experiments);
}

// Returns the Value representing the choice data in the specified experiment.
Value* CreateChoiceData(const Experiment& experiment,
                        const std::set<std::string>& enabled_experiments) {
  DCHECK_EQ(Experiment::MULTI_VALUE, experiment.type);
  ListValue* result = new ListValue;
  for (int i = 0; i < experiment.num_choices; ++i) {
    const Experiment::Choice& choice = experiment.choices[i];
    DictionaryValue* value = new DictionaryValue;
    std::string name = NameForChoice(experiment, i);
    value->SetString("description",
                     l10n_util::GetStringUTF16(choice.description_id));
    value->SetString("internal_name", name);
    value->SetBoolean("selected", enabled_experiments.count(name) > 0);
    result->Append(value);
  }
  return result;
}

}  // namespace

void ConvertFlagsToSwitches(PrefService* prefs, CommandLine* command_line) {
  FlagsState::GetInstance()->ConvertFlagsToSwitches(prefs, command_line);
}

ListValue* GetFlagsExperimentsData(PrefService* prefs) {
  std::set<std::string> enabled_experiments;
  GetSanitizedEnabledFlags(prefs, &enabled_experiments);

  int current_platform = GetCurrentPlatform();

  ListValue* experiments_data = new ListValue();
  for (size_t i = 0; i < num_experiments; ++i) {
    const Experiment& experiment = experiments[i];

    DictionaryValue* data = new DictionaryValue();
    data->SetString("internal_name", experiment.internal_name);
    data->SetString("name",
                    l10n_util::GetStringUTF16(experiment.visible_name_id));
    data->SetString("description",
                    l10n_util::GetStringUTF16(
                        experiment.visible_description_id));
    bool supported = !!(experiment.supported_platforms & current_platform);
    data->SetBoolean("supported", supported);

    ListValue* supported_platforms = new ListValue();
    AddOsStrings(experiment.supported_platforms, supported_platforms);
    data->Set("supported_platforms", supported_platforms);

    switch (experiment.type) {
      case Experiment::SINGLE_VALUE:
        data->SetBoolean(
            "enabled",
            enabled_experiments.count(experiment.internal_name) > 0);
        break;
      case Experiment::MULTI_VALUE:
        data->Set("choices", CreateChoiceData(experiment, enabled_experiments));
        break;
      default:
        NOTREACHED();
    }

    experiments_data->Append(data);
  }
  return experiments_data;
}

bool IsRestartNeededToCommitChanges() {
  return FlagsState::GetInstance()->IsRestartNeededToCommitChanges();
}

void SetExperimentEnabled(
    PrefService* prefs, const std::string& internal_name, bool enable) {
  FlagsState::GetInstance()->SetExperimentEnabled(prefs, internal_name, enable);
}

void RemoveFlagsSwitches(
    std::map<std::string, CommandLine::StringType>* switch_list) {
  FlagsState::GetInstance()->RemoveFlagsSwitches(switch_list);
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

void RecordUMAStatistics(const PrefService* prefs) {
  std::set<std::string> flags;
  GetEnabledFlags(prefs, &flags);
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

void FlagsState::ConvertFlagsToSwitches(
    PrefService* prefs, CommandLine* command_line) {
  if (command_line->HasSwitch(switches::kNoExperiments))
    return;

  std::set<std::string> enabled_experiments;

  GetSanitizedEnabledFlagsForCurrentPlatform(prefs, &enabled_experiments);

  typedef std::map<std::string, std::pair<std::string, std::string> >
      NameToSwitchAndValueMap;
  NameToSwitchAndValueMap name_to_switch_map;
  for (size_t i = 0; i < num_experiments; ++i) {
    const Experiment& e = experiments[i];
    if (e.type == Experiment::SINGLE_VALUE) {
      name_to_switch_map[e.internal_name] =
          std::pair<std::string, std::string>(e.command_line_switch,
                                              e.command_line_value);
    } else {
      for (int j = 0; j < e.num_choices; ++j)
        name_to_switch_map[NameForChoice(e, j)] =
            std::pair<std::string, std::string>(
                e.choices[j].command_line_switch,
                e.choices[j].command_line_value);
    }
  }

  command_line->AppendSwitch(switches::kFlagSwitchesBegin);
  flags_switches_.insert(
      std::pair<std::string, std::string>(switches::kFlagSwitchesBegin,
                                          std::string()));
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

    command_line->AppendSwitchASCII(switch_and_value_pair.first,
                                    switch_and_value_pair.second);
    flags_switches_[switch_and_value_pair.first] = switch_and_value_pair.second;
  }
  command_line->AppendSwitch(switches::kFlagSwitchesEnd);
  flags_switches_.insert(
      std::pair<std::string, std::string>(switches::kFlagSwitchesEnd,
                                          std::string()));
}

bool FlagsState::IsRestartNeededToCommitChanges() {
  return needs_restart_;
}

void FlagsState::SetExperimentEnabled(
    PrefService* prefs, const std::string& internal_name, bool enable) {
  needs_restart_ = true;

  size_t at_index = internal_name.find(about_flags::testing::kMultiSeparator);
  if (at_index != std::string::npos) {
    DCHECK(enable);
    // We're being asked to enable a multi-choice experiment. Disable the
    // currently selected choice.
    DCHECK_NE(at_index, 0u);
    const std::string experiment_name = internal_name.substr(0, at_index);
    SetExperimentEnabled(prefs, experiment_name, false);

    // And enable the new choice, if it is not the default first choice.
    if (internal_name != experiment_name + "@0") {
      std::set<std::string> enabled_experiments;
      GetSanitizedEnabledFlags(prefs, &enabled_experiments);
      enabled_experiments.insert(internal_name);
      SetEnabledFlags(prefs, enabled_experiments);
    }
    return;
  }

  std::set<std::string> enabled_experiments;
  GetSanitizedEnabledFlags(prefs, &enabled_experiments);

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
      enabled_experiments.insert(internal_name);
    else
      enabled_experiments.erase(internal_name);
  } else {
    if (enable) {
      // Enable the first choice.
      enabled_experiments.insert(NameForChoice(*e, 0));
    } else {
      // Find the currently enabled choice and disable it.
      for (int i = 0; i < e->num_choices; ++i) {
        std::string choice_name = NameForChoice(*e, i);
        if (enabled_experiments.find(choice_name) !=
            enabled_experiments.end()) {
          enabled_experiments.erase(choice_name);
          // Continue on just in case there's a bug and more than one
          // experiment for this choice was enabled.
        }
      }
    }
  }

  SetEnabledFlags(prefs, enabled_experiments);
}

void FlagsState::RemoveFlagsSwitches(
    std::map<std::string, CommandLine::StringType>* switch_list) {
  for (std::map<std::string, std::string>::const_iterator
           it = flags_switches_.begin(); it != flags_switches_.end(); ++it) {
    switch_list->erase(it->first);
  }
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
