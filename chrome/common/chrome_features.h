// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines all the public base::FeatureList features for the chrome
// module.

#ifndef CHROME_COMMON_CHROME_FEATURES_H_
#define CHROME_COMMON_CHROME_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/common/buildflags.h"
#include "device/vr/buildflags/buildflags.h"
#include "extensions/buildflags/buildflags.h"
#include "ppapi/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "ui/base/buildflags.h"

namespace features {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.

#if defined(OS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kAddToHomescreenMessaging;
#endif  // defined(OS_ANDROID)

#if defined(OS_MACOSX)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kAppleScriptExecuteJavaScriptMenuItem;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kShow10_9ObsoleteInfobar;
COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kViewsTaskManager;
#endif  // defined(OS_MACOSX)

#if !defined(OS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kAppBanners;
#endif  // !defined(OS_ANDROID)

#if defined(OS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kAppNotificationStatusMessaging;
#endif  // defined(OS_ANDROID)

#if !defined(OS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kAppServiceAsh;
#endif  // !defined(OS_ANDROID)

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kAssetDownloadSuggestionsFeature;

COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kAsyncDns;

#if defined(OS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kAutoFetchOnNetErrorPage;
#endif

#if defined(OS_WIN) || defined(OS_MACOSX)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kAutomaticTabDiscarding;
#endif  // defined(OS_WIN) || defined(OS_MACOSX)

#if defined(OS_WIN) || defined(OS_LINUX)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kBackgroundModeAllowRestart;
#endif  // defined(OS_WIN) || defined(OS_LINUX)

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kBlockPromptsIfDismissedOften;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kBlockPromptsIfIgnoredOften;

#if defined(OS_MACOSX)
COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kBookmarkApps;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kBlockRepeatedNotificationPermissionPrompts;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kBrowserHangFixesExperiment;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kBundledConnectionHelpFeature;

#if (defined(OS_LINUX) && !defined(OS_CHROMEOS)) || defined(OS_MACOSX)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kCertDualVerificationTrialFeature;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kChangePictureVideoMode;

#if defined(OS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kDMServerOAuthForChildUser;
#endif

#if defined(OS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kClearOldBrowsingData;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kClickToOpenPDFPlaceholder;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kClipboardContentSetting;

#if defined(OS_MACOSX)
COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kContentFullscreen;
#endif

#if defined(OS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kCrostini;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kCrostiniAppSearch;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kCrostiniAppUninstallGui;
COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kPluginVm;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kUsageTimeLimitPolicy;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kDesktopPWAWindowing;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kDesktopPWAsLinkCapturing;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kDesktopPWAsCustomTabUI;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kDesktopPWAsStayInWindow;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kDesktopPWAsWithoutExtensions;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kDesktopPWAsOmniboxInstall;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kDisallowUnsafeHttpDownloads;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const char kDisallowUnsafeHttpDownloadsParamName[];

#if defined(OS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kDownloadsLocationChange;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kEnableIncognitoWindowCounter;

#if defined(OS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kEventBasedStatusReporting;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kExperimentalAppBanners;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kExternalExtensionDefaultButtonControl;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kFocusMode;

// Android expects this string from Java code, so it is always needed.
// TODO(crbug.com/731802): Use #if BUILDFLAG(ENABLE_VR_BROWSING) instead.
#if BUILDFLAG(ENABLE_VR) || defined(OS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kVrBrowsing;
#endif
#if BUILDFLAG(ENABLE_VR)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kVrBrowsingExperimentalFeatures;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kVrBrowsingExperimentalRendering;

#if BUILDFLAG(ENABLE_OCULUS_VR)
COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kOculusVR;
#endif  // ENABLE_OCULUS_VR

#if BUILDFLAG(ENABLE_OPENVR)
COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kOpenVR;
#endif  // ENABLE_OPENVR

#if BUILDFLAG(ENABLE_WINDOWS_MR)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kWindowsMixedReality;
#endif  // ENABLE_WINDOWS_MR

#endif  // ENABLE_VR

#if defined(OS_WIN)
COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kGdiTextPrinting;
#endif

COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kGeoLanguage;

#if !defined(OS_ANDROID) && defined(GOOGLE_CHROME_BUILD)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kGoogleBrandedContextMenu;
#endif

#if defined(OS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kGrantNotificationsToDSE;
#endif

#if defined(OS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kHappinessTrackingSystem;
#endif

#if !defined(OS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kHappinessTrackingSurveysForDesktop;
#endif

#if !defined(OS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kViewsCastDialog;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kImprovedRecoveryComponent;

#if defined(OS_WIN) && defined(GOOGLE_CHROME_BUILD)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kIncompatibleApplicationsWarning;
#endif

#if !defined(OS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kLocalScreenCasting;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kLookalikeUrlNavigationSuggestionsUI;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kLsdPermissionPrompt;

#if defined(OS_MACOSX)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kMacFullSizeContentView;
#endif

#if defined(OS_MACOSX)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kMacMaterialDesignDownloadShelf;
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kAcknowledgeNtpOverrideOnDeactivate;
#endif

#if defined(OS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kManagedGuestSessionNotification;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kModalPermissionPrompts;

#if BUILDFLAG(ENABLE_NATIVE_NOTIFICATIONS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kNativeNotifications;
#endif

#if defined(OS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kNewNetErrorPageUI;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const char kNewNetErrorPageUIAlternateParameterName[];
COMPONENT_EXPORT(CHROME_FEATURES)
extern const char kNewNetErrorPageUIAlternateContentList[];
COMPONENT_EXPORT(CHROME_FEATURES)
extern const char kNewNetErrorPageUIAlternateContentPreview[];
#endif

#if !defined(OS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kNewTabLoadingAnimation;
#endif

#if defined(OS_POSIX)
COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kNtlmV2Enabled;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kOfflinePageDownloadSuggestionsFeature;

#if defined(OS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kOomIntervention;
#endif

#if defined(OS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kOobeRecommendAppsScreen;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kUseNewAcceptLanguageHeader;

#if defined(OS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kParentAccessCode;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kPermissionDelegation;

#if defined(OS_WIN)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kDisablePostScriptPrinting;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kPredictivePrefetchingAllowedOnAllConnectionTypes;

#if BUILDFLAG(ENABLE_PLUGINS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kPreferHtmlOverPlugins;
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kCloudPrinterHandler;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kPushMessagingBackgroundMode;

#if defined(OS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kRemoveSupervisedUsersOnStartup;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kLoadBrokenImagesFromContextMenu;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kSafeSearchUrlReporting;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kSecurityKeyAttestationPrompt;

#if !defined(OS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kShowManagedUi;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kLinkManagedNoticeToChromeUIManagementURL;
#endif

#if defined(OS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kShowTrustedPublisherURL;
#endif

COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kSiteSettings;

COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kSitePerProcess;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kSiteIsolationForPasswordSites;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kSitePerProcessOnlyForHighMemoryClients;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const char kSitePerProcessOnlyForHighMemoryClientsParamName[];

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kSSLCommittedInterstitials;

#if defined(OS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kNativeSmb;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kSoundContentSetting;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kSupervisedUserCommittedInterstitials;

#if defined(OS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kSysInternals;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kSystemWebApps;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kAppManagement;

#if !defined(OS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kTabMetricsLogging;
#endif

#if defined(OS_WIN) && defined(GOOGLE_CHROME_BUILD)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kThirdPartyModulesBlocking;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kTopSitesFromSiteEngagement;

#if defined(OS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kUsageTimeStateNotifier;
#endif

#if defined(OS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kUseDisplayWideColorGamut;

COMPONENT_EXPORT(CHROME_FEATURES)
bool UseDisplayWideColorGamut();
#endif

#if defined(OS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kAdaptiveScreenBrightnessLogging;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kUserActivityEventLogging;

#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kUseSameCacheForMedia;

#if defined(OS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kArcCupsApi;

COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kQuickUnlockPin;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kQuickUnlockPinSignin;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kQuickUnlockFingerprint;

COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kBulkPrinters;

COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kCrosCompUpdates;

COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kTPMFirmwareUpdate;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kCrOSEnableUSMUserService;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kMachineLearningService;

COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kUsbguard;

COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kUsbbouncer;

COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kShillSandboxing;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kSchedulerConfiguration;
#endif  // defined(OS_CHROMEOS)

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kWebAuthenticationUI;

#if !defined(OS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kWebRtcRemoteEventLog;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kWebRtcRemoteEventLogGzipped;
#endif

#if defined(OS_WIN)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kWin10AcceleratedDefaultBrowserFlow;
#endif  // defined(OS_WIN)

#if defined(OS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kIncognitoStrings;
#endif  // defined(OS_ANDROID)

bool PrefServiceEnabled();

// DON'T ADD RANDOM STUFF HERE. Put it in the main section above in
// alphabetical order, or in one of the ifdefs (also in order in each section).

}  // namespace features

#endif  // CHROME_COMMON_CHROME_FEATURES_H_
