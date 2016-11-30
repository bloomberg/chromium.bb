// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_features.h"

#include "extensions/features/features.h"
#include "ppapi/features/features.h"

namespace features {

// All features in alphabetical order.

#if defined(OS_MACOSX)
// Enables Javascript execution via AppleScript.
const base::Feature kAppleScriptExecuteJavaScript{
    "AppleScriptExecuteJavaScript", base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // defined(OS_MACOSX)

#if defined(OS_CHROMEOS)
// Whether to handle low memory kill of ARC apps by Chrome.
const base::Feature kArcMemoryManagement{
    "ArcMemoryManagement", base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // defined(OS_CHROMEOS)

// If enabled, the list of content suggestions on the New Tab page will contain
// assets (e.g. books, pictures, audio) that the user downloaded for later use.
const base::Feature kAssetDownloadSuggestionsFeature{
    "NTPAssetDownloadSuggestions", base::FEATURE_ENABLED_BY_DEFAULT};

#if !defined(OS_ANDROID) && !defined(OS_IOS)
// Enables auto-dismissing JavaScript dialogs.
const base::Feature kAutoDismissingDialogs{"AutoDismissingDialogs",
                                           base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if defined(OS_WIN) || defined(OS_MACOSX)
// Enables automatic tab discarding, when the system is in low memory state.
const base::Feature kAutomaticTabDiscarding{"AutomaticTabDiscarding",
                                            base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // defined(OS_WIN) || defined(OS_MACOSX)

#if defined(OS_WIN) || defined(OS_LINUX)
// Enables the Restart background mode optimization. When all Chrome UI is
// closed and it goes in the background, allows to restart the browser to
// discard memory.
const base::Feature kBackgroundModeAllowRestart{
    "BackgroundModeAllowRestart", base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // defined(OS_WIN) || defined(OS_LINUX)

// Enables the Backspace key to navigate back in the browser, as well as
// Shift+Backspace to navigate forward.
const base::Feature kBackspaceGoesBackFeature {
  "BackspaceGoesBack", base::FEATURE_DISABLED_BY_DEFAULT
};

// Enables or disables whether permission prompts are automatically blocked
// after the user has explicitly dismissed them too many times.
const base::Feature kBlockPromptsIfDismissedOften{
    "BlockPromptsIfDismissedOften", base::FEATURE_DISABLED_BY_DEFAULT};

// Fixes for browser hang bugs are deployed in a field trial in order to measure
// their impact. See crbug.com/478209.
const base::Feature kBrowserHangFixesExperiment{
    "BrowserHangFixesExperiment", base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(OS_ANDROID)
// Experiment to make Geolocation permissions in the omnibox and the default
// search engine's search page consistent.
const base::Feature kConsistentOmniboxGeolocation{
    "ConsistentOmniboxGeolocation", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if defined(OS_WIN)
// Disables the AutoImport feature on first run. See crbug.com/555550
const base::Feature kDisableFirstRunAutoImportWin{
    "DisableFirstRunAutoImport", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Experiment to display a toggle allowing users to opt-out of persisting a
// Grant or Deny decision in a permission prompt.
const base::Feature kDisplayPersistenceToggleInPermissionPrompts{
    "DisplayPersistenceToggleInPermissionPrompts",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables Expect CT reporting, which sends reports for opted-in sites
// that don't serve sufficient Certificate Transparency information.
const base::Feature kExpectCTReporting{"ExpectCTReporting",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// An experimental fullscreen prototype that allows pages to map browser and
// system-reserved keyboard shortcuts.
const base::Feature kExperimentalKeyboardLockUI{
    "ExperimentalKeyboardLockUI", base::FEATURE_DISABLED_BY_DEFAULT};

#if defined (OS_CHROMEOS)
// Enables or disables the Happiness Tracking System for the device.
const base::Feature kHappinessTrackingSystem {
    "HappinessTrackingSystem", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if defined(GOOGLE_CHROME_BUILD) && defined(OS_LINUX) && !defined(OS_CHROMEOS)
// Enables showing the "This computer will no longer receive Google Chrome
// updates" infobar instead of the "will soon stop receiving" infobar on
// deprecated systems.
const base::Feature kLinuxObsoleteSystemIsEndOfTheLine{
    "LinuxObsoleteSystemIsEndOfTheLine", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Enables or disables the Material Design version of chrome://bookmarks.
const base::Feature kMaterialDesignBookmarks{"MaterialDesignBookmarks",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

#if BUILDFLAG(ENABLE_EXTENSIONS)
// Enabled or disabled the Material Design version of chrome://extensions.
const base::Feature kMaterialDesignExtensions{
    "MaterialDesignExtensions", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Enables or disables the Material Design version of chrome://history.
const base::Feature kMaterialDesignHistory{"MaterialDesignHistory",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables the Material Design version of chrome://settings.
// Also affects chrome://help.
const base::Feature kMaterialDesignSettings{"MaterialDesignSettings",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

#if !defined(OS_ANDROID) && !defined(OS_IOS)
// Enables media content bitstream remoting, an optimization that can activate
// during Cast Tab Mirroring. When kMediaRemotingEncrypted is disabled, the
// feature will not activate for encrypted content.
const base::Feature kMediaRemoting{"MediaRemoting",
                                   base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kMediaRemotingEncrypted{"MediaRemotingEncrypted",
                                            base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // !defined(OS_ANDROID) && !defined(OS_IOS)

// Enables or disables modal permission prompts.
const base::Feature kModalPermissionPrompts{"ModalPermissionPrompts",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the use of native notification centers instead of using the Message
// Center for displaying the toasts.
#if defined(OS_MACOSX)
const base::Feature kNativeNotifications{"NativeNotifications",
                                         base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // defined(OS_MACOSX)

// If enabled, the list of content suggestions on the New Tab page will contain
// pages that the user downloaded for later use.
const base::Feature kOfflinePageDownloadSuggestionsFeature{
    "NTPOfflinePageDownloadSuggestions", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables YouTube Flash videos to be overridden.
const base::Feature kOverrideYouTubeFlashEmbed{
    "OverrideYouTubeFlashEmbed", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables Permissions Blacklisting via Safe Browsing.
const base::Feature kPermissionsBlacklist{
    "PermissionsBlacklist", base::FEATURE_DISABLED_BY_DEFAULT};

#if BUILDFLAG(ENABLE_PLUGINS)
// Prefer HTML content by hiding Flash from the list of plugins.
// https://crbug.com/626728
const base::Feature kPreferHtmlOverPlugins{"PreferHtmlOverPlugins",
                                           base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Enables the Print Scaling feature in print preview.
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
const base::Feature kPrintScaling{"PrintScaling",
                                  base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Enables or disables push subscriptions keeping Chrome running in the
// background when closed.
const base::Feature kPushMessagingBackgroundMode{
    "PushMessagingBackgroundMode", base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(OS_CHROMEOS)
// Runtime flag that indicates whether this leak detector should be enabled in
// the current instance of Chrome.
const base::Feature kRuntimeMemoryLeakDetector{
    "RuntimeMemoryLeakDetector", base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // defined(OS_CHROMEOS)

#if BUILDFLAG(ENABLE_PLUGINS)
// Disables Plugin Power Saver when Flash is in ALLOW mode.
const base::Feature kRunAllFlashInAllowMode{"RunAllFlashInAllowMode",
                                            base::FEATURE_DISABLED_BY_DEFAULT};
#endif

const base::Feature kSafeSearchUrlReporting{"SafeSearchUrlReporting",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

#if !defined(OS_ANDROID) && !defined(OS_IOS)
// Sets the visibility and animation of the security chip.
const base::Feature kSecurityChip{"SecurityChip",
                                  base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// A new user experience for transitioning into fullscreen and mouse pointer
// lock states.
const base::Feature kSimplifiedFullscreenUI{"ViewsSimplifiedFullscreenUI",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

#if defined(SYZYASAN)
// Enable the deferred free mechanism in the syzyasan module, which helps the
// performance by deferring some work on the critical path to a background
// thread.
const base::Feature kSyzyasanDeferredFree{"SyzyasanDeferredFree",
                                          base::FEATURE_ENABLED_BY_DEFAULT};
#endif

// Experiment to use grouped permission infobars which could show and handle
// multiple permission requests.
const base::Feature kUseGroupedPermissionInfobars{
    "UseGroupedPermissionInfobars", base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(OS_CHROMEOS)
// Enables or disables the opt-in IME menu in the language settings page.
const base::Feature kOptInImeMenu{"OptInImeMenu",
                                  base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables PIN quick unlock settings integration.
const base::Feature kQuickUnlockPin{"QuickUnlockPin",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables emoji, handwriting and voice input on opt-in IME menu.
const base::Feature kEHVInputOnImeMenu{"EmojiHandwritingVoiceInput",
                                       base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // defined(OS_CHROMEOS)

}  // namespace features
