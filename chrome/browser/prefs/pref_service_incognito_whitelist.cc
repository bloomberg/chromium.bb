// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_service_incognito_whitelist.h"

#include <vector>
#include "build/build_config.h"
#include "chrome/common/pref_names.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/certificate_transparency/pref_names.h"
#include "components/component_updater/pref_names.h"
#include "components/consent_auditor/pref_names.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/dom_distiller/core/pref_names.h"
#include "components/flags_ui/flags_ui_pref_names.h"
#include "components/google/core/browser/google_pref_names.h"
#include "components/invalidation/impl/invalidation_prefs.h"
#include "components/language/core/browser/pref_names.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/network_time/network_time_pref_names.h"
#include "components/ntp_snippets/pref_names.h"
#include "components/ntp_tiles/pref_names.h"
#include "components/omnibox/browser/omnibox_pref_names.h"
#include "components/onc/onc_pref_names.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/proxy_config/proxy_prefs.h"
#include "components/rappor/rappor_pref_names.h"
#include "components/rappor/rappor_prefs.h"
#include "components/reading_list/core/reading_list_pref_names.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/signin/core/browser/signin_pref_names.h"
#include "components/spellcheck/browser/pref_names.h"
#include "components/startup_metric_utils/browser/pref_names.h"
#include "components/suggestions/suggestions_pref_names.h"
#include "components/sync/base/pref_names.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/ukm/ukm_pref_names.h"
#include "components/unified_consent/pref_names.h"
#include "components/variations/pref_names.h"
#include "components/web_resource/web_resource_pref_names.h"
#include "services/preferences/public/cpp/tracked/pref_names.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/contextual_suggestions/contextual_suggestions_prefs.h"
#include "components/feed/buildflags.h"
#include "components/feed/core/pref_names.h"
#endif  // defined(OS_ANDROID)

#if !defined(OS_ANDROID)
#include "chrome/browser/accessibility/animation_policy_prefs.h"
#include "extensions/browser/api/audio/pref_names.h"
#include "extensions/browser/pref_names.h"
#endif  // !defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/shelf_prefs.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chromeos/chromeos_pref_names.h"
#include "chromeos/components/proximity_auth/proximity_auth_pref_names.h"
#include "chromeos/components/tether/pref_names.h"
#include "components/arc/arc_prefs.h"
#include "components/cryptauth/pref_names.h"
#include "components/drive/drive_pref_names.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"
#include "ui/chromeos/events/pref_names.h"
#endif  // defined(OS_CHROMEOS)

namespace {

// TODO(https://crbug.com/861722): Remove this list and file.
//
// WARNING: PLEASE DO NOT ADD ANYTHING TO THIS FILE.
// This file is temporarily added for transition of incognito preferences
// storage default, from on disk to in memory. All items in this list will be
// audited and checked with owners and removed from whitelist.
const char* incognito_whitelist[] = {
// ash/public/cpp/ash_pref_names.h
#if defined(OS_CHROMEOS)
    ash::prefs::kAccessibilityLargeCursorEnabled,
    ash::prefs::kAccessibilityLargeCursorDipSize,
    ash::prefs::kAccessibilityStickyKeysEnabled,

    // TODO(https://crbug.com/861722): Check with code owners why this pref is
    // required in tests, if possible, update tests and remove.
    ash::prefs::kAccessibilitySpokenFeedbackEnabled,
    ash::prefs::kAccessibilityHighContrastEnabled,
    ash::prefs::kAccessibilityScreenMagnifierCenterFocus,
    ash::prefs::kAccessibilityScreenMagnifierEnabled,
    ash::prefs::kAccessibilityScreenMagnifierScale,
    ash::prefs::kAccessibilityVirtualKeyboardEnabled,
    ash::prefs::kAccessibilityMonoAudioEnabled,
    ash::prefs::kAccessibilityAutoclickEnabled,
    ash::prefs::kAccessibilityAutoclickDelayMs,
    ash::prefs::kAccessibilityCaretHighlightEnabled,
    ash::prefs::kAccessibilityCursorHighlightEnabled,
    ash::prefs::kAccessibilityFocusHighlightEnabled,
    ash::prefs::kAccessibilitySelectToSpeakEnabled,
    ash::prefs::kAccessibilitySwitchAccessEnabled,
    ash::prefs::kAccessibilityDictationEnabled,
    ash::prefs::kShouldAlwaysShowAccessibilityMenu,
    ash::prefs::kDockedMagnifierEnabled, ash::prefs::kDockedMagnifierScale,
    ash::prefs::kDockedMagnifierAcceleratorDialogHasBeenAccepted,
    ash::prefs::kHighContrastAcceleratorDialogHasBeenAccepted,
    ash::prefs::kScreenMagnifierAcceleratorDialogHasBeenAccepted,
    ash::prefs::kDisplayMixedMirrorModeParams, ash::prefs::kDisplayPowerState,
    ash::prefs::kDisplayProperties, ash::prefs::kDisplayRotationLock,
    ash::prefs::kDisplayTouchAssociations,
    ash::prefs::kDisplayTouchPortAssociations,
    ash::prefs::kExternalDisplayMirrorInfo, ash::prefs::kSecondaryDisplays,
    ash::prefs::kHasSeenStylus, ash::prefs::kShownPaletteWelcomeBubble,
    ash::prefs::kEnableStylusTools, ash::prefs::kLaunchPaletteOnEjectEvent,
    ash::prefs::kNightLightEnabled, ash::prefs::kNightLightTemperature,
    ash::prefs::kNightLightScheduleType, ash::prefs::kNightLightCustomStartTime,
    ash::prefs::kNightLightCustomEndTime, ash::prefs::kAllowScreenLock,
    ash::prefs::kEnableAutoScreenLock, ash::prefs::kPowerAcScreenDimDelayMs,
    ash::prefs::kPowerAcScreenOffDelayMs, ash::prefs::kPowerAcScreenLockDelayMs,
    ash::prefs::kPowerAcIdleWarningDelayMs, ash::prefs::kPowerAcIdleDelayMs,
    ash::prefs::kPowerBatteryScreenDimDelayMs,
    ash::prefs::kPowerBatteryScreenOffDelayMs,
    ash::prefs::kPowerBatteryScreenLockDelayMs,
    ash::prefs::kPowerBatteryIdleWarningDelayMs,
    ash::prefs::kPowerBatteryIdleDelayMs,
    ash::prefs::kPowerLockScreenDimDelayMs,
    ash::prefs::kPowerLockScreenOffDelayMs, ash::prefs::kPowerAcIdleAction,
    ash::prefs::kPowerBatteryIdleAction, ash::prefs::kPowerLidClosedAction,
    ash::prefs::kPowerUseAudioActivity, ash::prefs::kPowerUseVideoActivity,
    ash::prefs::kPowerAllowScreenWakeLocks,
    ash::prefs::kPowerPresentationScreenDimDelayFactor,
    ash::prefs::kPowerUserActivityScreenDimDelayFactor,
    ash::prefs::kPowerWaitForInitialUserActivity,
    ash::prefs::kPowerForceNonzeroBrightnessForUserActivity,
    ash::prefs::kShelfAlignment, ash::prefs::kShelfAlignmentLocal,
    ash::prefs::kShelfAutoHideBehavior, ash::prefs::kShelfAutoHideBehaviorLocal,
    ash::prefs::kShelfPreferences, ash::prefs::kShowLogoutButtonInTray,
    ash::prefs::kLogoutDialogDurationMs, ash::prefs::kUserWallpaperInfo,
    ash::prefs::kWallpaperColors, ash::prefs::kUserBluetoothAdapterEnabled,
    ash::prefs::kSystemBluetoothAdapterEnabled, ash::prefs::kTapDraggingEnabled,
    ash::prefs::kTapToClickEnabled, ash::prefs::kOwnerTapToClickEnabled,
    ash::prefs::kTouchpadEnabled, ash::prefs::kTouchscreenEnabled,
    ash::prefs::kQuickUnlockPinSalt, ash::prefs::kDetachableBaseDevices,
#endif  // defined(OS_CHROMEOS)

// ash/public/cpp/shelf_prefs.h
#if defined(OS_CHROMEOS)
    ash::kShelfAutoHideBehaviorAlways, ash::kShelfAutoHideBehaviorNever,
    ash::kShelfAlignmentBottom, ash::kShelfAlignmentLeft,
    ash::kShelfAlignmentRight,
#endif  // defined(OS_CHROMEOS)

// chrome/browser/chromeos/crostini/crostini_pref_names.h
#if defined(OS_CHROMEOS)
    crostini::prefs::kCrostiniEnabled,
#endif  // defined(OS_CHROMEOS)

// chrome/browser/accessibility/animation_policy_prefs.h
#if !defined(OS_ANDROID)
    kAnimationPolicyAllowed, kAnimationPolicyOnce, kAnimationPolicyNone,
#endif  // !defined(OS_ANDROID)

// chrome/browser/android/contextual_suggestions/contextual_suggestions_prefs.h
#if defined(OS_ANDROID)
    contextual_suggestions::prefs::kContextualSuggestionsEnabled,
#endif  // defined(OS_ANDROID)

    // chrome/common/pref_names.h
    prefs::kAbusiveExperienceInterventionEnforce,
    prefs::kChildAccountStatusKnown, prefs::kDefaultApps,
    prefs::kSafeBrowsingForTrustedSourcesEnabled, prefs::kDisableScreenshots,
    prefs::kDownloadRestrictions, prefs::kForceEphemeralProfiles,
    prefs::kHomePageIsNewTabPage, prefs::kHomePage,
    prefs::kImportantSitesDialogHistory,
#if defined(OS_WIN)
    prefs::kLastProfileResetTimestamp, prefs::kChromeCleanerResetPending,
#endif
    prefs::kNewTabPageLocationOverride, prefs::kProfileIconVersion,
    prefs::kRestoreOnStartup, prefs::kSessionExitedCleanly,
    prefs::kSessionExitType, prefs::kObservedSessionTime,
    prefs::kRecurrentSSLInterstitial, prefs::kSiteEngagementLastUpdateTime,
    prefs::kSupervisedUserApprovedExtensions,
    prefs::kSupervisedUserCustodianEmail, prefs::kSupervisedUserCustodianName,
    prefs::kSupervisedUserCustodianProfileImageURL,
    prefs::kSupervisedUserCustodianProfileURL,
    prefs::kSupervisedUserManualHosts, prefs::kSupervisedUserManualURLs,
    prefs::kSupervisedUserSafeSites, prefs::kSupervisedUserSecondCustodianEmail,
    prefs::kSupervisedUserSecondCustodianName,
    prefs::kSupervisedUserSecondCustodianProfileImageURL,
    prefs::kSupervisedUserSecondCustodianProfileURL,
    prefs::kSupervisedUserSharedSettings, prefs::kSupervisedUserWhitelists,
    prefs::kURLsToRestoreOnStartup,

#if BUILDFLAG(ENABLE_RLZ)
    prefs::kRlzPingDelaySeconds,
#endif  // BUILDFLAG(ENABLE_RLZ)

#if defined(OS_CHROMEOS)
    prefs::kApplicationLocaleBackup, prefs::kApplicationLocaleAccepted,
    prefs::kOwnerLocale, prefs::kAllowedUILocales,
#endif

    prefs::kDefaultCharset, prefs::kAcceptLanguages, prefs::kWebKitCommonScript,
    prefs::kWebKitStandardFontFamily, prefs::kWebKitFixedFontFamily,
    prefs::kWebKitSerifFontFamily, prefs::kWebKitSansSerifFontFamily,
    prefs::kWebKitCursiveFontFamily, prefs::kWebKitFantasyFontFamily,
    prefs::kWebKitPictographFontFamily,

    // prefs::kWebKitScriptsForFontFamilyMaps,
    // prefs::kWebKitScriptsForFontFamilyMapsLength,

    prefs::kWebKitStandardFontFamilyMap, prefs::kWebKitFixedFontFamilyMap,
    prefs::kWebKitSerifFontFamilyMap, prefs::kWebKitSansSerifFontFamilyMap,
    prefs::kWebKitCursiveFontFamilyMap, prefs::kWebKitFantasyFontFamilyMap,
    prefs::kWebKitPictographFontFamilyMap,

    prefs::kWebKitStandardFontFamilyArabic,
#if defined(OS_WIN)
    prefs::kWebKitFixedFontFamilyArabic,
#endif
    prefs::kWebKitSerifFontFamilyArabic,
    prefs::kWebKitSansSerifFontFamilyArabic,
#if defined(OS_WIN)
    prefs::kWebKitStandardFontFamilyCyrillic,
    prefs::kWebKitFixedFontFamilyCyrillic,
    prefs::kWebKitSerifFontFamilyCyrillic,
    prefs::kWebKitSansSerifFontFamilyCyrillic,
    prefs::kWebKitStandardFontFamilyGreek, prefs::kWebKitFixedFontFamilyGreek,
    prefs::kWebKitSerifFontFamilyGreek, prefs::kWebKitSansSerifFontFamilyGreek,
#endif
    prefs::kWebKitStandardFontFamilyJapanese,
    prefs::kWebKitFixedFontFamilyJapanese,
    prefs::kWebKitSerifFontFamilyJapanese,
    prefs::kWebKitSansSerifFontFamilyJapanese,
    prefs::kWebKitStandardFontFamilyKorean, prefs::kWebKitFixedFontFamilyKorean,
    prefs::kWebKitSerifFontFamilyKorean,
    prefs::kWebKitSansSerifFontFamilyKorean,
#if defined(OS_WIN)
    prefs::kWebKitCursiveFontFamilyKorean,
#endif
    prefs::kWebKitStandardFontFamilySimplifiedHan,
    prefs::kWebKitFixedFontFamilySimplifiedHan,
    prefs::kWebKitSerifFontFamilySimplifiedHan,
    prefs::kWebKitSansSerifFontFamilySimplifiedHan,
    prefs::kWebKitStandardFontFamilyTraditionalHan,
    prefs::kWebKitFixedFontFamilyTraditionalHan,
    prefs::kWebKitSerifFontFamilyTraditionalHan,
    prefs::kWebKitSansSerifFontFamilyTraditionalHan,
#if defined(OS_WIN) || defined(OS_MACOSX)
    prefs::kWebKitCursiveFontFamilySimplifiedHan,
    prefs::kWebKitCursiveFontFamilyTraditionalHan,
#endif

    prefs::kWebKitDefaultFontSize, prefs::kWebKitDefaultFixedFontSize,
    prefs::kWebKitMinimumFontSize, prefs::kWebKitMinimumLogicalFontSize,
    prefs::kWebKitJavascriptEnabled, prefs::kWebKitWebSecurityEnabled,
    prefs::kWebKitLoadsImagesAutomatically, prefs::kWebKitPluginsEnabled,
    prefs::kWebKitDomPasteEnabled, prefs::kWebKitTextAreasAreResizable,
    prefs::kWebKitJavascriptCanAccessClipboard, prefs::kWebkitTabsToLinks,
    prefs::kWebKitAllowRunningInsecureContent,
#if defined(OS_ANDROID)
    prefs::kWebKitFontScaleFactor, prefs::kWebKitForceEnableZoom,
    prefs::kWebKitPasswordEchoEnabled,
#endif
    prefs::kDataSaverEnabled, prefs::kSSLErrorOverrideAllowed,
    prefs::kIncognitoModeAvailability, prefs::kSearchSuggestEnabled,
#if defined(OS_ANDROID)
    prefs::kContextualSearchEnabled,
#endif  // defined(OS_ANDROID)
#if defined(OS_MACOSX) || defined(OS_WIN) || \
    (defined(OS_LINUX) && !defined(OS_CHROMEOS))
    prefs::kConfirmToQuitEnabled,
#endif
#if defined(OS_MACOSX)
    prefs::kShowFullscreenToolbar, prefs::kAllowJavascriptAppleEvents,
#endif
    prefs::kPromptForDownload, prefs::kAlternateErrorPagesEnabled,
    prefs::kDnsPrefetchingStartupList, prefs::kDnsPrefetchingHostReferralList,
    prefs::kQuicAllowed, prefs::kNetworkQualities,
#if defined(OS_ANDROID)
    prefs::kLastPolicyCheckTime,
#endif
    prefs::kNetworkPredictionOptions, prefs::kDefaultAppsInstallState,
    prefs::kHideWebStoreIcon,
#if defined(OS_CHROMEOS)
    prefs::kEnableTouchpadThreeFingerClick, prefs::kNaturalScroll,
    prefs::kPrimaryMouseButtonRight, prefs::kMouseReverseScroll,
    prefs::kMouseSensitivity, prefs::kTouchpadSensitivity,
    prefs::kUse24HourClock, prefs::kUserTimezone,
    prefs::kResolveTimezoneByGeolocation,
    prefs::kResolveTimezoneByGeolocationMethod,
    prefs::kResolveTimezoneByGeolocationMigratedToMethod,
    prefs::kLanguageCurrentInputMethod, prefs::kLanguagePreviousInputMethod,
    prefs::kLanguageAllowedInputMethods, prefs::kLanguagePreferredLanguages,
    prefs::kLanguagePreferredLanguagesSyncable, prefs::kLanguagePreloadEngines,
    prefs::kLanguagePreloadEnginesSyncable, prefs::kLanguageEnabledImes,
    prefs::kLanguageEnabledImesSyncable, prefs::kLanguageImeMenuActivated,
    prefs::kLanguageShouldMergeInputMethods, prefs::kLanguageSendFunctionKeys,
    prefs::kLanguageXkbAutoRepeatEnabled, prefs::kLanguageXkbAutoRepeatDelay,
    prefs::kLanguageXkbAutoRepeatInterval,

    prefs::kLabsAdvancedFilesystemEnabled, prefs::kLabsMediaplayerEnabled,
    prefs::kShow3gPromoNotification, prefs::kDataSaverPromptsShown,
    prefs::kChromeOSReleaseNotesVersion, prefs::kNoteTakingAppId,
    prefs::kNoteTakingAppEnabledOnLockScreen,
    prefs::kNoteTakingAppsLockScreenWhitelist,
    prefs::kNoteTakingAppsLockScreenToastShown,
    prefs::kRestoreLastLockScreenNote, prefs::kSessionUserActivitySeen,
    prefs::kSessionStartTime, prefs::kSessionLengthLimit,
    prefs::kSessionWaitForInitialUserActivity, prefs::kLastSessionType,
    prefs::kLastSessionLength, prefs::kTermsOfServiceURL,
    prefs::kAttestationEnabled, prefs::kAttestationExtensionWhitelist,
    prefs::kMultiProfileNeverShowIntro,
    prefs::kMultiProfileWarningShowDismissed, prefs::kMultiProfileUserBehavior,
    prefs::kFirstRunTutorialShown, prefs::kSAMLOfflineSigninTimeLimit,
    prefs::kSAMLLastGAIASignInTime, prefs::kTimeOnOobe,
    prefs::kFileSystemProviderMounted, prefs::kTouchVirtualKeyboardEnabled,
    prefs::kWakeOnWifiDarkConnect,
    prefs::kCaptivePortalAuthenticationIgnoresProxy,
    prefs::kForceMaximizeOnFirstRun, prefs::kPlatformKeys,
    prefs::kUnifiedDesktopEnabledByDefault,
    prefs::kHatsLastInteractionTimestamp, prefs::kHatsSurveyCycleEndTimestamp,
    prefs::kHatsDeviceIsSelected, prefs::kQuickUnlockPinSecret,
    prefs::kQuickUnlockFingerprintRecord, prefs::kEolStatus,
    prefs::kEolNotificationDismissed, prefs::kPinUnlockFeatureNotificationShown,
    prefs::kFingerprintUnlockFeatureNotificationShown,
    prefs::kQuickUnlockModeWhitelist, prefs::kQuickUnlockTimeout,
    prefs::kPinUnlockMinimumLength, prefs::kPinUnlockMaximumLength,
    prefs::kPinUnlockWeakPinsAllowed, prefs::kEnableQuickUnlockFingerprint,
    prefs::kInstantTetheringAllowed, prefs::kInstantTetheringEnabled,
    prefs::kInstantTetheringBleAdvertisingSupported,
    prefs::kCastReceiverEnabled, prefs::kMinimumAllowedChromeVersion,
    prefs::kShowSyncSettingsOnSessionStart, prefs::kTextToSpeechLangToVoiceName,
    prefs::kTextToSpeechRate, prefs::kTextToSpeechPitch,
    prefs::kTextToSpeechVolume, prefs::kFirstScreenStartTime,
    prefs::kCurrentScreenStartTime, prefs::kScreenTimeMinutesUsed,
    prefs::kUsageTimeLimit, prefs::kScreenTimeLastState,
    prefs::kEnableSyncConsent,
#endif  // defined(OS_CHROMEOS)
    prefs::kShowHomeButton, prefs::kSpeechRecognitionFilterProfanities,
    prefs::kSavingBrowserHistoryDisabled, prefs::kAllowDeletingBrowserHistory,
#if !defined(OS_ANDROID)
    prefs::kMdHistoryMenuPromoShown,
#endif
    prefs::kForceGoogleSafeSearch, prefs::kForceYouTubeRestrict,
    prefs::kForceSessionSync, prefs::kAllowedDomainsForApps,
    prefs::kSafeSitesFilterBehavior,
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
    prefs::kUsesSystemTheme,
#endif
    prefs::kCurrentThemePackFilename, prefs::kCurrentThemeID,
    prefs::kCurrentThemeImages, prefs::kCurrentThemeColors,
    prefs::kCurrentThemeTints, prefs::kCurrentThemeDisplayProperties,
    prefs::kExtensionsUIDeveloperMode,
    // prefs::kExtensionsUIDismissedADTPromo,
    prefs::kExtensionCommands, prefs::kPluginsLastInternalDirectory,
    prefs::kPluginsPluginsList, prefs::kPluginsDisabledPlugins,
    prefs::kPluginsDisabledPluginsExceptions, prefs::kPluginsEnabledPlugins,
    prefs::kPluginsAlwaysOpenPdfExternally,
#if BUILDFLAG(ENABLE_PLUGINS)
    prefs::kPluginsShowDetails,
#endif
    prefs::kPluginsAllowOutdated, prefs::kRunAllFlashInAllowMode,
#if BUILDFLAG(ENABLE_PLUGINS)
    prefs::kPluginsMetadata, prefs::kPluginsResourceCacheUpdate,
#endif
    prefs::kDefaultBrowserLastDeclined, prefs::kResetCheckDefaultBrowser,
    prefs::kDefaultBrowserSettingEnabled,
#if defined(OS_MACOSX)
    prefs::kShowUpdatePromotionInfoBar,
#endif
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
    prefs::kUseCustomChromeFrame,
#endif
#if BUILDFLAG(ENABLE_PLUGINS)
    prefs::kContentSettingsPluginWhitelist,
#endif
#if !defined(OS_ANDROID)
    prefs::kPartitionDefaultZoomLevel, prefs::kPartitionPerHostZoomLevels,

    prefs::kPinnedTabs,
#endif  // !defined(OS_ANDROID)

    prefs::kDisable3DAPIs, prefs::kEnableDeprecatedWebPlatformFeatures,
    prefs::kEnableHyperlinkAuditing,

    // TODO(https://crbug.com/861722): Check with code owners why this pref is
    // required in tests, if possible, update tests and remove.
    prefs::kEnableReferrers, prefs::kEnableDoNotTrack,
    prefs::kEnableEncryptedMedia,

    prefs::kImportAutofillFormData, prefs::kImportBookmarks,
    prefs::kImportHistory, prefs::kImportHomepage, prefs::kImportSavedPasswords,
    prefs::kImportSearchEngine,

    prefs::kImportDialogAutofillFormData, prefs::kImportDialogBookmarks,
    prefs::kImportDialogHistory, prefs::kImportDialogSavedPasswords,
    prefs::kImportDialogSearchEngine,

    prefs::kProfileAvatarIndex, prefs::kProfileUsingDefaultName,
    prefs::kProfileName, prefs::kProfileUsingDefaultAvatar,
    prefs::kProfileUsingGAIAAvatar, prefs::kSupervisedUserId,

    prefs::kProfileGAIAInfoUpdateTime, prefs::kProfileGAIAInfoPictureURL,

    prefs::kProfileAvatarTutorialShown,

    prefs::kInvertNotificationShown,

    prefs::kPrintingEnabled, prefs::kPrintPreviewDisabled,
    prefs::kPrintPreviewDefaultDestinationSelectionRules,

#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
    prefs::kPrintPreviewUseSystemDefaultPrinter,
#endif

#if defined(OS_CHROMEOS)
    prefs::kPrintingDevices, prefs::kRecommendedNativePrinters,
    prefs::kRecommendedNativePrintersFile,
    prefs::kRecommendedNativePrintersAccessMode,
    prefs::kRecommendedNativePrintersBlacklist,
    prefs::kRecommendedNativePrintersWhitelist,
    prefs::kUserNativePrintersAllowed,
#endif  // OS_CHROMEOS

    prefs::kDefaultSupervisedUserFilteringBehavior,

    prefs::kSupervisedUserCreationAllowed, prefs::kSupervisedUsers,

    prefs::kMessageCenterDisabledExtensionIds,
    prefs::kMessageCenterDisabledSystemComponentIds,

    prefs::kFullscreenAllowed,

    prefs::kLocalDiscoveryNotificationsEnabled,

#if defined(OS_ANDROID)
    prefs::kNotificationsVibrateEnabled,
    prefs::kMigratedToSiteNotificationChannels,
    prefs::kClearedBlockedSiteNotificationChannels,
#endif

    prefs::kPushMessagingAppIdentifierMap,

    prefs::kGCMProductCategoryForSubtypes,

    prefs::kEasyUnlockAllowed, prefs::kEasyUnlockPairing,

#if BUILDFLAG(ENABLE_EXTENSIONS)
    prefs::kToolbarIconSurfacingBubbleAcknowledged,
    prefs::kToolbarIconSurfacingBubbleLastShowTime,
#endif

    prefs::kWebRTCMultipleRoutesEnabled, prefs::kWebRTCNonProxiedUdpEnabled,
    prefs::kWebRTCIPHandlingPolicy, prefs::kWebRTCUDPPortRange,

#if !defined(OS_ANDROID)
    prefs::kHasSeenWelcomePage,
#endif

#if defined(OS_WIN)
    prefs::kHasSeenWin10PromoPage,
#if defined(GOOGLE_CHROME_BUILD)
    prefs::kHasSeenGoogleAppsPromoPage,
#endif  // defined(GOOGLE_CHROME_BUILD)
#endif  // defined(OS_WIN)

#if defined(OS_ANDROID)
    prefs::kCrashReportingEnabled,
#endif  // defined(OS_ANDROID)

    prefs::kProfileLastUsed, prefs::kProfilesLastActive,
    prefs::kProfilesNumCreated, prefs::kProfileInfoCache,
    prefs::kProfileCreatedByVersion, prefs::kProfilesDeleted,

    prefs::kStabilityOtherUserCrashCount, prefs::kStabilityKernelCrashCount,
    prefs::kStabilitySystemUncleanShutdownCount,

    prefs::kStabilityPluginStats, prefs::kStabilityPluginName,
    prefs::kStabilityPluginLaunches, prefs::kStabilityPluginInstances,
    prefs::kStabilityPluginCrashes, prefs::kStabilityPluginLoadingErrors,

    prefs::kBrowserSuppressDefaultBrowserPrompt,

    // prefs::kBrowserWindowPlacement, prefs::kBrowserWindowPlacementPopup,
    prefs::kTaskManagerWindowPlacement, prefs::kTaskManagerColumnVisibility,
    prefs::kTaskManagerEndProcessEnabled, prefs::kAppWindowPlacement,

    prefs::kDownloadDefaultDirectory, prefs::kDownloadExtensionsToOpen,
    prefs::kDownloadDirUpgraded,
#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_MACOSX)
    prefs::kOpenPdfDownloadInSystemReader,
#endif
#if defined(OS_ANDROID)
    prefs::kPromptForDownloadAndroid, prefs::kShowMissingSdCardErrorAndroid,
#endif

    // prefs::kSaveFileDefaultDirectory, prefs::kSaveFileType,
    prefs::kTrustedDownloadSources,

    prefs::kAllowFileSelectionDialogs, prefs::kDefaultTasksByMimeType,
    prefs::kDefaultTasksBySuffix,

    prefs::kSelectFileLastDirectory,

    prefs::kExcludedSchemes,

    prefs::kOptionsWindowLastTabIndex,

    prefs::kLastKnownIntranetRedirectOrigin,

    prefs::kShutdownType, prefs::kShutdownNumProcesses,
    prefs::kShutdownNumProcessesSlow,

    prefs::kRestartLastSessionOnShutdown,
#if !defined(OS_ANDROID)
#if !defined(OS_CHROMEOS)
    prefs::kPromotionalTabsEnabled,
#endif
    prefs::kSuppressUnsupportedOSWarning, prefs::kWasRestarted,
#endif  // !defined(OS_ANDROID)

    prefs::kDisableExtensions,

    prefs::kNtpAppPageNames, prefs::kNtpCollapsedForeignSessions,
#if defined(OS_ANDROID)
    prefs::kNtpCollapsedRecentlyClosedTabs,
    prefs::kNtpCollapsedSnapshotDocument, prefs::kNtpCollapsedSyncPromo,
    prefs::kContentSuggestionsNotificationsEnabled,
    prefs::kContentSuggestionsConsecutiveIgnoredPrefName,
    prefs::kContentSuggestionsNotificationsSentDay,
    prefs::kContentSuggestionsNotificationsSentCount,
#endif  // defined(OS_ANDROID)
    prefs::kNtpShownPage,

    prefs::kDevToolsAdbKey, prefs::kDevToolsAvailability,
    prefs::kDevToolsDiscoverUsbDevicesEnabled, prefs::kDevToolsEditedFiles,
    prefs::kDevToolsFileSystemPaths, prefs::kDevToolsPortForwardingEnabled,
    prefs::kDevToolsPortForwardingDefaultSet,
    prefs::kDevToolsPortForwardingConfig, prefs::kDevToolsPreferences,
    prefs::kDevToolsDiscoverTCPTargetsEnabled,
    prefs::kDevToolsTCPDiscoveryConfig,
#if defined(OS_ANDROID)
    prefs::kDevToolsRemoteEnabled,
#endif

    prefs::kGoogleServicesPasswordHash,

#if !defined(OS_ANDROID)
    prefs::kDiceSigninUserMenuPromoCount, prefs::kSignInPromoStartupCount,
    prefs::kSignInPromoUserSkipped, prefs::kSignInPromoShowOnFirstRunAllowed,
    prefs::kSignInPromoShowNTPBubble,
#endif

#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
    prefs::kCrossDevicePromoOptedOut, prefs::kCrossDevicePromoShouldBeShown,
    prefs::kCrossDevicePromoObservedSingleAccountCookie,
    prefs::kCrossDevicePromoNextFetchListDevicesTime,
    prefs::kCrossDevicePromoNumDevices,
    prefs::kCrossDevicePromoLastDeviceActiveTime,
#endif

    prefs::kWebAppCreateOnDesktop, prefs::kWebAppCreateInAppsMenu,
    prefs::kWebAppCreateInQuickLaunchBar,

    prefs::kWebAppInstallForceList,

    prefs::kGeolocationAccessToken,

    prefs::kDefaultAudioCaptureDevice, prefs::kDefaultVideoCaptureDevice,
    prefs::kMediaDeviceIdSalt, prefs::kMediaStorageIdSalt,

    prefs::kPrintPreviewStickySettings, prefs::kCloudPrintRoot,
    prefs::kCloudPrintProxyEnabled, prefs::kCloudPrintProxyId,
    prefs::kCloudPrintAuthToken, prefs::kCloudPrintEmail,
    prefs::kCloudPrintPrintSystemSettings, prefs::kCloudPrintEnableJobPoll,
    prefs::kCloudPrintRobotRefreshToken, prefs::kCloudPrintRobotEmail,
    prefs::kCloudPrintConnectNewPrinters, prefs::kCloudPrintXmppPingEnabled,
    prefs::kCloudPrintXmppPingTimeout, prefs::kCloudPrintPrinters,
    prefs::kCloudPrintSubmitEnabled, prefs::kCloudPrintUserSettings,

    prefs::kMaxConnectionsPerProxy,

    prefs::kAudioCaptureAllowed, prefs::kAudioCaptureAllowedUrls,
    prefs::kVideoCaptureAllowed, prefs::kVideoCaptureAllowedUrls,

#if defined(OS_CHROMEOS)
    prefs::kDeviceSettingsCache, prefs::kHardwareKeyboardLayout,
    prefs::kCarrierDealPromoShown, prefs::kShouldAutoEnroll,
    prefs::kAutoEnrollmentPowerLimit, prefs::kDeviceActivityTimes,
    prefs::kUserActivityTimes, prefs::kExternalStorageDisabled,
    prefs::kExternalStorageReadOnly, prefs::kOwnerPrimaryMouseButtonRight,
    prefs::kUptimeLimit, prefs::kRebootAfterUpdate,
    prefs::kDeviceRobotAnyApiRefreshToken, prefs::kDeviceEnrollmentRequisition,
    prefs::kDeviceEnrollmentAutoStart, prefs::kDeviceEnrollmentCanExit,
    prefs::kDeviceDMToken, prefs::kTimesHIDDialogShown,
    prefs::kUsersLastInputMethod, prefs::kEchoCheckedOffers,
    prefs::kCachedMultiProfileUserBehavior, prefs::kInitialLocale,
    prefs::kOobeComplete, prefs::kOobeScreenPending,
    prefs::kOobeControllerDetected, prefs::kCanShowOobeGoodiesPage,
    prefs::kDeviceRegistered, prefs::kEnrollmentRecoveryRequired,
    prefs::kUsedPolicyCertificates, prefs::kServerBackedDeviceState,
    prefs::kCustomizationDefaultWallpaperURL, prefs::kLogoutStartedLast,
    // prefs::kConsumerManagementStage,
    prefs::kIsBootstrappingSlave, prefs::kReportArcStatusEnabled,
    prefs::kNetworkThrottlingEnabled, prefs::kPowerMetricsDailySample,
    prefs::kPowerMetricsIdleScreenDimCount,
    prefs::kPowerMetricsIdleScreenOffCount,
    prefs::kPowerMetricsIdleSuspendCount,
    prefs::kPowerMetricsLidClosedSuspendCount, prefs::kReportingUsers,
    prefs::kArcAppInstallEventLoggingEnabled, prefs::kRemoveUsersRemoteCommand,
    prefs::kCameraMediaConsolidated,
#endif  // defined(OS_CHROMEOS)

    prefs::kClearPluginLSODataEnabled, prefs::kPepperFlashSettingsEnabled,
    prefs::kDiskCacheDir, prefs::kDiskCacheSize, prefs::kMediaCacheSize,

    prefs::kChromeOsReleaseChannel,

    prefs::kPerformanceTracingEnabled,

    prefs::kTabStripStackedLayout,

    prefs::kRegisteredBackgroundContents,

#if defined(OS_WIN)
    prefs::kLastWelcomedOSVersion, prefs::kWelcomePageOnOSUpgradeEnabled,
#endif

    prefs::kAuthSchemes, prefs::kDisableAuthNegotiateCnameLookup,
    prefs::kEnableAuthNegotiatePort, prefs::kAuthServerWhitelist,
    prefs::kAuthNegotiateDelegateWhitelist, prefs::kGSSAPILibraryName,
    prefs::kAuthAndroidNegotiateAccountType, prefs::kAllowCrossOriginAuthPrompt,

#if defined(OS_POSIX)
    prefs::kNtlmV2Enabled,
#endif  // defined(OS_POSIX)

    prefs::kCertRevocationCheckingEnabled,
    prefs::kCertRevocationCheckingRequiredLocalAnchors,
    prefs::kCertEnableSha1LocalAnchors,
    prefs::kCertEnableSymantecLegacyInfrastructure, prefs::kSSLVersionMin,
    prefs::kSSLVersionMax, prefs::kTLS13Variant, prefs::kCipherSuiteBlacklist,

    prefs::kBuiltInDnsClientEnabled, prefs::kDnsOverHttpsServers,
    prefs::kDnsOverHttpsServerMethods,

    prefs::kRegisteredProtocolHandlers, prefs::kIgnoredProtocolHandlers,
    prefs::kPolicyRegisteredProtocolHandlers,
    prefs::kPolicyIgnoredProtocolHandlers, prefs::kCustomHandlersEnabled,

#if defined(OS_MACOSX)
    prefs::kUserRemovedLoginItem, prefs::kChromeCreatedLoginItem,
    prefs::kMigratedLoginItemPref, prefs::kNotifyWhenAppsKeepChromeAlive,
#endif

    prefs::kBackgroundModeEnabled, prefs::kHardwareAccelerationModeEnabled,
    prefs::kHardwareAccelerationModePrevious,

    prefs::kDevicePolicyRefreshRate,

    prefs::kFactoryResetRequested, prefs::kFactoryResetTPMFirmwareUpdateMode,
    prefs::kDebuggingFeaturesRequested,

#if defined(OS_CHROMEOS)
    prefs::kSigninScreenTimezone, prefs::kResolveDeviceTimezoneByGeolocation,
    prefs::kResolveDeviceTimezoneByGeolocationMethod,
    prefs::kSystemTimezoneAutomaticDetectionPolicy,
#endif  // defined(OS_CHROMEOS)

    prefs::kEnableMediaRouter,
#if !defined(OS_ANDROID)
    prefs::kShowCastIconInToolbar,
#endif  // !defined(OS_ANDROID)

#if !defined(OS_ANDROID)
#if !defined(OS_CHROMEOS)
    prefs::kRelaunchNotification,
#endif  // !defined(OS_CHROMEOS)
    prefs::kRelaunchNotificationPeriod,
#endif  // !defined(OS_ANDROID)

#if !defined(OS_ANDROID)
    prefs::kAttemptedToEnableAutoupdate,

    prefs::kMediaGalleriesUniqueId, prefs::kMediaGalleriesRememberedGalleries,
#endif  // !defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
    prefs::kShelfChromeIconIndex, prefs::kPinnedLauncherApps,
    prefs::kPolicyPinnedLauncherApps,
#endif  // defined(OS_CHROMEOS)

#if defined(OS_WIN)
    prefs::kNetworkProfileWarningsLeft, prefs::kNetworkProfileLastWarningTime,
#endif

#if defined(OS_CHROMEOS)
    prefs::kRLZBrand, prefs::kRLZDisabled,
#endif

#if BUILDFLAG(ENABLE_APP_LIST)
    prefs::kAppListLocalState,
#endif  // BUILDFLAG(ENABLE_APP_LIST)

#if defined(OS_WIN)
    prefs::kAppLaunchForMetroRestart, prefs::kAppLaunchForMetroRestartProfile,
#endif
    prefs::kAppShortcutsVersion,

    prefs::kModuleConflictBubbleShown,

    prefs::kDRMSalt, prefs::kEnableDRM,

    prefs::kWatchdogExtensionActive,

#if defined(OS_ANDROID)
    prefs::kPartnerBookmarkMappings,
#endif  // defined(OS_ANDROID)

    prefs::kQuickCheckEnabled, prefs::kPacHttpsUrlStrippingEnabled,
    prefs::kBrowserGuestModeEnabled, prefs::kBrowserAddPersonEnabled,
    prefs::kForceBrowserSignin,

    prefs::kCryptAuthDeviceId, prefs::kEasyUnlockHardlockState,
    prefs::kEasyUnlockLocalStateTpmKeys, prefs::kEasyUnlockLocalStateUserPrefs,

    prefs::kRecoveryComponentNeedsElevation,

    prefs::kRegisteredSupervisedUserWhitelists,

    prefs::kCloudPolicyOverridesMachinePolicy,

#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
// prefs::kRestartInBackground,
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
    prefs::kAnimationPolicy, prefs::kSecurityKeyPermitAttestation,
#endif

    prefs::kBackgroundTracingLastUpload,

    prefs::kAllowDinosaurEasterEgg,

#if defined(OS_ANDROID)
    prefs::kClickedUpdateMenuItem,
    prefs::kLatestVersionWhenClickedUpdateMenuItem,
#endif

    prefs::kMediaRouterCloudServicesPrefSet,
    prefs::kMediaRouterEnableCloudServices,
    prefs::kMediaRouterFirstRunFlowAcknowledged,
    prefs::kMediaRouterMediaRemotingEnabled,
    // prefs::kMediaRouterTabMirroringSources,

    prefs::kOriginTrialPublicKey, prefs::kOriginTrialDisabledFeatures,
    prefs::kOriginTrialDisabledTokens,

    prefs::kComponentUpdatesEnabled,

#if defined(OS_ANDROID)
    prefs::kLocationSettingsBackoffLevelDSE,
    prefs::kLocationSettingsBackoffLevelDefault,
    prefs::kLocationSettingsNextShowDSE,
    prefs::kLocationSettingsNextShowDefault,

    prefs::kSearchGeolocationDisclosureDismissed,
    prefs::kSearchGeolocationDisclosureShownCount,
    prefs::kSearchGeolocationDisclosureLastShowDate,
    prefs::kSearchGeolocationPreDisclosureMetricsRecorded,
    prefs::kSearchGeolocationPostDisclosureMetricsRecorded,
#endif

    prefs::kDSEGeolocationSettingDeprecated,

    prefs::kDSEPermissionsSettings, prefs::kDSEWasDisabledByPolicy,

    prefs::kWebShareVisitedTargets,

#if defined(OS_WIN)
    prefs::kIOSPromotionEligible, prefs::kIOSPromotionDone,
    prefs::kIOSPromotionSMSEntryPoint, prefs::kIOSPromotionShownEntryPoints,
    prefs::kIOSPromotionLastImpression, prefs::kIOSPromotionVariationId,
    prefs::kNumberSavePasswordsBubbleIOSPromoShown,
    prefs::kSavePasswordsBubbleIOSPromoDismissed,
    prefs::kNumberBookmarksBubbleIOSPromoShown,
    prefs::kBookmarksBubbleIOSPromoDismissed,
    prefs::kNumberBookmarksFootNoteIOSPromoShown,
    prefs::kBookmarksFootNoteIOSPromoDismissed,
    prefs::kNumberHistoryPageIOSPromoShown,
    prefs::kHistoryPageIOSPromoDismissed,

#if defined(GOOGLE_CHROME_BUILD)
    prefs::kIncompatibleApplications, prefs::kModuleBlacklistCacheMD5Digest,
    prefs::kProblematicPrograms, prefs::kThirdPartyBlockingEnabled,
#endif  // defined(GOOGLE_CHROME_BUILD)
#endif  // defined(OS_WIN)

    prefs::kSettingsResetPromptPromptWave,
    prefs::kSettingsResetPromptLastTriggeredForDefaultSearch,
    prefs::kSettingsResetPromptLastTriggeredForStartupUrls,
    prefs::kSettingsResetPromptLastTriggeredForHomepage,

#if defined(OS_ANDROID)
    prefs::kClipboardLastModifiedTime,
#endif

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
    prefs::kOfflinePrefetchBackoff, prefs::kOfflineUsageStartObserved,
    prefs::kOfflineUsageOnlineObserved, prefs::kOfflineUsageOfflineObserved,
    prefs::kPrefetchUsageEnabledObserved, prefs::kPrefetchUsageHasPagesObserved,
    prefs::kPrefetchUsageFetchObserved, prefs::kPrefetchUsageOpenObserved,
    prefs::kOfflineUsageTrackingDay, prefs::kOfflineUsageUnusedCount,
    prefs::kOfflineUsageStartedCount, prefs::kOfflineUsageOfflineCount,
    prefs::kOfflineUsageOnlineCount, prefs::kOfflineUsageMixedCount,
    prefs::kPrefetchUsageEnabledCount, prefs::kPrefetchUsageHasPagesCount,
    prefs::kPrefetchUsageFetchedCount, prefs::kPrefetchUsageOpenedCount,
    prefs::kPrefetchUsageMixedCount,
#endif

    prefs::kMediaEngagementSchemaVersion,

    // Preferences for recording metrics about tab and window usage.
    prefs::kTabStatsTotalTabCountMax, prefs::kTabStatsMaxTabsPerWindow,
    prefs::kTabStatsWindowCountMax, prefs::kTabStatsDailySample,

    prefs::kUnsafelyTreatInsecureOriginAsSecure,

    prefs::kIsolateOrigins, prefs::kSitePerProcess,
    prefs::kWebDriverOverridesIncompatiblePolicies,

#if !defined(OS_ANDROID)
    prefs::kAutoplayAllowed, prefs::kAutoplayWhitelist,
    prefs::kNtpCustomBackgroundDict,
#endif

// chromeos/chromeos_pref_names.h
#if defined(OS_CHROMEOS)
    chromeos::prefs::kAudioDevicesMute,
    chromeos::prefs::kAudioDevicesVolumePercent, chromeos::prefs::kAudioMute,
    chromeos::prefs::kAudioOutputAllowed, chromeos::prefs::kAudioVolumePercent,
    chromeos::prefs::kAudioDevicesState,
    chromeos::prefs::kQuirksClientLastServerCheck,
#endif  // defined(OS_CHROMEOS)

// chromeos/components/proximity_auth/proximity_auth_pref_names.h
#if defined(OS_CHROMEOS)
    proximity_auth::prefs::kEasyUnlockEnabled,
    proximity_auth::prefs::kEasyUnlockEnabledStateSet,
    proximity_auth::prefs::kEasyUnlockProximityThreshold,
    proximity_auth::prefs::kProximityAuthLastPromotionCheckTimestampMs,
    proximity_auth::prefs::kProximityAuthPromotionShownCount,
    proximity_auth::prefs::kProximityAuthRemoteBleDevices,
    proximity_auth::prefs::kProximityAuthIsChromeOSLoginEnabled,
#endif  // defined(OS_CHROMEOS)

// chromeos/components/tether/pref_names.h
#if defined(OS_CHROMEOS)
    chromeos::tether::prefs::kMostRecentTetherAvailablilityResponderIds,
    chromeos::tether::prefs::kMostRecentConnectTetheringResponderIds,
    chromeos::tether::prefs::kActiveHostStatus,
    chromeos::tether::prefs::kActiveHostDeviceId,
    chromeos::tether::prefs::kTetherNetworkGuid,
    chromeos::tether::prefs::kWifiNetworkGuid,
    chromeos::tether::prefs::kDisconnectingWifiNetworkPath,
    chromeos::tether::prefs::kHostScanCache,
#endif  // defined(OS_CHROMEOS)

// components/arc/arc_prefs.h
#if defined(OS_CHROMEOS)
    arc::prefs::kArcActiveDirectoryPlayUserId, arc::prefs::kArcApps,
    arc::prefs::kArcBackupRestoreEnabled, arc::prefs::kArcDataRemoveRequested,
    arc::prefs::kArcEnabled, arc::prefs::kArcFastAppReinstallPackages,
    arc::prefs::kArcFastAppReinstallStarted,
    arc::prefs::kArcInitialSettingsPending,
    arc::prefs::kArcPolicyComplianceReported, arc::prefs::kArcTermsAccepted,
    arc::prefs::kArcTermsShownInOobe, arc::prefs::kArcLocationServiceEnabled,
    arc::prefs::kArcPackages, arc::prefs::kArcPaiStarted,
    arc::prefs::kArcPushInstallAppsRequested,
    arc::prefs::kArcPushInstallAppsPending,
    arc::prefs::kArcSetNotificationsEnabledDeferred, arc::prefs::kArcSignedIn,
    arc::prefs::kArcSkippedReportingNotice,
    arc::prefs::kArcSupervisionTransition,
    arc::prefs::kArcCompatibleFilesystemChosen,
    arc::prefs::kArcVoiceInteractionValuePropAccepted,
    arc::prefs::kEcryptfsMigrationStrategy, arc::prefs::kSmsConnectEnabled,
    arc::prefs::kVoiceInteractionEnabled,
    arc::prefs::kVoiceInteractionContextEnabled,
    arc::prefs::kVoiceInteractionHotwordEnabled,
#endif  // defined(OS_CHROMEOS)

    // components/autofill/core/common/autofill_pref_names.h
    autofill::prefs::kAutofillAcceptSaveCreditCardPromptState,
    autofill::prefs::kAutofillBillingCustomerNumber,
    autofill::prefs::kAutofillCreditCardEnabled,
    autofill::prefs::kAutofillCreditCardSigninPromoImpressionCount,
    autofill::prefs::kAutofillProfileEnabled, autofill::prefs::kAutofillEnabled,
    autofill::prefs::kAutofillLastVersionDeduped,
    autofill::prefs::kAutofillLastVersionDisusedAddressesDeleted,
    autofill::prefs::kAutofillLastVersionDisusedCreditCardsDeleted,
    autofill::prefs::kAutofillOrphanRowsRemoved,
    autofill::prefs::kAutofillWalletImportEnabled,
    autofill::prefs::kAutofillWalletImportStorageCheckboxState,

    // components/bookmarks/common/bookmark_pref_names.h
    bookmarks::prefs::kBookmarkEditorExpandedNodes,
    bookmarks::prefs::kEditBookmarksEnabled,
    bookmarks::prefs::kManagedBookmarks,
    bookmarks::prefs::kManagedBookmarksFolderName,
    bookmarks::prefs::kShowAppsShortcutInBookmarkBar,
    bookmarks::prefs::kShowManagedBookmarksInBookmarkBar,
    bookmarks::prefs::kShowBookmarkBar,

    // components/certificate_transparency/pref_names.h
    certificate_transparency::prefs::kCTRequiredHosts,
    certificate_transparency::prefs::kCTExcludedHosts,
    certificate_transparency::prefs::kCTExcludedSPKIs,
    certificate_transparency::prefs::kCTExcludedLegacySPKIs,

    // components/component_updater/pref_names.h
    prefs::kRecoveryComponentVersion, prefs::kRecoveryComponentUnpackPath,

#if defined(OS_WIN)
    // Local state prefs.
    prefs::kSwReporterLastExitCode, prefs::kSwReporterLastTimeTriggered,
    prefs::kSwReporterLastTimeSentReport, prefs::kSwReporterEnabled,
    prefs::kSwReporterReportingEnabled,

    // Profile prefs.
    // prefs::kSwReporterPromptReason,
    prefs::kSwReporterPromptVersion, prefs::kSwReporterPromptSeed,
#endif

    // components/consent_auditor/pref_names.h
    consent_auditor::prefs::kLocalConsentsDictionary,

// components/cryptauth/pref_names.h
#if defined(CHROMEOS)
    cryptauth::prefs::kCryptAuthDeviceSyncLastSyncTimeSeconds,
    cryptauth::prefs::kCryptAuthDeviceSyncIsRecoveringFromFailure,
    cryptauth::prefs::kCryptAuthDeviceSyncReason,
    cryptauth::prefs::kCryptAuthDeviceSyncUnlockKeys,
    cryptauth::prefs::kCryptAuthEnrollmentIsRecoveringFromFailure,
    cryptauth::prefs::kCryptAuthEnrollmentLastEnrollmentTimeSeconds,
    cryptauth::prefs::kCryptAuthEnrollmentReason,
    cryptauth::prefs::kCryptAuthEnrollmentUserPublicKey,
    cryptauth::prefs::kCryptAuthEnrollmentUserPrivateKey,
    cryptauth::prefs::kCryptAuthGCMRegistrationId,
#endif  // defined(CHROMEOS)

    // components/data_reduction_proxy/core/common/
    // data_reduction_proxy_pref_names.h
    data_reduction_proxy::prefs::
        kDailyContentLengthHttpsWithDataReductionProxyEnabled,
    data_reduction_proxy::prefs::
        kDailyContentLengthLongBypassWithDataReductionProxyEnabled,
    data_reduction_proxy::prefs::
        kDailyContentLengthShortBypassWithDataReductionProxyEnabled,
    data_reduction_proxy::prefs::
        kDailyContentLengthUnknownWithDataReductionProxyEnabled,
    data_reduction_proxy::prefs::kDailyContentLengthViaDataReductionProxy,
    data_reduction_proxy::prefs::
        kDailyContentLengthViaDataReductionProxyApplication,
    data_reduction_proxy::prefs::kDailyContentLengthViaDataReductionProxyVideo,
    data_reduction_proxy::prefs::
        kDailyContentLengthViaDataReductionProxyUnknown,
    data_reduction_proxy::prefs::
        kDailyContentLengthWithDataReductionProxyEnabled,
    data_reduction_proxy::prefs::
        kDailyContentLengthWithDataReductionProxyEnabledApplication,
    data_reduction_proxy::prefs::
        kDailyContentLengthWithDataReductionProxyEnabledVideo,
    data_reduction_proxy::prefs::
        kDailyContentLengthWithDataReductionProxyEnabledUnknown,
    data_reduction_proxy::prefs::kDailyHttpContentLengthLastUpdateDate,
    data_reduction_proxy::prefs::kDailyHttpOriginalContentLength,
    data_reduction_proxy::prefs::kDailyHttpOriginalContentLengthApplication,
    data_reduction_proxy::prefs::kDailyHttpOriginalContentLengthVideo,
    data_reduction_proxy::prefs::kDailyHttpOriginalContentLengthUnknown,
    data_reduction_proxy::prefs::kDailyHttpReceivedContentLength,
    data_reduction_proxy::prefs::kDailyHttpReceivedContentLengthApplication,
    data_reduction_proxy::prefs::kDailyHttpReceivedContentLengthVideo,
    data_reduction_proxy::prefs::kDailyHttpReceivedContentLengthUnknown,
    data_reduction_proxy::prefs::
        kDailyOriginalContentLengthViaDataReductionProxy,
    data_reduction_proxy::prefs::
        kDailyOriginalContentLengthViaDataReductionProxyApplication,
    data_reduction_proxy::prefs::
        kDailyOriginalContentLengthViaDataReductionProxyVideo,
    data_reduction_proxy::prefs::
        kDailyOriginalContentLengthViaDataReductionProxyUnknown,
    data_reduction_proxy::prefs::
        kDailyOriginalContentLengthWithDataReductionProxyEnabled,
    data_reduction_proxy::prefs::
        kDailyOriginalContentLengthWithDataReductionProxyEnabledApplication,
    data_reduction_proxy::prefs::
        kDailyOriginalContentLengthWithDataReductionProxyEnabledVideo,
    data_reduction_proxy::prefs::
        kDailyOriginalContentLengthWithDataReductionProxyEnabledUnknown,
    data_reduction_proxy::prefs::kDataReductionProxy,
    data_reduction_proxy::prefs::kDataReductionProxyConfig,
    data_reduction_proxy::prefs::kDataUsageReportingEnabled,
    data_reduction_proxy::prefs::kDataReductionProxyWasEnabledBefore,
    data_reduction_proxy::prefs::kDataReductionProxyLastEnabledTime,
    data_reduction_proxy::prefs::
        kDataReductionProxySavingsClearedNegativeSystemClock,
    data_reduction_proxy::prefs::kHttpOriginalContentLength,
    data_reduction_proxy::prefs::kHttpReceivedContentLength,
    data_reduction_proxy::prefs::kDataReductionProxyLastConfigRetrievalTime,
    data_reduction_proxy::prefs::kNetworkProperties,

    data_reduction_proxy::prefs::kThisWeekNumber,
    data_reduction_proxy::prefs::kThisWeekServicesDownstreamBackgroundKB,
    data_reduction_proxy::prefs::kThisWeekServicesDownstreamForegroundKB,
    data_reduction_proxy::prefs::kLastWeekServicesDownstreamBackgroundKB,
    data_reduction_proxy::prefs::kLastWeekServicesDownstreamForegroundKB,
    data_reduction_proxy::prefs::kThisWeekUserTrafficContentTypeDownstreamKB,
    data_reduction_proxy::prefs::kLastWeekUserTrafficContentTypeDownstreamKB,

    // components/dom_distiller/core/pref_names.h
    dom_distiller::prefs::kFont, dom_distiller::prefs::kTheme,
    dom_distiller::prefs::kFontScale,
    dom_distiller::prefs::kReaderForAccessibility,

// components/drive/drive_pref_names.h
#if defined(OS_CHROMEOS)
    drive::prefs::kDisableDrive, drive::prefs::kDisableDriveOverCellular,
    drive::prefs::kDisableDriveHostedFiles,
#endif  // defined(OS_CHROMEOS)

// components/feed/core/pref_names.h
#if defined(OS_ANDROID)
#if BUILDFLAG(ENABLE_FEED_IN_CHROME)
    feed::prefs::kBackgroundRefreshPeriod, feed::prefs::kLastFetchAttemptTime,
    feed::prefs::kUserClassifierAverageNTPOpenedPerHour,
    feed::prefs::kUserClassifierAverageSuggestionsUsedPerHour,
    feed::prefs::kUserClassifierLastTimeToOpenNTP,
    feed::prefs::kUserClassifierLastTimeToUseSuggestions,
#endif  // BUILDFLAG(ENABLE_FEED_IN_CHROME)
#endif  // defined(OS_ANDROID)

    // components/flags_ui/flags_ui_pref_names.h
    flags_ui::prefs::kEnabledLabsExperiments,

    // components/google/core/browser/google_pref_names.h
    prefs::kLastKnownGoogleURL, prefs::kLastPromptedGoogleURL,

    // components/invalidation/impl/invalidation_prefs.h
    invalidation::prefs::kInvalidatorClientId,
    invalidation::prefs::kInvalidatorInvalidationState,
    invalidation::prefs::kInvalidatorSavedInvalidations,
    invalidation::prefs::kInvalidationServiceUseGCMChannel,

    // components/language/core/browser/pref_names.h
    language::prefs::kApplicationLocale, language::prefs::kUserLanguageProfile,

    // components/metrics/metrics_pref_names.h
    // metrics::prefs::kDeprecatedMetricsInitialLogs,
    // metrics::prefs::kDeprecatedMetricsOngoingLogs,
    metrics::prefs::kInstallDate, metrics::prefs::kMetricsClientID,
    metrics::prefs::kMetricsDefaultOptIn, metrics::prefs::kMetricsInitialLogs,
    metrics::prefs::kMetricsLowEntropySource, metrics::prefs::kMetricsMachineId,
    metrics::prefs::kMetricsOngoingLogs, metrics::prefs::kMetricsResetIds,

    metrics::prefs::kMetricsReportingEnabled,
    metrics::prefs::kMetricsReportingEnabledTimestamp,
    metrics::prefs::kMetricsSessionID, metrics::prefs::kMetricsLastSeenPrefix,

    // Preferences for recording stability logs.
    metrics::prefs::kStabilityBreakpadRegistrationFail,
    metrics::prefs::kStabilityBreakpadRegistrationSuccess,
    metrics::prefs::kStabilityBrowserLastLiveTimeStamp,
    metrics::prefs::kStabilityChildProcessCrashCount,
    metrics::prefs::kStabilityCrashCount,
    metrics::prefs::kStabilityCrashCountWithoutGmsCoreUpdate,
    metrics::prefs::kStabilityDebuggerNotPresent,
    metrics::prefs::kStabilityDebuggerPresent,
    metrics::prefs::kStabilityDeferredCount,
    metrics::prefs::kStabilityDiscardCount,
    metrics::prefs::kStabilityExecutionPhase,
    metrics::prefs::kStabilityExitedCleanly,
    metrics::prefs::kStabilityExtensionRendererCrashCount,
    metrics::prefs::kStabilityExtensionRendererFailedLaunchCount,
    metrics::prefs::kStabilityExtensionRendererLaunchCount,
    metrics::prefs::kStabilityGmsCoreVersion,
    metrics::prefs::kStabilityIncompleteSessionEndCount,
    metrics::prefs::kStabilityLaunchCount,
    metrics::prefs::kStabilityPageLoadCount,
    metrics::prefs::kStabilityRendererCrashCount,
    metrics::prefs::kStabilityRendererFailedLaunchCount,
    metrics::prefs::kStabilityRendererHangCount,
    metrics::prefs::kStabilityRendererLaunchCount,
    metrics::prefs::kStabilitySavedSystemProfile,
    metrics::prefs::kStabilitySavedSystemProfileHash,
    metrics::prefs::kStabilitySessionEndCompleted,
    metrics::prefs::kStabilityStatsBuildTime,
    metrics::prefs::kStabilityStatsVersion,
    metrics::prefs::kStabilitySystemCrashCount,
    metrics::prefs::kStabilityVersionMismatchCount,

    // Preferences for generating metrics at uninstall time.
    metrics::prefs::kUninstallLaunchCount,
    metrics::prefs::kUninstallMetricsPageLoadCount,
    metrics::prefs::kUninstallMetricsUptimeSec,

    // For measuring data use for throttling UMA log uploads on cellular.
    metrics::prefs::kUkmCellDataUse, metrics::prefs::kUmaCellDataUse,
    metrics::prefs::kUserCellDataUse,

    // components/network_time/network_time_pref_names.h
    network_time::prefs::kNetworkTimeMapping,
    network_time::prefs::kNetworkTimeQueriesEnabled,

    // components/ntp_snippets/pref_names.h
    ntp_snippets::prefs::kEnableSnippets,
    ntp_snippets::prefs::kArticlesListVisible,
    ntp_snippets::prefs::kRemoteSuggestionCategories,
    ntp_snippets::prefs::kSnippetLastFetchAttemptTime,
    ntp_snippets::prefs::kSnippetLastSuccessfulFetchTime,
    ntp_snippets::prefs::kSnippetPersistentFetchingIntervalWifi,
    ntp_snippets::prefs::kSnippetPersistentFetchingIntervalFallback,
    ntp_snippets::prefs::kSnippetStartupFetchingIntervalWifi,
    ntp_snippets::prefs::kSnippetStartupFetchingIntervalFallback,
    ntp_snippets::prefs::kSnippetShownFetchingIntervalWifi,
    ntp_snippets::prefs::kSnippetShownFetchingIntervalFallback,
    ntp_snippets::prefs::kSnippetFetcherRequestCount,
    ntp_snippets::prefs::kSnippetFetcherInteractiveRequestCount,
    ntp_snippets::prefs::kSnippetFetcherRequestsDay,
    ntp_snippets::prefs::kSnippetThumbnailsRequestCount,
    ntp_snippets::prefs::kSnippetThumbnailsInteractiveRequestCount,
    ntp_snippets::prefs::kSnippetThumbnailsRequestsDay,
    ntp_snippets::prefs::kDismissedAssetDownloadSuggestions,
    ntp_snippets::prefs::kDismissedForeignSessionsSuggestions,
    ntp_snippets::prefs::kDismissedOfflinePageDownloadSuggestions,
    ntp_snippets::prefs::kDismissedCategories,
    ntp_snippets::prefs::kUserClassifierAverageNTPOpenedPerHour,
    ntp_snippets::prefs::kUserClassifierAverageSuggestionsShownPerHour,
    ntp_snippets::prefs::kUserClassifierAverageSuggestionsUsedPerHour,
    ntp_snippets::prefs::kUserClassifierLastTimeToOpenNTP,
    ntp_snippets::prefs::kUserClassifierLastTimeToShowSuggestions,
    ntp_snippets::prefs::kUserClassifierLastTimeToUseSuggestions,
    ntp_snippets::prefs::kClickBasedCategoryRankerOrderWithClicks,
    ntp_snippets::prefs::kClickBasedCategoryRankerLastDecayTime,
    ntp_snippets::prefs::kBreakingNewsSubscriptionDataToken,
    ntp_snippets::prefs::kBreakingNewsSubscriptionDataIsAuthenticated,
    ntp_snippets::prefs::kBreakingNewsGCMSubscriptionTokenCache,
    ntp_snippets::prefs::kBreakingNewsGCMLastTokenValidationTime,
    ntp_snippets::prefs::kBreakingNewsGCMLastForcedSubscriptionTime,

    // components/ntp_tiles/pref_names.h
    ntp_tiles::prefs::kNumPersonalTiles,

    ntp_tiles::prefs::kPopularSitesOverrideURL,
    ntp_tiles::prefs::kPopularSitesOverrideDirectory,
    ntp_tiles::prefs::kPopularSitesOverrideCountry,
    ntp_tiles::prefs::kPopularSitesOverrideVersion,

    ntp_tiles::prefs::kPopularSitesLastDownloadPref,
    ntp_tiles::prefs::kPopularSitesURLPref,
    ntp_tiles::prefs::kPopularSitesJsonPref,
    ntp_tiles::prefs::kPopularSitesVersionPref,

    // components/omnibox/browser/omnibox_pref_names.h
    omnibox::kZeroSuggestCachedResults,

    // components/onc/onc_pref_names.h
    onc::prefs::kDeviceOpenNetworkConfiguration,
    onc::prefs::kOpenNetworkConfiguration,

    // components/password_manager/core/common/password_manager_pref_names.h
    password_manager::prefs::kCredentialsEnableAutosignin,
    password_manager::prefs::kCredentialsEnableService,

#if !defined(OS_MACOSX) && !defined(OS_CHROMEOS) && defined(OS_POSIX)
    password_manager::prefs::kLocalProfileId,
#endif

#if defined(OS_WIN)
    password_manager::prefs::kOsPasswordBlank,
    password_manager::prefs::kOsPasswordLastChanged,
#endif

#if defined(OS_MACOSX)
    password_manager::prefs::kKeychainMigrationStatus,
#endif

    password_manager::prefs::kWasAutoSignInFirstRunExperienceShown,
    password_manager::prefs::kWasSignInPasswordPromoClicked,
    password_manager::prefs::kNumberSignInPasswordPromoShown,
    password_manager::prefs::kSyncPasswordHash,
    password_manager::prefs::kSyncPasswordLengthAndHashSalt,
    password_manager::prefs::kBlacklistedCredentialsStripped,
    password_manager::prefs::kPasswordHashDataList,

    // components/policy/core/common/policy_pref_names.h
    policy::policy_prefs::kLastPolicyStatisticsUpdate,
    policy::policy_prefs::kUrlBlacklist, policy::policy_prefs::kUrlWhitelist,
    policy::policy_prefs::kUserPolicyRefreshRate,
    policy::policy_prefs::kMachineLevelUserCloudPolicyEnrollmentToken,

    // components/proxy_config/proxy_config_pref_names.h
    // proxy_config::prefs::kProxy,
    proxy_config::prefs::kUseSharedProxies,

    // components/proxy_config/proxy_prefs.h
    ProxyPrefs::kDirectProxyModeName, ProxyPrefs::kAutoDetectProxyModeName,
    ProxyPrefs::kPacScriptProxyModeName, ProxyPrefs::kFixedServersProxyModeName,
    ProxyPrefs::kSystemProxyModeName,

    // components/rappor/rappor_pref_names.h
    rappor::prefs::kRapporCohortDeprecated, rappor::prefs::kRapporCohortSeed,
    rappor::prefs::kRapporLastDailySample, rappor::prefs::kRapporSecret,

    // components/rappor/rappor_prefs.h
    rappor::internal::kLoadCohortHistogramName,
    rappor::internal::kLoadSecretHistogramName,

    // components/reading_list/core/reading_list_pref_names.h
    reading_list::prefs::kReadingListHasUnseenEntries,

    // components/safe_browsing/common/safe_browsing_prefs.h
    prefs::kSafeBrowsingExtendedReportingOptInAllowed,
    prefs::kSafeBrowsingIncidentsSent,
    prefs::kSafeBrowsingProceedAnywayDisabled,
    prefs::kSafeBrowsingSawInterstitialExtendedReporting,
    prefs::kSafeBrowsingSawInterstitialScoutReporting,
    prefs::kSafeBrowsingScoutGroupSelected,
    prefs::kSafeBrowsingScoutReportingEnabled,
    prefs::kSafeBrowsingTriggerEventTimestamps,
    prefs::kSafeBrowsingUnhandledSyncPasswordReuses,
    prefs::kSafeBrowsingWhitelistDomains,
    prefs::kPasswordProtectionChangePasswordURL,
    prefs::kPasswordProtectionLoginURLs,
    prefs::kPasswordProtectionWarningTrigger,

    // components/search_engines/search_engines_pref_names.h
    prefs::kSyncedDefaultSearchProviderGUID,
    prefs::kDefaultSearchProviderEnabled, prefs::kSearchProviderOverrides,
    prefs::kSearchProviderOverridesVersion, prefs::kCountryIDAtInstall,

// components/signin/core/browser/signin_pref_names.h
#if defined(OS_CHROMEOS)
    prefs::kAccountConsistencyMirrorRequired,
#endif
    prefs::kAccountIdMigrationState, prefs::kAutologinEnabled,
    prefs::kGaiaCookieHash, prefs::kGaiaCookieChangedTime,
    prefs::kGaiaCookiePeriodicReportTime, prefs::kGoogleServicesAccountId,
    prefs::kGoogleServicesHostedDomain, prefs::kGoogleServicesLastAccountId,
    prefs::kGoogleServicesLastUsername,
    prefs::kGoogleServicesSigninScopedDeviceId,
    prefs::kGoogleServicesUserAccountId, prefs::kGoogleServicesUsername,
    prefs::kGoogleServicesUsernamePattern, prefs::kReverseAutologinEnabled,
    prefs::kReverseAutologinRejectedEmailList, prefs::kSignedInTime,
    prefs::kSigninAllowed, prefs::kTokenServiceDiceCompatible,
    prefs::kTokenServiceExcludeAllSecondaryAccounts,
    prefs::kTokenServiceExcludedSecondaryAccounts,

    // components/spellcheck/browser/pref_names.h
    spellcheck::prefs::kSpellCheckEnable,
    spellcheck::prefs::kSpellCheckDictionaries,
    spellcheck::prefs::kSpellCheckForcedDictionaries,
    spellcheck::prefs::kSpellCheckDictionary,
    spellcheck::prefs::kSpellCheckUseSpellingService,

    // components/startup_metric_utils/browser/pref_names.h
    startup_metric_utils::prefs::kLastStartupTimestamp,
    startup_metric_utils::prefs::kLastStartupVersion,
    startup_metric_utils::prefs::kSameVersionStartupCount,

    // components/suggestions/suggestions_pref_names.h
    suggestions::prefs::kSuggestionsBlacklist,
    suggestions::prefs::kSuggestionsData,

    // components/sync/base/pref_names.h
    syncer::prefs::kSyncLastSyncedTime, syncer::prefs::kSyncLastPollTime,
    syncer::prefs::kSyncShortPollIntervalSeconds,
    syncer::prefs::kSyncLongPollIntervalSeconds,
    syncer::prefs::kSyncHasAuthError, syncer::prefs::kSyncFirstSetupComplete,
    syncer::prefs::kSyncKeepEverythingSynced, syncer::prefs::kSyncAppList,
    syncer::prefs::kSyncAppNotifications, syncer::prefs::kSyncAppSettings,
    syncer::prefs::kSyncApps, syncer::prefs::kSyncArcPackage,
    syncer::prefs::kSyncArticles, syncer::prefs::kSyncAutofillProfile,
    syncer::prefs::kSyncAutofillWallet,
    syncer::prefs::kSyncAutofillWalletMetadata, syncer::prefs::kSyncAutofill,
    syncer::prefs::kSyncBookmarks, syncer::prefs::kSyncDeviceInfo,
    syncer::prefs::kSyncDictionary, syncer::prefs::kSyncExtensionSettings,
    syncer::prefs::kSyncExtensions, syncer::prefs::kSyncFaviconImages,
    syncer::prefs::kSyncFaviconTracking,
    syncer::prefs::kSyncHistoryDeleteDirectives,
    syncer::prefs::kSyncMountainShares, syncer::prefs::kSyncPasswords,
    syncer::prefs::kSyncPreferences, syncer::prefs::kSyncPriorityPreferences,
    syncer::prefs::kSyncPrinters, syncer::prefs::kSyncReadingList,
    syncer::prefs::kSyncSearchEngines, syncer::prefs::kSyncSessions,
    syncer::prefs::kSyncSupervisedUserSettings,
    syncer::prefs::kSyncSupervisedUserSharedSettings,
    syncer::prefs::kSyncSupervisedUserWhitelists,
    syncer::prefs::kSyncSupervisedUsers,
    syncer::prefs::kSyncSyncedNotificationAppInfo,
    syncer::prefs::kSyncSyncedNotifications, syncer::prefs::kSyncTabs,
    syncer::prefs::kSyncThemes, syncer::prefs::kSyncTypedUrls,
    syncer::prefs::kSyncUserConsents, syncer::prefs::kSyncUserEvents,
    syncer::prefs::kSyncWifiCredentials, syncer::prefs::kSyncManaged,
    syncer::prefs::kSyncSuppressStart,
    syncer::prefs::kSyncEncryptionBootstrapToken,
    syncer::prefs::kSyncKeystoreEncryptionBootstrapToken,
    syncer::prefs::kSyncSessionsGUID,

#if defined(OS_CHROMEOS)
    syncer::prefs::kSyncSpareBootstrapToken,
#endif  // defined(OS_CHROMEOS)

    syncer::prefs::kSyncFirstSyncTime, syncer::prefs::kSyncPassphrasePrompted,
    syncer::prefs::kSyncMemoryPressureWarningCount,
    syncer::prefs::kSyncShutdownCleanly,
    syncer::prefs::kSyncInvalidationVersions,
    syncer::prefs::kSyncLastRunVersion,
    syncer::prefs::kSyncPassphraseEncryptionTransitionInProgress,
    syncer::prefs::kSyncNigoriStateForPassphraseTransition,
    syncer::prefs::kEnableLocalSyncBackend, syncer::prefs::kLocalSyncBackendDir,

    // components/translate/core/browser/translate_pref_names.h
    prefs::kOfferTranslateEnabled,

    // components/translate/core/browser/translate_prefs.h
    // translate::TranslatePrefs::kPrefLanguageProfile,
    // translate::TranslatePrefs::kPrefForceTriggerTranslateCount,
    // translate::TranslatePrefs::kPrefTranslateSiteBlacklist,
    // translate::TranslatePrefs::kPrefTranslateWhitelists,
    translate::TranslatePrefs::kPrefTranslateDeniedCount,
    translate::TranslatePrefs::kPrefTranslateIgnoredCount,
    translate::TranslatePrefs::kPrefTranslateAcceptedCount,
    // translate::TranslatePrefs::kPrefTranslateBlockedLanguages,
    translate::TranslatePrefs::kPrefTranslateLastDeniedTimeForLanguage,
    translate::TranslatePrefs::kPrefTranslateTooOftenDeniedForLanguage,
    translate::TranslatePrefs::kPrefTranslateRecentTarget,
#if defined(OS_ANDROID)
    translate::TranslatePrefs::kPrefTranslateAutoAlwaysCount,
    translate::TranslatePrefs::kPrefTranslateAutoNeverCount,
#endif

    // components/ukm/ukm_pref_names.h
    ukm::prefs::kUkmClientId, ukm::prefs::kUkmPersistedLogs,
    ukm::prefs::kUkmSessionId,

    // components/unified_consent/pref_names.h
    unified_consent::prefs::kUnifiedConsentGiven,
    unified_consent::prefs::kUnifiedConsentMigrationState,
    unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled,

    // components/variations/pref_names.h
    variations::prefs::kVariationsCompressedSeed,
    variations::prefs::kVariationsCountry,
    variations::prefs::kVariationsCrashStreak,
    variations::prefs::kVariationsFailedToFetchSeedStreak,
    variations::prefs::kVariationsLastFetchTime,
    variations::prefs::kVariationsPermanentConsistencyCountry,
    variations::prefs::kVariationsPermutedEntropyCache,
    variations::prefs::kVariationsRestrictParameter,
    variations::prefs::kVariationsSafeCompressedSeed,
    variations::prefs::kVariationsSafeSeedDate,
    variations::prefs::kVariationsSafeSeedFetchTime,
    variations::prefs::kVariationsSafeSeedLocale,
    variations::prefs::kVariationsSafeSeedPermanentConsistencyCountry,
    variations::prefs::kVariationsSafeSeedSessionConsistencyCountry,
    variations::prefs::kVariationsSafeSeedSignature,
    variations::prefs::kVariationsSeedDate,
    variations::prefs::kVariationsSeedSignature,

    // components/web_resource/web_resource_pref_names.h
    prefs::kEulaAccepted,

// extensions/browser/api/audio/pref_names.h
#if !defined(OS_ANDROID)
    extensions::kAudioApiStableDeviceIds,
#endif  // !defined(OS_ANDROID)

// extensions/browser/pref_names.h
#if !defined(OS_ANDROID)
    extensions::pref_names::kAlertsInitialized,
    extensions::pref_names::kAllowedInstallSites,
    extensions::pref_names::kAllowedTypes,
    extensions::pref_names::kAppFullscreenAllowed,
    extensions::pref_names::kBookmarkAppCreationLaunchType,
    extensions::pref_names::kExtensions,
    extensions::pref_names::kExtensionManagement,
    extensions::pref_names::kInstallAllowList,
    extensions::pref_names::kInstallDenyList,
    extensions::pref_names::kInstallForceList,
    extensions::pref_names::kInstallLoginScreenAppList,
    extensions::pref_names::kLastChromeVersion,
    extensions::pref_names::kLastUpdateCheck,
    extensions::pref_names::kNativeMessagingBlacklist,
    extensions::pref_names::kNativeMessagingWhitelist,
    extensions::pref_names::kNativeMessagingUserLevelHosts,
    extensions::pref_names::kNextUpdateCheck,
    extensions::pref_names::kStorageGarbageCollect,
    extensions::pref_names::kToolbar, extensions::pref_names::kToolbarSize,
    extensions::pref_names::kPrefPreferences,
    extensions::pref_names::kPrefIncognitoPreferences,
    extensions::pref_names::kPrefRegularOnlyPreferences,
    extensions::pref_names::kPrefContentSettings,
    extensions::pref_names::kPrefIncognitoContentSettings,
#endif  //! defined(OS_ANDROID)

    // services/preferences/public/cpp/tracked/pref_names.h
    user_prefs::kPreferenceResetTime,

// ui/base/ime/chromeos/extension_ime_util.h
#if defined(OS_CHROMEOS)
    chromeos::extension_ime_util::kBrailleImeExtensionId,
    chromeos::extension_ime_util::kBrailleImeExtensionPath,

    // TODO(https://crbug.com/861722): Check with code owners why this pref is
    // required in tests, if possible, update tests and remove.
    chromeos::extension_ime_util::kBrailleImeEngineId,
    chromeos::extension_ime_util::kArcImeLanguage,
#endif  // defined(OS_CHROMEOS)

// ui/chromeos/events/pref_names.h
#if defined(OS_CHROMEOS)
    prefs::kLanguageRemapCapsLockKeyTo, prefs::kLanguageRemapSearchKeyTo,
    prefs::kLanguageRemapControlKeyTo, prefs::kLanguageRemapAltKeyTo,
    prefs::kLanguageRemapEscapeKeyTo, prefs::kLanguageRemapBackspaceKeyTo,
    prefs::kLanguageRemapDiamondKeyTo,
#endif  // defined(OS_CHROMEOS)
};

}  // namespace

namespace prefs {

// TODO(https://crbug.com/861722): Remove this function and file.
// WARNING: PLEASE DO NOT ADD ANYTHING TO THIS FILE.
// This is temporarily added for transition of incognito preferences
// storage default, from on disk to in memory. All items in this list will be
// audited and checked with owners and removed from whitelist.
void GetIncognitoWhitelist(std::vector<const char*>* whitelist) {
  whitelist->insert(
      whitelist->end(), incognito_whitelist,
      incognito_whitelist + sizeof(incognito_whitelist) / sizeof(char*));
}

}  // namespace prefs
