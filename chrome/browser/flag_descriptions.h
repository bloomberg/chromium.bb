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
//
// For descriptions of the flags, see flat_descriptions.cc

namespace flag_descriptions {

extern const char kEnableMaterialDesignBookmarksName[];
extern const char kEnableMaterialDesignBookmarksDescription[];

extern const char kEnableMaterialDesignPolicyPageName[];
extern const char kEnableMaterialDesignPolicyPageDescription[];

extern const char kEnableMaterialDesignSettingsName[];
extern const char kEnableMaterialDesignSettingsDescription[];

extern const char kEnableMaterialDesignExtensionsName[];
extern const char kEnableMaterialDesignExtensionsDescription[];

extern const char kEnableMaterialDesignFeedbackName[];
extern const char kEnableMaterialDesignFeedbackDescription[];

extern const char kContextualSuggestionsCarouselName[];
extern const char kContextualSuggestionsCarouselDescription[];

extern const char kSafeSearchUrlReportingName[];
extern const char kSafeSearchUrlReportingDescription[];

extern const char kEnableUseZoomForDsfName[];
extern const char kEnableUseZoomForDsfDescription[];
extern const char kEnableUseZoomForDsfChoiceDefault[];
extern const char kEnableUseZoomForDsfChoiceEnabled[];
extern const char kEnableUseZoomForDsfChoiceDisabled[];

extern const char kNostatePrefetchName[];
extern const char kNostatePrefetchDescription[];

extern const char kSpeculativePrefetchName[];
extern const char kSpeculativePrefetchDescription[];

extern const char kForceTabletModeName[];
extern const char kForceTabletModeDescription[];
extern const char kForceTabletModeTouchview[];
extern const char kForceTabletModeClamshell[];
extern const char kForceTabletModeAuto[];

extern const char kPrintPdfAsImageName[];
extern const char kPrintPdfAsImageDescription[];

#if !defined(DISABLE_NACL)
extern const char kNaclName[];
extern const char kNaclDescription[];

extern const char kNaclDebugName[];
extern const char kNaclDebugDescription[];

extern const char kPnaclSubzeroName[];
extern const char kPnaclSubzeroDescription[];

extern const char kNaclDebugMaskName[];
extern const char kNaclDebugMaskDescription[];
extern const char kNaclDebugMaskChoiceDebugAll[];
extern const char kNaclDebugMaskChoiceExcludeUtilsPnacl[];
extern const char kNaclDebugMaskChoiceIncludeDebug[];
#endif  // !defined(DISABLE_NACL)

extern const char kEnableHttpFormWarningName[];
extern const char kEnableHttpFormWarningDescription[];

extern const char kMarkHttpAsName[];
extern const char kMarkHttpAsDescription[];
extern const char kMarkHttpAsDangerous[];

extern const char kMaterialDesignIncognitoNTPName[];
extern const char kMaterialDesignIncognitoNTPDescription[];

extern const char kSavePageAsMhtmlName[];
extern const char kSavePageAsMhtmlDescription[];

extern const char kMhtmlGeneratorOptionName[];
extern const char kMhtmlGeneratorOptionDescription[];
extern const char kMhtmlSkipNostoreMain[];
extern const char kMhtmlSkipNostoreAll[];

extern const char kDeviceDiscoveryNotificationsName[];
extern const char kDeviceDiscoveryNotificationsDescription[];

#if defined(OS_WIN)

extern const char kCloudPrintXpsName[];
extern const char kCloudPrintXpsDescription[];

#endif  // defined(OS_WIN)

extern const char kLoadMediaRouterComponentExtensionName[];
extern const char kLoadMediaRouterComponentExtensionDescription[];

extern const char kPrintPreviewRegisterPromosName[];
extern const char kPrintPreviewRegisterPromosDescription[];

extern const char kScrollPredictionName[];
extern const char kScrollPredictionDescription[];

extern const char kTopChromeMd[];
extern const char kTopChromeMdDescription[];
extern const char kTopChromeMdMaterial[];
extern const char kTopChromeMdMaterialHybrid[];

extern const char kSiteDetails[];
extern const char kSiteDetailsDescription[];

extern const char kSiteSettings[];
extern const char kSiteSettingsDescription[];

extern const char kSecondaryUiMd[];
extern const char kSecondaryUiMdDescription[];

extern const char kAddToShelfName[];
extern const char kAddToShelfDescription[];

extern const char kBypassAppBannerEngagementChecksName[];
extern const char kBypassAppBannerEngagementChecksDescription[];

#if defined(OS_ANDROID)

extern const char kAccessibilityTabSwitcherName[];
extern const char kAccessibilityTabSwitcherDescription[];

extern const char kAndroidAutofillAccessibilityName[];
extern const char kAndroidAutofillAccessibilityDescription[];

extern const char kEnablePhysicalWebName[];
extern const char kEnablePhysicalWebDescription[];

#endif  // defined(OS_ANDROID)

extern const char kTouchEventsName[];
extern const char kTouchEventsDescription[];

extern const char kTouchAdjustmentName[];
extern const char kTouchAdjustmentDescription[];

extern const char kCompositedLayerBordersName[];
extern const char kCompositedLayerBordersDescription[];

extern const char kGlCompositedTextureQuadBordersName[];
extern const char kGlCompositedTextureQuadBordersDescription[];

extern const char kShowOverdrawFeedbackName[];
extern const char kShowOverdrawFeedbackDescription[];

extern const char kUiPartialSwapName[];
extern const char kUiPartialSwapDescription[];

extern const char kDebugShortcutsName[];
extern const char kDebugShortcutsDescription[];

extern const char kIgnoreGpuBlacklistName[];
extern const char kIgnoreGpuBlacklistDescription[];

extern const char kInertVisualViewportName[];
extern const char kInertVisualViewportDescription[];

extern const char kColorCorrectRenderingName[];
extern const char kColorCorrectRenderingDescription[];

extern const char kExperimentalCanvasFeaturesName[];
extern const char kExperimentalCanvasFeaturesDescription[];

extern const char kAccelerated2dCanvasName[];
extern const char kAccelerated2dCanvasDescription[];

extern const char kDisplayList2dCanvasName[];
extern const char kDisplayList2dCanvasDescription[];

extern const char kEnable2dCanvasDynamicRenderingModeSwitchingName[];
extern const char kEnable2dCanvasDynamicRenderingModeSwitchingDescription[];

extern const char kExperimentalExtensionApisName[];
extern const char kExperimentalExtensionApisDescription[];

extern const char kExtensionsOnChromeUrlsName[];
extern const char kExtensionsOnChromeUrlsDescription[];

extern const char kFastUnloadName[];
extern const char kFastUnloadDescription[];

extern const char kUserConsentForExtensionScriptsName[];
extern const char kUserConsentForExtensionScriptsDescription[];

extern const char kHistoryRequiresUserGestureName[];
extern const char kHistoryRequiresUserGestureDescription[];
extern const char kHyperlinkAuditingName[];
extern const char kHyperlinkAuditingDescription[];

#if defined(OS_ANDROID)

extern const char kContextualSearchName[];
extern const char kContextualSearchDescription[];

extern const char kContextualSearchContextualCardsBarIntegration[];
extern const char kContextualSearchContextualCardsBarIntegrationDescription[];

extern const char kContextualSearchSingleActionsName[];
extern const char kContextualSearchSingleActionsDescription[];

extern const char kContextualSearchUrlActionsName[];
extern const char kContextualSearchUrlActionsDescription[];

#endif  // defined(OS_ANDROID)

extern const char kSmoothScrollingName[];
extern const char kSmoothScrollingDescription[];

extern const char kOverlayScrollbarsName[];
extern const char kOverlayScrollbarsDescription[];

extern const char kShowAutofillTypePredictionsName[];
extern const char kShowAutofillTypePredictionsDescription[];

extern const char kTcpFastOpenName[];
extern const char kTcpFastOpenDescription[];

extern const char kTouchDragDropName[];
extern const char kTouchDragDropDescription[];

extern const char kTouchSelectionStrategyName[];
extern const char kTouchSelectionStrategyDescription[];
extern const char kTouchSelectionStrategyCharacter[];
extern const char kTouchSelectionStrategyDirection[];

extern const char kVibrateRequiresUserGestureName[];
extern const char kVibrateRequiresUserGestureDescription[];

extern const char kWalletServiceUseSandboxName[];
extern const char kWalletServiceUseSandboxDescription[];

extern const char kOverscrollHistoryNavigationName[];
extern const char kOverscrollHistoryNavigationDescription[];
extern const char kOverscrollHistoryNavigationSimpleUi[];

extern const char kOverscrollStartThresholdName[];
extern const char kOverscrollStartThresholdDescription[];
extern const char kOverscrollStartThreshold133Percent[];
extern const char kOverscrollStartThreshold166Percent[];
extern const char kOverscrollStartThreshold200Percent[];

extern const char kScrollEndEffectName[];
extern const char kScrollEndEffectDescription[];

extern const char kWebgl2Name[];
extern const char kWebgl2Description[];

extern const char kWebglDraftExtensionsName[];
extern const char kWebglDraftExtensionsDescription[];

extern const char kWebrtcHwDecodingName[];
extern const char kWebrtcHwDecodingDescription[];

extern const char kWebrtcHwEncodingName[];
extern const char kWebrtcHwEncodingDescription[];

extern const char kWebrtcHwH264EncodingName[];
extern const char kWebrtcHwH264EncodingDescription[];

extern const char kWebrtcHwVP8EncodingName[];
extern const char kWebrtcHwVP8EncodingDescription[];

extern const char kWebrtcSrtpAesGcmName[];
extern const char kWebrtcSrtpAesGcmDescription[];

extern const char kWebrtcStunOriginName[];
extern const char kWebrtcStunOriginDescription[];

extern const char kWebrtcEchoCanceller3Name[];
extern const char kWebrtcEchoCanceller3Description[];

#if defined(OS_ANDROID)

extern const char kMediaScreenCaptureName[];
extern const char kMediaScreenCaptureDescription[];

#endif  // defined(OS_ANDROID)

#if BUILDFLAG(ENABLE_WEBRTC)

extern const char kWebrtcH264WithOpenh264FfmpegName[];
extern const char kWebrtcH264WithOpenh264FfmpegDescription[];

#endif  // BUILDFLAG(ENABLE_WEBRTC)

extern const char kWebvrName[];
extern const char kWebvrDescription[];

extern const char kWebvrExperimentalRenderingName[];
extern const char kWebvrExperimentalRenderingDescription[];

extern const char kGamepadExtensionsName[];
extern const char kGamepadExtensionsDescription[];

#if defined(OS_ANDROID)

extern const char kNewPhotoPickerName[];
extern const char kNewPhotoPickerDescription[];

#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)

extern const char kEnableOskOverscrollName[];
extern const char kEnableOskOverscrollDescription[];

#endif  // defined(OS_ANDROID)

extern const char kQuicName[];
extern const char kQuicDescription[];

extern const char kSslVersionMaxName[];
extern const char kSslVersionMaxDescription[];
extern const char kSslVersionMaxTls12[];
extern const char kSslVersionMaxTls13[];

extern const char kEnableTokenBindingName[];
extern const char kEnableTokenBindingDescription[];

extern const char kPassiveDocumentEventListenersName[];
extern const char kPassiveDocumentEventListenersDescription[];

extern const char kPassiveEventListenersDueToFlingName[];
extern const char kPassiveEventListenersDueToFlingDescription[];

extern const char kPassiveEventListenerDefaultName[];
extern const char kPassiveEventListenerDefaultDescription[];
extern const char kPassiveEventListenerTrue[];
extern const char kPassiveEventListenerForceAllTrue[];

#if defined(OS_ANDROID)

extern const char kImportantSitesInCbdName[];
extern const char kImportantSitesInCbdDescription[];

#endif  // defined(OS_ANDROID)

#if defined(USE_ASH)

extern const char kAshShelfColor[];
extern const char kAshShelfColorDescription[];

extern const char kAshShelfColorScheme[];
extern const char kAshShelfColorSchemeDescription[];
extern const char kAshShelfColorSchemeLightVibrant[];
extern const char kAshShelfColorSchemeNormalVibrant[];
extern const char kAshShelfColorSchemeDarkVibrant[];
extern const char kAshShelfColorSchemeLightMuted[];
extern const char kAshShelfColorSchemeNormalMuted[];
extern const char kAshShelfColorSchemeDarkMuted[];

extern const char kAshMaximizeModeWindowBackdropName[];
extern const char kAshMaximizeModeWindowBackdropDescription[];

extern const char kAshScreenOrientationLockName[];
extern const char kAshScreenOrientationLockDescription[];

extern const char kAshEnableMirroredScreenName[];
extern const char kAshEnableMirroredScreenDescription[];

extern const char kAshDisableSmoothScreenRotationName[];
extern const char kAshDisableSmoothScreenRotationDescription[];

extern const char kMaterialDesignInkDropAnimationSpeedName[];
extern const char kMaterialDesignInkDropAnimationSpeedDescription[];
extern const char kMaterialDesignInkDropAnimationFast[];
extern const char kMaterialDesignInkDropAnimationSlow[];

extern const char kUiSlowAnimationsName[];
extern const char kUiSlowAnimationsDescription[];

extern const char kUiShowCompositedLayerBordersName[];
extern const char kUiShowCompositedLayerBordersDescription[];
extern const char kUiShowCompositedLayerBordersRenderPass[];
extern const char kUiShowCompositedLayerBordersSurface[];
extern const char kUiShowCompositedLayerBordersLayer[];
extern const char kUiShowCompositedLayerBordersAll[];

#endif  // defined(USE_ASH)

extern const char kJavascriptHarmonyShippingName[];
extern const char kJavascriptHarmonyShippingDescription[];

extern const char kJavascriptHarmonyName[];
extern const char kJavascriptHarmonyDescription[];

extern const char kV8FutureName[];
extern const char kV8FutureDescription[];

extern const char kV8DisableIgnitionTurboName[];
extern const char kV8DisableIgnitionTurboDescription[];

extern const char kEnableAsmWasmName[];
extern const char kEnableAsmWasmDescription[];

extern const char kEnableSharedArrayBufferName[];
extern const char kEnableSharedArrayBufferDescription[];

extern const char kEnableWasmName[];
extern const char kEnableWasmDescription[];

extern const char kEnableWasmStreamingName[];
extern const char kEnableWasmStreamingDescription[];

#if defined(OS_ANDROID)

extern const char kMediaDocumentDownloadButtonName[];
extern const char kMediaDocumentDownloadButtonDescription[];

#endif  // defined(OS_ANDROID)

extern const char kSoftwareRasterizerName[];
extern const char kSoftwareRasterizerDescription[];

extern const char kGpuRasterizationName[];
extern const char kGpuRasterizationDescription[];
extern const char kForceGpuRasterization[];

extern const char kGpuRasterizationMsaaSampleCountName[];
extern const char kGpuRasterizationMsaaSampleCountDescription[];
extern const char kGpuRasterizationMsaaSampleCountZero[];
extern const char kGpuRasterizationMsaaSampleCountTwo[];
extern const char kGpuRasterizationMsaaSampleCountFour[];
extern const char kGpuRasterizationMsaaSampleCountEight[];
extern const char kGpuRasterizationMsaaSampleCountSixteen[];

extern const char kSlimmingPaintInvalidationName[];
extern const char kSlimmingPaintInvalidationDescription[];

extern const char kExperimentalSecurityFeaturesName[];
extern const char kExperimentalSecurityFeaturesDescription[];

extern const char kExperimentalWebPlatformFeaturesName[];
extern const char kExperimentalWebPlatformFeaturesDescription[];

extern const char kExperimentalPointerEventName[];
extern const char kExperimentalPointerEventDescription[];

extern const char kOriginTrialsName[];
extern const char kOriginTrialsDescription[];

extern const char kBleAdvertisingInExtensionsName[];
extern const char kBleAdvertisingInExtensionsDescription[];

extern const char kDevtoolsExperimentsName[];
extern const char kDevtoolsExperimentsDescription[];

extern const char kSilentDebuggerExtensionApiName[];
extern const char kSilentDebuggerExtensionApiDescription[];

extern const char kShowTouchHudName[];
extern const char kShowTouchHudDescription[];

extern const char kPreferHtmlOverPluginsName[];
extern const char kPreferHtmlOverPluginsDescription[];

extern const char kAllowNaclSocketApiName[];
extern const char kAllowNaclSocketApiDescription[];

extern const char kRunAllFlashInAllowModeName[];
extern const char kRunAllFlashInAllowModeDescription[];

extern const char kPinchScaleName[];
extern const char kPinchScaleDescription[];

extern const char kReducedReferrerGranularityName[];
extern const char kReducedReferrerGranularityDescription[];

#if defined(OS_CHROMEOS)

extern const char kUseMashName[];
extern const char kUseMashDescription[];

extern const char kAllowTouchpadThreeFingerClickName[];
extern const char kAllowTouchpadThreeFingerClickDescription[];

extern const char kAshEnableUnifiedDesktopName[];
extern const char kAshEnableUnifiedDesktopDescription[];

extern const char kBootAnimationName[];
extern const char kBootAnimationDescription[];

extern const char kTetherName[];
extern const char kTetherDescription[];

extern const char kCrOSComponentName[];
extern const char kCrOSComponentDescription[];

#endif  // defined(OS_CHROMEOS)

extern const char kAcceleratedVideoDecodeName[];
extern const char kAcceleratedVideoDecodeDescription[];

extern const char kEnableHDRName[];
extern const char kEnableHDRDescription[];

extern const char kCloudImportName[];
extern const char kCloudImportDescription[];

extern const char kRequestTabletSiteName[];
extern const char kRequestTabletSiteDescription[];

extern const char kDebugPackedAppName[];
extern const char kDebugPackedAppDescription[];

extern const char kDropSyncCredentialName[];
extern const char kDropSyncCredentialDescription[];

extern const char kPasswordGenerationName[];
extern const char kPasswordGenerationDescription[];

extern const char kPasswordForceSavingName[];
extern const char kPasswordForceSavingDescription[];

extern const char kManualPasswordGenerationName[];
extern const char kManualPasswordGenerationDescription[];

extern const char kShowAutofillSignaturesName[];
extern const char kShowAutofillSignaturesDescription[];

extern const char kSuggestionsWithSubStringMatchName[];
extern const char kSuggestionsWithSubStringMatchDescription[];

extern const char kAffiliationBasedMatchingName[];
extern const char kAffiliationBasedMatchingDescription[];

extern const char kProtectSyncCredentialName[];
extern const char kProtectSyncCredentialDescription[];

extern const char kPasswordImportExportName[];
extern const char kPasswordImportExportDescription[];

extern const char kProtectSyncCredentialOnReauthName[];
extern const char kProtectSyncCredentialOnReauthDescription[];

extern const char kIconNtpName[];
extern const char kIconNtpDescription[];

extern const char kPushApiBackgroundModeName[];
extern const char kPushApiBackgroundModeDescription[];

extern const char kEnableNavigationTracingName[];
extern const char kEnableNavigationTracingDescription[];

extern const char kTraceUploadUrlName[];
extern const char kTraceUploadUrlDescription[];
extern const char kTraceUploadUrlChoiceOther[];
extern const char kTraceUploadUrlChoiceEmloading[];
extern const char kTraceUploadUrlChoiceQa[];
extern const char kTraceUploadUrlChoiceTesting[];

extern const char kDisableAudioForDesktopShareName[];
extern const char kDisableAudioForDesktopShareDescription[];

extern const char kDisableTabForDesktopShareName[];
extern const char kDisableTabForDesktopShareDescription[];

extern const char kSupervisedUserManagedBookmarksFolderName[];
extern const char kSupervisedUserManagedBookmarksFolderDescription[];

extern const char kSyncAppListName[];
extern const char kSyncAppListDescription[];

extern const char kDriveSearchInChromeLauncherName[];
extern const char kDriveSearchInChromeLauncherDescription[];

extern const char kV8CacheOptionsName[];
extern const char kV8CacheOptionsDescription[];
extern const char kV8CacheOptionsParse[];
extern const char kV8CacheOptionsCode[];

extern const char kV8CacheStrategiesForCacheStorageName[];
extern const char kV8CacheStrategiesForCacheStorageDescription[];
extern const char kV8CacheStrategiesForCacheStorageNormal[];
extern const char kV8CacheStrategiesForCacheStorageAggressive[];

extern const char kMemoryCoordinatorName[];
extern const char kMemoryCoordinatorDescription[];

extern const char kServiceWorkerNavigationPreloadName[];
extern const char kServiceWorkerNavigationPreloadDescription[];

extern const char kDataReductionProxyLoFiName[];
extern const char kDataReductionProxyLoFiDescription[];
extern const char kDataReductionProxyLoFiAlwaysOn[];
extern const char kDataReductionProxyLoFiCellularOnly[];
extern const char kDataReductionProxyLoFiDisabled[];

extern const char kDataReductionProxyLoFiSlowConnectionsOnly[];

extern const char kEnableDataReductionProxyLitePageName[];
extern const char kEnableDataReductionProxyLitePageDescription[];

extern const char kDataReductionProxyCarrierTestName[];
extern const char kDataReductionProxyCarrierTestDescription[];

extern const char kEnableDataReductionProxySavingsPromoName[];
extern const char kEnableDataReductionProxySavingsPromoDescription[];

#if defined(OS_ANDROID)

extern const char kEnableDataReductionProxyMainMenuName[];
extern const char kEnableDataReductionProxyMainMenuDescription[];

extern const char kEnableDataReductionProxySiteBreakdownName[];
extern const char kEnableDataReductionProxySiteBreakdownDescription[];

extern const char kEnableOfflinePreviewsName[];
extern const char kEnableOfflinePreviewsDescription[];

extern const char kEnableClientLoFiName[];
extern const char kEnableClientLoFiDescription[];

#endif  // defined(OS_ANDROID)

extern const char kLcdTextName[];
extern const char kLcdTextDescription[];

extern const char kDistanceFieldTextName[];
extern const char kDistanceFieldTextDescription[];

extern const char kZeroCopyName[];
extern const char kZeroCopyDescription[];

extern const char kHideInactiveStackedTabCloseButtonsName[];
extern const char kHideInactiveStackedTabCloseButtonsDescription[];

extern const char kDefaultTileWidthName[];
extern const char kDefaultTileWidthDescription[];
extern const char kDefaultTileWidthShort[];
extern const char kDefaultTileWidthTall[];
extern const char kDefaultTileWidthGrande[];
extern const char kDefaultTileWidthVenti[];

extern const char kDefaultTileHeightName[];
extern const char kDefaultTileHeightDescription[];
extern const char kDefaultTileHeightShort[];
extern const char kDefaultTileHeightTall[];
extern const char kDefaultTileHeightGrande[];
extern const char kDefaultTileHeightVenti[];

extern const char kNumRasterThreadsName[];
extern const char kNumRasterThreadsDescription[];
extern const char kNumRasterThreadsOne[];
extern const char kNumRasterThreadsTwo[];
extern const char kNumRasterThreadsThree[];
extern const char kNumRasterThreadsFour[];

extern const char kResetAppListInstallStateName[];
extern const char kResetAppListInstallStateDescription[];

#if defined(OS_CHROMEOS)

extern const char kFirstRunUiTransitionsName[];
extern const char kFirstRunUiTransitionsDescription[];

#endif  // defined(OS_CHROMEOS)

extern const char kNewBookmarkAppsName[];
extern const char kNewBookmarkAppsDescription[];

#if defined(OS_MACOSX)

extern const char kHostedAppsInWindowsName[];
extern const char kHostedAppsInWindowsDescription[];

extern const char kTabDetachingInFullscreenName[];
extern const char kTabDetachingInFullscreenDescription[];

extern const char kFullscreenToolbarRevealName[];
extern const char kFullscreenToolbarRevealDescription[];

extern const char kTabStripKeyboardFocusName[];
extern const char kTabStripKeyboardFocusDescription[];

#endif  // defined(OS_MACOSX)

extern const char kHostedAppShimCreationName[];
extern const char kHostedAppShimCreationDescription[];

extern const char kHostedAppQuitNotificationName[];
extern const char kHostedAppQuitNotificationDescription[];

#if defined(OS_ANDROID)

extern const char kPullToRefreshEffectName[];
extern const char kPullToRefreshEffectDescription[];

extern const char kTranslateCompactUIName[];
extern const char kTranslateCompactUIDescription[];

#endif  // defined(OS_ANDROID)

#if defined(OS_MACOSX)

extern const char kTranslateNewUxName[];
extern const char kTranslateNewUxDescription[];

#endif  // defined(OS_MACOSX)

extern const char kTranslate2016q2UiName[];
extern const char kTranslate2016q2UiDescription[];

extern const char kTranslateLanguageByUlpName[];
extern const char kTranslateLanguageByUlpDescription[];

extern const char kViewsRectBasedTargetingName[];
extern const char kViewsRectBasedTargetingDescription[];

extern const char kPermissionActionReportingName[];
extern const char kPermissionActionReportingDescription[];

extern const char kPermissionsBlacklistName[];
extern const char kPermissionsBlacklistDescription[];

extern const char kThreadedScrollingName[];
extern const char kThreadedScrollingDescription[];

extern const char kHarfbuzzRendertextName[];
extern const char kHarfbuzzRendertextDescription[];

extern const char kEmbeddedExtensionOptionsName[];
extern const char kEmbeddedExtensionOptionsDescription[];

extern const char kTabAudioMutingName[];
extern const char kTabAudioMutingDescription[];

extern const char kEasyUnlockBluetoothLowEnergyDiscoveryName[];
extern const char kEasyUnlockBluetoothLowEnergyDiscoveryDescription[];

extern const char kEasyUnlockProximityDetectionName[];
extern const char kEasyUnlockProximityDetectionDescription[];

extern const char kWifiCredentialSyncName[];
extern const char kWifiCredentialSyncDescription[];

extern const char kSyncSandboxName[];
extern const char kSyncSandboxDescription[];

extern const char kDatasaverPromptName[];
extern const char kDatasaverPromptDescription[];

extern const char kDatasaverPromptDemoMode[];
extern const char kDisableUnifiedMediaPipelineDescription[];

extern const char kTrySupportedChannelLayoutsName[];
extern const char kTrySupportedChannelLayoutsDescription[];

#if defined(OS_MACOSX)

extern const char kAppInfoDialogName[];
extern const char kAppInfoDialogDescription[];

extern const char kMacViewsNativeAppWindowsName[];
extern const char kMacViewsNativeAppWindowsDescription[];

extern const char kMacViewsTaskManagerName[];
extern const char kMacViewsTaskManagerDescription[];

extern const char kAppWindowCyclingName[];
extern const char kAppWindowCyclingDescription[];

#endif  // defined(OS_MACOSX)

#if defined(OS_CHROMEOS)

extern const char kAcceleratedMjpegDecodeName[];
extern const char kAcceleratedMjpegDecodeDescription[];

#endif  // defined(OS_CHROMEOS)

extern const char kSimplifiedFullscreenUiName[];
extern const char kSimplifiedFullscreenUiDescription[];

extern const char kExperimentalKeyboardLockUiName[];
extern const char kExperimentalKeyboardLockUiDescription[];

#if defined(OS_ANDROID)

extern const char kProgressBarAnimationName[];
extern const char kProgressBarAnimationDescription[];
extern const char kProgressBarAnimationLinear[];
extern const char kProgressBarAnimationSmooth[];
extern const char kProgressBarAnimationSmoothIndeterminate[];
extern const char kProgressBarAnimationFastStart[];

extern const char kProgressBarCompletionName[];
extern const char kProgressBarCompletionDescription[];
extern const char kProgressBarCompletionLoadEvent[];
extern const char kProgressBarCompletionResourcesBeforeDcl[];
extern const char kProgressBarCompletionDomContentLoaded[];
extern const char
    kProgressBarCompletionResourcesBeforeDclAndSameOriginIframes[];

#endif  // defined(OS_ANDROID)

extern const char kDisallowDocWrittenScriptsUiName[];
extern const char kDisallowDocWrittenScriptsUiDescription[];

#if defined(OS_WIN)

extern const char kEnableAppcontainerName[];
extern const char kEnableAppcontainerDescription[];

#endif  // defined(OS_WIN)

#if defined(TOOLKIT_VIEWS) || (defined(OS_MACOSX) && !defined(OS_IOS))

extern const char kShowCertLinkOnPageInfoName[];
extern const char kShowCertLinkOnPageInfoDescription[];

#endif  // defined(TOOLKIT_VIEWS) || (defined(OS_MACOSX) && !defined(OS_IOS))

#if defined(TOOLKIT_VIEWS) || defined(OS_ANDROID)

extern const char kAutofillCreditCardUploadName[];
extern const char kAutofillCreditCardUploadDescription[];

#endif  // defined(TOOLKIT_VIEWS) || defined(OS_ANDROID)

extern const char kForceUiDirectionName[];
extern const char kForceUiDirectionDescription[];

extern const char kForceTextDirectionName[];
extern const char kForceTextDirectionDescription[];
extern const char kForceDirectionLtr[];
extern const char kForceDirectionRtl[];

#if defined(OS_WIN) || defined(OS_LINUX)

extern const char kEnableInputImeApiName[];
extern const char kEnableInputImeApiDescription[];

#endif  // defined(OS_WIN) || defined(OS_LINUX)

extern const char kEnableGroupedHistoryName[];
extern const char kEnableGroupedHistoryDescription[];
extern const char kSecurityChipDefault[];
extern const char kSecurityChipShowNonsecureOnly[];
extern const char kSecurityChipShowAll[];
extern const char kSecurityChipAnimationDefault[];
extern const char kSecurityChipAnimationNone[];
extern const char kSecurityChipAnimationNonsecureOnly[];
extern const char kSecurityChipAnimationAll[];

extern const char kSaveasMenuLabelExperimentName[];
extern const char kSaveasMenuLabelExperimentDescription[];

extern const char kEnableEnumeratingAudioDevicesName[];
extern const char kEnableEnumeratingAudioDevicesDescription[];

extern const char kNewUsbBackendName[];
extern const char kNewUsbBackendDescription[];

extern const char kNewOmniboxAnswerTypesName[];
extern const char kNewOmniboxAnswerTypesDescription[];

extern const char kEnableZeroSuggestRedirectToChromeName[];
extern const char kEnableZeroSuggestRedirectToChromeDescription[];

extern const char kFillOnAccountSelectName[];
extern const char kFillOnAccountSelectDescription[];

extern const char kEnableClearBrowsingDataCountersName[];
extern const char kEnableClearBrowsingDataCountersDescription[];

#if defined(OS_ANDROID)

extern const char kTabsInCbdName[];
extern const char kTabsInCbdDescription[];

#endif  // defined(OS_ANDROID)

extern const char kNotificationsNativeFlagName[];
extern const char kNotificationsNativeFlagDescription[];

#if defined(OS_ANDROID)

extern const char kEnableAndroidSpellcheckerDescription[];
extern const char kEnableAndroidSpellcheckerName[];

#endif  // defined(OS_ANDROID)

extern const char kEnableWebNotificationCustomLayoutsName[];
extern const char kEnableWebNotificationCustomLayoutsDescription[];

extern const char kAccountConsistencyName[];
extern const char kAccountConsistencyDescription[];

extern const char kGoogleProfileInfoName[];
extern const char kGoogleProfileInfoDescription[];

extern const char kOfferStoreUnmaskedWalletCardsName[];
extern const char kOfferStoreUnmaskedWalletCardsDescription[];

extern const char kOfflineAutoReloadName[];
extern const char kOfflineAutoReloadDescription[];

extern const char kOfflineAutoReloadVisibleOnlyName[];
extern const char kOfflineAutoReloadVisibleOnlyDescription[];

extern const char kShowSavedCopyName[];
extern const char kShowSavedCopyDescription[];
extern const char kEnableShowSavedCopyPrimary[];
extern const char kEnableShowSavedCopySecondary[];
extern const char kDisableShowSavedCopy[];

#if defined(OS_CHROMEOS)

extern const char kSmartVirtualKeyboardName[];
extern const char kSmartVirtualKeyboardDescription[];

extern const char kVirtualKeyboardName[];
extern const char kVirtualKeyboardDescription[];

extern const char kVirtualKeyboardOverscrollName[];
extern const char kVirtualKeyboardOverscrollDescription[];

extern const char kInputViewName[];
extern const char kInputViewDescription[];

extern const char kNewKoreanImeName[];
extern const char kNewKoreanImeDescription[];

extern const char kPhysicalKeyboardAutocorrectName[];
extern const char kPhysicalKeyboardAutocorrectDescription[];

extern const char kVoiceInputName[];
extern const char kVoiceInputDescription[];

extern const char kExperimentalInputViewFeaturesName[];
extern const char kExperimentalInputViewFeaturesDescription[];

extern const char kFloatingVirtualKeyboardName[];
extern const char kFloatingVirtualKeyboardDescription[];

extern const char kGestureTypingName[];
extern const char kGestureTypingDescription[];

extern const char kGestureEditingName[];
extern const char kGestureEditingDescription[];

extern const char kCaptivePortalBypassProxyName[];
extern const char kCaptivePortalBypassProxyDescription[];

extern const char kTouchscreenCalibrationName[];
extern const char kTouchscreenCalibrationDescription[];

extern const char kTeamDrivesName[];
extern const char kTeamDrivesDescription[];

#endif  // defined(OS_CHROMEOS)

extern const char kCreditCardAssistName[];
extern const char kCreditCardAssistDescription[];

extern const char kSimpleCacheBackendName[];
extern const char kSimpleCacheBackendDescription[];

extern const char kSpellingFeedbackFieldTrialName[];
extern const char kSpellingFeedbackFieldTrialDescription[];

extern const char kWebMidiName[];
extern const char kWebMidiDescription[];

extern const char kSitePerProcessName[];
extern const char kSitePerProcessDescription[];

extern const char kTopDocumentIsolationName[];
extern const char kTopDocumentIsolationDescription[];

extern const char kCrossProcessGuestViewIsolationName[];
extern const char kCrossProcessGuestViewIsolationDescription[];

extern const char kBrowserTaskSchedulerName[];
extern const char kBrowserTaskSchedulerDescription[];

#if defined(OS_CHROMEOS)

extern const char kArcUseAuthEndpointName[];
extern const char kArcUseAuthEndpointDescription[];

#endif  // defined(OS_CHROMEOS)

extern const char kSingleClickAutofillName[];
extern const char kSingleClickAutofillDescription[];

#if defined(OS_ANDROID)

extern const char kAutofillAccessoryViewName[];
extern const char kAutofillAccessoryViewDescription[];

#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)

extern const char kReaderModeHeuristicsName[];
extern const char kReaderModeHeuristicsDescription[];
extern const char kReaderModeHeuristicsMarkup[];
extern const char kReaderModeHeuristicsAdaboost[];
extern const char kReaderModeHeuristicsAlwaysOff[];
extern const char kReaderModeHeuristicsAlwaysOn[];

#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)

extern const char kChromeHomeName[];
extern const char kChromeHomeDescription[];

extern const char kChromeHomeExpandButtonName[];
extern const char kChromeHomeExpandButtonDescription[];

#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)

extern const char kEnableIphDemoModeName[];
extern const char kEnableIphDemoModeDescription[];

#endif  // defined(OS_ANDROID)

extern const char kSettingsWindowName[];
extern const char kSettingsWindowDescription[];

#if defined(OS_ANDROID)

extern const char kSeccompFilterSandboxAndroidName[];
extern const char kSeccompFilterSandboxAndroidDescription[];

#endif  // defined(OS_ANDROID)

extern const char kExtensionContentVerificationName[];
extern const char kExtensionContentVerificationDescription[];
extern const char kExtensionContentVerificationBootstrap[];
extern const char kExtensionContentVerificationEnforce[];
extern const char kExtensionContentVerificationEnforceStrict[];

extern const char kExperimentalHotwordHardwareName[];
extern const char kExperimentalHotwordHardwareDescription[];

extern const char kMessageCenterAlwaysScrollUpUponRemovalName[];
extern const char kMessageCenterAlwaysScrollUpUponRemovalDescription[];

extern const char kCastStreamingHwEncodingName[];
extern const char kCastStreamingHwEncodingDescription[];

extern const char kAllowInsecureLocalhostName[];
extern const char kAllowInsecureLocalhostDescription[];

#if defined(OS_WIN) || defined(OS_MACOSX)

extern const char kAutomaticTabDiscardingName[];
extern const char kAutomaticTabDiscardingDescription[];

#endif  // defined(OS_WIN) || defined(OS_MACOSX)

#if defined(OS_ANDROID)

extern const char kOfflineBookmarksName[];
extern const char kOfflineBookmarksDescription[];

extern const char kNtpOfflinePagesName[];
extern const char kNtpOfflinePagesDescription[];

extern const char kOfflinePagesAsyncDownloadName[];
extern const char kOfflinePagesAsyncDownloadDescription[];

extern const char kOfflinePagesSvelteConcurrentLoadingName[];
extern const char kOfflinePagesSvelteConcurrentLoadingDescription[];

extern const char kOfflinePagesLoadSignalCollectingName[];
extern const char kOfflinePagesLoadSignalCollectingDescription[];

extern const char kOfflinePagesPrefetchingName[];
extern const char kOfflinePagesPrefetchingDescription[];

extern const char kOfflinePagesSharingName[];
extern const char kOfflinePagesSharingDescription[];

extern const char kBackgroundLoaderForDownloadsName[];
extern const char kBackgroundLoaderForDownloadsDescription[];

extern const char kNewBackgroundLoaderName[];
extern const char kNewBackgroundLoaderDescription[];

extern const char kNtpPopularSitesName[];
extern const char kNtpPopularSitesDescription[];

extern const char kNtpSwitchToExistingTabName[];
extern const char kNtpSwitchToExistingTabDescription[];
extern const char kNtpSwitchToExistingTabMatchUrl[];
extern const char kNtpSwitchToExistingTabMatchHost[];

extern const char kUseAndroidMidiApiName[];
extern const char kUseAndroidMidiApiDescription[];

extern const char kWebPaymentsModifiersName[];
extern const char kWebPaymentsModifiersDescription[];

#endif  // defined(OS_ANDROID)

#if defined(OS_WIN)

extern const char kTraceExportEventsToEtwName[];
extern const char kTraceExportEventsToEtwDesription[];

extern const char kMergeKeyCharEventsName[];
extern const char kMergeKeyCharEventsDescription[];

extern const char kUseWinrtMidiApiName[];
extern const char kUseWinrtMidiApiDescription[];

#endif  // defined(OS_WIN)

#if defined(OS_ANDROID)

extern const char kUpdateMenuItemName[];
extern const char kUpdateMenuItemDescription[];

extern const char kUpdateMenuItemCustomSummaryDescription[];
extern const char kUpdateMenuItemCustomSummaryName[];

extern const char kUpdateMenuBadgeName[];
extern const char kUpdateMenuBadgeDescription[];

extern const char kSetMarketUrlForTestingName[];
extern const char kSetMarketUrlForTestingDescription[];

#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)

extern const char kHerbPrototypeChoicesName[];
extern const char kHerbPrototypeChoicesDescription[];
extern const char kHerbPrototypeFlavorElderberry[];

extern const char kEnableSpecialLocaleName[];
extern const char kEnableSpecialLocaleDescription[];

extern const char kEnableWebapk[];
extern const char kEnableWebapkDescription[];

#endif  // defined(OS_ANDROID)

extern const char kEnableBrotliName[];
extern const char kEnableBrotliDescription[];

extern const char kEnableWebfontsInterventionName[];
extern const char kEnableWebfontsInterventionDescription[];
extern const char kEnableWebfontsInterventionV2ChoiceDefault[];
extern const char kEnableWebfontsInterventionV2ChoiceEnabledWith2g[];
extern const char kEnableWebfontsInterventionV2ChoiceEnabledWith3g[];
extern const char kEnableWebfontsInterventionV2ChoiceEnabledWithSlow2g[];
extern const char kEnableWebfontsInterventionV2ChoiceDisabled[];

extern const char kEnableWebfontsInterventionTriggerName[];
extern const char kEnableWebfontsInterventionTriggerDescription[];

extern const char kEnableScrollAnchoringName[];
extern const char kEnableScrollAnchoringDescription[];

#if defined(OS_CHROMEOS)

extern const char kDisableNativeCupsName[];
extern const char kDisableNativeCupsDescription[];

extern const char kEnableAndroidWallpapersAppName[];
extern const char kEnableAndroidWallpapersAppDescription[];

extern const char kEnableTouchSupportForScreenMagnifierName[];
extern const char kEnableTouchSupportForScreenMagnifierDescription[];

extern const char kEnableZipArchiverOnFileManagerName[];
extern const char kEnableZipArchiverOnFileManagerDescription[];

#endif  // defined(OS_CHROMEOS)

#if defined(OS_ANDROID)

extern const char kContentSuggestionsCategoryOrderName[];
extern const char kContentSuggestionsCategoryOrderDescription[];

extern const char kContentSuggestionsCategoryRankerName[];
extern const char kContentSuggestionsCategoryRankerDescription[];

extern const char kEnableNtpSnippetsVisibilityName[];
extern const char kEnableNtpSnippetsVisibilityDescription[];

extern const char kEnableContentSuggestionsNewFaviconServerName[];
extern const char kEnableContentSuggestionsNewFaviconServerDescription[];

extern const char kEnableNtpMostLikelyFaviconsFromServerName[];
extern const char kEnableNtpMostLikelyFaviconsFromServerDescription[];

extern const char kEnableContentSuggestionsSettingsName[];
extern const char kEnableContentSuggestionsSettingsDescription[];

extern const char kEnableContentSuggestionsShowSummaryName[];
extern const char kEnableContentSuggestionsShowSummaryDescription[];

extern const char kEnableNtpRemoteSuggestionsName[];
extern const char kEnableNtpRemoteSuggestionsDescription[];

extern const char kEnableNtpRecentOfflineTabSuggestionsName[];
extern const char kEnableNtpRecentOfflineTabSuggestionsDescription[];

extern const char kEnableNtpAssetDownloadSuggestionsName[];
extern const char kEnableNtpAssetDownloadSuggestionsDescription[];

extern const char kEnableNtpOfflinePageDownloadSuggestionsName[];
extern const char kEnableNtpOfflinePageDownloadSuggestionsDescription[];

extern const char kEnableNtpBookmarkSuggestionsName[];
extern const char kEnableNtpBookmarkSuggestionsDescription[];

extern const char kEnableNtpPhysicalWebPageSuggestionsName[];
extern const char kEnableNtpPhysicalWebPageSuggestionsDescription[];

extern const char kEnableNtpForeignSessionsSuggestionsName[];
extern const char kEnableNtpForeignSessionsSuggestionsDescription[];

extern const char kEnableNtpSuggestionsNotificationsName[];
extern const char kEnableNtpSuggestionsNotificationsDescription[];

extern const char kNtpCondensedLayoutName[];
extern const char kNtpCondensedLayoutDescription[];

extern const char kNtpCondensedTileLayoutName[];
extern const char kNtpCondensedTileLayoutDescription[];

extern const char kNtpGoogleGInOmniboxName[];
extern const char kNtpGoogleGInOmniboxDescription[];

#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)

extern const char kOffliningRecentPagesName[];
extern const char kOffliningRecentPagesDescription[];

extern const char kOfflinePagesCtName[];
extern const char kOfflinePagesCtDescription[];

#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)

extern const char kEnableExpandedAutofillCreditCardPopupLayoutName[];
extern const char kEnableExpandedAutofillCreditCardPopupLayoutDescription[];

#endif  // defined(OS_ANDROID)

extern const char kEnableAutofillCreditCardLastUsedDateDisplayName[];
extern const char kEnableAutofillCreditCardLastUsedDateDisplayDescription[];

extern const char kEnableAutofillCreditCardUploadCvcPromptName[];
extern const char kEnableAutofillCreditCardUploadCvcPromptDescription[];

#if !defined(OS_ANDROID) && defined(GOOGLE_CHROME_BUILD)

extern const char kGoogleBrandedContextMenuName[];
extern const char kGoogleBrandedContextMenuDescription[];

#endif  // !defined(OS_ANDROID) && defined(GOOGLE_CHROME_BUILD)

extern const char kEnableWebUsbName[];
extern const char kEnableWebUsbDescription[];

extern const char kEnableGenericSensorName[];
extern const char kEnableGenericSensorDescription[];

extern const char kFontCacheScalingName[];
extern const char kFontCacheScalingDescription[];

extern const char kFramebustingName[];
extern const char kFramebustingDescription[];

#if defined(OS_ANDROID)

extern const char kEnableVrShellName[];
extern const char kEnableVrShellDescription[];

#endif  // defined(OS_ANDROID)

extern const char kWebPaymentsName[];
extern const char kWebPaymentsDescription[];

#if defined(OS_ANDROID)

extern const char kEnableAndroidPayIntegrationV1Name[];
extern const char kEnableAndroidPayIntegrationV1Description[];

extern const char kEnableAndroidPayIntegrationV2Name[];
extern const char kEnableAndroidPayIntegrationV2Description[];

extern const char kEnableWebPaymentsSingleAppUiSkipName[];
extern const char kEnableWebPaymentsSingleAppUiSkipDescription[];

extern const char kAndroidPaymentAppsName[];
extern const char kAndroidPaymentAppsDescription[];

extern const char kServiceWorkerPaymentAppsName[];
extern const char kServiceWorkerPaymentAppsDescription[];

#endif  // defined(OS_ANDROID)

extern const char kFeaturePolicyName[];
extern const char kFeaturePolicyDescription[];

extern const char kNewAudioRenderingMixingStrategyName[];
extern const char kNewAudioRenderingMixingStrategyDescription[];

extern const char kBackgroundVideoTrackOptimizationName[];
extern const char kBackgroundVideoTrackOptimizationDescription[];

extern const char kNewRemotePlaybackPipelineName[];
extern const char kNewRemotePlaybackPipelineDescription[];

extern const char kVideoFullscreenOrientationLockName[];
extern const char kVideoFullscreenOrientationLockDescription[];

extern const char kVideoRotateToFullscreenName[];
extern const char kVideoRotateToFullscreenDescription[];

extern const char kExpensiveBackgroundTimerThrottlingName[];
extern const char kExpensiveBackgroundTimerThrottlingDescription[];

#if !defined(OS_ANDROID)

extern const char kEnableAudioFocusName[];
extern const char kEnableAudioFocusDescription[];
extern const char kEnableAudioFocusDisabled[];
extern const char kEnableAudioFocusEnabled[];
extern const char kEnableAudioFocusEnabledDuckFlash[];

#endif  // !defined(OS_ANDROID)

#if defined(OS_WIN)

extern const char kGdiTextPrinting[];
extern const char kGdiTextPrintingDescription[];

#endif  // defined(OS_WIN)

#if defined(OS_ANDROID)

extern const char kModalPermissionPromptsName[];
extern const char kModalPermissionPromptsDescription[];

#endif  // defined(OS_ANDROID)

#if !defined(OS_MACOSX)

extern const char kPermissionPromptPersistenceToggleName[];
extern const char kPermissionPromptPersistenceToggleDescription[];

#endif  // !defined(OS_MACOSX)

#if defined(OS_ANDROID)

extern const char kNoCreditCardAbort[];
extern const char kNoCreditCardAbortDescription[];

#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)

extern const char kEnableConsistentOmniboxGeolocationName[];
extern const char kEnableConsistentOmniboxGeolocationDescription[];

#endif  // defined(OS_ANDROID)

extern const char kMediaRemotingName[];
extern const char kMediaRemotingDescription[];

extern const char kCrosCompUpdatesName[];
extern const char kCrosCompUpdatesDescription[];

#if defined(OS_ANDROID)

extern const char kLsdPermissionPromptName[];
extern const char kLsdPermissionPromptDescription[];

#endif  // defined(OS_ANDROID)

#if defined(OS_WIN)

extern const char kWindows10CustomTitlebarName[];
extern const char kWindows10CustomTitlebarDescription[];

#endif  // defined(OS_WIN)

#if defined(OS_WIN)

extern const char kDisablePostscriptPrinting[];
extern const char kDisablePostscriptPrintingDescription[];

#endif  // defined(OS_WIN)

#if defined(OS_ANDROID)

extern const char kAiaFetchingName[];
extern const char kAiaFetchingDescription[];

#endif  // defined(OS_ANDROID)

extern const char kEnableMidiManagerDynamicInstantiationName[];
extern const char kEnableMidiManagerDynamicInstantiationDescription[];

#if defined(OS_WIN)

extern const char kEnableDesktopIosPromotionsName[];
extern const char kEnableDesktopIosPromotionsDescription[];

#endif  // defined(OS_WIN)

#if defined(OS_ANDROID)

extern const char kEnableCustomFeedbackUiName[];
extern const char kEnableCustomFeedbackUiDescription[];

#endif  // defined(OS_ANDROID)

extern const char kEnableAdjustableLargeCursorName[];
extern const char kEnableAdjustableLargeCursorDescription[];

#if defined(OS_CHROMEOS) || defined(OS_LINUX) || defined(OS_MACOSX) || \
    defined(OS_WIN)

extern const char kOmniboxEntitySuggestionsName[];
extern const char kOmniboxEntitySuggestionsDescription[];

extern const char kPauseBackgroundTabsName[];
extern const char kPauseBackgroundTabsDescription[];

extern const char kEnableNewAppMenuIconName[];
extern const char kEnableNewAppMenuIconDescription[];

#endif  // defined(OS_CHROMEOS) || defined(OS_LINUX) || defined(OS_MACOSX) ||
        // defined(OS_WIN)

#if defined(OS_CHROMEOS)

extern const char kEnableChromevoxArcSupportName[];
extern const char kEnableChromevoxArcSupportDescription[];

#endif  // defined(OS_CHROMEOS)

extern const char kMojoLoadingName[];
extern const char kMojoLoadingDescription[];

#if defined(OS_ANDROID)

extern const char kUseNewDoodleApiName[];
extern const char kUseNewDoodleApiDescription[];

#endif  // defined(OS_ANDROID)

extern const char kDelayNavigationName[];
extern const char kDelayNavigationDescription[];

extern const char kMemoryAblationName[];
extern const char kMemoryAblationDescription[];

#if defined(OS_ANDROID)

extern const char kEnableCustomContextMenuName[];
extern const char kEnableCustomContextMenuDescription[];

#endif  // defined(OS_ANDROID)

#if defined(OS_CHROMEOS)

extern const char kVideoPlayerChromecastSupportName[];
extern const char kVideoPlayerChromecastSupportDescription[];

extern const char kNewZipUnpackerName[];
extern const char kNewZipUnpackerDescription[];

extern const char kShowArcFilesAppName[];
extern const char kShowArcFilesAppDescription[];

extern const char kOfficeEditingComponentAppName[];
extern const char kOfficeEditingComponentAppDescription[];

extern const char kDisplayColorCalibrationName[];
extern const char kDisplayColorCalibrationDescription[];

extern const char kMemoryPressureThresholdName[];
extern const char kMemoryPressureThresholdDescription[];
extern const char kConservativeThresholds[];
extern const char kAggressiveCacheDiscardThresholds[];
extern const char kAggressiveTabDiscardThresholds[];
extern const char kAggressiveThresholds[];

extern const char kWakeOnPacketsName[];
extern const char kWakeOnPacketsDescription[];

extern const char kQuickUnlockPinName[];
extern const char kQuickUnlockPinDescription[];
extern const char kQuickUnlockPinSignin[];
extern const char kQuickUnlockPinSigninDescription[];
extern const char kQuickUnlockFingerprint[];
extern const char kQuickUnlockFingerprintDescription[];

extern const char kExperimentalAccessibilityFeaturesName[];
extern const char kExperimentalAccessibilityFeaturesDescription[];

extern const char kDisableSystemTimezoneAutomaticDetectionName[];
extern const char kDisableSystemTimezoneAutomaticDetectionDescription[];

extern const char kEolNotificationName[];
extern const char kEolNotificationDescription[];

extern const char kForceEnableStylusToolsName[];
extern const char kForceEnableStylusToolsDescription[];

extern const char kNetworkPortalNotificationName[];
extern const char kNetworkPortalNotificationDescription[];

extern const char kMtpWriteSupportName[];
extern const char kMtpWriteSupportDescription[];

extern const char kCrosRegionsModeName[];
extern const char kCrosRegionsModeDescription[];
extern const char kCrosRegionsModeDefault[];
extern const char kCrosRegionsModeOverride[];
extern const char kCrosRegionsModeHide[];

extern const char kPrinterProviderSearchAppName[];
extern const char kPrinterProviderSearchAppDescription[];

extern const char kArcBootCompleted[];
extern const char kArcBootCompletedDescription[];

extern const char kEnableImeMenuName[];
extern const char kEnableImeMenuDescription[];

extern const char kEnableEhvInputName[];
extern const char kEnableEhvInputDescription[];

extern const char kEnableEncryptionMigrationName[];
extern const char kEnableEncryptionMigrationDescription[];

#endif  // #if defined(OS_CHROMEOS)

#if defined(OS_ANDROID)

extern const char kEnableCopylessPasteName[];
extern const char kEnableCopylessPasteDescription[];

extern const char kEnableWebNfcName[];
extern const char kEnableWebNfcDescription[];

#endif  // defined(OS_ANDROID)

extern const char kEnableIdleTimeSpellCheckingName[];
extern const char kEnableIdleTimeSpellCheckingDescription[];

#if defined(OS_ANDROID)

extern const char kEnableOmniboxClipboardProviderName[];
extern const char kEnableOmniboxClipboardProviderDescription[];

#endif  // defined(OS_ANDROID)

extern const char kAutoplayPolicyName[];
extern const char kAutoplayPolicyDescription[];

extern const char kAutoplayPolicyUserGestureRequiredForCrossOrigin[];
extern const char kAutoplayPolicyNoUserGestureRequired[];
extern const char kAutoplayPolicyUserGestureRequired[];

extern const char kOmniboxDisplayTitleForCurrentUrlName[];
extern const char kOmniboxDisplayTitleForCurrentUrlDescription[];

extern const char kOmniboxUIVerticalMarginName[];
extern const char kOmniboxUIVerticalMarginDescription[];

extern const char kForceEffectiveConnectionTypeName[];
extern const char kForceEffectiveConnectionTypeDescription[];

extern const char kEffectiveConnectionTypeUnknownDescription[];
extern const char kEffectiveConnectionTypeOfflineDescription[];
extern const char kEffectiveConnectionTypeSlow2GDescription[];
extern const char kEffectiveConnectionType2GDescription[];
extern const char kEffectiveConnectionType3GDescription[];
extern const char kEffectiveConnectionType4GDescription[];

extern const char kEnableHeapProfilingName[];
extern const char kEnableHeapProfilingDescription[];
extern const char kEnableHeapProfilingModePseudo[];
extern const char kEnableHeapProfilingModeNative[];
extern const char kEnableHeapProfilingTaskProfiler[];

extern const char kUseSuggestionsEvenIfFewFeatureName[];
extern const char kUseSuggestionsEvenIfFewFeatureDescription[];

extern const char kLocationHardReloadName[];
extern const char kLocationHardReloadDescription[];

extern const char kCaptureThumbnailOnLoadFinishedName[];
extern const char kCaptureThumbnailOnLoadFinishedDescription[];

#if defined(OS_WIN)

extern const char kEnableD3DVsync[];
extern const char kEnableD3DVsyncDescription[];

#endif  // defined(OS_WIN)

#if !defined(OS_ANDROID) && !defined(OS_IOS)

extern const char kUseGoogleLocalNtpName[];
extern const char kUseGoogleLocalNtpDescription[];

extern const char kOneGoogleBarOnLocalNtpName[];
extern const char kOneGoogleBarOnLocalNtpDescription[];

#endif

#if defined(OS_MACOSX)

extern const char kMacRTLName[];
extern const char kMacRTLDescription[];

#endif  // defined(OS_MACOSX)

}  // namespace flag_descriptions

#endif  // CHROME_BROWSER_FLAG_DESCRIPTIONS_H_
