// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FLAG_DESCRIPTIONS_H_
#define CHROME_BROWSER_FLAG_DESCRIPTIONS_H_

// Includes needed for macros allowing conditional compilation of some strings.
#include "base/logging.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/common/features.h"
#include "components/nacl/common/features.h"
#include "device/vr/features/features.h"
#include "media/media_features.h"

// This file declares strings used in chrome://flags. These messages are not
// translated, because instead of end-users they target Chromium developers and
// testers. See https://crbug.com/587272 and https://crbug.com/703134 for more
// details.
//
// Comments are not necessary. The contents of the strings (which appear in the
// UI) should be good enough documentation for what flags do and when they
// apply. If they aren't, fix them.
//
// Sort flags in each section alphabetically by the k...Name constant. Follow
// that by the k...Description constant and any special values associated with
// that.
//
// Put #ifdefed flags in the appropriate section toward the bottom, don't
// intersperse the file with ifdefs.

namespace flag_descriptions {

// Cross-platform -------------------------------------------------------------

extern const char kAccelerated2dCanvasName[];
extern const char kAccelerated2dCanvasDescription[];

extern const char kAcceleratedVideoDecodeName[];
extern const char kAcceleratedVideoDecodeDescription[];

extern const char kAffiliationBasedMatchingName[];
extern const char kAffiliationBasedMatchingDescription[];

extern const char kAllowInsecureLocalhostName[];
extern const char kAllowInsecureLocalhostDescription[];

extern const char kAllowNaclSocketApiName[];
extern const char kAllowNaclSocketApiDescription[];

extern const char kAppBannersName[];
extern const char kAppBannersDescription[];

extern const char kAsyncImageDecodingName[];
extern const char kAsyncImageDecodingDescription[];

extern const char kAutoplayPolicyName[];
extern const char kAutoplayPolicyDescription[];

extern const char kAutoplayPolicyUserGestureRequiredForCrossOrigin[];
extern const char kAutoplayPolicyNoUserGestureRequired[];
extern const char kAutoplayPolicyUserGestureRequired[];
extern const char kAutoplayPolicyDocumentUserActivation[];

extern const char kBackgroundVideoTrackOptimizationName[];
extern const char kBackgroundVideoTrackOptimizationDescription[];

extern const char kBleAdvertisingInExtensionsName[];
extern const char kBleAdvertisingInExtensionsDescription[];

extern const char kBlockTabUndersName[];
extern const char kBlockTabUndersDescription[];

extern const char kBrowserSideNavigationName[];
extern const char kBrowserSideNavigationDescription[];

extern const char kBrowserTaskSchedulerName[];
extern const char kBrowserTaskSchedulerDescription[];

extern const char kBypassAppBannerEngagementChecksName[];
extern const char kBypassAppBannerEngagementChecksDescription[];

extern const char kCaptureThumbnailOnNavigatingAwayName[];
extern const char kCaptureThumbnailOnNavigatingAwayDescription[];

extern const char kCastStreamingHwEncodingName[];
extern const char kCastStreamingHwEncodingDescription[];

extern const char kClickToOpenPDFName[];
extern const char kClickToOpenPDFDescription[];

extern const char kCloudImportName[];
extern const char kCloudImportDescription[];

extern const char kForceColorProfileSRGB[];
extern const char kForceColorProfileP3[];
extern const char kForceColorProfileColorSpin[];
extern const char kForceColorProfileHdr[];

extern const char kForceColorProfileName[];
extern const char kForceColorProfileDescription[];

extern const char kCompositedLayerBordersName[];
extern const char kCompositedLayerBordersDescription[];

extern const char kContextualSuggestionsCarouselName[];
extern const char kContextualSuggestionsCarouselDescription[];

extern const char kCreditCardAssistName[];
extern const char kCreditCardAssistDescription[];

extern const char kCrossProcessGuestViewIsolationName[];
extern const char kCrossProcessGuestViewIsolationDescription[];

extern const char kDataReductionProxyLoFiName[];
extern const char kDataReductionProxyLoFiDescription[];
extern const char kDataReductionProxyLoFiAlwaysOn[];
extern const char kDataReductionProxyLoFiCellularOnly[];
extern const char kDataReductionProxyLoFiDisabled[];
extern const char kDataReductionProxyLoFiSlowConnectionsOnly[];

extern const char kDatasaverPromptName[];
extern const char kDatasaverPromptDescription[];
extern const char kDatasaverPromptDemoMode[];

extern const char kDebugPackedAppName[];
extern const char kDebugPackedAppDescription[];

extern const char kDefaultTileHeightName[];
extern const char kDefaultTileHeightDescription[];
extern const char kDefaultTileHeightShort[];
extern const char kDefaultTileHeightTall[];
extern const char kDefaultTileHeightGrande[];
extern const char kDefaultTileHeightVenti[];

extern const char kDefaultTileWidthName[];
extern const char kDefaultTileWidthDescription[];
extern const char kDefaultTileWidthShort[];
extern const char kDefaultTileWidthTall[];
extern const char kDefaultTileWidthGrande[];
extern const char kDefaultTileWidthVenti[];

extern const char kDebugShortcutsName[];
extern const char kDebugShortcutsDescription[];

extern const char kDeviceDiscoveryNotificationsName[];
extern const char kDeviceDiscoveryNotificationsDescription[];

extern const char kDevtoolsExperimentsName[];
extern const char kDevtoolsExperimentsDescription[];

extern const char kDisableAudioForDesktopShareName[];
extern const char kDisableAudioForDesktopShareDescription[];

extern const char kDisableTabForDesktopShareName[];
extern const char kDisableTabForDesktopShareDescription[];

extern const char kDisallowDocWrittenScriptsUiName[];
extern const char kDisallowDocWrittenScriptsUiDescription[];

extern const char kDisplayList2dCanvasName[];
extern const char kDisplayList2dCanvasDescription[];

extern const char kDistanceFieldTextName[];
extern const char kDistanceFieldTextDescription[];

extern const char kDriveSearchInChromeLauncherName[];
extern const char kDriveSearchInChromeLauncherDescription[];

extern const char kDropSyncCredentialName[];
extern const char kDropSyncCredentialDescription[];

extern const char kEasyUnlockBluetoothLowEnergyDiscoveryName[];
extern const char kEasyUnlockBluetoothLowEnergyDiscoveryDescription[];

extern const char kEasyUnlockPromotionsName[];
extern const char kEasyUnlockPromotionsDescription[];

extern const char kEmbeddedExtensionOptionsName[];
extern const char kEmbeddedExtensionOptionsDescription[];

extern const char kEnableAsmWasmName[];
extern const char kEnableAsmWasmDescription[];

extern const char kEnableAutofillCreditCardAblationExperimentDisplayName[];
extern const char kEnableAutofillCreditCardAblationExperimentDescription[];

extern const char kEnableAutofillCreditCardBankNameDisplayName[];
extern const char kEnableAutofillCreditCardBankNameDisplayDescription[];

extern const char kEnableAutofillCreditCardLastUsedDateDisplayName[];
extern const char kEnableAutofillCreditCardLastUsedDateDisplayDescription[];

extern const char kEnableAutofillCreditCardUploadCvcPromptName[];
extern const char kEnableAutofillCreditCardUploadCvcPromptDescription[];

extern const char kEnableAutofillCreditCardUploadGoogleLogoName[];
extern const char kEnableAutofillCreditCardUploadGoogleLogoDescription[];

extern const char kEnableAutofillCreditCardUploadNewUiName[];
extern const char kEnableAutofillCreditCardUploadNewUiDescription[];

extern const char kEnableAutofillSendBillingCustomerNumberName[];
extern const char kEnableAutofillSendBillingCustomerNumberDescription[];

extern const char kEnableBreakingNewsPushName[];
extern const char kEnableBreakingNewsPushDescription[];

extern const char kEnableBrotliName[];
extern const char kEnableBrotliDescription[];

extern const char kEnableClearBrowsingDataCountersName[];
extern const char kEnableClearBrowsingDataCountersDescription[];

extern const char kEnableClientLoFiName[];
extern const char kEnableClientLoFiDescription[];

extern const char kEnableNoScriptPreviewsName[];
extern const char kEnableNoScriptPreviewsDescription[];

extern const char kEnableDataReductionProxyLitePageName[];
extern const char kEnableDataReductionProxyLitePageDescription[];

extern const char kDataReductionProxyServerAlternative[];
extern const char kEnableDataReductionProxyServerExperimentName[];
extern const char kEnableDataReductionProxyServerExperimentDescription[];

extern const char kEnableDataReductionProxySavingsPromoName[];
extern const char kEnableDataReductionProxySavingsPromoDescription[];

extern const char kEnableDesktopPWAWindowingName[];
extern const char kEnableDesktopPWAWindowingDescription[];

extern const char kEnableEnumeratingAudioDevicesName[];
extern const char kEnableEnumeratingAudioDevicesDescription[];

extern const char kEnableGenericSensorName[];
extern const char kEnableGenericSensorDescription[];

extern const char kEnableGenericSensorExtraClassesName[];
extern const char kEnableGenericSensorExtraClassesDescription[];

extern const char kEnableHDRName[];
extern const char kEnableHDRDescription[];

extern const char kEnableHeapProfilingName[];
extern const char kEnableHeapProfilingDescription[];
extern const char kEnableHeapProfilingModePseudo[];
extern const char kEnableHeapProfilingModeNative[];
extern const char kEnableHeapProfilingTaskProfiler[];

extern const char kEnableHttpFormWarningName[];
extern const char kEnableHttpFormWarningDescription[];

extern const char kEnableMacMaterialDesignDownloadShelfName[];
extern const char kEnableMacMaterialDesignDownloadShelfDescription[];

extern const char kEnableManualFallbacksFillingName[];
extern const char kEnableManualFallbacksFillingDescription[];

extern const char kEnableMaterialDesignBookmarksName[];
extern const char kEnableMaterialDesignBookmarksDescription[];

extern const char kEnableMaterialDesignExtensionsName[];
extern const char kEnableMaterialDesignExtensionsDescription[];

extern const char kEnablePolicyToolName[];
extern const char kEnablePolicyToolDescription[];

extern const char kEnableMidiManagerDynamicInstantiationName[];
extern const char kEnableMidiManagerDynamicInstantiationDescription[];

extern const char kEnableNavigationTracingName[];
extern const char kEnableNavigationTracingDescription[];

extern const char kEnableNetworkServiceName[];
extern const char kEnableNetworkServiceDescription[];

extern const char kEnableNightLightName[];
extern const char kEnableNightLightDescription[];

extern const char kEnableOutOfBlinkCORSName[];
extern const char kEnableOutOfBlinkCORSDescription[];

extern const char kEnableOutOfProcessHeapProfilingName[];
extern const char kEnableOutOfProcessHeapProfilingDescription[];
extern const char kEnableOutOfProcessHeapProfilingModeMinimal[];
extern const char kEnableOutOfProcessHeapProfilingModeAll[];

extern const char kEnablePictureInPictureName[];
extern const char kEnablePictureInPictureDescription[];

extern const char kEnablePixelCanvasRecordingName[];
extern const char kEnablePixelCanvasRecordingDescription[];

extern const char kEnableTokenBindingName[];
extern const char kEnableTokenBindingDescription[];

extern const char kEnableUsernameCorrectionName[];
extern const char kEnableUsernameCorrectionDescription[];

extern const char kEnableUseZoomForDsfName[];
extern const char kEnableUseZoomForDsfDescription[];
extern const char kEnableUseZoomForDsfChoiceDefault[];
extern const char kEnableUseZoomForDsfChoiceEnabled[];
extern const char kEnableUseZoomForDsfChoiceDisabled[];

extern const char kEnableScrollAnchoringName[];
extern const char kEnableScrollAnchoringDescription[];

extern const char kEnableWasmName[];
extern const char kEnableWasmDescription[];

extern const char kEnableWebUsbName[];
extern const char kEnableWebUsbDescription[];

extern const char kEnableImageCaptureAPIName[];
extern const char kEnableImageCaptureAPIDescription[];

extern const char kEnableZeroSuggestRedirectToChromeName[];
extern const char kEnableZeroSuggestRedirectToChromeDescription[];

extern const char kEnableWasmStreamingName[];
extern const char kEnableWasmStreamingDescription[];

extern const char kEnableWebNotificationCustomLayoutsName[];
extern const char kEnableWebNotificationCustomLayoutsDescription[];

extern const char kExpensiveBackgroundTimerThrottlingName[];
extern const char kExpensiveBackgroundTimerThrottlingDescription[];

extern const char kExperimentalAppBannersName[];
extern const char kExperimentalAppBannersDescription[];

extern const char kExperimentalCanvasFeaturesName[];
extern const char kExperimentalCanvasFeaturesDescription[];

extern const char kExperimentalExtensionApisName[];
extern const char kExperimentalExtensionApisDescription[];

extern const char kExperimentalFullscreenExitUIName[];
extern const char kExperimentalFullscreenExitUIDescription[];

extern const char kExperimentalHotwordHardwareName[];
extern const char kExperimentalHotwordHardwareDescription[];

extern const char kExperimentalKeyboardLockUiName[];
extern const char kExperimentalKeyboardLockUiDescription[];

extern const char kExperimentalSecurityFeaturesName[];
extern const char kExperimentalSecurityFeaturesDescription[];

extern const char kExperimentalVRFeaturesName[];
extern const char kExperimentalVRFeaturesDescription[];

extern const char kExperimentalWebPlatformFeaturesName[];
extern const char kExperimentalWebPlatformFeaturesDescription[];

extern const char kExtensionContentVerificationName[];
extern const char kExtensionContentVerificationDescription[];
extern const char kExtensionContentVerificationBootstrap[];
extern const char kExtensionContentVerificationEnforce[];
extern const char kExtensionContentVerificationEnforceStrict[];

extern const char kExtensionsOnChromeUrlsName[];
extern const char kExtensionsOnChromeUrlsDescription[];

extern const char kFastUnloadName[];
extern const char kFastUnloadDescription[];

extern const char kFeaturePolicyName[];
extern const char kFeaturePolicyDescription[];

extern const char kFetchKeepaliveTimeoutSettingName[];
extern const char kFetchKeepaliveTimeoutSettingDescription[];

extern const char kFontCacheScalingName[];
extern const char kFontCacheScalingDescription[];

extern const char kForceEffectiveConnectionTypeName[];
extern const char kForceEffectiveConnectionTypeDescription[];
extern const char kEffectiveConnectionTypeUnknownDescription[];
extern const char kEffectiveConnectionTypeOfflineDescription[];
extern const char kEffectiveConnectionTypeSlow2GDescription[];
extern const char kEffectiveConnectionType2GDescription[];
extern const char kEffectiveConnectionType3GDescription[];
extern const char kEffectiveConnectionType4GDescription[];

extern const char kFillOnAccountSelectName[];
extern const char kFillOnAccountSelectDescription[];

extern const char kForceTextDirectionName[];
extern const char kForceTextDirectionDescription[];
extern const char kForceDirectionLtr[];
extern const char kForceDirectionRtl[];

extern const char kForceUiDirectionName[];
extern const char kForceUiDirectionDescription[];

extern const char kFramebustingName[];
extern const char kFramebustingDescription[];

extern const char kGamepadExtensionsName[];
extern const char kGamepadExtensionsDescription[];

extern const char kGlCompositedOverlayCandidateQuadBordersName[];
extern const char kGlCompositedOverlayCandidateQuadBordersDescription[];

extern const char kGpuRasterizationMsaaSampleCountName[];
extern const char kGpuRasterizationMsaaSampleCountDescription[];
extern const char kGpuRasterizationMsaaSampleCountZero[];
extern const char kGpuRasterizationMsaaSampleCountTwo[];
extern const char kGpuRasterizationMsaaSampleCountFour[];
extern const char kGpuRasterizationMsaaSampleCountEight[];
extern const char kGpuRasterizationMsaaSampleCountSixteen[];

extern const char kGpuRasterizationName[];
extern const char kGpuRasterizationDescription[];
extern const char kForceGpuRasterization[];

extern const char kGoogleProfileInfoName[];
extern const char kGoogleProfileInfoDescription[];

extern const char kHarfbuzzRendertextName[];
extern const char kHarfbuzzRendertextDescription[];

extern const char kHistoryRequiresUserGestureName[];
extern const char kHistoryRequiresUserGestureDescription[];
extern const char kHyperlinkAuditingName[];
extern const char kHyperlinkAuditingDescription[];

extern const char kHostedAppQuitNotificationName[];
extern const char kHostedAppQuitNotificationDescription[];

extern const char kHostedAppShimCreationName[];
extern const char kHostedAppShimCreationDescription[];

extern const char kHtmlBasedUsernameDetectorName[];
extern const char kHtmlBasedUsernameDetectorDescription[];

extern const char kIconNtpName[];
extern const char kIconNtpDescription[];

extern const char kIgnoreGpuBlacklistName[];
extern const char kIgnoreGpuBlacklistDescription[];

extern const char kImportantSitesInCbdName[];
extern const char kImportantSitesInCbdDescription[];

extern const char kInertVisualViewportName[];
extern const char kInertVisualViewportDescription[];

extern const char kInProductHelpDemoModeChoiceName[];
extern const char kInProductHelpDemoModeChoiceDescription[];

extern const char kJavascriptHarmonyName[];
extern const char kJavascriptHarmonyDescription[];

extern const char kJavascriptHarmonyShippingName[];
extern const char kJavascriptHarmonyShippingDescription[];

extern const char kKeepAliveRendererForKeepaliveRequestsName[];
extern const char kKeepAliveRendererForKeepaliveRequestsDescription[];

extern const char kLcdTextName[];
extern const char kLcdTextDescription[];

extern const char kLeftToRightUrlsName[];
extern const char kLeftToRightUrlsDescription[];

extern const char kLoadMediaRouterComponentExtensionName[];
extern const char kLoadMediaRouterComponentExtensionDescription[];

extern const char kManualPasswordGenerationName[];
extern const char kManualPasswordGenerationDescription[];

extern const char kManualPasswordSavingName[];
extern const char kManualPasswordSavingDescription[];

extern const char kMarkHttpAsName[];
extern const char kMarkHttpAsDescription[];
extern const char kMarkHttpAsDangerous[];

extern const char kMaterialDesignIncognitoNTPName[];
extern const char kMaterialDesignIncognitoNTPDescription[];

extern const char kMediaRemotingName[];
extern const char kMediaRemotingDescription[];

extern const char kMemoryAblationName[];
extern const char kMemoryAblationDescription[];

extern const char kMemoryCoordinatorName[];
extern const char kMemoryCoordinatorDescription[];

extern const char kMessageCenterNewStyleNotificationName[];
extern const char kMessageCenterNewStyleNotificationDescription[];

extern const char kMhtmlGeneratorOptionName[];
extern const char kMhtmlGeneratorOptionDescription[];
extern const char kMhtmlSkipNostoreMain[];
extern const char kMhtmlSkipNostoreAll[];

extern const char kMojoLoadingName[];
extern const char kMojoLoadingDescription[];

extern const char kMojoVideoEncodeAcceleratorName[];
extern const char kMojoVideoEncodeAcceleratorDescription[];

extern const char kModuleScriptsName[];
extern const char kModuleScriptsDescription[];

extern const char kModuleScriptsDynamicImportName[];
extern const char kModuleScriptsDynamicImportDescription[];

extern const char kModuleScriptsImportMetaUrlName[];
extern const char kModuleScriptsImportMetaUrlDescription[];

extern const char kNewAudioRenderingMixingStrategyName[];
extern const char kNewAudioRenderingMixingStrategyDescription[];

extern const char kNewBookmarkAppsName[];
extern const char kNewBookmarkAppsDescription[];

extern const char kNewOmniboxAnswerTypesName[];
extern const char kNewOmniboxAnswerTypesDescription[];

extern const char kNewRemotePlaybackPipelineName[];
extern const char kNewRemotePlaybackPipelineDescription[];

extern const char kNewUsbBackendName[];
extern const char kNewUsbBackendDescription[];

extern const char kNostatePrefetchName[];
extern const char kNostatePrefetchDescription[];

extern const char kNotificationsNativeFlagName[];
extern const char kNotificationsNativeFlagDescription[];

#if defined(OS_POSIX)
extern const char kNtlmV2EnabledName[];
extern const char kNtlmV2EnabledDescription[];
#endif

extern const char kNumRasterThreadsName[];
extern const char kNumRasterThreadsDescription[];
extern const char kNumRasterThreadsOne[];
extern const char kNumRasterThreadsTwo[];
extern const char kNumRasterThreadsThree[];
extern const char kNumRasterThreadsFour[];

extern const char kOfferStoreUnmaskedWalletCardsName[];
extern const char kOfferStoreUnmaskedWalletCardsDescription[];

extern const char kOfflineAutoReloadName[];
extern const char kOfflineAutoReloadDescription[];

extern const char kOfflineAutoReloadVisibleOnlyName[];
extern const char kOfflineAutoReloadVisibleOnlyDescription[];

extern const char kOffMainThreadFetchName[];
extern const char kOffMainThreadFetchDescription[];

extern const char kOmniboxDisplayTitleForCurrentUrlName[];
extern const char kOmniboxDisplayTitleForCurrentUrlDescription[];

extern const char kOmniboxSpareRendererName[];
extern const char kOmniboxSpareRendererDescription[];

extern const char kOmniboxUIElideSuggestionUrlAfterHostName[];
extern const char kOmniboxUIElideSuggestionUrlAfterHostDescription[];

extern const char kOmniboxUIHideSuggestionUrlSchemeName[];
extern const char kOmniboxUIHideSuggestionUrlSchemeDescription[];

extern const char kOmniboxUIHideSuggestionUrlTrivialSubdomainsName[];
extern const char kOmniboxUIHideSuggestionUrlTrivialSubdomainsDescription[];

extern const char kOmniboxUIMaxAutocompleteMatchesName[];
extern const char kOmniboxUIMaxAutocompleteMatchesDescription[];

extern const char kOmniboxUINarrowDropdownName[];
extern const char kOmniboxUINarrowDropdownDescription[];

extern const char kOmniboxUIShowSuggestionFaviconsName[];
extern const char kOmniboxUIShowSuggestionFaviconsDescription[];

extern const char kOmniboxUISwapTitleAndUrlName[];
extern const char kOmniboxUISwapTitleAndUrlDescription[];

extern const char kOmniboxUIVerticalLayoutName[];
extern const char kOmniboxUIVerticalLayoutDescription[];

extern const char kOmniboxUIVerticalMarginName[];
extern const char kOmniboxUIVerticalMarginDescription[];

extern const char kOverflowIconsForMediaControlsName[];
extern const char kOverflowIconsForMediaControlsDescription[];

extern const char kOriginTrialsName[];
extern const char kOriginTrialsDescription[];

extern const char kOverlayScrollbarsName[];
extern const char kOverlayScrollbarsDescription[];

extern const char kOverlayScrollbarsFlashAfterAnyScrollUpdateName[];
extern const char kOverlayScrollbarsFlashAfterAnyScrollUpdateDescription[];

extern const char kOverlayScrollbarsFlashWhenMouseEnterName[];
extern const char kOverlayScrollbarsFlashWhenMouseEnterDescription[];

extern const char kUseNewAcceptLanguageHeaderName[];
extern const char kUseNewAcceptLanguageHeaderDescription[];

extern const char kOverscrollHistoryNavigationName[];
extern const char kOverscrollHistoryNavigationDescription[];
extern const char kOverscrollHistoryNavigationSimpleUi[];

extern const char kOverscrollStartThresholdName[];
extern const char kOverscrollStartThresholdDescription[];
extern const char kOverscrollStartThreshold133Percent[];
extern const char kOverscrollStartThreshold166Percent[];
extern const char kOverscrollStartThreshold200Percent[];

extern const char kParallelDownloadingName[];
extern const char kParallelDownloadingDescription[];

extern const char kPassiveEventListenerDefaultName[];
extern const char kPassiveEventListenerDefaultDescription[];
extern const char kPassiveEventListenerTrue[];
extern const char kPassiveEventListenerForceAllTrue[];

extern const char kPassiveEventListenersDueToFlingName[];
extern const char kPassiveEventListenersDueToFlingDescription[];

extern const char kPassiveDocumentEventListenersName[];
extern const char kPassiveDocumentEventListenersDescription[];

extern const char kPasswordForceSavingName[];
extern const char kPasswordForceSavingDescription[];

extern const char kPasswordGenerationName[];
extern const char kPasswordGenerationDescription[];

extern const char kPasswordImportExportName[];
extern const char kPasswordImportExportDescription[];

extern const char kPasswordSelectionName[];
extern const char kPasswordSelectionDescription[];

extern const char kPermissionActionReportingName[];
extern const char kPermissionActionReportingDescription[];

extern const char kPermissionsBlacklistName[];
extern const char kPermissionsBlacklistDescription[];

extern const char kPinchScaleName[];
extern const char kPinchScaleDescription[];

extern const char kPrintPdfAsImageName[];
extern const char kPrintPdfAsImageDescription[];

extern const char kPrintPreviewRegisterPromosName[];
extern const char kPrintPreviewRegisterPromosDescription[];

extern const char kProtectSyncCredentialName[];
extern const char kProtectSyncCredentialDescription[];

extern const char kProtectSyncCredentialOnReauthName[];
extern const char kProtectSyncCredentialOnReauthDescription[];

extern const char kPullToRefreshName[];
extern const char kPullToRefreshDescription[];

extern const char kPushApiBackgroundModeName[];
extern const char kPushApiBackgroundModeDescription[];

extern const char kPwaMinimalUiName[];
extern const char kPwaMinimalUiDescription[];

extern const char kQuicName[];
extern const char kQuicDescription[];

extern const char kReducedReferrerGranularityName[];
extern const char kReducedReferrerGranularityDescription[];

extern const char kRequestTabletSiteName[];
extern const char kRequestTabletSiteDescription[];

extern const char kResetAppListInstallStateName[];
extern const char kResetAppListInstallStateDescription[];

extern const char kResourceLoadSchedulerName[];
extern const char kResourceLoadSchedulerDescription[];

extern const char kSafeSearchUrlReportingName[];
extern const char kSafeSearchUrlReportingDescription[];

extern const char kSaveasMenuLabelExperimentName[];
extern const char kSaveasMenuLabelExperimentDescription[];

extern const char kSavePageAsMhtmlName[];
extern const char kSavePageAsMhtmlDescription[];

extern const char kSavePreviousDocumentResourcesName[];
extern const char kSavePreviousDocumentResourcesDescription[];
extern const char kSavePreviousDocumentResourcesNever[];
extern const char kSavePreviousDocumentResourcesUntilOnDOMContentLoaded[];
extern const char kSavePreviousDocumentResourcesUntilOnLoad[];

extern const char kScrollPredictionName[];
extern const char kScrollPredictionDescription[];

extern const char kSecondaryUiMd[];
extern const char kSecondaryUiMdDescription[];

extern const char kServiceWorkerNavigationPreloadName[];
extern const char kServiceWorkerNavigationPreloadDescription[];

extern const char kServiceWorkerPaymentAppsName[];
extern const char kServiceWorkerPaymentAppsDescription[];

extern const char kServiceWorkerScriptStreamingName[];
extern const char kServiceWorkerScriptStreamingDescription[];

extern const char kSettingsWindowName[];
extern const char kSettingsWindowDescription[];

extern const char kShowAutofillSignaturesName[];
extern const char kShowAutofillSignaturesDescription[];

extern const char kShowAutofillTypePredictionsName[];
extern const char kShowAutofillTypePredictionsDescription[];

extern const char kShowOverdrawFeedbackName[];
extern const char kShowOverdrawFeedbackDescription[];

extern const char kShowSavedCopyName[];
extern const char kShowSavedCopyDescription[];
extern const char kEnableShowSavedCopyPrimary[];
extern const char kEnableShowSavedCopySecondary[];
extern const char kDisableShowSavedCopy[];

extern const char kShowTouchHudName[];
extern const char kShowTouchHudDescription[];

extern const char kSilentDebuggerExtensionApiName[];
extern const char kSilentDebuggerExtensionApiDescription[];

extern const char kSimpleCacheBackendName[];
extern const char kSimpleCacheBackendDescription[];

extern const char kSimplifiedFullscreenUiName[];
extern const char kSimplifiedFullscreenUiDescription[];

extern const char kSingleClickAutofillName[];
extern const char kSingleClickAutofillDescription[];

extern const char kSiteDetails[];
extern const char kSiteDetailsDescription[];

extern const char kSitePerProcessName[];
extern const char kSitePerProcessDescription[];

extern const char kSiteSettings[];
extern const char kSiteSettingsDescription[];

extern const char kSlimmingPaintInvalidationName[];
extern const char kSlimmingPaintInvalidationDescription[];

extern const char kSmoothScrollingName[];
extern const char kSmoothScrollingDescription[];

extern const char kSoftwareRasterizerName[];
extern const char kSoftwareRasterizerDescription[];

extern const char kSoundContentSettingName[];
extern const char kSoundContentSettingDescription[];

extern const char kSpeculativePreconnectName[];
extern const char kSpeculativePreconnectDescription[];

extern const char kSpeculativePrefetchName[];
extern const char kSpeculativePrefetchDescription[];

extern const char kSpeculativeServiceWorkerStartOnQueryInputName[];
extern const char kSpeculativeServiceWorkerStartOnQueryInputDescription[];

extern const char kSpellingFeedbackFieldTrialName[];
extern const char kSpellingFeedbackFieldTrialDescription[];

extern const char kTLS13VariantName[];
extern const char kTLS13VariantDescription[];
extern const char kTLS13VariantDisabled[];
extern const char kTLS13VariantDeprecated[];
extern const char kTLS13VariantDraft[];
extern const char kTLS13VariantExperiment[];
extern const char kTLS13VariantExperiment2[];
extern const char kTLS13VariantExperiment3[];

extern const char kSuggestionsWithSubStringMatchName[];
extern const char kSuggestionsWithSubStringMatchDescription[];

extern const char kSyncAppListName[];
extern const char kSyncAppListDescription[];

extern const char kSyncSandboxName[];
extern const char kSyncSandboxDescription[];

extern const char kSysInternalsName[];
extern const char kSysInternalsDescription[];

extern const char kTabAudioMutingName[];
extern const char kTabAudioMutingDescription[];

extern const char kTabModalJsDialogName[];
extern const char kTabModalJsDialogDescription[];

extern const char kTabsInCbdName[];
extern const char kTabsInCbdDescription[];

extern const char kTcpFastOpenName[];
extern const char kTcpFastOpenDescription[];

extern const char kTopChromeMd[];
extern const char kTopChromeMdDescription[];
extern const char kTopChromeMdMaterial[];
extern const char kTopChromeMdMaterialAuto[];
extern const char kTopChromeMdMaterialHybrid[];

extern const char kThreadedScrollingName[];
extern const char kThreadedScrollingDescription[];

extern const char kTopDocumentIsolationName[];
extern const char kTopDocumentIsolationDescription[];

extern const char kTopSitesFromSiteEngagementName[];
extern const char kTopSitesFromSiteEngagementDescription[];

extern const char kTouchAdjustmentName[];
extern const char kTouchAdjustmentDescription[];

extern const char kTouchDragDropName[];
extern const char kTouchDragDropDescription[];

extern const char kTouchEventsName[];
extern const char kTouchEventsDescription[];

extern const char kTouchSelectionStrategyName[];
extern const char kTouchSelectionStrategyDescription[];
extern const char kTouchSelectionStrategyCharacter[];
extern const char kTouchSelectionStrategyDirection[];

extern const char kTraceUploadUrlName[];
extern const char kTraceUploadUrlDescription[];
extern const char kTraceUploadUrlChoiceOther[];
extern const char kTraceUploadUrlChoiceEmloading[];
extern const char kTraceUploadUrlChoiceQa[];
extern const char kTraceUploadUrlChoiceTesting[];

extern const char kTranslate2016q2UiName[];
extern const char kTranslate2016q2UiDescription[];

extern const char kTranslateLanguageByUlpName[];
extern const char kTranslateLanguageByUlpDescription[];

extern const char kTranslateRankerEnforcementName[];
extern const char kTranslateRankerEnforcementDescription[];

extern const char kTrySupportedChannelLayoutsName[];
extern const char kTrySupportedChannelLayoutsDescription[];

extern const char kUiPartialSwapName[];
extern const char kUiPartialSwapDescription[];

extern const char kUseDdljsonApiName[];
extern const char kUseDdljsonApiDescription[];

extern const char kUserConsentForExtensionScriptsName[];
extern const char kUserConsentForExtensionScriptsDescription[];

extern const char kUseSuggestionsEvenIfFewFeatureName[];
extern const char kUseSuggestionsEvenIfFewFeatureDescription[];

extern const char kV8CacheOptionsName[];
extern const char kV8CacheOptionsDescription[];
extern const char kV8CacheOptionsParse[];
extern const char kV8CacheOptionsCode[];

extern const char kV8CacheStrategiesForCacheStorageName[];
extern const char kV8CacheStrategiesForCacheStorageDescription[];
extern const char kV8CacheStrategiesForCacheStorageNormal[];
extern const char kV8CacheStrategiesForCacheStorageAggressive[];

extern const char kV8ContextSnapshotName[];
extern const char kV8ContextSnapshotDescription[];

extern const char kVibrateRequiresUserGestureName[];
extern const char kVibrateRequiresUserGestureDescription[];

extern const char kVideoFullscreenOrientationLockName[];
extern const char kVideoFullscreenOrientationLockDescription[];

extern const char kVideoRotateToFullscreenName[];
extern const char kVideoRotateToFullscreenDescription[];

extern const char kWalletServiceUseSandboxName[];
extern const char kWalletServiceUseSandboxDescription[];

extern const char kWebglDraftExtensionsName[];
extern const char kWebglDraftExtensionsDescription[];

extern const char kWebMidiName[];
extern const char kWebMidiDescription[];

extern const char kWebPaymentsName[];
extern const char kWebPaymentsDescription[];

extern const char kWebPaymentsModifiersName[];
extern const char kWebPaymentsModifiersDescription[];

extern const char kWebrtcEchoCanceller3Name[];
extern const char kWebrtcEchoCanceller3Description[];

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

extern const char kWebrtcSrtpEncryptedHeadersName[];
extern const char kWebrtcSrtpEncryptedHeadersDescription[];

extern const char kWebrtcStunOriginName[];
extern const char kWebrtcStunOriginDescription[];

extern const char kWebvrName[];
extern const char kWebvrDescription[];

extern const char kWifiCredentialSyncName[];
extern const char kWifiCredentialSyncDescription[];

extern const char kZeroCopyName[];
extern const char kZeroCopyDescription[];

// Android --------------------------------------------------------------------

#if defined(OS_ANDROID)

extern const char kAiaFetchingName[];
extern const char kAiaFetchingDescription[];

extern const char kAccessibilityTabSwitcherName[];
extern const char kAccessibilityTabSwitcherDescription[];

extern const char kAllowReaderForAccessibilityName[];
extern const char kAllowReaderForAccessibilityDescription[];

extern const char kAndroidAutofillAccessibilityName[];
extern const char kAndroidAutofillAccessibilityDescription[];

extern const char kAndroidPaymentAppsName[];
extern const char kAndroidPaymentAppsDescription[];

extern const char kAndroidSigninPromosName[];
extern const char kAndroidSigninPromosDescription[];

extern const char kAndroidViewPasswordsName[];
extern const char kAndroidViewPasswordsDescription[];

extern const char kAsyncDnsName[];
extern const char kAsyncDnsDescription[];

extern const char kAutofillAccessoryViewName[];
extern const char kAutofillAccessoryViewDescription[];

extern const char kBackgroundLoaderForDownloadsName[];
extern const char kBackgroundLoaderForDownloadsDescription[];

extern const char kChromeHomeBottomNavLabelsName[];
extern const char kChromeHomeBottomNavLabelsDescription[];

extern const char kChromeHomeDoodleName[];
extern const char kChromeHomeDoodleDescription[];

extern const char kChromeHomeOptOutSnackbarName[];
extern const char kChromeHomeOptOutSnackbarDescription[];

extern const char kChromeHomePersonalizedOmniboxSuggestionsName[];
extern const char kChromeHomePersonalizedOmniboxSuggestionsDescription[];

extern const char kChromeHomePromoName[];
extern const char kChromeHomePromoDescription[];

extern const char kChromeHomeSwipeLogicName[];
extern const char kChromeHomeSwipeLogicDescription[];
extern const char kChromeHomeSwipeLogicRestrictArea[];

extern const char kChromeHomeName[];
extern const char kChromeHomeDescription[];

extern const char kChromeMemexName[];
extern const char kChromeMemexDescription[];

extern const char kContentSuggestionsCategoryOrderName[];
extern const char kContentSuggestionsCategoryOrderDescription[];

extern const char kContentSuggestionsCategoryRankerName[];
extern const char kContentSuggestionsCategoryRankerDescription[];

extern const char kContentSuggestionsDebugLogName[];
extern const char kContentSuggestionsDebugLogDescription[];

extern const char kContextualSearchContextualCardsBarIntegration[];
extern const char kContextualSearchContextualCardsBarIntegrationDescription[];

extern const char kContextualSearchSingleActionsName[];
extern const char kContextualSearchSingleActionsDescription[];

extern const char kContextualSearchUrlActionsName[];
extern const char kContextualSearchUrlActionsDescription[];

extern const char kContextualSearchName[];
extern const char kContextualSearchDescription[];

extern const char kDontPrefetchLibrariesName[];
extern const char kDontPrefetchLibrariesDescription[];

extern const char kDownloadsForegroundName[];
extern const char kDownloadsForegroundDescription[];

extern const char kEnableAndroidPayIntegrationV1Name[];
extern const char kEnableAndroidPayIntegrationV1Description[];

extern const char kEnableAndroidPayIntegrationV2Name[];
extern const char kEnableAndroidPayIntegrationV2Description[];

extern const char kEnableAndroidSpellcheckerName[];
extern const char kEnableAndroidSpellcheckerDescription[];

extern const char kEnableContentSuggestionsLargeThumbnailName[];
extern const char kEnableContentSuggestionsLargeThumbnailDescription[];

extern const char kEnableContentSuggestionsNewFaviconServerName[];
extern const char kEnableContentSuggestionsNewFaviconServerDescription[];

extern const char kEnableContentSuggestionsSettingsName[];
extern const char kEnableContentSuggestionsSettingsDescription[];

extern const char kEnableContentSuggestionsThumbnailDominantColorName[];
extern const char kEnableContentSuggestionsThumbnailDominantColorDescription[];

extern const char kEnableCopylessPasteName[];
extern const char kEnableCopylessPasteDescription[];

extern const char kEnableCustomContextMenuName[];
extern const char kEnableCustomContextMenuDescription[];

extern const char kEnableCustomFeedbackUiName[];
extern const char kEnableCustomFeedbackUiDescription[];

extern const char kEnableDataReductionProxyMainMenuName[];
extern const char kEnableDataReductionProxyMainMenuDescription[];

extern const char kEnableDataReductionProxySiteBreakdownName[];
extern const char kEnableDataReductionProxySiteBreakdownDescription[];

extern const char kEnableOmniboxClipboardProviderName[];
extern const char kEnableOmniboxClipboardProviderDescription[];

extern const char kEnableExpandedAutofillCreditCardPopupLayoutName[];
extern const char kEnableExpandedAutofillCreditCardPopupLayoutDescription[];

extern const char kEnableFaviconsFromWebManifestName[];
extern const char kEnableFaviconsFromWebManifestDescription[];

extern const char kEnableNtpAssetDownloadSuggestionsName[];
extern const char kEnableNtpAssetDownloadSuggestionsDescription[];

extern const char kEnableNtpBookmarkSuggestionsName[];
extern const char kEnableNtpBookmarkSuggestionsDescription[];

extern const char kEnableNtpForeignSessionsSuggestionsName[];
extern const char kEnableNtpForeignSessionsSuggestionsDescription[];

extern const char kEnableNtpMostLikelyFaviconsFromServerName[];
extern const char kEnableNtpMostLikelyFaviconsFromServerDescription[];

extern const char kNtpTilesLowerResolutionFaviconsName[];
extern const char kNtpTilesLowerResolutionFaviconsDescription[];

extern const char kEnableNtpOfflinePageDownloadSuggestionsName[];
extern const char kEnableNtpOfflinePageDownloadSuggestionsDescription[];

extern const char kEnableNtpRemoteSuggestionsName[];
extern const char kEnableNtpRemoteSuggestionsDescription[];

extern const char kEnableNtpSnippetsVisibilityName[];
extern const char kEnableNtpSnippetsVisibilityDescription[];

extern const char kEnableNtpSuggestionsNotificationsName[];
extern const char kEnableNtpSuggestionsNotificationsDescription[];

extern const char kEnablePhysicalWebName[];
extern const char kEnablePhysicalWebDescription[];

extern const char kEnableOfflinePreviewsName[];
extern const char kEnableOfflinePreviewsDescription[];

extern const char kEnableOskOverscrollName[];
extern const char kEnableOskOverscrollDescription[];

extern const char kEnableSpecialLocaleName[];
extern const char kEnableSpecialLocaleDescription[];

extern const char kEnableWebNfcName[];
extern const char kEnableWebNfcDescription[];

extern const char kEnableWebPaymentsMethodSectionOrderV2Name[];
extern const char kEnableWebPaymentsMethodSectionOrderV2Description[];

extern const char kEnableWebPaymentsSingleAppUiSkipName[];
extern const char kEnableWebPaymentsSingleAppUiSkipDescription[];

extern const char kHerbPrototypeChoicesName[];
extern const char kHerbPrototypeChoicesDescription[];
extern const char kHerbPrototypeFlavorElderberry[];

extern const char kKeepPrefetchedContentSuggestionsName[];
extern const char kKeepPrefetchedContentSuggestionsDescription[];

extern const char kLsdPermissionPromptName[];
extern const char kLsdPermissionPromptDescription[];

extern const char kMediaScreenCaptureName[];
extern const char kMediaScreenCaptureDescription[];

extern const char kModalPermissionPromptsName[];
extern const char kModalPermissionPromptsDescription[];

extern const char kNewPhotoPickerName[];
extern const char kNewPhotoPickerDescription[];

extern const char kNoCreditCardAbort[];
extern const char kNoCreditCardAbortDescription[];

extern const char kNtpCondensedLayoutName[];
extern const char kNtpCondensedLayoutDescription[];

extern const char kNtpCondensedTileLayoutName[];
extern const char kNtpCondensedTileLayoutDescription[];

extern const char kSiteExplorationUiName[];
extern const char kSiteExplorationUiDescription[];

extern const char kNtpGoogleGInOmniboxName[];
extern const char kNtpGoogleGInOmniboxDescription[];

extern const char kNtpOfflinePagesName[];
extern const char kNtpOfflinePagesDescription[];

extern const char kNtpPopularSitesName[];
extern const char kNtpPopularSitesDescription[];

extern const char kOfflineBookmarksName[];
extern const char kOfflineBookmarksDescription[];

extern const char kOfflinePagesAsyncDownloadName[];
extern const char kOfflinePagesAsyncDownloadDescription[];

extern const char kOfflinePagesCtName[];
extern const char kOfflinePagesCtDescription[];

extern const char kOfflinePagesCtV2Name[];
extern const char kOfflinePagesCtV2Description[];

extern const char kOfflinePagesLoadSignalCollectingName[];
extern const char kOfflinePagesLoadSignalCollectingDescription[];

extern const char kOfflinePagesPrefetchingName[];
extern const char kOfflinePagesPrefetchingDescription[];

extern const char kOfflinePagesPrefetchingUIName[];
extern const char kOfflinePagesPrefetchingUIDescription[];

extern const char kOfflinePagesResourceBasedSnapshotName[];
extern const char kOfflinePagesResourceBasedSnapshotDescription[];

extern const char kOfflinePagesRenovationsName[];
extern const char kOfflinePagesRenovationsDescription[];

extern const char kOfflinePagesSharingName[];
extern const char kOfflinePagesSharingDescription[];

extern const char kOfflinePagesSvelteConcurrentLoadingName[];
extern const char kOfflinePagesSvelteConcurrentLoadingDescription[];

extern const char kOffliningRecentPagesName[];
extern const char kOffliningRecentPagesDescription[];

extern const char kPayWithGoogleV1Name[];
extern const char kPayWithGoogleV1Description[];

extern const char kProgressBarThrottleName[];
extern const char kProgressBarThrottleDescription[];

extern const char kProgressBarCompletionName[];
extern const char kProgressBarCompletionDescription[];
extern const char kProgressBarCompletionLoadEvent[];
extern const char kProgressBarCompletionResourcesBeforeDcl[];
extern const char kProgressBarCompletionDomContentLoaded[];
extern const char
    kProgressBarCompletionResourcesBeforeDclAndSameOriginIframes[];

extern const char kPullToRefreshEffectName[];
extern const char kPullToRefreshEffectDescription[];

extern const char kPwaImprovedSplashScreenName[];
extern const char kPwaImprovedSplashScreenDescription[];
extern const char kPwaPersistentNotificationName[];
extern const char kPwaPersistentNotificationDescription[];

extern const char kReaderModeHeuristicsName[];
extern const char kReaderModeHeuristicsDescription[];
extern const char kReaderModeHeuristicsMarkup[];
extern const char kReaderModeHeuristicsAdaboost[];
extern const char kReaderModeHeuristicsAllArticles[];
extern const char kReaderModeHeuristicsAlwaysOff[];
extern const char kReaderModeHeuristicsAlwaysOn[];

extern const char kReaderModeInCCTName[];
extern const char kReaderModeInCCTDescription[];

extern const char kSetMarketUrlForTestingName[];
extern const char kSetMarketUrlForTestingDescription[];

extern const char kSpannableInlineAutocompleteName[];
extern const char kSpannableInlineAutocompleteDescription[];

extern const char kTranslateCompactUIName[];
extern const char kTranslateCompactUIDescription[];

extern const char kUpdateMenuBadgeName[];
extern const char kUpdateMenuBadgeDescription[];

extern const char kUpdateMenuItemCustomSummaryDescription[];
extern const char kUpdateMenuItemCustomSummaryName[];

extern const char kUpdateMenuItemName[];
extern const char kUpdateMenuItemDescription[];

extern const char kUseAndroidMidiApiName[];
extern const char kUseAndroidMidiApiDescription[];

extern const char kThirdPartyDoodlesName[];
extern const char kThirdPartyDoodlesDescription[];

// Non-Android ----------------------------------------------------------------

#else  // !defined(OS_ANDROID)

extern const char kAccountConsistencyName[];
extern const char kAccountConsistencyDescription[];
extern const char kAccountConsistencyChoiceMirror[];
extern const char kAccountConsistencyChoiceDice[];

extern const char kDoodlesOnLocalNtpName[];
extern const char kDoodlesOnLocalNtpDescription[];

extern const char kEnableAudioFocusName[];
extern const char kEnableAudioFocusDescription[];
extern const char kEnableAudioFocusDisabled[];
extern const char kEnableAudioFocusEnabled[];
extern const char kEnableAudioFocusEnabledDuckFlash[];

extern const char kEnableNewAppMenuIconName[];
extern const char kEnableNewAppMenuIconDescription[];

extern const char kOmniboxEntitySuggestionsName[];
extern const char kOmniboxEntitySuggestionsDescription[];

extern const char kOmniboxTailSuggestionsName[];
extern const char kOmniboxTailSuggestionsDescription[];

extern const char kOneGoogleBarOnLocalNtpName[];
extern const char kOneGoogleBarOnLocalNtpDescription[];

extern const char kPauseBackgroundTabsName[];
extern const char kPauseBackgroundTabsDescription[];

extern const char kUseGoogleLocalNtpName[];
extern const char kUseGoogleLocalNtpDescription[];

extern const char kVoiceSearchOnLocalNtpName[];
extern const char kVoiceSearchOnLocalNtpDescription[];

#if defined(GOOGLE_CHROME_BUILD)

extern const char kGoogleBrandedContextMenuName[];
extern const char kGoogleBrandedContextMenuDescription[];

#endif  // defined(GOOGLE_CHROME_BUILD)

#endif  // defined(OS_ANDROID)

// Windows --------------------------------------------------------------------

#if defined(OS_WIN)

extern const char kCloudPrintXpsName[];
extern const char kCloudPrintXpsDescription[];

extern const char kDirectManipulationStylusName[];
extern const char kDirectManipulationStylusDescription[];

extern const char kDisablePostscriptPrinting[];
extern const char kDisablePostscriptPrintingDescription[];

extern const char kEnableAppcontainerName[];
extern const char kEnableAppcontainerDescription[];

extern const char kEnableD3DVsync[];
extern const char kEnableD3DVsyncDescription[];

extern const char kEnableDesktopIosPromotionsName[];
extern const char kEnableDesktopIosPromotionsDescription[];

extern const char kGdiTextPrinting[];
extern const char kGdiTextPrintingDescription[];

extern const char kMergeKeyCharEventsName[];
extern const char kMergeKeyCharEventsDescription[];

extern const char kTraceExportEventsToEtwName[];
extern const char kTraceExportEventsToEtwDesription[];

extern const char kUseWinrtMidiApiName[];
extern const char kUseWinrtMidiApiDescription[];

extern const char kWindows10CustomTitlebarName[];
extern const char kWindows10CustomTitlebarDescription[];

#if DCHECK_IS_ON() && defined(SYZYASAN)
extern const char kSyzyAsanDcheckIsFatalName[];
extern const char kSyzyAsanDcheckIsFatalDescription[];
#endif  // DCHECK_IS_ON() && defined(SYZYASAN)

#endif  // defined(OS_WIN)

// Mac ------------------------------------------------------------------------

#if defined(OS_MACOSX)

extern const char kAppInfoDialogName[];
extern const char kAppInfoDialogDescription[];

extern const char kAppWindowCyclingName[];
extern const char kAppWindowCyclingDescription[];

extern const char kFullscreenToolbarRevealName[];
extern const char kFullscreenToolbarRevealDescription[];

extern const char kContentFullscreenName[];
extern const char kContentFullscreenDescription[];

extern const char kCreditCardAutofillTouchBarName[];
extern const char kCreditCardAutofillTouchBarDescription[];

extern const char kDialogTouchBarName[];
extern const char kDialogTouchBarDescription[];

extern const char kHostedAppsInWindowsName[];
extern const char kHostedAppsInWindowsDescription[];

extern const char kMacRTLName[];
extern const char kMacRTLDescription[];

extern const char kMacSystemShareMenuName[];
extern const char kMacSystemShareMenuDescription[];

extern const char kMacTouchBarName[];
extern const char kMacTouchBarDescription[];

extern const char kMacV2SandboxName[];
extern const char kMacV2SandboxDescription[];

extern const char kMacViewsNativeAppWindowsName[];
extern const char kMacViewsNativeAppWindowsDescription[];

extern const char kMacViewsTaskManagerName[];
extern const char kMacViewsTaskManagerDescription[];

extern const char kTabDetachingInFullscreenName[];
extern const char kTabDetachingInFullscreenDescription[];

extern const char kTabStripKeyboardFocusName[];
extern const char kTabStripKeyboardFocusDescription[];

extern const char kTranslateNewUxName[];
extern const char kTranslateNewUxDescription[];

// Non-Mac --------------------------------------------------------------------

#else  // !defined(OS_MACOSX)

extern const char kPermissionPromptPersistenceToggleName[];
extern const char kPermissionPromptPersistenceToggleDescription[];

#endif  // defined(OS_MACOSX)

// Chrome OS ------------------------------------------------------------------

#if defined(OS_CHROMEOS)

extern const char kAcceleratedMjpegDecodeName[];
extern const char kAcceleratedMjpegDecodeDescription[];

extern const char kAllowTouchpadThreeFingerClickName[];
extern const char kAllowTouchpadThreeFingerClickDescription[];

extern const char kArcBootCompleted[];
extern const char kArcBootCompletedDescription[];

extern const char kArcNativeBridgeExperimentName[];
extern const char kArcNativeBridgeExperimentDescription[];

extern const char kAshEnableUnifiedDesktopName[];
extern const char kAshEnableUnifiedDesktopDescription[];

extern const char kBootAnimationName[];
extern const char kBootAnimationDescription[];

extern const char kBulkPrintersName[];
extern const char kBulkPrintersDescription[];

extern const char kCaptivePortalBypassProxyName[];
extern const char kCaptivePortalBypassProxyDescription[];

extern const char kCrOSComponentName[];
extern const char kCrOSComponentDescription[];

extern const char kCrOSContainerName[];
extern const char kCrOSContainerDescription[];

extern const char kCrosRegionsModeName[];
extern const char kCrosRegionsModeDescription[];
extern const char kCrosRegionsModeDefault[];
extern const char kCrosRegionsModeOverride[];
extern const char kCrosRegionsModeHide[];

extern const char kDisableLockScreenAppsName[];
extern const char kDisableLockScreenAppsDescription[];

extern const char kDisableTabletAutohideTitlebarsName[];
extern const char kDisableTabletAutohideTitlebarsDescription[];

extern const char kEnablePerUserTimezoneName[];
extern const char kEnablePerUserTimezoneDescription[];

extern const char kDisableSystemTimezoneAutomaticDetectionName[];
extern const char kDisableSystemTimezoneAutomaticDetectionDescription[];

extern const char kEnableFullscreenAppListName[];
extern const char kEnableFullscreenAppListDescription[];

extern const char kEnableBackgroundBlurName[];
extern const char kEnableBackgroundBlurDescription[];

extern const char kEnableAndroidWallpapersAppName[];
extern const char kEnableAndroidWallpapersAppDescription[];

extern const char kEnableChromevoxArcSupportName[];
extern const char kEnableChromevoxArcSupportDescription[];

extern const char kEnableEhvInputName[];
extern const char kEnableEhvInputDescription[];

extern const char kEnableEncryptionMigrationName[];
extern const char kEnableEncryptionMigrationDescription[];

extern const char kEnableExternalDriveRename[];
extern const char kEnableExternalDriveRenameDescription[];

extern const char kEnableImeMenuName[];
extern const char kEnableImeMenuDescription[];

extern const char kEnableTabletSplitViewName[];
extern const char kEnableTabletSplitViewDescription[];

extern const char kEnableZipArchiverPackerName[];
extern const char kEnableZipArchiverPackerDescription[];

extern const char kEnableZipArchiverUnpackerName[];
extern const char kEnableZipArchiverUnpackerDescription[];

extern const char kEolNotificationName[];
extern const char kEolNotificationDescription[];

extern const char kExperimentalAccessibilityFeaturesName[];
extern const char kExperimentalAccessibilityFeaturesDescription[];

extern const char kExperimentalInputViewFeaturesName[];
extern const char kExperimentalInputViewFeaturesDescription[];

extern const char kFileManagerTouchModeName[];
extern const char kFileManagerTouchModeDescription[];

extern const char kFirstRunUiTransitionsName[];
extern const char kFirstRunUiTransitionsDescription[];

extern const char kFloatingVirtualKeyboardName[];
extern const char kFloatingVirtualKeyboardDescription[];

extern const char kForceEnableStylusToolsName[];
extern const char kForceEnableStylusToolsDescription[];

extern const char kUiModeName[];
extern const char kUiModeDescription[];
extern const char kUiModeTablet[];
extern const char kUiModeClamshell[];
extern const char kUiModeAuto[];

extern const char kGestureEditingName[];
extern const char kGestureEditingDescription[];

extern const char kGestureTypingName[];
extern const char kGestureTypingDescription[];

extern const char kInputViewName[];
extern const char kInputViewDescription[];

extern const char kMemoryPressureThresholdName[];
extern const char kMemoryPressureThresholdDescription[];
extern const char kConservativeThresholds[];
extern const char kAggressiveCacheDiscardThresholds[];
extern const char kAggressiveTabDiscardThresholds[];
extern const char kAggressiveThresholds[];

extern const char kMtpWriteSupportName[];
extern const char kMtpWriteSupportDescription[];

extern const char kMultideviceName[];
extern const char kMultideviceDescription[];

extern const char kNetworkPortalNotificationName[];
extern const char kNetworkPortalNotificationDescription[];

extern const char kNetworkSettingsConfigName[];
extern const char kNetworkSettingsConfigDescription[];

extern const char kNewKoreanImeName[];
extern const char kNewKoreanImeDescription[];

extern const char kNewZipUnpackerName[];
extern const char kNewZipUnpackerDescription[];

extern const char kPhysicalKeyboardAutocorrectName[];
extern const char kPhysicalKeyboardAutocorrectDescription[];

extern const char kPrinterProviderSearchAppName[];
extern const char kPrinterProviderSearchAppDescription[];

extern const char kQuickUnlockPinName[];
extern const char kQuickUnlockPinDescription[];
extern const char kQuickUnlockPinSignin[];
extern const char kQuickUnlockPinSigninDescription[];
extern const char kQuickUnlockFingerprint[];
extern const char kQuickUnlockFingerprintDescription[];

extern const char kOfficeEditingComponentAppName[];
extern const char kOfficeEditingComponentAppDescription[];

extern const char kSpuriousPowerButtonWindowName[];
extern const char kSpuriousPowerButtonWindowDescription[];

extern const char kSpuriousPowerButtonAccelCountName[];
extern const char kSpuriousPowerButtonAccelCountDescription[];

extern const char kSpuriousPowerButtonScreenAccelName[];
extern const char kSpuriousPowerButtonScreenAccelDescription[];

extern const char kSpuriousPowerButtonKeyboardAccelName[];
extern const char kSpuriousPowerButtonKeyboardAccelDescription[];

extern const char kSpuriousPowerButtonLidAngleChangeName[];
extern const char kSpuriousPowerButtonLidAngleChangeDescription[];

extern const char kTeamDrivesName[];
extern const char kTeamDrivesDescription[];

extern const char kTetherName[];
extern const char kTetherDescription[];

extern const char kTouchscreenCalibrationName[];
extern const char kTouchscreenCalibrationDescription[];

extern const char kUiDevToolsName[];
extern const char kUiDevToolsDescription[];

extern const char kUseMusName[];
extern const char kUseMusDescription[];
extern const char kEnableMashDescription[];
extern const char kEnableMusDescription[];

extern const char kVideoPlayerChromecastSupportName[];
extern const char kVideoPlayerChromecastSupportDescription[];

extern const char kVirtualKeyboardName[];
extern const char kVirtualKeyboardDescription[];

extern const char kVirtualKeyboardOverscrollName[];
extern const char kVirtualKeyboardOverscrollDescription[];

extern const char kVoiceInputName[];
extern const char kVoiceInputDescription[];

extern const char kWakeOnPacketsName[];
extern const char kWakeOnPacketsDescription[];

#endif  // #if defined(OS_CHROMEOS)

// Random platform combinations -----------------------------------------------

#if defined(OS_WIN) || defined(OS_LINUX)

extern const char kEnableInputImeApiName[];
extern const char kEnableInputImeApiDescription[];

#endif  // defined(OS_WIN) || defined(OS_LINUX)

#if defined(OS_WIN) || defined(OS_MACOSX)

extern const char kAutomaticTabDiscardingName[];
extern const char kAutomaticTabDiscardingDescription[];

#endif  // defined(OS_WIN) || defined(OS_MACOSX)

// Feature flags --------------------------------------------------------------

#if BUILDFLAG(ENABLE_VR)

#if defined(OS_ANDROID)

extern const char kEnableVrShellName[];
extern const char kEnableVrShellDescription[];

extern const char kVrCustomTabBrowsingName[];
extern const char kVrCustomTabBrowsingDescription[];

extern const char kWebVrAutopresentName[];
extern const char kWebVrAutopresentDescription[];

extern const char kWebVrVsyncAlignName[];
extern const char kWebVrVsyncAlignDescription[];

#endif  // OS_ANDROID

extern const char kWebvrExperimentalRenderingName[];
extern const char kWebvrExperimentalRenderingDescription[];

#endif  // ENABLE_VR

#if BUILDFLAG(ENABLE_NACL)

extern const char kNaclDebugMaskName[];
extern const char kNaclDebugMaskDescription[];
extern const char kNaclDebugMaskChoiceDebugAll[];
extern const char kNaclDebugMaskChoiceExcludeUtilsPnacl[];
extern const char kNaclDebugMaskChoiceIncludeDebug[];

extern const char kNaclDebugName[];
extern const char kNaclDebugDescription[];

extern const char kNaclName[];
extern const char kNaclDescription[];

extern const char kPnaclSubzeroName[];
extern const char kPnaclSubzeroDescription[];

#endif  // BUILDFLAG(ENABLE_NACL)

#if BUILDFLAG(ENABLE_WEBRTC)

extern const char kWebrtcH264WithOpenh264FfmpegName[];
extern const char kWebrtcH264WithOpenh264FfmpegDescription[];

#endif  // BUILDFLAG(ENABLE_WEBRTC)

#if defined(OS_CHROMEOS)

extern const char kAshDisableSmoothScreenRotationName[];
extern const char kAshDisableSmoothScreenRotationDescription[];

extern const char kAshEnableMirroredScreenName[];
extern const char kAshEnableMirroredScreenDescription[];

extern const char kAshShelfColorName[];
extern const char kAshShelfColorDescription[];

extern const char kAshShelfColorScheme[];
extern const char kAshShelfColorSchemeDescription[];
extern const char kAshShelfColorSchemeLightVibrant[];
extern const char kAshShelfColorSchemeNormalVibrant[];
extern const char kAshShelfColorSchemeDarkVibrant[];
extern const char kAshShelfColorSchemeLightMuted[];
extern const char kAshShelfColorSchemeNormalMuted[];
extern const char kAshShelfColorSchemeDarkMuted[];

extern const char kAshDisableShelfModelSynchronization[];
extern const char kAshDisableShelfModelSynchronizationDescription[];

extern const char kMaterialDesignInkDropAnimationSpeedName[];
extern const char kMaterialDesignInkDropAnimationSpeedDescription[];
extern const char kMaterialDesignInkDropAnimationFast[];
extern const char kMaterialDesignInkDropAnimationSlow[];

extern const char kUiShowCompositedLayerBordersName[];
extern const char kUiShowCompositedLayerBordersDescription[];
extern const char kUiShowCompositedLayerBordersRenderPass[];
extern const char kUiShowCompositedLayerBordersSurface[];
extern const char kUiShowCompositedLayerBordersLayer[];
extern const char kUiShowCompositedLayerBordersAll[];

extern const char kUiSlowAnimationsName[];
extern const char kUiSlowAnimationsDescription[];

#endif  // defined(OS_CHROMEOS)

#if defined(TOOLKIT_VIEWS) || defined(OS_ANDROID)

extern const char kAutofillCreditCardUploadName[];
extern const char kAutofillCreditCardUploadDescription[];

#endif  // defined(TOOLKIT_VIEWS) || defined(OS_ANDROID)

// ============================================================================
// Don't just add flags to the end, put them in the right section in
// alphabetical order. See top instructions for more.
// ============================================================================

}  // namespace flag_descriptions

#endif  // CHROME_BROWSER_FLAG_DESCRIPTIONS_H_
