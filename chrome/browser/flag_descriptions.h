// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FLAG_DESCRIPTIONS_H_
#define CHROME_BROWSER_FLAG_DESCRIPTIONS_H_

// Includes needed for macros allowing conditional compilation of some strings.
#include "build/build_config.h"
#include "build/buildflag.h"
#include "media/media_features.h"

// This file declares strings used in chrome://flags. These messages are not
// translated, because instead of end-users they target Chromium developers and
// testers. See https://crbug.com/587272 and https://crbug.com/703134 for more
// details.

namespace flag_descriptions {

//  Material Design version of chrome://bookmarks

// Name for the flag to enable the material design bookmarks page.
extern const char kEnableMaterialDesignBookmarksName[];

// Description for the flag to enable the material design bookmarks page.
extern const char kEnableMaterialDesignBookmarksDescription[];

//  Material Design version of chrome://policy

// Name for the flag to enable the material design policy page.
extern const char kEnableMaterialDesignPolicyPageName[];

// Description for the flag to enable the material design policy page.
extern const char kEnableMaterialDesignPolicyPageDescription[];

//  Material Design version of chrome://settings

// Name for the flag to enable the material design settings page.
extern const char kEnableMaterialDesignSettingsName[];

// Description for the flag to enable the material design settings page.
extern const char kEnableMaterialDesignSettingsDescription[];

//  Material Design version of chrome://extensions

// Name for the flag to enable the material design extensions page.
extern const char kEnableMaterialDesignExtensionsName[];

// Description for the flag to enable the material design extensions page.
extern const char kEnableMaterialDesignExtensionsDescription[];

//  Material Design version of feedback form

// Name for the flag to enable the material design feedback UI.
extern const char kEnableMaterialDesignFeedbackName[];

// Description for the flag to enable the material design feedback UI.
extern const char kEnableMaterialDesignFeedbackDescription[];

//  Report URL to SafeSearch

// Name for the flag to enable reporting URLs to SafeSearch.
extern const char kSafeSearchUrlReportingName[];

// Description for the flag to enable reporting URLs to SafeSearch.
extern const char kSafeSearchUrlReportingDescription[];

//  Device scale factor change in content crbug.com/485650.

// Name for the flag to use Blink's zooming mechanism to implement device scale
// factor.
extern const char kEnableUseZoomForDsfName[];

// Description for the flag to use Blink's zooming mechanism to implement device
// scale factor.
extern const char kEnableUseZoomForDsfDescription[];

// Text to indicate that it'll use the platform settings to enable/disable
// use-zoom-for-dsf mode.
extern const char kEnableUseZoomForDsfChoiceDefault[];

// Text to indicate the use-zoom-for-dsf mode is enabled.
extern const char kEnableUseZoomForDsfChoiceEnabled[];

// Text to indicate the use-zoom-for-dsf mode is disabled.
extern const char kEnableUseZoomForDsfChoiceDisabled[];

// Name for the flag to set parameters for No-State Prefetch.
extern const char kNostatePrefetch[];

// Description for the flag to set parameters for No-State Prefetch.
extern const char kNostatePrefetchDescription[];

// Name for the flag to set parameters for Speculative Prefetch
extern const char kSpeculativePrefetchName[];

// Description for the flag to set parameters for Speculative Prefetch.
extern const char kSpeculativePrefetchDescription[];

//  Force Tablet Mode

// Name for the flag to force tablet mode.
extern const char kForceTabletModeName[];

// Description for the flag to force tablet mode.
extern const char kForceTabletModeDescription[];

// Description of the TouchView mode. In TouchView mode, the keyboard can flip
// behind the screen so the Chromebook operates like a tablet computer.
extern const char kForceTabletModeTouchview[];

// Description of the Clamshell mode. In Clamshell mode, the Chromebook operates
// like a standard laptop computer, keyboard on the bottom, screen on top, with
// a hinge like a clamshell.
extern const char kForceTabletModeClamshell[];

// Description of the default or 'automatic' mode.
extern const char kForceTabletModeAuto[];

//  Print Preview features

// Name for the flag to add the option to print PDFs as images to print preview.
extern const char kPrintPdfAsImageName[];

// Description for the flag to add the option to print PDFs as images in print
// preview.
extern const char kPrintPdfAsImageDescription[];

// Name of the 'Native Client' lab.
extern const char kNaclName[];

#if defined(OS_ANDROID)

// Mobile: Description of the 'Native Client' lab.
extern const char kNaclDescription[];

#endif  // defined(OS_ANDROID)

#if !defined(OS_ANDROID)

// Description of the 'Native Client' lab.
extern const char kNaclDescription[];

#endif  // !defined(OS_ANDROID)

// Name of the 'NaCl GDB debug stub' lab.
extern const char kNaclDebugName[];

// Description of the 'NaCl GDB debug stub' lab.
extern const char kNaclDebugDescription[];

#if !defined(OS_ANDROID)

// Name of the 'Force PNaCl Subzero' lab.
extern const char kPnaclSubzeroName[];

// Description of the 'Force PNaCl Subzero' lab.
extern const char kPnaclSubzeroDescription[];

#endif  // !defined(OS_ANDROID)

// Name of the 'Restrict NaCl GDB debugging' lab.
extern const char kNaclDebugMaskName[];

// Description of the 'Restrict NaCl GDB debugging' lab.
extern const char kNaclDebugMaskDescription[];

extern const char kNaclDebugMaskChoiceDebugAll[];

extern const char kNaclDebugMaskChoiceExcludeUtilsPnacl[];

extern const char kNaclDebugMaskChoiceIncludeDebug[];

// Name of the 'HTTP form warning' feature.
extern const char kEnableHttpFormWarningName[];

// Description of the 'HTTP form warning' feature.
extern const char kEnableHttpFormWarningDescription[];

// Name of the 'Mark Non-Secure Origins As' lab.
extern const char kMarkHttpAsName[];

// Description of the 'Mark Non-Secure Origins As' lab.
extern const char kMarkHttpAsDescription[];

extern const char kMarkHttpAsDangerous[];

//  Material Design of the Incognito NTP.

// Name for the flag to enable the material redesign of the Incognito NTP.
extern const char kMaterialDesignIncognitoNTPName[];

// Description for the flag to enable the material redesign
// of the Incognito NTP.
extern const char kMaterialDesignIncognitoNTPDescription[];

// Name of the 'Save Page as MHTML' lab.
extern const char kSavePageAsMhtmlName[];

// Description of the 'Save Page as MHTML' lab.
extern const char kSavePageAsMhtmlDescription[];

//  Flag and values for MHTML Geenrator options lab.

// Name of the 'MHTML Generator Options' lab.
extern const char kMhtmlGeneratorOptionName[];

// Description of the 'MHTML Generator Options' lab.
extern const char kMhtmlGeneratorOptionDescription[];

extern const char kMhtmlSkipNostoreMain[];

extern const char kMhtmlSkipNostoreAll[];

// Title of the 'device discovery notificatios' flag.
extern const char kDeviceDiscoveryNotificationsName[];

// Description of the 'device discovery notifications' flag.
extern const char kDeviceDiscoveryNotificationsDescription[];

#if defined(OS_WIN)

extern const char kCloudPrintXpsName[];

extern const char kCloudPrintXpsDescription[];

#endif  // defined(OS_WIN)

// Title of the flag for loading the Media Router component extension.
extern const char kLoadMediaRouterComponentExtensionName[];

// Description of the flag for loading the Media Router component extension.
extern const char kLoadMediaRouterComponentExtensionDescription[];

extern const char kPrintPreviewRegisterPromosName[];

extern const char kPrintPreviewRegisterPromosDescription[];

// Title of the flag which enables scroll prediction.
extern const char kScrollPredictionName[];

// Title of the flag which controls UI layout in the browser's top chrome.
extern const char kTopChromeMd[];

// Description of the flag which changes the material design elements in the top
// chrome of the browser.
extern const char kTopChromeMdDescription[];

// Top Chrome material design option (default).
extern const char kTopChromeMdMaterial[];

// Top Chrome material hybrid design option (for touchscreens).
extern const char kTopChromeMdMaterialHybrid[];

// Title of the flag which enables site details in MD settings.
extern const char kSiteDetails[];

// Description of the flag which enables or disables site details in MD
// settings.
extern const char kSiteDetailsDescription[];

// Title of the flag which enables the site settings all sites list and site
// details.
extern const char kSiteSettings[];

// Description of the flag which enables or disables the site settings all sites
// list and site details.
extern const char kSiteSettingsDescription[];

// Title of the flag which enables or disables material design in the rest of
// the native UI of the browser (beyond top chrome).
extern const char kSecondaryUiMd[];

// Description of the flag which enables or disables material design in the rest
// of the native UI of the browser (beyond top chrome).
extern const char kSecondaryUiMdDescription[];

// Description of the flag to enable scroll prediction.
extern const char kScrollPredictionDescription[];

// Title of the flag for add to shelf banners.
extern const char kAddToShelfName[];

// Description of the flag for add to shelf banners.
extern const char kAddToShelfDescription[];

// Title of the flag which bypasses the user engagement checks for app banners.
extern const char kBypassAppBannerEngagementChecksName[];

// Description of the flag to bypass the user engagement checks for app banners.
extern const char kBypassAppBannerEngagementChecksDescription[];

#if defined(OS_ANDROID)

// Name of the flag to enable the accessibility tab switcher.
extern const char kAccessibilityTabSwitcherName[];

// Description of the flag to enable the accessibility tab switcher.
extern const char kAccessibilityTabSwitcherDescription[];

// Name of the flag to enable autofill accessibility.
extern const char kAndroidAutofillAccessibilityName[];

// Description of the flag to enable autofill accessibility.
extern const char kAndroidAutofillAccessibilityDescription[];

// Name of the flag to enable the physical web feature.
extern const char kEnablePhysicalWebName[];

// Description of the flag to enable the physical web feature.
extern const char kEnablePhysicalWebDescription[];

#endif  // defined(OS_ANDROID)

// Title of the touch-events flag.
extern const char kTouchEventsName[];

// Description of the touch-events flag.
extern const char kTouchEventsDescription[];

// Title of the disable touch adjustment flag.
extern const char kTouchAdjustmentName[];

// Description of the disable touch adjustment flag.
extern const char kTouchAdjustmentDescription[];

// Name of the 'Composited layer borders' lab.
extern const char kCompositedLayerBorders[];

// Description of the 'Composited layer borders' lab.
extern const char kCompositedLayerBordersDescription[];

// Name of the 'Composited layer borders' lab.
extern const char kGlCompositedTextureQuadBorders[];

// Description of the 'Composited layer borders' lab.
extern const char kGlCompositedTextureQuadBordersDescription[];

// Name of the 'Overdraw feedback' lab.
extern const char kShowOverdrawFeedback[];

// Description of the 'Overdraw feedback' lab.
extern const char kShowOverdrawFeedbackDescription[];

// Name for the flag that sets partial swap behavior.
extern const char kUiPartialSwapName[];

// Description for the flag that sets partial swap behavior.
extern const char kUiPartialSwapDescription[];

// Name of the 'Debugging keyboard shortcuts' lab.
extern const char kDebugShortcutsName[];

// Name of the 'Ignore GPU blacklist' lab.
extern const char kIgnoreGpuBlacklistName[];

// Description of the 'Ignore GPU blacklist' lab.
extern const char kIgnoreGpuBlacklistDescription[];

// Title of the flag to enable the inert visual viewport experiment.
extern const char kInertVisualViewportName[];

// Description of the flag to enable the inert visual viewport experiment.
extern const char kInertVisualViewportDescription[];

// Name of the 'Color correct rendering' lab.
extern const char kColorCorrectRenderingName[];

// Description of the 'Color correct rendering' lab.
extern const char kColorCorrectRenderingDescription[];

// Name of the 'Enable experimental canvas features' lab.
extern const char kExperimentalCanvasFeaturesName[];

// Description of the 'Enable experimental canvas features' lab.
extern const char kExperimentalCanvasFeaturesDescription[];

// Name of the 'Accelerated 2D canvas' lab.
extern const char kAccelerated2dCanvasName[];

// Description of the 'Accelerated 2D canvas' lab.
extern const char kAccelerated2dCanvasDescription[];

// Name of the 'Enable display list 2D canvas' lab.
extern const char kDisplayList2dCanvasName[];

// Description of the 'Enable display list 2D canvas' lab.
extern const char kDisplayList2dCanvasDescription[];

// Option that allows canvas 2D contexts to switch from one rendering mode to
// another on the fly.
extern const char kEnable2dCanvasDynamicRenderingModeSwitchingName[];

// Description of 'Enable canvas 2D dynamic pipeline switching mode'
extern const char kEnable2dCanvasDynamicRenderingModeSwitchingDescription[];

// Name of the 'Experimental Extension APIs' lab.
extern const char kExperimentalExtensionApisName[];

// Description of the 'Experimental Extension APIs' lab.
extern const char kExperimentalExtensionApisDescription[];

// Name of the 'Extensions on chrome:// URLs' lab
extern const char kExtensionsOnChromeUrlsName[];

// Description of the 'Extensions on chrome:// URLs' lab
extern const char kExtensionsOnChromeUrlsDescription[];

// Name of the 'Fast Unload' lab
extern const char kFastUnloadName[];

// Description of the 'Fast Unload' lab.
extern const char kFastUnloadDescription[];

// Name of the 'User consent for extension scripts' lab.
extern const char kUserConsentForExtensionScriptsName[];

// Description of the 'User consent for extension scripts' lab
extern const char kUserConsentForExtensionScriptsDescription[];

// Name of the flag that requires a user gesture before script can add a histroy
// entry to the back/forward list.
extern const char kHistoryRequiresUserGestureName[];

// Description of the flag that requires that a page process a user action
// (e.g., a mouse click) before script can add an entry to the tab's
// back/forward list.
extern const char kHistoryRequiresUserGestureDescription[];

// Name of the 'Disable hyperlink auditing' lab.
extern const char kHyperlinkAuditingName[];

// Description of the 'Disable hyperlink auditing' lab.
extern const char kHyperlinkAuditingDescription[];

#if defined(OS_ANDROID)

// Title for the flag to enable Contextual Search.
extern const char kContextualSearch[];

// Description for the flag to enable Contextual Search.
extern const char kContextualSearchDescription[];

// Title for the flag to enable Contextual Search integration with Contextual
// Cards data in the bar.
extern const char kContextualSearchContextualCardsBarIntegration[];

// Description for the flag to enable Contextual Search integration with
// Contextual Cards data in the bar.
extern const char kContextualSearchContextualCardsBarIntegrationDescription[];

// Title for the flag to enable Contextual Search single actions using
// Contextual Cards data in the bar.
extern const char kContextualSearchSingleActions[];

// Description for the flag to enable Contextual Search single actions using
// Contextual Cards data in the bar.
extern const char kContextualSearchSingleActionsDescription[];

// Title for the flag to enable Contextual Search url actions using Contextual
// Cards data in the bar.
extern const char kContextualSearchUrlActions[];

// Description for the flag to enable Contextual Search URL actions using
// Contextual Cards data in the bar.
extern const char kContextualSearchUrlActionsDescription[];

#endif  // defined(OS_ANDROID)

// Name of the smooth scrolling flag
extern const char kSmoothScrollingName[];

// Description of the smooth scrolling flag
extern const char kSmoothScrollingDescription[];

// Title for the flag to turn on overlay scrollbars
extern const char kOverlayScrollbarsName[];

// Description for the flag to turn on overlay scrollbars
extern const char kOverlayScrollbarsDescription[];

// Title for the flag to show Autofill field type predictions for all forms
extern const char kShowAutofillTypePredictionsName[];

// Description for the flag to show Autofill field type predictions for all
// forms
extern const char kShowAutofillTypePredictionsDescription[];

// Name of the flag that enables TCP Fast Open.
extern const char kTcpFastOpenName[];

// Description of the flag that enables TCP Fast Open.
extern const char kTcpFastOpenDescription[];

// Name of the flag that enables touch initiated drag drop.
extern const char kTouchDragDropName[];

// Description of the flag to enable touch initiated drag drop.
extern const char kTouchDragDropDescription[];

// Name of the flag that controls touch based text selection strategy.
extern const char kTouchSelectionStrategyName[];

// Description of the flag that controls touch based text selection strategy.
extern const char kTouchSelectionStrategyDescription[];

// Description for the touch text selection strategy which always uses character
// granularity.
extern const char kTouchSelectionStrategyCharacter[];

// Description for the touch text selection strategy which is based on the
// direction in which the handle is dragged.
extern const char kTouchSelectionStrategyDirection[];

// Name of the flag that requires a user gesture for vibrate.
extern const char kVibrateRequiresUserGestureName[];

// Description of the flag that requires a user gesture for vibrate.
extern const char kVibrateRequiresUserGestureDescription[];

// Title for the flag to use the Online Wallet sandbox servers (instead of
// production).
extern const char kWalletServiceUseSandboxName[];

// Description of the flag to use the Online Wallet sandbox servers.
extern const char kWalletServiceUseSandboxDescription[];

// Title for the flag for history navigation from horizontal overscroll.
extern const char kOverscrollHistoryNavigationName[];

// Description for the flag to disable history navigation from horizontal
// overscroll.
extern const char kOverscrollHistoryNavigationDescription[];

// Description for the simple UI for history navigation from horizontal
// overscroll.
extern const char kOverscrollHistoryNavigationSimpleUi[];

// Title for the flag to specify the threshold for starting horizontal
// overscroll.
extern const char kOverscrollStartThresholdName[];

// Description for the flag to specify the threshold for starting horizontal
// overscroll.
extern const char kOverscrollStartThresholdDescription[];

// Description for the 133% threshold for starting horizontal overscroll.
extern const char kOverscrollStartThreshold133Percent[];

// Description for the 166% threshold for starting horizontal overscroll.
extern const char kOverscrollStartThreshold166Percent[];

// Description for the 200% threshold for starting horizontal overscroll.
extern const char kOverscrollStartThreshold200Percent[];

// Title for the flag for scroll end effect from vertical overscroll.
extern const char kScrollEndEffectName[];

// Description for the flag that controls scroll end effect from vertical
// overscroll.
extern const char kScrollEndEffectDescription[];

// Name of the 'Enable WebGL 2.0' flag.
extern const char kWebgl2Name[];

// Description for the flag to enable WebGL 2.0.
extern const char kWebgl2Description[];

// Name of the 'Enable WebGL Draft Extensions' flag.
extern const char kWebglDraftExtensionsName[];

// Description for the flag to enable WebGL Draft Extensions.
extern const char kWebglDraftExtensionsDescription[];

// Name of chrome:flags option to turn off WebRTC hardware video decoding
// support.
extern const char kWebrtcHwDecodingName[];

// Description of chrome:flags option to turn off WebRTC hardware video decoding
// support.
extern const char kWebrtcHwDecodingDescription[];

// Name of chrome:flags option to turn off WebRTC hardware video encoding
// support.
extern const char kWebrtcHwEncodingName[];

// Description of chrome:flags option to turn off WebRTC hardware video encoding
// support.
extern const char kWebrtcHwEncodingDescription[];

// Name of chrome:flags option to turn on WebRTC h264 hardware video encoding
// support.
extern const char kWebrtcHwH264EncodingName[];

// Description of chrome:flags option to turn on WebRTC hardware h264 video
// encoding support.
extern const char kWebrtcHwH264EncodingDescription[];

// Name of chrome:flags option to turn on WebRTC vp8 hardware video encoding
// support.
extern const char kWebrtcHwVP8EncodingName[];

// Description of chrome:flags option to turn on WebRTC hardware vp8 video
// encoding support.
extern const char kWebrtcHwVP8EncodingDescription[];

// Name of chrome:flags option to enable GCM cipher suites for WebRTC
extern const char kWebrtcSrtpAesGcmName[];

// Description of chrome:flags option to enable GCM cipher suites for WebRTC
extern const char kWebrtcSrtpAesGcmDescription[];

// Name of chrome:flags option to turn on Origin header for WebRTC STUN messages
extern const char kWebrtcStunOriginName[];

// Description of chrome:flags option to turn on Origin header for WebRTC STUN
// messages
extern const char kWebrtcStunOriginDescription[];

// Name of chrome:flags option to enable WebRTC Echo Canceller 3.
extern const char kWebrtcEchoCanceller3Name[];

// Description of chrome:flags option to enable WebRTC Echo Canceller 3.
extern const char kWebrtcEchoCanceller3Description[];

#if defined(OS_ANDROID)

// Name of chrome:flags option to enable screen capture.
extern const char kMediaScreenCaptureName[];

// Description of chrome:flags option to enable screen capture.
extern const char kMediaScreenCaptureDescription[];

#endif  // defined(OS_ANDROID)

#if BUILDFLAG(ENABLE_WEBRTC)

// Name of chrome:flags option to enable WebRTC H.264 sw encoder/decoder
extern const char kWebrtcH264WithOpenh264FfmpegName[];

// Description of chrome:flags option to enable WebRTC H.264 sw encoder/decoder
extern const char kWebrtcH264WithOpenh264FfmpegDescription[];

#endif  // BUILDFLAG(ENABLE_WEBRTC)

// Name of the 'Enable WebVR' flag.
extern const char kWebvrName[];

// Description for the flag to enable WebVR APIs.
extern const char kWebvrDescription[];

// Name of the 'Enable WebVR experimental optimizations' flag.
extern const char kWebvrExperimentalRenderingName[];

// Description for the flag to enable experimental WebVR rendering
// optimizations.
extern const char kWebvrExperimentalRenderingDescription[];

// Name of the flag which allows users to enable experimental extensions to the
// Gamepad API.
extern const char kGamepadExtensionsName[];

// Description for the flag which allows users to enable experimental extensions
// to the Gamepad API.
extern const char kGamepadExtensionsDescription[];

#if defined(OS_ANDROID)

// Name of about:flags option to turn on the new Photo picker.
extern const char kNewPhotoPickerName[];

// Description of about:flags option to turn on the new Photo picker.
extern const char kNewPhotoPickerDescription[];

#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)

// Name of about:flags option to turn on the overscrolling for the OSK.
extern const char kEnableOskOverscrollName[];

// Description of about:flags option to turn on overscrolling for the OSK.
extern const char kEnableOskOverscrollDescription[];

#endif  // defined(OS_ANDROID)

// Title for the flag to enable QUIC.
extern const char kQuicName[];

// Description for the flag to enable QUIC.
extern const char kQuicDescription[];

// Title for the flag to switch the maximum TLS version.
extern const char kSslVersionMaxName[];

// Description for the flag to switch the maximum TLS version.
extern const char kSslVersionMaxDescription[];

extern const char kSslVersionMaxTls12[];

extern const char kSslVersionMaxTls13[];

// Title for the flag to enable Token Binding. Please do not translate 'Token
// Binding'.
extern const char kEnableTokenBindingName[];

// Description for the flag to enable Token Binding. Please do not translate
// 'Token Binding'.
extern const char kEnableTokenBindingDescription[];

// Description for the flag to adjust the default behaviour for document level
// passive touch listeners.
extern const char kPassiveDocumentEventListenersDescription[];

// Name for the flag to adjust the default behaviour for document level passive
// listeners.
extern const char kPassiveDocumentEventListenersName[];

// Description for the flag to adjust the default behaviour for passive touch
// listeners during fling.
extern const char kPassiveEventListenersDueToFlingDescription[];

// Name for the flag to adjust the default behaviour for passive touch listeners
// during fling.
extern const char kPassiveEventListenersDueToFlingName[];

// Choice for passive listeners to default to true on all nodes not explicitly
// set.
extern const char kPassiveEventListenerTrue[];

// Choice for passive listeners to default to true on all nodes.
extern const char kPassiveEventListenerForceAllTrue[];

// Name for the flag to adjust the default behaviour for passive event
// listeners.
extern const char kPassiveEventListenerDefaultName[];

// Description for the flag to adjust default behaviour for passive event
// listeners.
extern const char kPassiveEventListenerDefaultDescription[];

#if defined(OS_ANDROID)

// Title for the flag for including important sites whitelisting in the clear
// browsing dialog.
extern const char kImportantSitesInCbdName[];

// Description for the flag for using important sites whitelisting in the clear
// browsing dialog.
extern const char kImportantSitesInCbdDescription[];

#endif  // defined(OS_ANDROID)

#if defined(USE_ASH)

// Title of the flag which specifies the shelf coloring in Chrome OS system UI.
extern const char kAshShelfColor[];

// Description of the flag which specifies the shelf coloring in Chrome OS
// system UI.
extern const char kAshShelfColorDescription[];

// Title of the flag which specifies the shelf coloring scheme in Chrome OS
// system UI.
extern const char kAshShelfColorScheme[];

// Description of the flag which specifies the shelf coloring scheme in Chrome
// OS system UI.
extern const char kAshShelfColorSchemeDescription[];

// A shelf coloring scheme to be used by Chrome OS UI.
extern const char kAshShelfColorSchemeLightVibrant[];

// A shelf coloring scheme to be used by Chrome OS UI.
extern const char kAshShelfColorSchemeNormalVibrant[];

// A shelf coloring scheme to be used by Chrome OS UI.
extern const char kAshShelfColorSchemeDarkVibrant[];

// A shelf coloring scheme to be used by Chrome OS UI.
extern const char kAshShelfColorSchemeLightMuted[];

// A shelf coloring scheme to be used by Chrome OS UI.
extern const char kAshShelfColorSchemeNormalMuted[];

// A shelf coloring scheme to be used by Chrome OS UI.
extern const char kAshShelfColorSchemeDarkMuted[];

// Title for the flag which can be used for window backdrops in TouchView.
extern const char kAshMaximizeModeWindowBackdropName[];

// Description for the flag which can be used for window backdrops in TouchView.
extern const char kAshMaximizeModeWindowBackdropDescription[];

// Title for the flag which can be used for support for javascript locking
// rotation.
extern const char kAshScreenOrientationLockName[];

// Description for the flag which can be used for support for javascript locking
// rotation
extern const char kAshScreenOrientationLockDescription[];

// Title for the flag to enable the mirrored screen mode.
extern const char kAshEnableMirroredScreenName[];

// Description for the flag to enable the mirrored screen mode.
extern const char kAshEnableMirroredScreenDescription[];

// Title for the flag to disable smooth rotation animations.
extern const char kAshDisableSmoothScreenRotationName[];

// Description for the flag to disable smooth rotation animations.
extern const char kAshDisableSmoothScreenRotationDescription[];

// Description for the flag that sets material design ink drop animation speed
// of fast.
extern const char kMaterialDesignInkDropAnimationFast[];

// Description for the flag that sets material design ink drop anation speeds of
// slow.
extern const char kMaterialDesignInkDropAnimationSlow[];

// Name for the flag that sets the material design ink drop animation speed.
extern const char kMaterialDesignInkDropAnimationSpeedName[];

// Description for the flag that sets the material design ink drop animtion
// speed.
extern const char kMaterialDesignInkDropAnimationSpeedDescription[];

// Name for the flag that enables slow UI animations.
extern const char kUiSlowAnimationsName[];

// Description for the flag that enables slow UI animations.
extern const char kUiSlowAnimationsDescription[];

// Name for the flag to show UI composited layer borders.
extern const char kUiShowCompositedLayerBordersName[];

// Description for the flag to show UI composited layer borders.
extern const char kUiShowCompositedLayerBordersDescription[];

// Description of the flag option to show renderpass borders.
extern const char kUiShowCompositedLayerBordersRenderPass[];

// Description of the flag option to show surface borders.
extern const char kUiShowCompositedLayerBordersSurface[];

// Description of the flag option to show layer borders.
extern const char kUiShowCompositedLayerBordersLayer[];

// Description of the flag option to show all borders.
extern const char kUiShowCompositedLayerBordersAll[];

#endif  // defined(USE_ASH)

// Title for the flag for latest (non-experimental) JavaScript fatures
extern const char kJavascriptHarmonyShippingName[];

// Description for the flag for latest (non-experimental) JavaScript fatures
extern const char kJavascriptHarmonyShippingDescription[];

// Title for the flag to enable JavaScript Harmony features.
extern const char kJavascriptHarmonyName[];

// Description for the flag to enable JavaScript Harmony features.
extern const char kJavascriptHarmonyDescription[];

// Title for the flag to enable future features in V8.
extern const char kV8FutureName[];

// Description for the flag to enable future features in V8.
extern const char kV8FutureDescription[];

// Title for disabling the Ignition and TurboFan compilation pipeline in V8.
extern const char kV8DisableIgnitionTurboName[];

// Description for disabling the Ignition and TurboFan compilation pipeline in
// V8.
extern const char kV8DisableIgnitionTurboDescription[];

// Title for the flag to enable Asm.js to WebAssembly.
extern const char kEnableAsmWasmName[];

// Description for the flag to enable Asm.js to WebAssembly.
extern const char kEnableAsmWasmDescription[];

// Title for the flag to enable SharedArrayBuffers in JavaScript.
extern const char kEnableSharedArrayBufferName[];

// Description for the flag to enable SharedArrayBuffers in JavaScript.
extern const char kEnableSharedArrayBufferDescription[];

// Title for the flag to enable WebAssembly structured cloning.
extern const char kEnableWasmName[];

// Description for the flag to enable WebAssembly.
extern const char kEnableWasmDescription[];

// Title for the flag to enable WebAssembly streaming compilation/instantiation.
extern const char kEnableWasmStreamingName[];

// Description for the flag to enable WebAssembly streaming
// compilation/instantiation.
extern const char kEnableWasmStreamingDescription[];

#if defined(OS_ANDROID)

// Title for the flag to enable the download button on MediaDocument.
extern const char kMediaDocumentDownloadButtonName[];

// Description for the flag to enable download button on MediaDocument.
extern const char kMediaDocumentDownloadButtonDescription[];

#endif  // defined(OS_ANDROID)

// Title for the flag for using a software rasterizer.
extern const char kSoftwareRasterizerName[];

// Description for the flag for using a software renderer.
extern const char kSoftwareRasterizerDescription[];

// Title for the flag to enable GPU rasterization.
extern const char kGpuRasterizationName[];

// Description for the flag to enable GPU rasterizer.
extern const char kGpuRasterizationDescription[];

// Description of the 'Force GPU rasterization' experiment
extern const char kForceGpuRasterization[];

// Name of about:flags option for number of GPU rasterization MSAA samples.
extern const char kGpuRasterizationMsaaSampleCountName[];

// Description of about:flags option for number of GPU rasterization MSAA
// samples.
extern const char kGpuRasterizationMsaaSampleCountDescription[];

extern const char kGpuRasterizationMsaaSampleCountZero[];

extern const char kGpuRasterizationMsaaSampleCountTwo[];

extern const char kGpuRasterizationMsaaSampleCountFour[];

extern const char kGpuRasterizationMsaaSampleCountEight[];

extern const char kGpuRasterizationMsaaSampleCountSixteen[];

// Title for about:flags option for slimming paint invalidation.
extern const char kSlimmingPaintInvalidationName[];

// Description of about:flags option for slimming paint invalidation.
extern const char kSlimmingPaintInvalidationDescription[];

// Name for the flag to enable experimental security features.
extern const char kExperimentalSecurityFeaturesName[];

// Description for the flag to enable experimental security features.
extern const char kExperimentalSecurityFeaturesDescription[];

// Name for the flag to enable experimental Web Platform features.
extern const char kExperimentalWebPlatformFeaturesName[];

// Description for the flag to enable experimental Web Platform features.
extern const char kExperimentalWebPlatformFeaturesDescription[];

// Name for the flag to enable pointer events.
extern const char kExperimentalPointerEventName[];

// Description for the flag to enable pointer events.
extern const char kExperimentalPointerEventDescription[];

// Name for the flag to enable origin trials.
extern const char kOriginTrialsName[];

// Description for the flag to enable origin trials.
extern const char kOriginTrialsDescription[];

// Name for the flag for BLE Advertising
extern const char kBleAdvertisingInExtensionsName[];

// Description of the flag to enable BLE Advertising
extern const char kBleAdvertisingInExtensionsDescription[];

// Name for the flag to enable experiments in Developer Tools
extern const char kDevtoolsExperimentsName[];

// Description for the flag to enable experiments in Developer Tools.
extern const char kDevtoolsExperimentsDescription[];

// Name for the flag to enable silent debugging via chrome.debugger extension
// API.
extern const char kSilentDebuggerExtensionApiName[];

// Description for the flag to enable silent debugging via chrome.debugger
// extension API.
extern const char kSilentDebuggerExtensionApiDescription[];

// Name for the flag to show a heads-up display for tracking touch-points.
extern const char kShowTouchHudName[];

// Description for the flag to show a heads-up display for tracking
// touch-points.
extern const char kShowTouchHudDescription[];

// Name for the Prefer HTML over Plugins feature.
extern const char kPreferHtmlOverPluginsName[];

// Description for the Prefer HTML over Plugins feature.
extern const char kPreferHtmlOverPluginsDescription[];

// Name for the NaCl Socket API feature.
extern const char kAllowNaclSocketApiName[];

// Description for the NaCl Socket API feature.
extern const char kAllowNaclSocketApiDescription[];

// Name for the Run all Flash in Allow mode feature.
extern const char kRunAllFlashInAllowModeName[];

// Description for the Run all Flash in Allow mode feature.
extern const char kRunAllFlashInAllowModeDescription[];

// Name of the flag to turn on experiental pinch to scale.
extern const char kPinchScaleName[];

// Description of the flag to turn on experiental pinch to scale.
extern const char kPinchScaleDescription[];

// Name for the flag to reduce referer granularity.
extern const char kReducedReferrerGranularityName[];

// Description for the flag to reduce referer granularity.
extern const char kReducedReferrerGranularityDescription[];

#if defined(OS_CHROMEOS)

// Name for the flag to enable the new UI service.
extern const char kUseMashName[];

// Description for the flag to enable the new UI service.
extern const char kUseMashDescription[];

// Name for the flag to enable touchpad three finger click as middle button.
extern const char kAllowTouchpadThreeFingerClickName[];

// Description for the flag to enable touchpad three finger click as middle
// button.
extern const char kAllowTouchpadThreeFingerClickDescription[];

// Name for the flag to enable unified desktop mode.
extern const char kAshEnableUnifiedDesktopName[];

// Description for the flag to enable unified desktop mode.
extern const char kAshEnableUnifiedDesktopDescription[];

// Name for the flag for wallpaper boot animation (except for OOBE).
extern const char kBootAnimation[];

// Description for the flag for wallpaper boot animation (except for OOBE).
extern const char kBootAnimationDescription[];

// Name for the flag for enabling Instant Tethering.
extern const char kTetherName[];

// Description for the flag for enabling Instant Tethering.
extern const char kTetherDescription[];

// Name for the flag for CrOS Component.
extern const char kCrOSComponentName[];

// Description for the flag for CrOS Component.
extern const char kCrOSComponentDescription[];

#endif  // defined(OS_CHROMEOS)

// Name of the flag for accelerated video decode where available.
extern const char kAcceleratedVideoDecodeName[];

// Description for the flag for accelerated video decode where available.
extern const char kAcceleratedVideoDecodeDescription[];

// Name/Description for the "enable-hdr" flag.
extern const char kEnableHDRName[];
extern const char kEnableHDRDescription[];

// Name for the flag for cloud import feature.
extern const char kCloudImport[];

// Description for the flag for cloud import.
extern const char kCloudImportDescription[];

// Name for the flag to set to enable Request Tablet Site in the wrench menu.
extern const char kRequestTabletSiteName[];

// Description for the flag to set to enable Request Tablet Site in the wrench
// menu.
extern const char kRequestTabletSiteDescription[];

// Name of the flag to enable debugging context menu options for packed apps.
extern const char kDebugPackedAppName[];

// Description of the flag to enable debugging context menu options for packed
// apps.
extern const char kDebugPackedAppDescription[];

// Name of the flag to enable drop sync credential
extern const char kDropSyncCredentialName[];

// Description of the flag to enable drop sync credential
extern const char kDropSyncCredentialDescription[];

// Name of the flag to enable password generation.
extern const char kPasswordGenerationName[];

// Description of flag to enable password generation.
extern const char kPasswordGenerationDescription[];

// Name of the flag for the password manager's force-saving option.
extern const char kPasswordForceSavingName[];

// Description of the flag for the password manager's force-saving option.
extern const char kPasswordForceSavingDescription[];

// Name of the flag for manual password generation.
extern const char kManualPasswordGenerationName[];

// Description of the flag for manual password generation.
extern const char kManualPasswordGenerationDescription[];

// Name of the flag to show autofill signatures.
extern const char kShowAutofillSignatures[];

// Description of the flag to show autofill signatures.
extern const char kShowAutofillSignaturesDescription[];

// Name of the flag for substring matching for Autofill suggestions.
extern const char kSuggestionsWithSubStringMatchName[];

// Description of the flag for substring matching for Autofill suggestions.
extern const char kSuggestionsWithSubStringMatchDescription[];

// Name of the flag to enable affiliation based matching, so that credentials
// stored for an Android application will also be considered matches for, and be
// filled into corresponding websites (so-called 'affiliated' websites).
extern const char kAffiliationBasedMatchingName[];

// Description of the flag to enable affiliation based matching, so that
// credentials stored for an Android application will also be considered matches
// for, and be filled into corresponding websites (so-called 'affiliated'
// websites).
extern const char kAffiliationBasedMatchingDescription[];

// Name of the flag specifying how the browser will handle autofilling the users
// sync credential.
extern const char kProtectSyncCredentialName[];

// Description of the flag specifying how the browser will handle autofilling
// the users sync credential.
extern const char kProtectSyncCredentialDescription[];

// Name of the flag for showing the Import and Export buttons for importing and
// exporting passwords in Chrome's settings.
extern const char kPasswordImportExportName[];

// Description of the flag for showing the Import and Export buttons for
// importing and exporting passwords in Chrome's settings.
extern const char kPasswordImportExportDescription[];

// Name of the flag specifying how the browser will handle autofilling the users
// sync credential.
extern const char kProtectSyncCredentialOnReauthName[];

// Description of the flag specifying how the browser will handle autofilling of
// the sync credential only for transactional reauth pages.
extern const char kProtectSyncCredentialOnReauthDescription[];

// Name of the flag to enable large icons on the NTP.
extern const char kIconNtpName[];

// Description for the flag to enable large icons on the NTP.
extern const char kIconNtpDescription[];

// Name of the flag to enable background mode for the Push API.
extern const char kPushApiBackgroundModeName[];

// Description for the flag to enable background mode for the Push API.
extern const char kPushApiBackgroundModeDescription[];

// Name of the flag to enable navigation tracing
extern const char kEnableNavigationTracing[];

// Description of the flag to enable navigation tracing
extern const char kEnableNavigationTracingDescription[];

// Name of the flag to set the trace upload url
extern const char kTraceUploadUrl[];

// Description of the flag to set the trace upload url
extern const char kTraceUploadUrlDescription[];

// Title for the flag to disable Audio Sharing.
extern const char kDisableAudioForDesktopShare[];

// Description for the flag to disable Audio Sharing.
extern const char kDisableAudioForDesktopShareDescription[];

// Title for the flag to disable tab for desktop share.
extern const char kDisableTabForDesktopShare[];

// Description for the flag to disable tab for desktop share.
extern const char kDisableTabForDesktopShareDescription[];

extern const char kTraceUploadUrlChoiceOther[];

extern const char kTraceUploadUrlChoiceEmloading[];

extern const char kTraceUploadUrlChoiceQa[];

extern const char kTraceUploadUrlChoiceTesting[];

// Name of the flag to enable the managed bookmarks folder for supervised users.
extern const char kSupervisedUserManagedBookmarksFolderName[];

// Description for the flag to enable the managed bookmarks folder for
// supervised users.
extern const char kSupervisedUserManagedBookmarksFolderDescription[];

// Name of the flag to enable syncing the app list.
extern const char kSyncAppListName[];

// Description for the flag to enable syncing the app list.
extern const char kSyncAppListDescription[];

// Name of the flag for drive search in chrome launcher.
extern const char kDriveSearchInChromeLauncher[];

// Description for the flag for drive search in chrome launcher.
extern const char kDriveSearchInChromeLauncherDescription[];

// Name of the about::flags setting for V8 cache options. The V8 JavaScript
// engine supports three caching modes: disabled; parser; code. This is the
// option to choose among them.
extern const char kV8CacheOptionsName[];

// Description of the about::flags setting for V8 cache options. The V8
// JavaScript engine supports three caching modes: disabled; parser; code.
extern const char kV8CacheOptionsDescription[];

// The V8 JavaScript engine supports three 'caching' modes: disabled; caching
// data generated by the JavaScript parser; or caching data generated by the
// JavaScript compiler. This option describes the 2nd of these, caching data
// generated by the parser.
extern const char kV8CacheOptionsParse[];

// The V8 JavaScript engine supports three 'caching' modes: disabled; caching
// data generated by the JavaScript parser; or caching data generated by the
// JavaScript compiler. This option describes the 3rd of these, caching data
// generated by the compiler.
extern const char kV8CacheOptionsCode[];

// Name of the about::flags setting for V8 cache strategies for CacheStorage.
// The V8 cache for CacheStorage supports three strategies: disabled; normal;
// aggressive. This is the option to choose among them.
extern const char kV8CacheStrategiesForCacheStorageName[];

// Description of the about::flags setting for V8 cache strategies for
// CacheStorage. The V8 cache for CacheStorage supports three strategies:
// disabled; normal; aggressive.
extern const char kV8CacheStrategiesForCacheStorageDescription[];

// The V8 cache for CacheStorage supports three strategies: disabled; caching
// same as if the script is in HTTPCache; force caching on the first load. This
// option describes the 2nd of these, caching same as if the script is in
// HTTPCache.
extern const char kV8CacheStrategiesForCacheStorageNormal[];

// The V8 cache for CacheStorage supports three strategies: disabled; caching
// same as if the script is in HTTPCache; force caching on the first load. This
// option describes the 3rd of these, force caching on the first load.
extern const char kV8CacheStrategiesForCacheStorageAggressive[];

// An about::flags experiment title to enable/disable memory coordinator
extern const char kMemoryCoordinatorName[];

// Description of an about::flags to enable/disable memory coordinator
extern const char kMemoryCoordinatorDescription[];

// Name of the about::flags setting for service worker navigation preload.
extern const char kServiceWorkerNavigationPreloadName[];

// Description of the about::flags setting for service worker navigation
// preload.
extern const char kServiceWorkerNavigationPreloadDescription[];

//  Data Reduction Proxy

// An about:flags experiment title to enable/disable Data Saver Lo-Fi
extern const char kDataReductionProxyLoFiName[];

// Describes an about:flags experiment to enable/disable Data Saver Lo-Fi
extern const char kDataReductionProxyLoFiDescription[];

// Option for IDS_FLAGS_DATA_REDUCTION_PROXY_LO_FI_NAME to enable Lo-Fi always
// on
extern const char kDataReductionProxyLoFiAlwaysOn[];

// Option for IDS_FLAGS_DATA_REDUCTION_PROXY_LO_FI_NAME to enable Lo-Fi only on
// celluar connections
extern const char kDataReductionProxyLoFiCellularOnly[];

// Option for IDS_FLAGS_DATA_REDUCTION_PROXY_LO_FI_NAME to disable Lo-Fi
extern const char kDataReductionProxyLoFiDisabled[];

// Option for IDS_FLAGS_DATA_REDUCTION_PROXY_LO_FI_NAME to enable Lo-Fi only on
// slow connections
extern const char kDataReductionProxyLoFiSlowConnectionsOnly[];

// An about:flags experiment title to enable Data Saver lite pages
extern const char kEnableDataReductionProxyLitePageName[];

// Describes an about:flags experiment to enable Data Saver lite pages
extern const char kEnableDataReductionProxyLitePageDescription[];

// An about:flags experiment title to enable using the carrier test data
// reduction proxy
extern const char kDataReductionProxyCarrierTestName[];

// Describes an about:flags experiment to enable using the carrier test data
// reduction proxy
extern const char kDataReductionProxyCarrierTestDescription[];

// An about:flags experiment title to enable a Data Saver snackbar promo for 1
// MB of savings
extern const char kEnableDataReductionProxySavingsPromoName[];

// Describes an about:flags experiment to enable a Data Saver snackbar promo for
// 1 MB of savings
extern const char kEnableDataReductionProxySavingsPromoDescription[];

#if defined(OS_ANDROID)

// An about:flags experiment title to enable the Data Saver footer on Android
extern const char kEnableDataReductionProxyMainMenuName[];

// Describes an about:flags experiment to enable the Data Saver footer in the
// main menu on Android
extern const char kEnableDataReductionProxyMainMenuDescription[];

// An about:flags experiment title to enable the site breakdown on the Data
// Saver settings page.
extern const char kEnableDataReductionProxySiteBreakdownName[];

// Describes an about:flags experiment to enable the site breakdown on the Data
// Saver settings page.
extern const char kEnableDataReductionProxySiteBreakdownDescription[];

// An about:flags experiment title to enable offline page previews.
extern const char kEnableOfflinePreviewsName[];

// Describes an about:flags experiment to enable offline page previews.
extern const char kEnableOfflinePreviewsDescription[];

#endif  // defined(OS_ANDROID)

// Name of about:flags option for LCD text.
extern const char kLcdTextName[];

// Description of about:flags option for LCD text.
extern const char kLcdTextDescription[];

// Name of about:flags option for distance field text.
extern const char kDistanceFieldTextName[];

// Description of about:flags option for distance field text.
extern const char kDistanceFieldTextDescription[];

// Name of about:flags option for zero-copy rasterizer.
extern const char kZeroCopyName[];

// Description of about:flags option for zero-copy rasterizer.
extern const char kZeroCopyDescription[];

extern const char kHideInactiveStackedTabCloseButtonsName[];

extern const char kHideInactiveStackedTabCloseButtonsDescription[];

// Name of about:flags option for default tile width.
extern const char kDefaultTileWidthName[];

// Description of about:flags option for default tile width.
extern const char kDefaultTileWidthDescription[];

extern const char kDefaultTileWidthShort[];

extern const char kDefaultTileWidthTall[];

extern const char kDefaultTileWidthGrande[];

extern const char kDefaultTileWidthVenti[];

// Name of about:flags option for default tile height.
extern const char kDefaultTileHeightName[];

// Description of about:flags option for default tile height.
extern const char kDefaultTileHeightDescription[];

extern const char kDefaultTileHeightShort[];

extern const char kDefaultTileHeightTall[];

extern const char kDefaultTileHeightGrande[];

extern const char kDefaultTileHeightVenti[];

// Name of about:flags option for number of raster threads.
extern const char kNumRasterThreadsName[];

// Description of about:flags option for number of raster threads.
extern const char kNumRasterThreadsDescription[];

extern const char kNumRasterThreadsOne[];

extern const char kNumRasterThreadsTwo[];

extern const char kNumRasterThreadsThree[];

extern const char kNumRasterThreadsFour[];

// Name of the flag to reset the app launcher install state.
extern const char kResetAppListInstallStateName[];

// Description of the flag to reset the app launcher install state.
extern const char kResetAppListInstallStateDescription[];

#if defined(OS_CHROMEOS)

// Name of the flag to enable animated transitions for the first-run tutorial.
extern const char kFirstRunUiTransitionsName[];

// Description for the flag to enable animated transition in the first-run
// tutorial.
extern const char kFirstRunUiTransitionsDescription[];

#endif  // defined(OS_CHROMEOS)

// Name of the flag to enable the new bookmark app system.
extern const char kNewBookmarkAppsName[];

// Description for the flag to enable the new bookmark app system.
extern const char kNewBookmarkAppsDescription[];

#if defined(OS_MACOSX)

// Name of the flag to allow hosted apps opening in windows.
extern const char kHostedAppsInWindowsName[];

// Description for the flag to allow hosted apps opening in windows
extern const char kHostedAppsInWindowsDescription[];

// Name of the flag to allow tabs detaching in fullscreen on Mac.
extern const char kTabDetachingInFullscreenName[];

// Description for the flag to allow tabs detaching in fullscreen on Mac
extern const char kTabDetachingInFullscreenDescription[];

// Name of the flag to reveal the fullscreen toolbar for tab strip changes
extern const char kFullscreenToolbarRevealName[];

// Description of the flag to reveal the fullscreen toolbar for tab strip
// changes
extern const char kFullscreenToolbarRevealDescription[];

// Name of the flag to enable keyboard focus for the tab strip
extern const char kTabStripKeyboardFocusName[];

// Description of the flag to enable keyboard focus for the tab strip
extern const char kTabStripKeyboardFocusDescription[];

#endif  // defined(OS_MACOSX)

// Name of the flag to enable creation of app shims for hosted apps on Mac.
extern const char kHostedAppShimCreationName[];

// Description for the flag to enable creation of app shims for hosted apps on
// Mac.
extern const char kHostedAppShimCreationDescription[];

// Name of the flag to enable a notification when quitting with hosted apps.
extern const char kHostedAppQuitNotificationName[];

// Description for the flag to enable a notification when quitting with hosted
// apps.
extern const char kHostedAppQuitNotificationDescription[];

#if defined(OS_ANDROID)

// Name of the flag for the pull-to-refresh effect.
extern const char kPullToRefreshEffectName[];

// Description of the flag for the pull-to-refresh effect.
extern const char kPullToRefreshEffectDescription[];

// Name of the flag for the translate compact infobar.
extern const char kTranslateCompactUIName[];

// Description of the flag for translate compact infobar.
extern const char kTranslateCompactUIDescription[];

#endif  // defined(OS_ANDROID)

#if defined(OS_MACOSX)

// Name of the flag to enable the new Translate UX.
extern const char kTranslateNewUxName[];

// Description for the flag to enable the new Translate UX.
extern const char kTranslateNewUxDescription[];

#endif  // defined(OS_MACOSX)

// Name of the flag to enable the Translate 2016Q2 UI.
extern const char kTranslate2016q2UiName[];

// Description for the flag to enable the Translate 2016Q2 UI.
extern const char kTranslate2016q2UiDescription[];

// Name of the flag to enable the Translate Language by ULP.
extern const char kTranslateLanguageByUlpName[];

// Description for the flag to enable the Translate Language by ULP.
extern const char kTranslateLanguageByUlpDescription[];

// Name of about:flags option for rect-based targeting in views
extern const char kViewsRectBasedTargetingName[];

// Description of about:flags option for rect-based targeting in views
extern const char kViewsRectBasedTargetingDescription[];

// Title for the flag to enable permission action reporting to safe browsing
// servers.
extern const char kPermissionActionReportingName[];

// Description for the flag to enable permission action reporting to safe
// browsing servers
extern const char kPermissionActionReportingDescription[];

// Title for the flag to enable the permissions blacklist.
extern const char kPermissionsBlacklistName[];

// Description for the flag to enable the permissions blacklist.
extern const char kPermissionsBlacklistDescription[];

// Title for the flag for threaded scrolling.
extern const char kThreadedScrollingName[];

// Description for the flag for threaded scrolling.
extern const char kThreadedScrollingDescription[];

// Name of the about:flags HarfBuzz RenderText experiment.
extern const char kHarfbuzzRendertextName[];

// Description of the about:flags HarfBuzz RenderText experiment.
extern const char kHarfbuzzRendertextDescription[];

// Name of the flag that enables embedding extension options in
// chrome://extensions.
extern const char kEmbeddedExtensionOptionsName[];

// Description of the flag that enables embedding extension options in
// chrome://extensions.
extern const char kEmbeddedExtensionOptionsDescription[];

// Name of the flag that enables the tab audio muting UI experiment in
// chrome://extensions.
extern const char kTabAudioMutingName[];

// Description of the flag that enables the tab audio muting UI experiment in
// chrome://extensions.
extern const char kTabAudioMutingDescription[];

// Title for the flag to enable Smart Lock to discover phones over Bluetooth Low
// Energy in order to unlock the Chromebook.
extern const char kEasyUnlockBluetoothLowEnergyDiscoveryName[];

// Description for the flag for Smart Lock to discover phones over Bluetooth Low
// Energy in order to unlock the Chromebook.
extern const char kEasyUnlockBluetoothLowEnergyDiscoveryDescription[];

// Title for the flag to enable Smart Lock to require close proximity between
// the phone and the Chromebook in order to unlock the Chromebook.
extern const char kEasyUnlockProximityDetectionName[];

// Description for the flag that enables Smart Lock to require close proximity
// between the phone and the Chromebook in order to unlock the Chromebook.
extern const char kEasyUnlockProximityDetectionDescription[];

// Title for the flag to enable WiFi credential sync, a feature which enables
// synchronizing WiFi network settings across devices.
extern const char kWifiCredentialSyncName[];

// Decription for the flag to enable WiFi credential sync, a feature which
// enables synchronizing WiFi network settings across devices.
extern const char kWifiCredentialSyncDescription[];

// Name for the flag that causes Chrome to use the sandbox (testing) server for
// Sync.
extern const char kSyncSandboxName[];

// Description for the flag that causes Chrome to use the sandbox (testing)
// server for Sync.
extern const char kSyncSandboxDescription[];

// Title for the flag to enable a prompt to install Data Saver when a cellular
// network is detected.
extern const char kDatasaverPromptName[];

// Decription for the flag to enable a prompt to install Data Saver when a
// cellular network is detected.
extern const char kDatasaverPromptDescription[];

// The value of the Data Saver prompt flag which always shows prompt on network
// properties change.
extern const char kDatasaverPromptDemoMode[];

// Description for the flag to disable the unified media pipeline on Android.
extern const char kDisableUnifiedMediaPipelineDescription[];

// Title for the flag to enable support for trying supported channel layouts.
extern const char kTrySupportedChannelLayoutsName[];

// Description for the flag to enable support for trying supported channel
// layouts.
extern const char kTrySupportedChannelLayoutsDescription[];

#if defined(OS_MACOSX)

// Name of the flag to enable or disable the toolkit-views App Info dialog on
// Mac.
extern const char kAppInfoDialogName[];

// Description of flag to enable or disable the toolkit-views App Info dialog on
// Mac.
extern const char kAppInfoDialogDescription[];

// Name of the flag to enable or disable toolkit-views Chrome App windows on
// Mac.
extern const char kMacViewsNativeAppWindowsName[];

// Description of flag to enable or disable toolkit-views Chrome App windows on
// Mac.
extern const char kMacViewsNativeAppWindowsDescription[];

// Name of the flag to enable or disable the toolkit-views Task Manager on Mac.
extern const char kMacViewsTaskManagerName[];

// Description of the flag to enable or disable the toolkit-views Task Manager
// on Mac.
extern const char kMacViewsTaskManagerDescription[];

// Name of the flag to enable or disable custom Cmd+` App window cycling on Mac.
extern const char kAppWindowCyclingName[];

// Description of flag to enable or disable custom Cmd+` App window cycling on
// Mac.
extern const char kAppWindowCyclingDescription[];

#endif  // defined(OS_MACOSX)

#if defined(OS_CHROMEOS)

// Name of the flag to enable accelerated mjpeg decode for captured frame where
// available.
extern const char kAcceleratedMjpegDecodeName[];

// Description for the flag to enable accelerated mjpeg decode for captured
// frame where available.
extern const char kAcceleratedMjpegDecodeDescription[];

#endif  // defined(OS_CHROMEOS)

// Name of the flag to enable the new simplified fullscreen and mouse lock UI.
extern const char kSimplifiedFullscreenUiName[];

// Description of the flag to enable the new simplified full screen and mouse
// lock UI.
extern const char kSimplifiedFullscreenUiDescription[];

// Name of the flag to enable the experimental prototype for full screen with
// keyboard lock.
extern const char kExperimentalKeyboardLockUiName[];

// Description of the flag to enable the experimental full screen keyboard lock
// UI.
extern const char kExperimentalKeyboardLockUiDescription[];

#if defined(OS_ANDROID)

// Name of the flag to configure Android page loading progress bar animation.
extern const char kProgressBarAnimationName[];

// Description of the flag to configure Android page loading progress bar
// animation.
extern const char kProgressBarAnimationDescription[];

// Linear progress bar animation style
extern const char kProgressBarAnimationLinear[];

// Smooth progress bar animation style
extern const char kProgressBarAnimationSmooth[];

// Smooth progress bar animation style with an indeterminate animation
extern const char kProgressBarAnimationSmoothIndeterminate[];

// Fast start progress bar animation style
extern const char kProgressBarAnimationFastStart[];

// Name of the flag to set when Android's page load progress bar completes.
extern const char kProgressBarCompletionName[];

// Description of the flag to set when Android's page load progress bar
// completes.
extern const char kProgressBarCompletionDescription[];

// Complete when the load event completes
extern const char kProgressBarCompletionLoadEvent[];

// Complete when domContentLoaded and any resources loaded started before
// domContentLoaded are done
extern const char kProgressBarCompletionResourcesBeforeDcl[];

// Complete when domContentLoaded is done
extern const char kProgressBarCompletionDomContentLoaded[];

// Complete when domContentLoaded and any resources loaded started before
// domContentLoaded, plus the same for same origin iframes
extern const char
    kProgressBarCompletionResourcesBeforeDclAndSameOriginIframes[];

#endif  // defined(OS_ANDROID)

// Name of the flag to disallow fetch of scripts inserted into the main frame by
// document.write.
extern const char kDisallowDocWrittenScriptsUiName[];

// Description of the flag to disallow fetch of scripts inserted into the main
// frame by document.write.
extern const char kDisallowDocWrittenScriptsUiDescription[];

#if defined(OS_WIN)

// Name of the flag to enable AppContainer lockdown experiment.
extern const char kEnableAppcontainerName[];

// Description of the flag to enable AppContainer lockdown experiment.
extern const char kEnableAppcontainerDescription[];

#endif  // defined(OS_WIN)

#if defined(TOOLKIT_VIEWS) || (defined(OS_MACOSX) && !defined(OS_IOS))

// Name of the flag to add the Certificate Viewer link to the Page Info UI.
extern const char kShowCertLinkOnPageInfoName[];

// Description of the flag to add the Certificate Viewer link to the Page
// Info UI.
extern const char kShowCertLinkOnPageInfoDescription[];

#endif  // defined(TOOLKIT_VIEWS) || (defined(OS_MACOSX) && !defined(OS_IOS))

#if defined(TOOLKIT_VIEWS) || defined(OS_ANDROID)

// Name of the flag to enable uploading Autofill credit cards.
extern const char kAutofillCreditCardUploadName[];

// Description of the flag to enable uploading Autofill credit cards.
extern const char kAutofillCreditCardUploadDescription[];

#endif  // defined(TOOLKIT_VIEWS) || defined(OS_ANDROID)

// Name for the flag to force a specific UI direction.
extern const char kForceUiDirectionName[];

// Description for the flag to force a specific UI direction.
extern const char kForceUiDirectionDescription[];

// Name for the flag to force a specific text rendering direction.
extern const char kForceTextDirectionName[];

// Description for the flag to force a specific text rendering direction.
extern const char kForceTextDirectionDescription[];

// Name for the option to force left-to-right UI or text direction mode.
extern const char kForceDirectionLtr[];

// Name for the option to force right-to-left UI or text direction mode.
extern const char kForceDirectionRtl[];

#if defined(OS_WIN) || defined(OS_LINUX)

// Name of the flag to enable che chrome.input.ime API.
extern const char kEnableInputImeApiName[];

// Description of the flag to enable the chrome.input.ime API.
extern const char kEnableInputImeApiDescription[];

#endif  // defined(OS_WIN) || defined(OS_LINUX)

// Enables grouping websites by domain on chrome://history.
extern const char kEnableGroupedHistoryName[];

// Description of the 'group by domain' feature on chrome://history
extern const char kEnableGroupedHistoryDescription[];

extern const char kSecurityChipDefault[];

extern const char kSecurityChipShowNonsecureOnly[];

extern const char kSecurityChipShowAll[];

extern const char kSecurityChipAnimationDefault[];

extern const char kSecurityChipAnimationNone[];

extern const char kSecurityChipAnimationNonsecureOnly[];

extern const char kSecurityChipAnimationAll[];

// Name of the flag to enable switching of 'Save as' menu label to 'Download'.
extern const char kSaveasMenuLabelExperimentName[];

// Description of the flag to enable switching of 'Save as' menu label to
// 'Download'.
extern const char kSaveasMenuLabelExperimentDescription[];

// Name of the flag to experimentally enable enumerating audio devices on
// ChromeOS.
extern const char kEnableEnumeratingAudioDevicesName[];

// Description of the flag that experimentally enables enumerating audio devices
// on ChromeOS.
extern const char kEnableEnumeratingAudioDevicesDescription[];

// Name of the flag to enable the new USB backend.
extern const char kNewUsbBackendName[];

// Description of the flag to enable the new USB backend.
extern const char kNewUsbBackendDescription[];

// Name of the flag to enable the new answers in suggest types in the omnibox.
extern const char kNewOmniboxAnswerTypesName[];

// Description of the flag to enable the new answers in suggest types in the
// omnibox.
extern const char kNewOmniboxAnswerTypesDescription[];

// Name of the flag option to enable the redirect of omnibox zero suggest
// requests to a backend maintained by Chrome.
extern const char kEnableZeroSuggestRedirectToChromeName[];

// Description of the flag option to enable the redirect of omnibox zero suggest
// requests to a backend maintained by Chrome.
extern const char kEnableZeroSuggestRedirectToChromeDescription[];

// Name of the experiment for the password manager to fill on account selection
// rather than page load
extern const char kFillOnAccountSelectName[];

// Description of the experiment for the password manager to fill on account
// selection rather than page load
extern const char kFillOnAccountSelectDescription[];

// Name of the flag that enables the data volume counters in the Clear browsing
// data dialog.
extern const char kEnableClearBrowsingDataCountersName[];

// Description of the flag that enables the data volume counters in the Clear
// browsing data dialog.
extern const char kEnableClearBrowsingDataCountersDescription[];

#if defined(OS_ANDROID)

// Name of the flag to enable a tabbed version of the Clear Browsing Data dialog
// in android.
extern const char kTabsInCbdName[];

// Description of the flag that enables a tabbed version of the Clear Browsing
// Data dialog.
extern const char kTabsInCbdDescription[];

#endif  // defined(OS_ANDROID)

// Name of the flag to enable native notifications.
extern const char kNotificationsNativeFlag[];

// Description of the flag to enable native notifications.
extern const char kNotificationsNativeFlagDescription[];

#if defined(OS_ANDROID)

// The description for the flag to enable use of the Android spellchecker.
extern const char kEnableAndroidSpellcheckerDescription[];

// The name of the flag to enable use of the Android spellchecker.
extern const char kEnableAndroidSpellcheckerName[];

#endif  // defined(OS_ANDROID)

// Name of the flag to enable custom layouts for Web Notifications.
extern const char kEnableWebNotificationCustomLayoutsName[];

// Description for the flag to enable custom layouts for Web Notifications.
extern const char kEnableWebNotificationCustomLayoutsDescription[];

// Title for the flag for account consistency between browser and cookie jar.
extern const char kAccountConsistencyName[];

// Description for the flag for account consistency between browser and cookie
// jar.
extern const char kAccountConsistencyDescription[];

// Title for the flag to enable the google profile information
extern const char kGoogleProfileInfoName[];

// Description for the flag to enable the google profile information
extern const char kGoogleProfileInfoDescription[];

// Name of the flag to force show the checkbox to save Wallet cards locally.
extern const char kOfferStoreUnmaskedWalletCards[];

// Name of the flag to force show the checkbox to save Wallet cards locally.
extern const char kOfferStoreUnmaskedWalletCardsDescription[];

// Name of the flag to make pages which failed to load while offline auto-reload
extern const char kOfflineAutoReloadName[];

// Description of the flag to make pages which failed to load while offline
// auto-reload
extern const char kOfflineAutoReloadDescription[];

// Name of the flag to only enable auto-reload on visible tabs
extern const char kOfflineAutoReloadVisibleOnlyName[];

// Description of the flag to only auto-reload visible tabs
extern const char kOfflineAutoReloadVisibleOnlyDescription[];

// Name of the flag to enable offering users the option of loading a stale copy
// of a page when an error occurs.
extern const char kShowSavedCopyName[];

// Description of the flag to enable offering users the option of loading a
// stale copy of a page when an error occurs.
extern const char kShowSavedCopyDescription[];

// Option for IDS_FLAGS_SHOW_SAVED_NAME to use the show saved copy as the
// primary button.
extern const char kEnableShowSavedCopyPrimary[];

// Option for IDS_FLAGS_SHOW_SAVED_COPY_NAME to use the reload as the primary
// button (in contrast to the show saved button).
extern const char kEnableShowSavedCopySecondary[];

// Option to disable IDS_FLAGS_SHOW_SAVED_COPY_NAME.
extern const char kDisableShowSavedCopy[];

#if defined(OS_CHROMEOS)

// Name of about:flags option to toggle smart deployment of the virtual
// keyboard.
extern const char kSmartVirtualKeyboardName[];

// Description of about:flags option to turn off smart deployment of the virtual
// keyboard
extern const char kSmartVirtualKeyboardDescription[];

// Name of about:flags option to turn on the virtual keyboard
extern const char kVirtualKeyboardName[];

// Description of about:flags option to turn on the virtual keyboard
extern const char kVirtualKeyboardDescription[];

// Name of about:flags option to turn on the overscrolling for the virtual
// keyboard
extern const char kVirtualKeyboardOverscrollName[];

// Description of about:flags option to turn on overscrolling for the virtual
// keyboard
extern const char kVirtualKeyboardOverscrollDescription[];

// Name of about::flags option to enable IME extensions to override the virtual
// keyboard view
extern const char kInputViewName[];

// Description of about::flags option to enable IME extensions to override the
// virtual keyboard view
extern const char kInputViewDescription[];

// Name of about::flags option for the new Korean IME
extern const char kNewKoreanImeName[];

// Description of about::flags option for the new Korean IME
extern const char kNewKoreanImeDescription[];

// Name of about::flags option to enable physical keyboard autocorrect for US
// keyboard
extern const char kPhysicalKeyboardAutocorrectName[];

// Description of about::flags option to enable physical keyboard autocorrect
// for US keyboard
extern const char kPhysicalKeyboardAutocorrectDescription[];

// Name of about::flags option for voice input on virtual keyboard
extern const char kVoiceInputName[];

// Description of about::flags option for voice input on virtual keyboard
extern const char kVoiceInputDescription[];

// Name of about::flags option to enable experimental features for IME
// input-views
extern const char kExperimentalInputViewFeaturesName[];

// Description of about::flags option to enable experimental features for IME
// input-views
extern const char kExperimentalInputViewFeaturesDescription[];

// Name of about::flags option to toggle floating virtual keyboard
extern const char kFloatingVirtualKeyboardName[];

// Description of about::flags option to toggle floating virtual keyboard
extern const char kFloatingVirtualKeyboardDescription[];

// Name of about::flags option to toggle gesture typing for the virtual keyboard
extern const char kGestureTypingName[];

// Description of about::flags option to toggle gesture typing for the virtual
// keyboard
extern const char kGestureTypingDescription[];

// Name of about::flags option to toggle gesture editing for the virtual
// keyboard
extern const char kGestureEditingName[];

// Description of about::flags option to toggle gesture editing in the settings
// page for the virtual keyboard
extern const char kGestureEditingDescription[];

// Name of about::flags option for bypass proxy for captive portal authorization
// on Chrome OS.
extern const char kCaptivePortalBypassProxyName[];

// Description of about::flags option for bypass proxy for captive portal
// authorization on Chrome OS.
extern const char kCaptivePortalBypassProxyDescription[];

// Name of option to enable touch screen calibration in material design settings
extern const char kTouchscreenCalibrationName[];

// Description of option to enable touch screen calibration settings in
// chrome://md-settings/display.
extern const char kTouchscreenCalibrationDescription[];

#endif  // defined(OS_CHROMEOS)

//  Strings for controlling credit card assist feature in about:flags.

// The name of about:flags option to enable the credit card assisted filling.
extern const char kCreditCardAssistName[];

// The description of about:flags option to enable the credit card assisted
// filling..
extern const char kCreditCardAssistDescription[];

//  Strings for controlling credit card scanning feature in about:flags.

//  Simple Cache Backend experiment.

// Name of about:flags option to turn on the Simple Cache Backend
extern const char kSimpleCacheBackendName[];

// Description of about:flags option to turn on the Simple Cache Backend
extern const char kSimpleCacheBackendDescription[];

//  Spelling feedback field trial.

// Name of about:flags option to enable the field trial for sending feedback to
// spelling service.
extern const char kSpellingFeedbackFieldTrialName[];

// Description of about:flags option to enable the field trial for sending
// feedback to spelling service.
extern const char kSpellingFeedbackFieldTrialDescription[];

//  Web MIDI API.

// Name of about:flag option to turn on Web MIDI API
extern const char kWebMidiName[];

// Description of about:flag option to turn on Web MIDI API
extern const char kWebMidiDescription[];

//  Site per process mode

// Name of about:flag option to turn on experimental out-of-process iframe
// support
extern const char kSitePerProcessName[];

// Description of about:flag option to turn on experimental out-of-process
// iframe support
extern const char kSitePerProcessDescription[];

//  Top document isolation mode

// Name of about:flag option to turn on experimental top document isolation
// support
extern const char kTopDocumentIsolationName[];

// Description of about:flag option to turn on top document isolation
extern const char kTopDocumentIsolationDescription[];

//  Cross process guest frames isolation mode

// Name of about:flag option to turn on experimental guests using out-of-process
// iframes
extern const char kCrossProcessGuestViewIsolationName[];

// Description of about:flag option to turn on experimental guests using
// out-of-process iframes
extern const char kCrossProcessGuestViewIsolationDescription[];

//  Task Scheduler

// Name of about:flag option to control redirection to the task scheduler.
extern const char kBrowserTaskSchedulerName[];

// Description of about:flag option to control redirection to the task
// scheduler.
extern const char kBrowserTaskSchedulerDescription[];

//  Arc authorization

#if defined(OS_CHROMEOS)

// Name of about:flag option to control the arc authorization endpoint.
extern const char kArcUseAuthEndpointName[];

// Desciption of about:flag option to control the arc authorization endpoint.
extern const char kArcUseAuthEndpointDescription[];

#endif  // defined(OS_CHROMEOS)

//  Autofill experiment flags

// Name of the single click autofill lab
extern const char kSingleClickAutofillName[];

// Description of the single click autofill lab
extern const char kSingleClickAutofillDescription[];

#if defined(OS_ANDROID)

// Title for the flag to show Autofill suggestions at top of keyboard
extern const char kAutofillAccessoryViewName[];

// Description for the flag to show Autofill suggestions at top of keyboard
extern const char kAutofillAccessoryViewDescription[];

#endif  // defined(OS_ANDROID)

//  Reader mode experiment flags

#if defined(OS_ANDROID)

// A name of an about:flags experiment for controlling when to show the reader
// mode button
extern const char kReaderModeHeuristicsName[];

// Describes about:flags experiment options for controlling when to show the
// reader mode button
extern const char kReaderModeHeuristicsDescription[];

// A choice in dropdown dialog on about:flags page to show the reader mode
// button with article structured markup
extern const char kReaderModeHeuristicsMarkup[];

// A choice in dropdown dialog on about:flags page to show the reader mode
// button on pages that appear to be long form content
extern const char kReaderModeHeuristicsAdaboost[];

// A choice in dropdown dialog on about:flags page to never show the reader mode
// button
extern const char kReaderModeHeuristicsAlwaysOff[];

// A choice in dropdown dialog on about:flags page to show the reader mode
// button on all pages
extern const char kReaderModeHeuristicsAlwaysOn[];

#endif  // defined(OS_ANDROID)

//  Chrome home flags

#if defined(OS_ANDROID)

// The name of the Chrome Home experiment in about:flags.
extern const char kChromeHomeName[];

// Description of the Chrome Home experiment in about:flags.
extern const char kChromeHomeDescription[];

// The name of the Chrome Home expand button experiment in about:flags.
extern const char kChromeHomeExpandButtonName[];

// Description of the Chrome Home expand button experiment in about:flags.
extern const char kChromeHomeExpandButtonDescription[];

#endif  // defined(OS_ANDROID)

//  In-Product Help flags

#if defined(OS_ANDROID)

// The name of the In-Product Help demo mode in about:flags.
extern const char kEnableIphDemoMode[];

// Description of the In-Product Help demo mode in about:flags.
extern const char kEnableIphDemoModeDescription[];

#endif  // defined(OS_ANDROID)

//  Settings window flags

// An about::flags experiment title to show settings in a separate window
extern const char kSettingsWindowName[];

// Describes an about:flags experiment to show settings in a separate window
extern const char kSettingsWindowDescription[];

//  Mixed content issue workaround flags

#if defined(OS_ANDROID)

//  Flag strings for seccomp-bpf sandbox flag.

// Title for the flag to enable the seccomp-bpf sandbox on Android.
extern const char kSeccompFilterSandboxAndroidName[];

// Description for the flag to enable the seccomp-bpf sandbox on Android.
extern const char kSeccompFilterSandboxAndroidDescription[];

#endif  // defined(OS_ANDROID)

//  Extension Content Verification

// Name of the 'Extension Content Verification' flag
extern const char kExtensionContentVerificationName[];

// Title for the flag to turn on verification of the contents of extensions from
// the webstore
extern const char kExtensionContentVerificationDescription[];

// Description of the 'Extension Content Verification' bootstrap mode
extern const char kExtensionContentVerificationBootstrap[];

// Description of the 'Extension Content Verification' enforce mode
extern const char kExtensionContentVerificationEnforce[];

// Description of the 'Extension Content Verification' enforce strict mode
extern const char kExtensionContentVerificationEnforceStrict[];

//  Built-in hotword detection display strings

// Name of about:flags option for hotword hardware detection.
extern const char kExperimentalHotwordHardwareName[];

// Description of about:flags option for hotword hardware detection.
extern const char kExperimentalHotwordHardwareDescription[];

//  Message center strings

// Name of about:flags option for message center always scroll up experiment.
extern const char kMessageCenterAlwaysScrollUpUponRemovalName[];

// Description of about:flags option for message center always scroll up
// experiment.
extern const char kMessageCenterAlwaysScrollUpUponRemovalDescription[];

// Name of chrome:flags option for Cast Streaming hardware video encoding
// support.
extern const char kCastStreamingHwEncodingName[];

// Description of chrome:flags option for Cast Streaming hardware video encoding
// support.
extern const char kCastStreamingHwEncodingDescription[];

// Name of the 'Allow insecure localhost' flag.
extern const char kAllowInsecureLocalhost[];

// Description of the 'Allow insecure localhost' flag.
extern const char kAllowInsecureLocalhostDescription[];

#if defined(OS_WIN) || defined(OS_MACOSX)

//  Tab discarding

// Name for the flag to enable or disable automatic tab discarding.
extern const char kAutomaticTabDiscardingName[];

// Description for the flag to enable or disable automatic tab description.
extern const char kAutomaticTabDiscardingDescription[];

#endif  // defined(OS_WIN) || defined(OS_MACOSX)

#if defined(OS_ANDROID)

// Name for the flag to enable offline bookmarks.
extern const char kOfflineBookmarksName[];

// Description for the flag to enable offline bookmarks.
extern const char kOfflineBookmarksDescription[];

// Name for the flag to enable badging of offline pages on the NTP.
extern const char kNtpOfflinePagesName[];

// Description for the flag to enable badging of offline pages on the NTP.
extern const char kNtpOfflinePagesDescription[];

// Name for the flag to enable offline pages for async download.
extern const char kOfflinePagesAsyncDownloadName[];

// Description for the flag to enable offline pages.
extern const char kOfflinePagesAsyncDownloadDescription[];

// Name for the flag to enable concurrent background loading on svelte (512MB
// RAM) devices.
extern const char kOfflinePagesSvelteConcurrentLoadingName[];

// Description for the flag to enable concurrent background loading on svelte
// (512MB RAM) devices.
extern const char kOfflinePagesSvelteConcurrentLoadingDescription[];

// Name for the flag to enable collecting load completeness signals to improve
// prerendering snapshot timing.
extern const char kOfflinePagesLoadSignalCollectingName[];

// Description for the flag to enable collecting load timing signals to improve
// prerendering snapshot timing.
extern const char kOfflinePagesLoadSignalCollectingDescription[];

// Name for the flag to enable offline pages to be prefetched.
extern const char kOfflinePagesPrefetchingName[];

// Description for the flag to enable offline pages prefetching.
extern const char kOfflinePagesPrefetchingDescription[];

// Name for the flag to enable offline pages to be shared.
extern const char kOfflinePagesSharingName[];

// Description for the flag to enable offline pages sharing.
extern const char kOfflinePagesSharingDescription[];

// Name for the flag to enable downloads to load pages in the background.
extern const char kBackgroundLoaderForDownloadsName[];

// Description for the flag to enable offline pages for downloads.
extern const char kBackgroundLoaderForDownloadsDescription[];

// Name for the flag to use background loader to offline and download pages
extern const char kNewBackgroundLoaderName[];

// Description for the flag to enable background offlining of pages to use
// background loader rather than prerenderer.
extern const char kNewBackgroundLoaderDescription[];

// Name for the flag to enable showing non-personalized popular suggestions on
// the New Tab Page, when no personal suggestions are available yet.
extern const char kNtpPopularSitesName[];

// Description for the flag to enable showing non-personalized popular
// suggestions on the New Tab Page, when no personal suggestions are available
// yet.
extern const char kNtpPopularSitesDescription[];

// Name of the flag to control the behaviour when opening a suggested web page
// from the New Tab Page if there is an existing tab open for it.
extern const char kNtpSwitchToExistingTabName[];

// Description of the flag to control the behaviour when opening a suggested web
// page from the New Tab Page if there is an existing tab open for it.
extern const char kNtpSwitchToExistingTabDescription[];

// Match the webpage to open with existing tabs by their URLs.
extern const char kNtpSwitchToExistingTabMatchUrl[];

// Match the webpage to open with existing tabs by their Hostnames.
extern const char kNtpSwitchToExistingTabMatchHost[];

// Name for the flag to use android midi api.
extern const char kUseAndroidMidiApiName[];

// Description for the flag to use android midi api.
extern const char kUseAndroidMidiApiDescription[];

// Name for the flag to enable web payments modifiers.
extern const char kWebPaymentsModifiersName[];

// Description for the flag to use android midi api.
extern const char kWebPaymentsModifiersDescription[];

#endif  // defined(OS_ANDROID)

#if defined(OS_WIN)

//  Exporting tracing events to ETW

// Name for the flag to enable exporting of tracing events to ETW.
extern const char kTraceExportEventsToEtwName[];

// Description for the flag to enable exporting of tracing events to ETW.
extern const char kTraceExportEventsToEtwDesription[];

// Name for the flag to enable/disable merging the key event with char event.
extern const char kMergeKeyCharEventsName[];

// Description for the flag to enable/disable merging the key event with char
// event.
extern const char kMergeKeyCharEventsDescription[];

// Name for the flag to use Windows Runtime MIDI API.
extern const char kUseWinrtMidiApiName[];

// Description for the flag to use Windows Runtime MIDI API.
extern const char kUseWinrtMidiApiDescription[];

#endif  // defined(OS_WIN)

#if defined(OS_ANDROID)

//  Data Use

//  Update Menu Item Flags

// Name of the flag to force show the update menu item.
extern const char kUpdateMenuItemName[];

// Description of the flag to force show the update menu item.
extern const char kUpdateMenuItemDescription[];

// Description of the flag to show a custom summary below the update menu item.
extern const char kUpdateMenuItemCustomSummaryDescription[];

// Name of the flag to show a custom summary below the update menu item.
extern const char kUpdateMenuItemCustomSummaryName[];

// Name of the flag to force show the update menu badge.
extern const char kUpdateMenuBadgeName[];

// Description of the flag to force show the update menu badge.
extern const char kUpdateMenuBadgeDescription[];

// Name of the flag to set a market URL for testing the update menu item.
extern const char kSetMarketUrlForTestingName[];

// Description of the flag to set a market URL for testing the update menu item.
extern const char kSetMarketUrlForTestingDescription[];

#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)

// Name for the flag to enable the new UI prototypes for tab management.
extern const char kHerbPrototypeChoicesName[];

// Description for the flag to enable the new UI prototypes for tab management.
extern const char kHerbPrototypeChoicesDescription[];

// CCT everywhere
extern const char kHerbPrototypeFlavorElderberry[];

// Name of the option to show special locale.
extern const char kEnableSpecialLocaleName[];

// Description of the option to show special locale.
extern const char kEnableSpecialLocaleDescription[];

//  WebApks

// Name for the flag to enable WebAPK feature.
extern const char kEnableWebapk[];

// Description for the flag to enable WebAPK feature.
extern const char kEnableWebapkDescription[];

#endif  // defined(OS_ANDROID)

// Title for the flag to enable Brotli Content-Encoding.
extern const char kEnableBrotliName[];

// Description for the flag to enable Brotli Content-Encoding.
extern const char kEnableBrotliDescription[];

// Title for the flag to enable WebFonts User Agent Intervention.
extern const char kEnableWebfontsInterventionName[];

// Description for the flag to enable WebFonts User Agent Intervention.
extern const char kEnableWebfontsInterventionDescription[];

// Text to indicate a experiment group name for enable-webfonts-intervention-v2
extern const char kEnableWebfontsInterventionV2ChoiceDefault[];

// Text to indicate a experiment group name for enable-webfonts-intervention-v2
extern const char kEnableWebfontsInterventionV2ChoiceEnabledWith2g[];

// Text to indicate a experiment group name for enable-webfonts-intervention-v2
extern const char kEnableWebfontsInterventionV2ChoiceEnabledWith3g[];

// Text to indicate a experiment group name for enable-webfonts-intervention-v2
extern const char kEnableWebfontsInterventionV2ChoiceEnabledWithSlow2g[];

// Text to indicate a experiment group name for enable-webfonts-intervention-v2
extern const char kEnableWebfontsInterventionV2ChoiceDisabled[];

// Title for the flag to trigger WebFonts User Agent Intervention always.
extern const char kEnableWebfontsInterventionTriggerName[];

// Description for the flag to trigger WebFonts User Agent Intervention always.
extern const char kEnableWebfontsInterventionTriggerDescription[];

// Name of the scroll anchoring flag.
extern const char kEnableScrollAnchoringName[];

// Description of the scroll anchoring flag
extern const char kEnableScrollAnchoringDescription[];

#if defined(OS_CHROMEOS)

// Name of the native cups flag.
extern const char kDisableNativeCupsName[];

// Description of the native CUPS flag
extern const char kDisableNativeCupsDescription[];

// Name of the Android Wallpapers App flag.
extern const char kEnableAndroidWallpapersAppName[];

// Description of the Android Wallpapers App flag.
extern const char kEnableAndroidWallpapersAppDescription[];

// Name of the touch support for screen magnifier flag.
extern const char kEnableTouchSupportForScreenMagnifierName[];

// Description of the touch support for screen magnifier flag.
extern const char kEnableTouchSupportForScreenMagnifierDescription[];

// Name of zip archiver on files app flag.
extern const char kEnableZipArchiverOnFileManagerName[];

// Description of zip archiver on files app flag.
extern const char kEnableZipArchiverOnFileManagerDescription[];

#endif  // defined(OS_CHROMEOS)

#if defined(OS_ANDROID)

// Name for the flag to choose a default category order for content suggestions,
// e.g. on the New Tab page.
extern const char kContentSuggestionsCategoryOrderName[];

// Description for the flag to choose a default category order for content
// suggestions, e.g. on the New Tab page.
extern const char kContentSuggestionsCategoryOrderDescription[];

// Name for the flag to choose a category ranker for content suggestions, e.g.
// on the New Tab page.
extern const char kContentSuggestionsCategoryRankerName[];

// Description for the flag to choose a category ranker for content suggestions,
// e.g. on the New Tab page.
extern const char kContentSuggestionsCategoryRankerDescription[];

// Name for the flag to enable increased visibilty for snippets on the New Tab
// Page.
extern const char kEnableNtpSnippetsVisibilityName[];

// Description for the flag to enable increased visibility for recently viewed
// tabs on the New Tab page.
extern const char kEnableNtpSnippetsVisibilityDescription[];

// Name for the flag to enable new favicon server for content suggestions on the
// New Tab Page.
extern const char kEnableContentSuggestionsNewFaviconServerName[];

// Description for the flag to enable new favicon server for content suggestions
// on the New Tab Page.
extern const char kEnableContentSuggestionsNewFaviconServerDescription[];

// Name for the flag to enable fetching favicons from a Google server for tiles
// on the New Tab Page (that originate from synced history).
extern const char kEnableNtpMostLikelyFaviconsFromServerName[];

// Description for the flag to enable fetching favicons from a Google server for
// tiles on the New Tab Page (that originate from synced history).
extern const char kEnableNtpMostLikelyFaviconsFromServerDescription[];

// Name for the flag to enable the settings entry for content suggestions.
extern const char kEnableContentSuggestionsSettingsName[];

// Description for the flag to enable the settings entry for content
// suggestions.
extern const char kEnableContentSuggestionsSettingsDescription[];

// Name for the flag to enable server-side suggestions on the New Tab Page.
extern const char kEnableNtpRemoteSuggestionsName[];

// Description for the flag to enable server-side suggestions on the New Tab
// Page.
extern const char kEnableNtpRemoteSuggestionsDescription[];

// Name for the flag to enable suggestions for recently viewed tabs, which were
// captured offline, on the New Tab page.
extern const char kEnableNtpRecentOfflineTabSuggestionsName[];

// Description for the flag to enable suggestions for recently viewed tabs on
// the New Tab page.
extern const char kEnableNtpRecentOfflineTabSuggestionsDescription[];

// Name for the flag to enable asset downloads suggestions (e.g. books,
// pictures, audio) on the New Tab page.
extern const char kEnableNtpAssetDownloadSuggestionsName[];

// Description for the flag to enable asset downloads suggestions (e.g. books,
// pictures, audio) on the New Tab page.
extern const char kEnableNtpAssetDownloadSuggestionsDescription[];

// Name for the flag to enable offline page downloads suggestions on the New Tab
// page.
extern const char kEnableNtpOfflinePageDownloadSuggestionsName[];

// Description for the flag to enable offline page downloads suggestions on the
// New Tab page.
extern const char kEnableNtpOfflinePageDownloadSuggestionsDescription[];

// Name for the flag to enable bookmark suggestions on the New Tab page.
extern const char kEnableNtpBookmarkSuggestionsName[];

// Description for the flag to enable bookmark suggestions on the New Tab page.
extern const char kEnableNtpBookmarkSuggestionsDescription[];

// Name for the flag to enable physical web page suggestions on the New Tab
// page.
extern const char kEnableNtpPhysicalWebPageSuggestionsName[];

// Description for the flag to enable physical web page suggestions on the New
// Tab page.
extern const char kEnableNtpPhysicalWebPageSuggestionsDescription[];

// Name for the flag to enable foreign sessions suggestions on the New Tab page.
extern const char kEnableNtpForeignSessionsSuggestionsName[];

// Description for the flag to enable foreign sessions suggestions on the New
// Tab page.
extern const char kEnableNtpForeignSessionsSuggestionsDescription[];

// Name for the flag to enable notifications for content suggestions on the New
// Tab page.
extern const char kEnableNtpSuggestionsNotificationsName[];

// Description for the flag to enable notifications for content suggestions on
// the New Tab page.
extern const char kEnableNtpSuggestionsNotificationsDescription[];

// Name for the flag to enable the condensed New Tab Page layout.
extern const char kNtpCondensedLayoutName[];

// Description for the flag to enable the condensed New Tab Page layout.
extern const char kNtpCondensedLayoutDescription[];

// Name for the flag to enable the condensed tile layout on the New Tab Page.
extern const char kNtpCondensedTileLayoutName[];

// Description for the flag to enable the condensed tile layout on the New Tab
// Page.
extern const char kNtpCondensedTileLayoutDescription[];

// Name for the flag to show a Google G in the omnibox on the New Tab Page.
extern const char kNtpGoogleGInOmniboxName[];

// Description for the flag to show a Google G in the omnibox on the New Tab
// Page.
extern const char kNtpGoogleGInOmniboxDescription[];

#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)

// Name for the flag to enable offlining of recently visited pages.
extern const char kOffliningRecentPagesName[];

// Description for the flag to enable offlining of recently visited pages.
extern const char kOffliningRecentPagesDescription[];

// Name for the flag to enable offlining CT features.
extern const char kOfflinePagesCtName[];

// Description for the flag to enable offline pages CT features.
extern const char kOfflinePagesCtDescription[];

#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)

// Name for the flag to enable expanded autofill poup layout for credit cards.
extern const char kEnableExpandedAutofillCreditCardPopupLayout[];

// Description for the flag to enable expanded autofill poup layout for credit
// cards.
extern const char kEnableExpandedAutofillCreditCardPopupLayoutDescription[];

#endif  // defined(OS_ANDROID)

// Name for the flag to enable display the last used date of a credit card in
// autofill.
extern const char kEnableAutofillCreditCardLastUsedDateDisplay[];

// Description for the flag to enable display the last used date of a credit
// card in autofill.
extern const char kEnableAutofillCreditCardLastUsedDateDisplayDescription[];

// Name for the flag to enable requesting CVC in the credit card upload "offer
// to save" bubble if it was not already detected in the submitted form.
extern const char kEnableAutofillCreditCardUploadCvcPrompt[];

// Description for the flag to enable requesting CVC in the credit card upload
// offer to save bubble if it was not detected during the checkout flow.
extern const char kEnableAutofillCreditCardUploadCvcPromptDescription[];

#if !defined(OS_ANDROID) && defined(GOOGLE_CHROME_BUILD)

// Name for the flag to enable Google branding in the context menu.
extern const char kGoogleBrandedContextMenuName[];

// Description for the flag to enable Google branding in the context menu.
extern const char kGoogleBrandedContextMenuDescription[];

#endif  // !defined(OS_ANDROID) && defined(GOOGLE_CHROME_BUILD)

// Title for the flag to enable WebUSB.
extern const char kEnableWebUsbName[];

// Description for the flag to enable WebUSB.
extern const char kEnableWebUsbDescription[];

// Title for the flag to enable Generic Sensor API.
extern const char kEnableGenericSensorName[];

// Description for the flag to enable Generic Sensor API.
extern const char kEnableGenericSensorDescription[];

// Title for the flag to enable FontCache scaling
extern const char kFontCacheScalingName[];

// Description for the flag to enable FontCache scaling
extern const char kFontCacheScalingDescription[];

// Title for the flag to enable Framebusting restrictions
extern const char kFramebustingName[];

// Description for the flag that prevents an iframe (typically an ad) from
// navigating the top level browsing context (the whole tab) unless the top and
// the iframe are part of the same website or the the navigation is requested in
// response to a user action (e.g., the user clicked on the tab)
extern const char kFramebustingDescription[];

#if defined(OS_ANDROID)

// Name of the flag to enable VR shell
extern const char kEnableVrShellName[];

// Description for the flag to enable VR shell
extern const char kEnableVrShellDescription[];

#endif  // defined(OS_ANDROID)

//  Web payments

// Name of the flag to enable Web Payments API.
extern const char kWebPaymentsName[];

// Description for the flag to enable Web Payments API
extern const char kWebPaymentsDescription[];

#if defined(OS_ANDROID)

// Name of the flag to enable the first version of Android Pay integration
extern const char kEnableAndroidPayIntegrationV1Name[];

// Description for the flag to enable the first version of Android Pay
// integration
extern const char kEnableAndroidPayIntegrationV1Description[];

// Name of the flag to enable the second version of Android Pay integration
extern const char kEnableAndroidPayIntegrationV2Name[];

// Description for the flag to enable the second version of Android Pay
// integration
extern const char kEnableAndroidPayIntegrationV2Description[];

// Name of the flag to enable the Web Payments to skip UI
extern const char kEnableWebPaymentsSingleAppUiSkipName[];

// Description for the flag to enable the Web Payments to skip UI
extern const char kEnableWebPaymentsSingleAppUiSkipDescription[];

// Name of the flag to enable third party Android payment apps
extern const char kAndroidPaymentAppsName[];

// Description for the flag to enable third party Android payment apps
extern const char kAndroidPaymentAppsDescription[];

// Name of the flag to enable Service Worker payment apps
extern const char kServiceWorkerPaymentAppsName[];

// Description for the flag to enable Service Worker payment apps
extern const char kServiceWorkerPaymentAppsDescription[];

#endif  // defined(OS_ANDROID)

// Name for the flag to enable feature policy.
extern const char kFeaturePolicyName[];

// Description for the flag to enable feature policy.
extern const char kFeaturePolicyDescription[];

//  Audio rendering mixing experiment strings.

// Name of the flag for enabling the new audio rendering mixing strategy.
extern const char kNewAudioRenderingMixingStrategyName[];

// Description of the flag for enabling the new audio rendering mixing strategy.
extern const char kNewAudioRenderingMixingStrategyDescription[];

//  Background video track disabling experiment strings.

// Name of the flag for enabling optimizing background video playback.
extern const char kBackgroundVideoTrackOptimizationName[];

// Description of the flag for enabling optimizing background video playback.
extern const char kBackgroundVideoTrackOptimizationDescription[];

//  New remote playback pipeline experiment strings.

// Name of the flag for enabling the new remote playback pipeline.
extern const char kNewRemotePlaybackPipelineName[];

// Description of the flag for enabling the new remote playback pipeline.
extern const char kNewRemotePlaybackPipelineDescription[];

//  Video fullscreen with orientation lock experiment strings.

// Name of the flag for enabling orientation lock for fullscreen video playback.
extern const char kVideoFullscreenOrientationLockName[];

// Description of the flag for enabling orientation lock for fullscreen video
// playback.
extern const char kVideoFullscreenOrientationLockDescription[];

//  Video rotate-to-fullscreen experiment strings.

// Name of the flag for enabling rotate-to-fullscreen for video playback.
extern const char kVideoRotateToFullscreenName[];

// Description of the flag for enabling rotate-to-fullscreen for video playback.
extern const char kVideoRotateToFullscreenDescription[];

//  Expensive background timer throttling flag

// Name for the flag to enable expensive background timer throttling
extern const char kExpensiveBackgroundTimerThrottlingName[];

// Description for the flag to enable expensive background timer throttling
extern const char kExpensiveBackgroundTimerThrottlingDescription[];

//  Enable default MediaSession flag

#if !defined(OS_ANDROID)

// Name for the flag to enable default MediaSession on desktop
extern const char kEnableDefaultMediaSessionName[];

// Desciption for the flag to enable default MediaSession on desktop
extern const char kEnableDefaultMediaSessionDescription[];

// Option for disabling the default MediaSession
extern const char kEnableDefaultMediaSessionDisabled[];

// Option for enabling the default MediaSession
extern const char kEnableDefaultMediaSessionEnabled[];

// Option for enabling the default MediaSession with Flash
extern const char kEnableDefaultMediaSessionEnabledDuckFlash[];

#endif  // !defined(OS_ANDROID)

#if defined(OS_WIN)

// Name for the flag that enables using GDI to print text
extern const char kGdiTextPrinting[];

// Description of the flag that enables using GDI to print text.
extern const char kGdiTextPrintingDescription[];

#endif  // defined(OS_WIN)

#if defined(OS_ANDROID)

// Name for the flag to enable modal permission prompts on Android
extern const char kModalPermissionPromptsName[];

// Description for the flag to enable modal permission prompts on Android.
extern const char kModalPermissionPromptsDescription[];

#endif  // defined(OS_ANDROID)

#if !defined(OS_MACOSX)

// Name for the flag to enable a persistence toggle in permission prompts
extern const char kPermissionPromptPersistenceToggleName[];

// Description for the flag to enable a persistence toggle in permission prompts
extern const char kPermissionPromptPersistenceToggleDescription[];

#endif  // !defined(OS_MACOSX)

#if defined(OS_ANDROID)

// Title for the flag to enable the No Card Abort in Payment Request.
extern const char kNoCreditCardAbort[];

// Description for the flag to enable the No Card Abort in Payment Request.
extern const char kNoCreditCardAbortDescription[];

#endif  // defined(OS_ANDROID)

//  Consistent omnibox geolocation

#if defined(OS_ANDROID)

// Name for the flag to enable consistent omnibox geolocation
extern const char kEnableConsistentOmniboxGeolocationName[];

// Desciption for the flag to enable consistent omnibox geolocation
extern const char kEnableConsistentOmniboxGeolocationDescription[];

#endif  // defined(OS_ANDROID)

//  Media Remoting chrome://flags strings

// Name for the flag to enable Media Remoting
extern const char kMediaRemotingName[];

// Desciption for the flag to enable Media Remoting
extern const char kMediaRemotingDescription[];

//  Chrome OS component updates chrome://flags strings

// Name for the flag to enable Chrome OS component flash updates
extern const char kCrosCompUpdatesName[];

// Description for the flag to enable Chrome OS component flash updates
extern const char kCrosCompUpdatesDescription[];

//  Play Services LSD permission prompt chrome://flags strings

#if defined(OS_ANDROID)

// Name for the flag to enable LSD permission prompts on Android
extern const char kLsdPermissionPromptName[];

// Description for the flag to enable LSD permission prompts on Android.
extern const char kLsdPermissionPromptDescription[];

#endif  // defined(OS_ANDROID)

#if defined(OS_WIN)

//  Custom draw the Windows 10 titlebar. crbug.com/505013

// Name of the flag that enables custom drawing of the Windows 10 titlebar.
extern const char kWindows10CustomTitlebarName[];

// Description for the flag that enables custom drawing of the Windows 10
// titlebar.
extern const char kWindows10CustomTitlebarDescription[];

#endif  // defined(OS_WIN)

#if defined(OS_WIN)

// Name of the flag that disables postscript printing.
extern const char kDisablePostscriptPrinting[];

// Description of the flag that disables postscript printing.
extern const char kDisablePostscriptPrintingDescription[];

#endif  // defined(OS_WIN)

#if defined(OS_ANDROID)

// Name of the flag that enables intermediate certificate fetching.
extern const char kAiaFetchingName[];

// Description of the flag that enables intermediate certificate fetching.
extern const char kAiaFetchingDescription[];

#endif  // defined(OS_ANDROID)

//  Web MIDI supports MIDIManager dynamic instantiation chrome://flags strings

// Name for the flag to enable MIDIManager dynamic instantiation
extern const char kEnableMidiManagerDynamicInstantiationName[];

// Description for the flag to enable MIDIManager dynamic instantiation
extern const char kEnableMidiManagerDynamicInstantiationDescription[];

//  Desktop iOS promotion chrome://flags strings

#if defined(OS_WIN)

// Name for the flag to enable desktop to iOS promotions
extern const char kEnableDesktopIosPromotionsName[];

// Description for the flag to enable desktop to iOS promotions
extern const char kEnableDesktopIosPromotionsDescription[];

#endif  // defined(OS_WIN)

#if defined(OS_ANDROID)

// Name for the flag to enable custom feeback UI
extern const char kEnableCustomFeedbackUiName[];

// Name for the flag to enable custom feeback UI
extern const char kEnableCustomFeedbackUiDescription[];

#endif  // defined(OS_ANDROID)

// Name of the flag that enables adjustable large cursor.
extern const char kEnableAdjustableLargeCursorName[];

// Description of the flag that enables adjustable large cursor.
extern const char kEnableAdjustableLargeCursorDescription[];

#if defined(OS_CHROMEOS) || defined(OS_LINUX) || defined(OS_MACOSX) || \
    defined(OS_WIN)

// Name of the flag that enables receiving entity suggestions.
extern const char kOmniboxEntitySuggestionsName[];

// Description of the flag that enables entity suggestions.
extern const char kOmniboxEntitySuggestionsDescription[];

extern const char kPauseBackgroundTabsName[];
extern const char kPauseBackgroundTabsDescription[];

// Name of the flag to enable the new app menu icon.
extern const char kEnableNewAppMenuIconName[];

// Description of the flag to enable the new app menu icon.
extern const char kEnableNewAppMenuIconDescription[];

#endif  // defined(OS_CHROMEOS) || defined(OS_LINUX) || defined(OS_MACOSX) ||
        // defined(OS_WIN)

#if defined(OS_CHROMEOS)

// Name of the flag that enables ChromeVox support for ARC.
extern const char kEnableChromevoxArcSupportName[];

// Description of the flag that enables ChromeVox support for ARC.
extern const char kEnableChromevoxArcSupportDescription[];

#endif  // defined(OS_CHROMEOS)

// Title for the flag to enable Mojo IPC for resource loading
extern const char kMojoLoadingName[];

// Description for the flag to enable Mojo IPC for resource loading
extern const char kMojoLoadingDescription[];

#if defined(OS_ANDROID)

// Name for the flag to enable the new Doodle API
extern const char kUseNewDoodleApiName[];

// Description for the flag to enable the new Doodle API
extern const char kUseNewDoodleApiDescription[];

#endif  // defined(OS_ANDROID)

// Title for the flag to delay navigation
extern const char kDelayNavigationName[];

// Description for the flag to delay navigation
extern const char kDelayNavigationDescription[];

// Description of the 'Debugging keyboard shortcuts' lab.
extern const char kDebugShortcutsDescription[];

extern const char kMemoryAblationName[];
extern const char kMemoryAblationDescription[];

#if defined(OS_ANDROID)

// Name for the flag to enable the custom context menu.
extern const char kEnableCustomContextMenuName[];

// Description for the flag to enable the custom context menu.
extern const char kEnableCustomContextMenuDescription[];

#endif  // defined(OS_ANDROID)

#if defined(OS_CHROMEOS)

//  File Manager

// Name of the about:flag option to enable the chromecast support for video app.
extern const char kVideoPlayerChromecastSupportName[];

// Description of the about:flag option to the enable the chromecast support for
// video app.
extern const char kVideoPlayerChromecastSupportDescription[];

// Name of about::flags option for the new ZIP unpacker based on the File System
// Provider API.
extern const char kNewZipUnpackerName[];

// Description of about::flags option for the new ZIP unpacker based on the File
// System Provider API.
extern const char kNewZipUnpackerDescription[];

// Name of about::flags option for showing Android Files app in launcher.
extern const char kShowArcFilesAppName[];

// Description of the about::flag option for showing ARC Files app in launcher.
extern const char kShowArcFilesAppDescription[];

// Name for the flag for Office Editing for Docs, Sheets & Slides component
// extension.
extern const char kOfficeEditingComponentAppName[];

// Description for the flag for Office Editing for Docs, Sheets & Slides
// component extension.
extern const char kOfficeEditingComponentAppDescription[];

// Name for the flag for the color calibration of the display.
extern const char kDisplayColorCalibrationName[];

// Description for the flag for the color calibration of the display.
extern const char kDisplayColorCalibrationDescription[];

// Name for the flag which specifies which memory pressure strategy should be
// used on ChromeOS.
extern const char kMemoryPressureThresholdName[];

// Description for the flag which specifies which memory pressure strategy
// should be used on ChromeOS.
extern const char kMemoryPressureThresholdDescription[];

// The value of the Memory pressure for ChromeOS which requests conservative
// thresholds.
extern const char kConservativeThresholds[];

// The value of the Memory pressure thresholds for ChromeOS which use an
// aggressive cache release strategy.
extern const char kAggressiveCacheDiscardThresholds[];

// The value of the Memory pressure thresholds for ChromeOS which uses an
// aggressive tab release strategy.
extern const char kAggressiveTabDiscardThresholds[];

// The value of the Memory pressure thresholds for ChromeOS which use an
// aggressive release strategy.
extern const char kAggressiveThresholds[];

// Name for the flag to enable wake on packets.
extern const char kWakeOnPacketsName[];

// Description for the flag to enable wake on packets.
extern const char kWakeOnPacketsDescription[];

// Title of the flag used to enable quick unlock PIN.
extern const char kQuickUnlockPin[];

// Description of the flag used to enable quick unlock PIN.
extern const char kQuickUnlockPinDescription[];

// Title of the flag used to enable PIN on signin.
extern const char kQuickUnlockPinSignin[];

// Description of the flag used to enable PIN on signin.
extern const char kQuickUnlockPinSigninDescription[];

// Title of the flag used to enable quick unlock fingerprint.
extern const char kQuickUnlockFingerprint[];

// Description of the flag used to enable quick unlock fingerprint.
extern const char kQuickUnlockFingerprintDescription[];

// Name of the about:flag option for experimental accessibility features.
extern const char kExperimentalAccessibilityFeaturesName[];

// Description of the about:flag option for experimental accessibility features.
extern const char kExperimentalAccessibilityFeaturesDescription[];

// Name of the about:flag option for disabling
// EnableSystemTimezoneAutomaticDetection policy.
extern const char kDisableSystemTimezoneAutomaticDetectionName[];

// Description of the about:flag option for disabling
// EnableSystemTimezoneAutomaticDetection policy.
extern const char kDisableSystemTimezoneAutomaticDetectionDescription[];

// Name of the about:flag option to disable eol notification.
extern const char kEolNotificationName[];

// Description of the about:flag option to disable eol notification.
extern const char kEolNotificationDescription[];

//  Stylus strings

// Name of the about:flag option to enable stylus tools.
extern const char kForceEnableStylusToolsName[];

// Description of the about:flag option to enable stylus tools.
extern const char kForceEnableStylusToolsDescription[];

//  Network portal notification

// Title for the flag to enable/disable notifications about captive portals.
extern const char kNetworkPortalNotificationName[];

// Description for the flag to enable/disable notifications about captive
// portals.
extern const char kNetworkPortalNotificationDescription[];

// Name of the option for mtp write support.
extern const char kMtpWriteSupportName[];

// Description of the option for mtp write support.
extern const char kMtpWriteSupportDescription[];

// Title for the flag to select cros-regions file handling mode.
extern const char kCrosRegionsModeName[];

// Description for the flag to select cros-regions file handling mode.
extern const char kCrosRegionsModeDescription[];

// Name of the value for cros-regions file handling mode for 'default' mode.
extern const char kCrosRegionsModeDefault[];

// Name of the value for cros-regions file handling mode for 'override' mode
// (values from region file replace matching VPD values).
extern const char kCrosRegionsModeOverride[];

// Name of the value for cros-regions file handling mode for 'hide' mode (VPD
// values are hidden, only cros-region values are used).
extern const char kCrosRegionsModeHide[];

// Name of the flag used to enable launching Chrome Web Store Gallery widget app
// for searching for printer provider apps.
extern const char kPrinterProviderSearchAppName[];

// Description of a flag in chrome://flags that enables launching Chrome Web
// Store Gallery widget app for searching for printer provider apps.
extern const char kPrinterProviderSearchAppDescription[];

// Name of the flag for blocking the ACTION_BOOT_COMPLETED broadcast for
// third-party apps on ARC.
extern const char kArcBootCompleted[];

// Description for the flag for blocking ACTION_BOOT_COMPLETED broadcast for
// third-party apps.
extern const char kArcBootCompletedDescription[];

// Name of the about: flag for enabling opt-in IME menu.
extern const char kEnableImeMenuName[];

// Description of the about: flag for enabling opt-in IME menu.
extern const char kEnableImeMenuDescription[];

// Name of the about: flag for enabling emoji, handwriting and voice input on
// opt-in IME menu.
extern const char kEnableEhvInputName[];

// Description of the about: flag enabling emoji, handwriting and voice input on
// opt-in IME menu.
extern const char kEnableEhvInputDescription[];

// Name of the about: flag for enabling encryption migratoin, which ensures the
// user's cryptohome is encrypted by ext4 dircrypto instead of ecryptfs.
extern const char kEnableEncryptionMigrationName[];

// Description of the about: flag for enabling encryption migratoin.
extern const char kEnableEncryptionMigrationDescription[];

#endif  // #if defined(OS_CHROMEOS)

#if defined(OS_ANDROID)

// Name of the flag that enables Copyless Paste.
extern const char kEnableCopylessPasteName[];

// Description of the flag that enables Copyless Paste.
extern const char kEnableCopylessPasteDescription[];

// Title for the flag to enable WebNFC.
extern const char kEnableWebNfcName[];

// Description for the flag to enable WebNFC.
extern const char kEnableWebNfcDescription[];

#endif  // defined(OS_ANDROID)

// Name of the flag that enables Blink's idle time spell checker.
extern const char kEnableIdleTimeSpellCheckingName[];

// Description of the flag that enables Blink's idle time spell checker.
extern const char kEnableIdleTimeSpellCheckingDescription[];

#if defined(OS_ANDROID)

// Name of the flag that enables the omnibox's clipboard URL provider.
extern const char kEnableOmniboxClipboardProviderName[];

// Description of the flag that enables the omnibox's clipboard URL provider.
extern const char kEnableOmniboxClipboardProviderDescription[];

#endif  // defined(OS_ANDROID)

// Name of the autoplay policy flag.
extern const char kAutoplayPolicyName[];

// Description of the autoplay policy entry.
extern const char kAutoplayPolicyDescription[];

// Description of the autoplay policy that requires a user gesture on cross
// origin iframes.
extern const char kAutoplayPolicyCrossOriginUserGestureRequired[];

// Description of the autoplay policy that has no user gesture requirements.
extern const char kAutoplayPolicyNoUserGestureRequired[];

// Description of the autoplay policy that requires a user gesture.
extern const char kAutoplayPolicyUserGestureRequired[];

// Name of the about: flag for displaying the title of the omnibox match for
// current URL.
extern const char kOmniboxDisplayTitleForCurrentUrlName[];

// Description of the about: flag for displaying the title of the omnibox match
// for current URL.
extern const char kOmniboxDisplayTitleForCurrentUrlDescription[];

// Name of the Omnibox UI vertical margin flag.
extern const char kOmniboxUIVerticalMarginName[];

// Description of the Omnibox UI vertical margin flag.
extern const char kOmniboxUIVerticalMarginDescription[];

// Name of the flag that forces Network Quality Estimator (NQE) to always
// return the specified effective connection type.
extern const char kForceEffectiveConnectionTypeName[];

// Description of the flag that forces Network Quality Estimator (NQE) to always
// return the specified effective connection type.
extern const char kForceEffectiveConnectionTypeDescription[];

// Description of the various effective connection type choices that can be
// set using kForceEffectiveConnectionTypeName flag.
extern const char kEffectiveConnectionTypeUnknownDescription[];
extern const char kEffectiveConnectionTypeOfflineDescription[];
extern const char kEffectiveConnectionTypeSlow2GDescription[];
extern const char kEffectiveConnectionType2GDescription[];
extern const char kEffectiveConnectionType3GDescription[];
extern const char kEffectiveConnectionType4GDescription[];

// Name & description for the heap profiling flag.
extern const char kEnableHeapProfilingName[];
extern const char kEnableHeapProfilingDescription[];

// Descriptions of the different heap profiling modes.
extern const char kEnableHeapProfilingModePseudo[];
extern const char kEnableHeapProfilingModeNative[];
extern const char kEnableHeapProfilingTaskProfiler[];

// Name and description of the flag allowing the usage of a small set of
// server-side suggestions in NTP tiles.
extern const char kUseSuggestionsEvenIfFewFeatureName[];
extern const char kUseSuggestionsEvenIfFewFeatureDescription[];

// Name of the about: flag for experimental location.reload() to trigger a
// hard-reload.
extern const char kLocationHardReloadName[];

// Description of the about: flag for experimental location.reload() to trigger
// a hard-reload.
extern const char kLocationHardReloadDescription[];

// Name and description for the capture-thumbnail-on-load-finished flag.
extern const char kCaptureThumbnailOnLoadFinishedName[];
extern const char kCaptureThumbnailOnLoadFinishedDescription[];

}  // namespace flag_descriptions

#endif  // CHROME_BROWSER_FLAG_DESCRIPTIONS_H_
