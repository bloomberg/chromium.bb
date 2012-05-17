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
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/user_metrics.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gl/gl_switches.h"

#if defined(USE_ASH)
#include "ash/ash_switches.h"
#endif

#if defined(USE_AURA)
#include "ui/aura/aura_switches.h"
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

// Names for former Chrome OS Labs experiments, shared with prefs migration
// code.
const char kMediaPlayerExperimentName[] = "media-player";
const char kAdvancedFileSystemExperimentName[] = "advanced-file-system";
const char kVerticalTabsExperimentName[] = "vertical-tabs";

const Experiment::Choice kOmniboxInlineHistoryQuickProviderChoices[] = {
  { IDS_FLAGS_OMNIBOX_INLINE_HISTORY_QUICK_PROVIDER_AUTOMATIC, "", "" },
  { IDS_FLAGS_OMNIBOX_INLINE_HISTORY_QUICK_PROVIDER_ALLOWED,
    switches::kOmniboxInlineHistoryQuickProvider,
    switches::kOmniboxInlineHistoryQuickProviderAllowed },
  { IDS_FLAGS_OMNIBOX_INLINE_HISTORY_QUICK_PROVIDER_PROHIBITED,
    switches::kOmniboxInlineHistoryQuickProvider,
    switches::kOmniboxInlineHistoryQuickProviderProhibited }
};

const Experiment::Choice kThreadedCompositingModeChoices[] = {
  { IDS_FLAGS_THREADED_COMPOSITING_MODE_DEFAULT, "", "" },
  { IDS_FLAGS_THREADED_COMPOSITING_MODE_DISABLED,
    switches::kDisableThreadedCompositing, ""},
  { IDS_FLAGS_THREADED_COMPOSITING_MODE_ENABLED,
    switches::kEnableThreadedCompositing, ""}
};

const Experiment::Choice kTouchOptimizedUIChoices[] = {
  { IDS_FLAGS_TOUCH_OPTIMIZED_UI_AUTOMATIC, "", "" },
  { IDS_FLAGS_TOUCH_OPTIMIZED_UI_ENABLED,
    switches::kTouchOptimizedUI,
    switches::kTouchOptimizedUIEnabled },
  { IDS_FLAGS_TOUCH_OPTIMIZED_UI_DISABLED,
    switches::kTouchOptimizedUI,
    switches::kTouchOptimizedUIDisabled }
};


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
// After your feature has shipped under a flag, you can locate the metrics
// under the action name AboutFlags_internal-action-name. Actions are recorded
// once per startup, so you should divide this number by AboutFlags_StartupTick
// to get a sense of usage. Note that this will not be the same as number of
// users with a given feature enabled because users can quit and relaunch
// the application multiple times over a given time interval.
// TODO(rsesek): See if there's a way to count per-user, rather than
// per-startup.

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
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kForceCompositingMode)
  },
  {
    "threaded-compositing-mode",
    IDS_FLAGS_THREADED_COMPOSITING_MODE_NAME,
    IDS_FLAGS_THREADED_COMPOSITING_MODE_DESCRIPTION,
    kOsAll,
    MULTI_VALUE_TYPE(kThreadedCompositingModeChoices)
  },
  {
    "disable-threaded-animation",
    IDS_FLAGS_DISABLE_THREADED_ANIMATION_NAME,
    IDS_FLAGS_DISABLE_THREADED_ANIMATION_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kDisableThreadedAnimation)
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
  // TODO(dspringer): When NaCl is on by default, remove this flag entry.
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
    kOsWin,
    SINGLE_VALUE_TYPE(switches::kEnableNaClExceptionHandling)
  },
  {
    "nacl-gdb",  // FLAGS:RECORD_UMA
    IDS_FLAGS_NACL_GDB_NAME,
    IDS_FLAGS_NACL_GDB_DESCRIPTION,
    kOsWin,
    SINGLE_VALUE_TYPE(switches::kNaClGdb)
  },
  {
    "extension-apis",  // FLAGS:RECORD_UMA
    IDS_FLAGS_EXPERIMENTAL_EXTENSION_APIS_NAME,
    IDS_FLAGS_EXPERIMENTAL_EXTENSION_APIS_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kEnableExperimentalExtensionApis)
  },
  {
    "browser-actions-for-all",
    IDS_FLAGS_BROWSER_ACTIONS_FOR_ALL_NAME,
    IDS_FLAGS_BROWSER_ACTIONS_FOR_ALL_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kEnableBrowserActionsForAll),
  },
  {
    "action-box",
    IDS_FLAGS_ACTION_BOX_NAME,
    IDS_FLAGS_ACTION_BOX_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kEnableActionBox),
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
    "disable-interactive-form-validation",
    IDS_FLAGS_DISABLE_INTERACTIVE_FORM_VALIDATION_NAME,
    IDS_FLAGS_DISABLE_INTERACTIVE_FORM_VALIDATION_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kDisableInteractiveFormValidation)
  },
  {
    "focus-existing-tab-on-open",  // FLAGS:RECORD_UMA
    IDS_FLAGS_FOCUS_EXISTING_TAB_ON_OPEN_NAME,
    IDS_FLAGS_FOCUS_EXISTING_TAB_ON_OPEN_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kFocusExistingTabOnOpen)
  },
  {
    "tab-groups-context-menu",
    IDS_FLAGS_TAB_GROUPS_CONTEXT_MENU_NAME,
    IDS_FLAGS_TAB_GROUPS_CONTEXT_MENU_DESCRIPTION,
    kOsWin,
    SINGLE_VALUE_TYPE(switches::kEnableTabGroupsContextMenu)
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
    "sync-tabs",
    IDS_FLAGS_SYNC_TABS_NAME,
    IDS_FLAGS_SYNC_TABS_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kEnableSyncTabs)
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
    "enable-smooth-scrolling",  // FLAGS:RECORD_UMA
    IDS_FLAGS_ENABLE_SMOOTH_SCROLLING_NAME,
    IDS_FLAGS_ENABLE_SMOOTH_SCROLLING_DESCRIPTION,
    // Can't expose the switch unless the code is compiled in.
    // On by default for the Mac (different implementation in WebKit).
    kOsWin | kOsLinux | kOsCrOS,
    SINGLE_VALUE_TYPE(switches::kEnableSmoothScrolling)
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
    "disable-shortcuts-provider",
    IDS_FLAGS_DISABLE_SHORTCUTS_PROVIDER,
    IDS_FLAGS_DISABLE_SHORTCUTS_PROVIDER_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kDisableShortcutsProvider)
  },
  {
    "downloads-new-ui",  // FLAGS:RECORD_UMA
    IDS_FLAGS_DOWNLOADS_NEW_UI_NAME,
    IDS_FLAGS_DOWNLOADS_NEW_UI_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kDownloadsNewUI)
  },
  {
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
    SINGLE_VALUE_TYPE(switches::kEnableAsyncDns)
  },
  {
    "enable-video-track",
    IDS_FLAGS_ENABLE_VIDEO_TRACK_NAME,
    IDS_FLAGS_ENABLE_VIDEO_TRACK_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kEnableVideoTrack)
  },
  {
    "enable-media-source",
    IDS_FLAGS_ENABLE_MEDIA_SOURCE_NAME,
    IDS_FLAGS_ENABLE_MEDIA_SOURCE_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kEnableMediaSource)
  },
  {
    "enable-encrypted-media",
    IDS_FLAGS_ENABLE_ENCRYPTED_MEDIA_NAME,
    IDS_FLAGS_ENABLE_ENCRYPTED_MEDIA_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kEnableEncryptedMedia)
  },
  {
    "enable-pointer-lock",
    IDS_FLAGS_ENABLE_POINTER_LOCK_NAME,
    IDS_FLAGS_ENABLE_POINTER_LOCK_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kEnablePointerLock)
  },
#if defined(USE_ASH)
  {
    "aura-google-dialog-frames",
    IDS_FLAGS_AURA_GOOGLE_DIALOG_FRAMES_NAME,
    IDS_FLAGS_AURA_GOOGLE_DIALOG_FRAMES_DESCRIPTION,
    kOsWin | kOsLinux | kOsCrOS,
    SINGLE_VALUE_TYPE(ash::switches::kAuraGoogleDialogFrames)
  },
#endif
#if defined(USE_AURA)
  {
    "aura-disable-hold-mouse-moves",
    IDS_FLAGS_AURA_DISABLE_HOLD_MOUSE_MOVES_NAME,
    IDS_FLAGS_AURA_DISABLE_HOLD_MOUSE_MOVES_DESCRIPTION,
    kOsWin | kOsLinux | kOsCrOS,
    SINGLE_VALUE_TYPE(switches::kAuraDisableHoldMouseMoves)
  },
#endif  // defined(USE_AURA)
  {
    "enable-gamepad",
    IDS_FLAGS_ENABLE_GAMEPAD_NAME,
    IDS_FLAGS_ENABLE_GAMEPAD_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kEnableGamepad)
  },
  {
    "per-tile-painting",
    IDS_FLAGS_PER_TILE_PAINTING_NAME,
    IDS_FLAGS_PER_TILE_PAINTING_DESCRIPTION,
#if defined(USE_SKIA)
    kOsMac | kOsLinux | kOsCrOS,
#else
    0,
#endif
    SINGLE_VALUE_TYPE(switches::kEnablePerTilePainting)
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
    "enable-peer-connection",
    IDS_FLAGS_PEER_CONNECTION_NAME,
    IDS_FLAGS_PEER_CONNECTION_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kEnablePeerConnection)
  },
  {
    "enable-shadow-dom",
    IDS_FLAGS_SHADOW_DOM_NAME,
    IDS_FLAGS_SHADOW_DOM_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kEnableShadowDOM)
  },
  {
    "enable-style-scoped",
    IDS_FLAGS_STYLE_SCOPED_NAME,
    IDS_FLAGS_STYLE_SCOPED_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kEnableStyleScoped)
  },
  {
    "enable-css-regions",
    IDS_FLAGS_CSS_REGIONS_NAME,
    IDS_FLAGS_CSS_REGIONS_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kEnableCssRegions)
  },
  {
    "enable-extension-activity-ui",
    IDS_FLAGS_ENABLE_EXTENSION_ACTIVITY_UI_NAME,
    IDS_FLAGS_ENABLE_EXTENSION_ACTIVITY_UI_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kEnableExtensionActivityUI)
  },
  {
    "webui-task-manager",
    IDS_FLAGS_WEBUI_TASK_MANAGER_NAME,
    IDS_FLAGS_WEBUI_TASK_MANAGER_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kWebUITaskManager)
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
    "disable-chrome-to-mobile",  // FLAGS:RECORD_UMA
    IDS_FLAGS_DISABLE_CHROME_TO_MOBILE_NAME,
    IDS_FLAGS_DISABLE_CHROME_TO_MOBILE_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kDisableChromeToMobile)
  },
  {
    "enable-captive-portal-detection",
    IDS_FLAGS_CAPTIVE_PORTAL_CHECK_ON_ERROR_NAME,
    IDS_FLAGS_CAPTIVE_PORTAL_CHECK_ON_ERROR_DESCRIPTION,
    kOsMac | kOsWin | kOsLinux | kOsCrOS,
    SINGLE_VALUE_TYPE(switches::kCaptivePortalDetection)
  },
#if defined(GOOGLE_CHROME_BUILD)
  {
    "enable-asynchronous-spellchecking",
    IDS_FLAGS_ENABLE_ASYNCHRONOUS_SPELLCHECKING,
    IDS_FLAGS_ENABLE_ASYNCHRONOUS_SPELLCHECKING_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kEnableAsynchronousSpellChecking)
  },
#endif
  {
    "touch-optimized-ui",
    IDS_FLAGS_TOUCH_OPTIMIZED_UI_NAME,
    IDS_FLAGS_TOUCH_OPTIMIZED_UI_DESCRIPTION,
    kOsWin | kOsCrOS,
    MULTI_VALUE_TYPE(kTouchOptimizedUIChoices)
  },
  {
    "enable-touch-events",
    IDS_ENABLE_TOUCH_EVENTS_NAME,
    IDS_ENABLE_TOUCH_EVENTS_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kEnableTouchEvents)
  },
#if defined(OS_CHROMEOS)
  {
    "no-discard-tabs",
    IDS_FLAGS_NO_DISCARD_TABS_NAME,
    IDS_FLAGS_NO_DISCARD_TABS_DESCRIPTION,
    kOsCrOS,
    SINGLE_VALUE_TYPE(switches::kNoDiscardTabs)
  },
#endif
  {
    "enable-sync-signin",
    IDS_ENABLE_SYNC_SIGNIN_NAME,
    IDS_ENABLE_SYNC_SIGNIN_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kEnableSyncSignin)
  },
  {
    "enable-platform-apps",
    IDS_FLAGS_ENABLE_PLATFORM_APPS_NAME,
    IDS_FLAGS_ENABLE_PLATFORM_APPS_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(switches::kEnablePlatformApps)
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
    "default-device-scale-factor",
    IDS_FLAGS_FORCE_HIGH_DPI_NAME,
    IDS_FLAGS_FORCE_HIGH_DPI_DESCRIPTION,
    kOsCrOS,
    SINGLE_VALUE_TYPE_AND_VALUE(switches::kDefaultDeviceScaleFactor, "2")
  },
#if defined(OS_CHROMEOS)
  {
    "allow-touchpad-three-finger-click",
    IDS_FLAGS_ALLOW_TOUCHPAD_THREE_FINGER_CLICK_NAME,
    IDS_FLAGS_ALLOW_TOUCHPAD_THREE_FINGER_CLICK_DESCRIPTION,
    kOsCrOS,
    SINGLE_VALUE_TYPE(switches::kEnableTouchpadThreeFingerClick)
  },
#endif
  {
    "enable-client-oauth-signin",
    IDS_FLAGS_ENABLE_CLIENT_OAUTH_SIGNIN_NAME,
    IDS_FLAGS_ENABLE_CLIENT_OAUTH_SIGNIN_DESCRIPTION,
    kOsMac | kOsWin | kOsLinux,
    SINGLE_VALUE_TYPE(switches::kEnableClientOAuthSignin)
  },
#if defined(USE_ASH)
  {
    "enable-applist-v2",
    IDS_FLAGS_ENABLE_APPLIST_V2_NAME,
    IDS_FLAGS_ENABLE_APPLIST_V2_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(ash::switches::kEnableAppListV2),
  },
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
    "ash-notify",
    IDS_FLAGS_ENABLE_ASH_NOTIFY_NAME,
    IDS_FLAGS_ENABLE_ASH_NOTIFY_DESCRIPTION,
    kOsAll,
    SINGLE_VALUE_TYPE(ash::switches::kAshNotify),
  },
#endif  // defined(USE_ASH)
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
