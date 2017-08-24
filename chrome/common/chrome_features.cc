// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_features.h"

#include "base/command_line.h"
#include "build/build_config.h"
#include "chrome/common/chrome_switches.h"
#include "extensions/features/features.h"
#include "ppapi/features/features.h"

namespace features {

// All features in alphabetical order.

#if defined(OS_ANDROID)
const base::Feature kAllowAutoplayUnmutedInWebappManifestScope{
    "AllowAutoplayUnmutedInWebappManifestScope",
    base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // defined(OS_ANDROID)

#if defined(OS_MACOSX)
// Enables Javascript execution via AppleScript.
const base::Feature kAppleScriptExecuteJavaScript{
    "AppleScriptExecuteJavaScript", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the fullscreen toolbar to reveal itself if it's hidden.
const base::Feature kFullscreenToolbarReveal{"FullscreenToolbarReveal",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// Use the Toolkit-Views Task Manager window.
const base::Feature kViewsTaskManager{"ViewsTaskManager",
                                      base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // defined(OS_MACOSX)

#if !defined(OS_ANDROID)
const base::Feature kAnimatedAppMenuIcon{"AnimatedAppMenuIcon",
                                         base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kAppBanners {
  "AppBanners",
#if defined(OS_CHROMEOS)
      base::FEATURE_ENABLED_BY_DEFAULT,
#else
      base::FEATURE_DISABLED_BY_DEFAULT,
#endif  // defined(OS_CHROMEOS)
};
#endif  // !defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
// Whether to handle low memory kill of ARC apps by Chrome.
const base::Feature kArcMemoryManagement{
    "ArcMemoryManagement", base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // defined(OS_CHROMEOS)

// If enabled, the list of content suggestions on the New Tab page will contain
// assets (e.g. books, pictures, audio) that the user downloaded for later use.
const base::Feature kAssetDownloadSuggestionsFeature{
    "NTPAssetDownloadSuggestions", base::FEATURE_ENABLED_BY_DEFAULT};

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

// Enables or disables whether permission prompts are automatically blocked
// after the user has explicitly dismissed them too many times.
const base::Feature kBlockPromptsIfDismissedOften{
    "BlockPromptsIfDismissedOften", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables whether permission prompts are automatically blocked
// after the user has ignored them too many times.
const base::Feature kBlockPromptsIfIgnoredOften{
    "BlockPromptsIfIgnoredOften", base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(OS_MACOSX)
// Enables the new bookmark app system (e.g. Add To Applications on Mac).
const base::Feature kBookmarkApps{"BookmarkAppsMac",
                                  base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Fixes for browser hang bugs are deployed in a field trial in order to measure
// their impact. See crbug.com/478209.
const base::Feature kBrowserHangFixesExperiment{
    "BrowserHangFixesExperiment", base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(OS_MACOSX)
// Enables or disables the browser's touch bar.
const base::Feature kBrowserTouchBar{"BrowserTouchBar",
                                     base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables touch bar support for dialogs.
const base::Feature kDialogTouchBar{"DialogTouchBar",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables keyboard focus for the tab strip.
const base::Feature kTabStripKeyboardFocus{"TabStripKeyboardFocus",
                                           base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // defined(OS_MACOSX)

// Enables Basic/Advanced tabs in ClearBrowsingData.
const base::Feature kTabsInCbd {
  "TabsInCBD",
#if defined(OS_ANDROID)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// If enabled, we'll only take thumbnails of unknown URLs (i.e. URLs that are
// not (yet) part of TopSites) if they have an interesting transition type, i.e.
// one that qualifies for inclusion in TopSites.
const base::Feature kCaptureThumbnailDependingOnTransitionType{
    "CaptureThumbnailDependingOnTransitionType",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Whether to capture page thumbnails when the page load finishes (in addition
// to any other times this might happen).
const base::Feature kCaptureThumbnailOnLoadFinished{
    "CaptureThumbnailOnLoadFinished", base::FEATURE_DISABLED_BY_DEFAULT};

// Whether to capture page thumbnails when navigating away from the current page
// (in addition to any other times this might happen).
const base::Feature kCaptureThumbnailOnNavigatingAway{
    "CaptureThumbnailOnNavigatingAway", base::FEATURE_DISABLED_BY_DEFAULT};

// Whether to trigger app banner installability checks on page load.
const base::Feature kCheckInstallabilityForBannerOnLoad{
    "CheckInstallabilityForBannerOnLoad", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kClickToOpenPDFPlaceholder{
    "ClickToOpenPDFPlaceholder", base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(OS_ANDROID)
// Experiment to make Geolocation permissions in the omnibox and the default
// search engine's search page consistent.
const base::Feature kConsistentOmniboxGeolocation{
    "ConsistentOmniboxGeolocation", base::FEATURE_ENABLED_BY_DEFAULT};
#endif

#if defined(OS_MACOSX)
const base::Feature kContentFullscreen{"ContentFullscreen",
                                       base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if defined(OS_ANDROID)
// Experiment to extract structured metadata for app indexing.
const base::Feature kCopylessPaste{"CopylessPaste",
                                   base::FEATURE_ENABLED_BY_DEFAULT};
#endif

#if defined(OS_WIN)
// Enables or disables desktop ios promotion, which shows a promotion to the
// user promoting Chrome for iOS.
const base::Feature kDesktopIOSPromotion{"DesktopIOSPromotion",
                                         base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Enables or disables windowing related features for desktop PWAs.
const base::Feature kDesktopPWAWindowing{"DesktopPWAWindowing",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

// Experiment to display a toggle allowing users to opt-out of persisting a
// Grant or Deny decision in a permission prompt.
const base::Feature kDisplayPersistenceToggleInPermissionPrompts{
    "DisplayPersistenceToggleInPermissionPrompts",
    base::FEATURE_DISABLED_BY_DEFAULT};

#if !defined(OS_ANDROID)
const base::Feature kDoodlesOnLocalNtp{"DoodlesOnLocalNtp",
                                       base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Enables Expect CT reporting, which sends reports for opted-in sites
// that don't serve sufficient Certificate Transparency information.
const base::Feature kExpectCTReporting{"ExpectCTReporting",
                                       base::FEATURE_ENABLED_BY_DEFAULT};

// An experimental way of showing app banners, which has modal banners and gives
// developers more control over when to show them.
const base::Feature kExperimentalAppBanners{"ExperimentalAppBanners",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// An experimental fullscreen prototype that allows pages to map browser and
// system-reserved keyboard shortcuts.
const base::Feature kExperimentalKeyboardLockUI{
    "ExperimentalKeyboardLockUI", base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(OS_WIN)
// Enables using GDI to print text as simply text.
const base::Feature kGdiTextPrinting {"GdiTextPrinting",
                                      base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if defined (OS_CHROMEOS)
// Enables or disables the Happiness Tracking System for the device.
const base::Feature kHappinessTrackingSystem {
    "HappinessTrackingSystem", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

const base::Feature kImportantSitesInCbd{"ImportantSitesInCBD",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether the "improved recovery component" is used. The improved
// recovery component is a redesigned Chrome component intended to restore
// a broken Chrome updater in more scenarios than before.
const base::Feature kImprovedRecoveryComponent{
    "ImprovedRecoveryComponent", base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(GOOGLE_CHROME_BUILD) && defined(OS_LINUX) && !defined(OS_CHROMEOS)
// Enables showing the "This computer will no longer receive Google Chrome
// updates" infobar instead of the "will soon stop receiving" infobar on
// deprecated systems.
const base::Feature kLinuxObsoleteSystemIsEndOfTheLine{
    "LinuxObsoleteSystemIsEndOfTheLine", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Enables or disables the Location Settings Dialog (LSD). The LSD is an Android
// system-level geolocation permission prompt.
const base::Feature kLsdPermissionPrompt{"LsdPermissionPrompt",
                                         base::FEATURE_ENABLED_BY_DEFAULT};

#if defined(OS_MACOSX)
// Enables RTL layout in macOS top chrome.
const base::Feature kMacRTL{"MacRTL", base::FEATURE_DISABLED_BY_DEFAULT};

// Uses NSFullSizeContentViewWindowMask where available instead of adding our
// own views to the window frame. This is a temporary kill switch, it can be
// removed once we feel okay about leaving it on.
const base::Feature kMacFullSizeContentView{"MacFullSizeContentView",
                                            base::FEATURE_ENABLED_BY_DEFAULT};
#endif

// Enables or disables the Material Design version of chrome://bookmarks.
const base::Feature kMaterialDesignBookmarks{"MaterialDesignBookmarks",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

#if BUILDFLAG(ENABLE_EXTENSIONS)
// Enabled or disabled the Material Design version of chrome://extensions.
const base::Feature kMaterialDesignExtensions{
    "MaterialDesignExtensions", base::FEATURE_DISABLED_BY_DEFAULT};

// Sets whether dismissing the new-tab-page override bubble counts as
// acknowledgement.
extern const base::Feature kAcknowledgeNtpOverrideOnDeactivate{
    "AcknowledgeNtpOverrideOnDeactivate", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// The material redesign of the Incognito NTP.
const base::Feature kMaterialDesignIncognitoNTP{
  "MaterialDesignIncognitoNTP",
#if defined(OS_ANDROID)
      base::FEATURE_DISABLED_BY_DEFAULT
#else
      base::FEATURE_ENABLED_BY_DEFAULT
#endif
};

#if !defined(OS_ANDROID)
// Enables media content bitstream remoting, an optimization that can activate
// during Cast Tab Mirroring.
const base::Feature kMediaRemoting{"MediaRemoting",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, replaces the <extensionview> controller in the route details view
// of the Media Router dialog with the controller bundled with the WebUI
// resources.
const base::Feature kMediaRouterUIRouteController{
    "MediaRouterUIRouteController", base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // !defined(OS_ANDROID)

// Enables or disables modal permission prompts.
const base::Feature kModalPermissionPrompts{"ModalPermissionPrompts",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(OS_WIN)
// Enables or disables the ModuleDatabase backend for the conflicts UI.
const base::Feature kModuleDatabase{"ModuleDatabase",
                                    base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if defined(OS_CHROMEOS)
// Enables or disables multidevice features and corresponding UI on Chrome OS.
const base::Feature kMultidevice{"Multidevice",
                                 base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Enables the use of native notification centers instead of using the Message
// Center for displaying the toasts.
#if BUILDFLAG(ENABLE_NATIVE_NOTIFICATIONS)
#if defined(OS_MACOSX) || defined(OS_ANDROID)
const base::Feature kNativeNotifications{"NativeNotifications",
                                         base::FEATURE_ENABLED_BY_DEFAULT};
#else
const base::Feature kNativeNotifications{"NativeNotifications",
                                         base::FEATURE_DISABLED_BY_DEFAULT};
#endif
#endif  // BUILDFLAG(ENABLE_NATIVE_NOTIFICATIONS)

#if BUILDFLAG(ENABLE_NATIVE_WINDOW_NAV_BUTTONS)
const base::Feature kNativeWindowNavButtons{"NativeWindowNavButtons",
                                            base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // BUILDFLAG(ENABLE_NATIVE_WINDOW_NAV_BUTTONS)

const base::Feature kNetworkPrediction{"NetworkPrediction",
                                       base::FEATURE_ENABLED_BY_DEFAULT};

// If enabled, the list of content suggestions on the New Tab page will contain
// pages that the user downloaded for later use.
const base::Feature kOfflinePageDownloadSuggestionsFeature{
    "NTPOfflinePageDownloadSuggestions", base::FEATURE_ENABLED_BY_DEFAULT};

#if !defined(OS_ANDROID)
// Enables or disabled the OneGoogleBar on the local NTP.
const base::Feature kOneGoogleBarOnLocalNtp{"OneGoogleBarOnLocalNtp",
                                            base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Enables Permissions Blacklisting via Safe Browsing.
const base::Feature kPermissionsBlacklist{
    "PermissionsBlacklist", base::FEATURE_DISABLED_BY_DEFAULT};

// Disables PostScript generation when printing to PostScript capable printers
// and instead sends Emf files.
#if defined(OS_WIN)
const base::Feature kDisablePostScriptPrinting{
    "DisablePostScriptPrinting", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
// Prefer HTML content by hiding Flash from the list of plugins.
// https://crbug.com/626728
const base::Feature kPreferHtmlOverPlugins{"PreferHtmlOverPlugins",
                                           base::FEATURE_ENABLED_BY_DEFAULT};
#endif

#if defined(OS_CHROMEOS)
// The lock screen will be preloaded so it is instantly available when the user
// locks the Chromebook device.
const base::Feature kPreloadLockScreen{"PreloadLockScreen",
                                       base::FEATURE_ENABLED_BY_DEFAULT};
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW) && !defined(OS_WIN) && !defined(OS_MACOSX)
// Enables the Print as Image feature in print preview.
const base::Feature kPrintPdfAsImage{"PrintPdfAsImage",
                                     base::FEATURE_ENABLED_BY_DEFAULT};
#endif

// Enables or disables push subscriptions keeping Chrome running in the
// background when closed.
const base::Feature kPushMessagingBackgroundMode{
    "PushMessagingBackgroundMode", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables support for Minimal-UI PWA display mode.
const base::Feature kPwaMinimalUi{"PwaMinimalUi",
                                  base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(OS_CHROMEOS)
// Runtime flag that indicates whether this leak detector should be enabled in
// the current instance of Chrome.
const base::Feature kRuntimeMemoryLeakDetector{
    "RuntimeMemoryLeakDetector", base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // defined(OS_CHROMEOS)

const base::Feature kSafeSearchUrlReporting{"SafeSearchUrlReporting",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(OS_ANDROID)
// Enables separate notification channels in Android O for notifications from
// different origins, instead of sending them all to a single 'Sites' channel.
const base::Feature kSiteNotificationChannels{
    "SiteNotificationChannels", base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // defined (OS_ANDROID)

// A new user experience for transitioning into fullscreen and mouse pointer
// lock states.
const base::Feature kSimplifiedFullscreenUI{"ViewsSimplifiedFullscreenUI",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables UI in MD Settings to view content settings grouped by
// origin.
const base::Feature kSiteDetails{"SiteDetails",
                                 base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables the ability to use the sound content setting to mute a
// website.
const base::Feature kSoundContentSetting{"SoundContentSetting",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

#if !defined(OS_ANDROID)
// Enables delaying the navigation of background tabs in order to improve
// foreground tab's user experience.
const base::Feature kStaggeredBackgroundTabOpen{
    "StaggeredBackgroundTabOpen", base::FEATURE_DISABLED_BY_DEFAULT};

// This controls whether we are running experiment with staggered background
// tab open. For control group, this should be disabled. This depends on
// |kStaggeredBackgroundTabOpen| above.
const base::Feature kStaggeredBackgroundTabOpenExperiment{
    "StaggeredBackgroundTabOpenExperiment", base::FEATURE_ENABLED_BY_DEFAULT};
#endif

// Enables or disables the creation of (legacy) supervised users. Does not
// affect existing supervised users.
const base::Feature kSupervisedUserCreation{"SupervisedUserCreation",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(SYZYASAN)
// Enable the deferred free mechanism in the syzyasan module, which helps the
// performance by deferring some work on the critical path to a background
// thread.
const base::Feature kSyzyasanDeferredFree{"SyzyasanDeferredFree",
                                          base::FEATURE_ENABLED_BY_DEFAULT};
#endif

// Enables using the local NTP if Google is the default search engine.
const base::Feature kUseGoogleLocalNtp{"UseGoogleLocalNtp",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// Experiment to use grouped permission infobars which could show and handle
// multiple permission requests.
const base::Feature kUseGroupedPermissionInfobars{
    "UseGroupedPermissionInfobars", base::FEATURE_ENABLED_BY_DEFAULT};

// Feature to use the PermissionManager to show prompts for WebRTC permission
// requests.
const base::Feature kUsePermissionManagerForMediaRequests{
    "UsePermissionManagerForMediaRequests", base::FEATURE_ENABLED_BY_DEFAULT};

#if !defined(OS_ANDROID)
// Enables or disables Voice Search on the local NTP.
const base::Feature kVoiceSearchOnLocalNtp{"VoiceSearchOnLocalNtp",
                                           base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if defined(OS_CHROMEOS)
// Enables or disables the opt-in IME menu in the language settings page.
const base::Feature kOptInImeMenu{"OptInImeMenu",
                                  base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables pin quick unlock.
const base::Feature kQuickUnlockPin{"QuickUnlockPin",
                                    base::FEATURE_ENABLED_BY_DEFAULT};

// Enables pin on the login screen.
const base::Feature kQuickUnlockPinSignin{"QuickUnlockPinSignin",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables fingerprint quick unlock.
const base::Feature kQuickUnlockFingerprint{"QuickUnlockFingerprint",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables emoji, handwriting and voice input on opt-in IME menu.
const base::Feature kEHVInputOnImeMenu{"EmojiHandwritingVoiceInput",
                                       base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables flash component updates on Chrome OS.
const base::Feature kCrosCompUpdates{"CrosCompUpdates",
                                     base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables Chrome OS Component updates on Chrome OS.
const base::Feature kCrOSComponent{"CrOSComponent",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables Instant Tethering on Chrome OS.
const base::Feature kInstantTethering{"InstantTethering",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables EasyUnlock promotions on Chrome OS.
const base::Feature kEasyUnlockPromotions{"EasyUnlockPromotions",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables TPM firmware update capability on Chrome OS.
const base::Feature kTPMFirmwareUpdate{"TPMFirmwareUpdate",
                                       base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // defined(OS_CHROMEOS)

}  // namespace features
