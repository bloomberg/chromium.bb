// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/url_constants.h"

#include <algorithm>

#include "base/basictypes.h"
#include "content/public/common/url_constants.h"
#include "url/url_util.h"

namespace chrome {

#if defined(OS_CHROMEOS)
const char kCrosScheme[] = "cros";
const char kDriveScheme[] = "drive";
#endif

// Add Chrome UI URLs as necessary, in alphabetical order.
// Be sure to add the corresponding kChromeUI*Host constant below.
// This is a WebUI page that lists other WebUI pages.
const char kChromeUIAboutURL[] = "chrome://about/";
const char kChromeUIAppsURL[] = "chrome://apps/";
const char kChromeUIAppListStartPageURL[] = "chrome://app-list/";
const char kChromeUIBookmarksURL[] = "chrome://bookmarks/";
const char kChromeUICertificateViewerURL[] = "chrome://view-cert/";
const char kChromeUICertificateViewerDialogURL[] = "chrome://view-cert-dialog/";
const char kChromeUIChromeSigninURL[] = "chrome://chrome-signin/";
const char kChromeUIChromeURLsURL[] = "chrome://chrome-urls/";
const char kChromeUICloudPrintResourcesURL[] = "chrome://cloudprintresources/";
const char kChromeUIComponentsURL[] = "chrome://components/";
const char kChromeUIConflictsURL[] = "chrome://conflicts/";
const char kChromeUIConstrainedHTMLTestURL[] = "chrome://constrained-test/";
const char kChromeUIContextualSearchPromoURL[] =
    "chrome://contextual-search-promo";
const char kChromeUICrashesURL[] = "chrome://crashes/";
const char kChromeUICreditsURL[] = "chrome://credits/";
const char kChromeUIDevicesURL[] = "chrome://devices/";
const char kChromeUIDevToolsURL[] =
    "chrome-devtools://devtools/bundled/devtools.html";
const char kChromeUIDomainReliabilityInternalsURL[] =
    "chrome://domain-reliability-internals/";
const char kChromeUIDownloadsURL[] = "chrome://downloads/";
const char kChromeUIEditSearchEngineDialogURL[] = "chrome://editsearchengine/";
const char kChromeUIExtensionIconURL[] = "chrome://extension-icon/";
const char kChromeUIExtensionInfoURL[] = "chrome://extension-info/";
const char kChromeUIExtensionsFrameURL[] = "chrome://extensions-frame/";
const char kChromeUIExtensionsURL[] = "chrome://extensions/";
const char kChromeUIFaviconURL[] = "chrome://favicon/";
const char kChromeUIFeedbackURL[] = "chrome://feedback/";
const char kChromeUIFlagsURL[] = "chrome://flags/";
const char kChromeUIFlashURL[] = "chrome://flash/";
const char kChromeUIGCMInternalsURL[] = "chrome://gcm-internals/";
const char kChromeUIHelpFrameURL[] = "chrome://help-frame/";
const char kChromeUIHistoryURL[] = "chrome://history/";
const char kChromeUIHistoryFrameURL[] = "chrome://history-frame/";
const char kChromeUIIdentityInternalsURL[] = "chrome://identity-internals/";
const char kChromeUIInspectURL[] = "chrome://inspect/";
const char kChromeUIInstantURL[] = "chrome://instant/";
const char kChromeUIInvalidationsURL[] = "chrome://invalidations/";
const char kChromeUIIPCURL[] = "chrome://ipc/";
const char kChromeUIMemoryRedirectURL[] = "chrome://memory-redirect/";
const char kChromeUIMemoryURL[] = "chrome://memory/";
const char kChromeUIMetroFlowURL[] = "chrome://make-metro/";
const char kChromeUINaClURL[] = "chrome://nacl/";
const char kChromeUINetInternalsURL[] = "chrome://net-internals/";
const char kChromeUINewProfile[] = "chrome://newprofile/";
const char kChromeUINewTabURL[] = "chrome://newtab/";
const char kChromeUIOmniboxURL[] = "chrome://omnibox/";
const char kChromeUIPerformanceMonitorURL[] = "chrome://performance/";
const char kChromeUIPluginsURL[] = "chrome://plugins/";
const char kChromeUIPolicyURL[] = "chrome://policy/";
const char kChromeUIProfileSigninConfirmationURL[] =
    "chrome://profile-signin-confirmation/";
const char kChromeUIUserManagerURL[] = "chrome://user-manager/";
const char kChromeUIPrintURL[] = "chrome://print/";
const char kChromeUIQuitURL[] = "chrome://quit/";
const char kChromeUIRestartURL[] = "chrome://restart/";
const char kChromeUISettingsURL[] = "chrome://settings/";
const char kChromeUISettingsFrameURL[] = "chrome://settings-frame/";
const char kChromeUISSLClientCertificateSelectorURL[] = "chrome://select-cert/";
const char kChromeUISuggestions[] = "chrome://suggestions/";
const char kChromeUISuggestionsInternalsURL[] =
    "chrome://suggestions-internals/";
const char kChromeUISupervisedUserPassphrasePageURL[] =
    "chrome://managed-user-passphrase/";
const char kChromeUITermsURL[] = "chrome://terms/";
const char kChromeUIThemeURL[] = "chrome://theme/";
const char kChromeUIThumbnailURL[] = "chrome://thumb/";
const char kChromeUIThumbnailListURL[] = "chrome://thumbnails/";
const char kChromeUIUberURL[] = "chrome://chrome/";
const char kChromeUIUberFrameURL[] = "chrome://uber-frame/";
const char kChromeUIUserActionsURL[] = "chrome://user-actions/";
const char kChromeUIVersionURL[] = "chrome://version/";
const char kChromeUIVoiceSearchURL[] = "chrome://voicesearch/";

#if defined(OS_ANDROID)
const char kChromeUINativeNewTabURL[] = "chrome-native://newtab/";
const char kChromeUINativeBookmarksURL[] = "chrome-native://bookmarks/";
const char kChromeUINativeRecentTabsURL[] = "chrome-native://recent-tabs/";
const char kChromeUIWelcomeURL[] = "chrome://welcome/";
#endif

#if defined(OS_CHROMEOS)
const char kChromeUIActivationMessage[] = "chrome://activationmessage/";
const char kChromeUIBluetoothPairingURL[] = "chrome://bluetooth-pairing/";
const char kChromeUICertificateManagerDialogURL[] =
    "chrome://certificate-manager/";
const char kChromeUIChargerReplacementURL[] = "chrome://charger-replacement/";
const char kChromeUIChooseMobileNetworkURL[] =
    "chrome://choose-mobile-network/";
const char kChromeUIDiscardsURL[] = "chrome://discards/";
const char kChromeUIFirstRunURL[] = "chrome://first-run/";
const char kChromeUIIdleLogoutDialogURL[] = "chrome://idle-logout/";
const char kChromeUIImageBurnerURL[] = "chrome://imageburner/";
const char kChromeUIKeyboardOverlayURL[] = "chrome://keyboardoverlay/";
const char kChromeUILockScreenURL[] = "chrome://lock/";
const char kChromeUIMediaplayerURL[] = "chrome://mediaplayer/";
const char kChromeUIMobileSetupURL[] = "chrome://mobilesetup/";
const char kChromeUINfcDebugURL[] = "chrome://nfc-debug/";
const char kChromeUIOobeURL[] = "chrome://oobe/";
const char kChromeUIOSCreditsURL[] = "chrome://os-credits/";
const char kChromeUIProxySettingsURL[] = "chrome://proxy-settings/";
const char kChromeUIScreenlockIconURL[] = "chrome://screenlock-icon/";
const char kChromeUISetTimeURL[] = "chrome://set-time/";
const char kChromeUISimUnlockURL[] = "chrome://sim-unlock/";
const char kChromeUISlideshowURL[] = "chrome://slideshow/";
const char kChromeUISlowURL[] = "chrome://slow/";
const char kChromeUISystemInfoURL[] = "chrome://system/";
const char kChromeUITermsOemURL[] = "chrome://terms/oem";
const char kChromeUIUserImageURL[] = "chrome://userimage/";
#endif

#if defined(USE_AURA)
const char kChromeUIGestureConfigURL[] = "chrome://gesture/";
const char kChromeUIGestureConfigHost[] = "gesture";
const char kChromeUISalsaURL[] = "chrome://salsa/";
const char kChromeUISalsaHost[] = "salsa";
#endif

#if (defined(OS_LINUX) && defined(TOOLKIT_VIEWS)) || defined(USE_AURA)
const char kChromeUITabModalConfirmDialogURL[] =
    "chrome://tab-modal-confirm-dialog/";
#endif

#if defined(ENABLE_WEBRTC)
const char kChromeUIWebRtcLogsURL[] = "chrome://webrtc-logs/";
#endif

// Add Chrome UI hosts here, in alphabetical order.
// Add hosts to kChromePaths in browser_about_handler.cc to be listed by
// chrome://chrome-urls (about:about) and the built-in AutocompleteProvider.
const char kChromeUIAboutHost[] = "about";
const char kChromeUIBlankHost[] = "blank";
const char kChromeUIAppLauncherPageHost[] = "apps";
const char kChromeUIAppListStartPageHost[] = "app-list";
const char kChromeUIBookmarksHost[] = "bookmarks";
const char kChromeUICacheHost[] = "cache";
const char kChromeUICertificateViewerHost[] = "view-cert";
const char kChromeUICertificateViewerDialogHost[] = "view-cert-dialog";
const char kChromeUIChromeSigninHost[] = "chrome-signin";
const char kChromeUIChromeURLsHost[] = "chrome-urls";
const char kChromeUICloudPrintResourcesHost[] = "cloudprintresources";
const char kChromeUICloudPrintSetupHost[] = "cloudprintsetup";
const char kChromeUIConflictsHost[] = "conflicts";
const char kChromeUIConstrainedHTMLTestHost[] = "constrained-test";
const char kChromeUIContextualSearchPromoHost[] = "contextual-search-promo";
const char kChromeUICrashesHost[] = "crashes";
const char kChromeUICrashHost[] = "crash";
const char kChromeUICreditsHost[] = "credits";
const char kChromeUIDefaultHost[] = "version";
const char kChromeUIDevicesHost[] = "devices";
const char kChromeUIDevToolsHost[] = "devtools";
const char kChromeUIDevToolsBundledPath[] = "bundled";
const char kChromeUIDevToolsRemotePath[] = "remote";
const char kChromeUIDNSHost[] = "dns";
const char kChromeUIDomainReliabilityInternalsHost[] =
    "domain-reliability-internals";
const char kChromeUIDownloadsHost[] = "downloads";
const char kChromeUIDriveInternalsHost[] = "drive-internals";
const char kChromeUIEditSearchEngineDialogHost[] = "editsearchengine";
const char kChromeUIExtensionIconHost[] = "extension-icon";
const char kChromeUIExtensionInfoHost[] = "extension-info";
const char kChromeUIExtensionsFrameHost[] = "extensions-frame";
const char kChromeUIExtensionsHost[] = "extensions";
const char kChromeUIFaviconHost[] = "favicon";
const char kChromeUIFeedbackHost[] = "feedback";
const char kChromeUIFlagsHost[] = "flags";
const char kChromeUIFlashHost[] = "flash";
const char kChromeUIGCMInternalsHost[] = "gcm-internals";
const char kChromeUIHangHost[] = "hang";
const char kChromeUIHelpFrameHost[] = "help-frame";
const char kChromeUIHelpHost[] = "help";
const char kChromeUIHistoryHost[] = "history";
const char kChromeUIHistoryFrameHost[] = "history-frame";
const char kChromeUIIdentityInternalsHost[] = "identity-internals";
const char kChromeUIInspectHost[] = "inspect";
const char kChromeUIInstantHost[] = "instant";
const char kChromeUIInvalidationsHost[] = "invalidations";
const char kChromeUIIPCHost[] = "ipc";
const char kChromeUIKillHost[] = "kill";
const char kChromeUIMemoryHost[] = "memory";
const char kChromeUIMemoryInternalsHost[] = "memory-internals";
const char kChromeUIMemoryRedirectHost[] = "memory-redirect";
const char kChromeUIMetroFlowHost[] = "make-metro";
const char kChromeUINaClHost[] = "nacl";
const char kChromeUINetExportHost[] = "net-export";
const char kChromeUINetInternalsHost[] = "net-internals";
const char kChromeUINewTabHost[] = "newtab";
const char kChromeUIOmniboxHost[] = "omnibox";
const char kChromeUIPasswordManagerInternalsHost[] =
    "password-manager-internals";
const char kChromeUIPerformanceMonitorHost[] = "performance";
const char kChromeUIPluginsHost[] = "plugins";
const char kChromeUIComponentsHost[] = "components";
const char kChromeUIPolicyHost[] = "policy";
const char kChromeUIProfileSigninConfirmationHost[] =
    "profile-signin-confirmation";
const char kChromeUIUserManagerHost[] = "user-manager";
const char kChromeUIPredictorsHost[] = "predictors";
const char kChromeUIPrintHost[] = "print";
const char kChromeUIProfilerHost[] = "profiler";
const char kChromeUIQuotaInternalsHost[] = "quota-internals";
const char kChromeUIQuitHost[] = "quit";
const char kChromeUIRestartHost[] = "restart";
const char kChromeUISettingsHost[] = "settings";
const char kChromeUISettingsFrameHost[] = "settings-frame";
const char kChromeUIShorthangHost[] = "shorthang";
const char kChromeUISignInInternalsHost[] = "signin-internals";
const char kChromeUISSLClientCertificateSelectorHost[] = "select-cert";
const char kChromeUIStatsHost[] = "stats";
const char kChromeUISuggestionsHost[] = "suggestions";
const char kChromeUISuggestionsInternalsHost[] = "suggestions-internals";
const char kChromeUISupervisedUserPassphrasePageHost[] =
    "managed-user-passphrase";
const char kChromeUISyncHost[] = "sync";
const char kChromeUISyncFileSystemInternalsHost[] = "syncfs-internals";
const char kChromeUISyncInternalsHost[] = "sync-internals";
const char kChromeUISyncResourcesHost[] = "syncresources";
const char kChromeUISystemInfoHost[] = "system";
const char kChromeUITaskManagerHost[] = "tasks";
const char kChromeUITermsHost[] = "terms";
const char kChromeUIThemeHost[] = "theme";
const char kChromeUIThumbnailHost[] = "thumb";
const char kChromeUIThumbnailHost2[] = "thumb2";
const char kChromeUIThumbnailListHost[] = "thumbnails";
const char kChromeUITouchIconHost[] = "touch-icon";
const char kChromeUITranslateInternalsHost[] = "translate-internals";
const char kChromeUIUberFrameHost[] = "uber-frame";
const char kChromeUIUberHost[] = "chrome";
const char kChromeUIUserActionsHost[] = "user-actions";
const char kChromeUIVersionHost[] = "version";
const char kChromeUIVoiceSearchHost[] = "voicesearch";
const char kChromeUIWorkersHost[] = "workers";

const char kChromeUIScreenshotPath[] = "screenshots";
const char kChromeUIThemePath[] = "theme";

#if defined(OS_ANDROID)
const char kChromeUIWelcomeHost[] = "welcome";
#endif

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
const char kChromeUILinuxProxyConfigHost[] = "linux-proxy-config";
const char kChromeUISandboxHost[] = "sandbox";
#endif

#if defined(OS_CHROMEOS)
const char kChromeUIActivationMessageHost[] = "activationmessage";
const char kChromeUIAppLaunchHost[] = "app-launch";
const char kChromeUIBluetoothPairingHost[] = "bluetooth-pairing";
const char kChromeUICertificateManagerHost[] = "certificate-manager";
const char kChromeUIChargerReplacementHost[] = "charger-replacement";
const char kChromeUIChooseMobileNetworkHost[] = "choose-mobile-network";
const char kChromeUICryptohomeHost[] = "cryptohome";
const char kChromeUIDiscardsHost[] = "discards";
const char kChromeUIFirstRunHost[] = "first-run";
const char kChromeUIIdleLogoutDialogHost[] = "idle-logout";
const char kChromeUIImageBurnerHost[] = "imageburner";
const char kChromeUIKeyboardOverlayHost[] = "keyboardoverlay";
const char kChromeUILockScreenHost[] = "lock";
const char kChromeUILoginContainerHost[] = "login-container";
const char kChromeUILoginHost[] = "login";
const char kChromeUIMediaplayerHost[] = "mediaplayer";
const char kChromeUIMobileSetupHost[] = "mobilesetup";
const char kChromeUINfcDebugHost[] = "nfc-debug";
const char kChromeUINetworkHost[] = "network";
const char kChromeUIOobeHost[] = "oobe";
const char kChromeUIOSCreditsHost[] = "os-credits";
const char kChromeUIPowerHost[] = "power";
const char kChromeUIProvidedFileSystemsHost[] = "provided-file-systems";
const char kChromeUIProxySettingsHost[] = "proxy-settings";
const char kChromeUIRotateHost[] = "rotate";
const char kChromeUIScreenlockIconHost[] = "screenlock-icon";
const char kChromeUISetTimeHost[] = "set-time";
const char kChromeUISimUnlockHost[] = "sim-unlock";
const char kChromeUISlideshowHost[] = "slideshow";
const char kChromeUISlowHost[] = "slow";
const char kChromeUISlowTraceHost[] = "slow_trace";
const char kChromeUIUserImageHost[] = "userimage";

const char kChromeUIMenu[] = "menu";
const char kChromeUINetworkMenu[] = "network-menu";
const char kChromeUIWrenchMenu[] = "wrench-menu";

const char kEULAPathFormat[] = "/usr/share/chromeos-assets/eula/%s/eula.html";
const char kOemEulaURLPath[] = "oem";
const char kOnlineEulaURLPath[] =
    "https://www.google.com/intl/%s/chrome/eula_text.html";
#endif

#if (defined(OS_LINUX) && defined(TOOLKIT_VIEWS)) || defined(USE_AURA)
const char kChromeUITabModalConfirmDialogHost[] = "tab-modal-confirm-dialog";
#endif

#if defined(ENABLE_WEBRTC)
const char kChromeUIWebRtcLogsHost[] = "webrtc-logs";
#endif

// Option sub pages.
// Add sub page paths to kChromeSettingsSubPages in builtin_provider.cc to be
// listed by the built-in AutocompleteProvider.
const char kAutofillSubPage[] = "autofill";
const char kClearBrowserDataSubPage[] = "clearBrowserData";
const char kContentSettingsExceptionsSubPage[] = "contentExceptions";
const char kContentSettingsSubPage[] = "content";
const char kCreateProfileSubPage[] = "createProfile";
const char kExtensionsSubPage[] = "extensions";
const char kHandlerSettingsSubPage[] = "handlers";
const char kImportDataSubPage[] = "importData";
const char kLanguageOptionsSubPage[] = "languages";
const char kManageProfileSubPage[] = "manageProfile";
const char kPasswordManagerSubPage[] = "passwords";
const char kResetProfileSettingsSubPage[] = "resetProfileSettings";
const char kSearchEnginesSubPage[] = "searchEngines";
const char kSearchSubPage[] = "search";
const char kSearchUsersSubPage[] = "search#Users";
const char kSupervisedUserSettingsSubPage[] = "managedUser";
const char kSyncSetupSubPage[] = "syncSetup";
#if defined(OS_CHROMEOS)
const char kInternetOptionsSubPage[] = "internet";
const char kBluetoothAddDeviceSubPage[] = "bluetooth";
const char kChangeProfilePictureSubPage[] = "changePicture";
#endif

// Extension sub pages.
const char kExtensionConfigureCommandsSubPage[] = "configureCommands";

const char kExtensionInvalidRequestURL[] = "chrome-extension://invalid/";
const char kExtensionResourceInvalidRequestURL[] =
    "chrome-extension-resource://invalid/";

const char kSyncGoogleDashboardURL[] =
    "https://www.google.com/settings/chrome/sync/";

const char kAutoPasswordGenerationLearnMoreURL[] =
    "https://support.google.com/chrome/?p=ui_generate_password";

const char kPasswordManagerLearnMoreURL[] =
#if defined(OS_CHROMEOS)
    "https://support.google.com/chromeos/?p=settings_password";
#else
    "https://support.google.com/chrome/?p=settings_password";
#endif

const char kExtensionControlledSettingLearnMoreURL[] =
    "https://support.google.com/chrome/?p=ui_settings_api_extension";

const char kChromeHelpViaKeyboardURL[] =
#if defined(OS_CHROMEOS)
#if defined(OFFICIAL_BUILD)
    "chrome-extension://honijodknafkokifofgiaalefdiedpko/main.html";
#else
    "https://support.google.com/chromeos/?p=help&ctx=keyboard";
#endif  // defined(OFFICIAL_BUILD
#else
    "https://support.google.com/chrome/?p=help&ctx=keyboard";
#endif  // defined(OS_CHROMEOS)

const char kChromeHelpViaMenuURL[] =
#if defined(OS_CHROMEOS)
#if defined(OFFICIAL_BUILD)
    "chrome-extension://honijodknafkokifofgiaalefdiedpko/main.html";
#else
    "https://support.google.com/chromeos/?p=help&ctx=menu";
#endif  // defined(OFFICIAL_BUILD
#else
    "https://support.google.com/chrome/?p=help&ctx=menu";
#endif  // defined(OS_CHROMEOS)

const char kChromeHelpViaWebUIURL[] =
#if defined(OS_CHROMEOS)
#if defined(OFFICIAL_BUILD)
    "chrome-extension://honijodknafkokifofgiaalefdiedpko/main.html";
#else
    "https://support.google.com/chromeos/?p=help&ctx=settings";
#endif  // defined(OFFICIAL_BUILD
#else
    "https://support.google.com/chrome/?p=help&ctx=settings";
#endif  // defined(OS_CHROMEOS)

#if defined(OS_CHROMEOS)
const char kChromeAccessibilityHelpURL[] =
    "https://support.google.com/chromeos/?p=accessibility_menu";
const char kChromeAccessibilitySettingsURL[] =
    "/chromevox/background/options.html";
#endif  // defined(OS_CHROMEOS)

#if defined(ENABLE_ONE_CLICK_SIGNIN)
const char kChromeSyncLearnMoreURL[] =
    "http://support.google.com/chrome/bin/answer.py?answer=165139";

const char kChromeSyncMergeTroubleshootingURL[] =
    "https://support.google.com/chrome/answer/1181420#merge";
#endif  // defined(ENABLE_ONE_CLICK_SIGNIN)

const char kChromeEnterpriseSignInLearnMoreURL[] =
  "http://support.google.com/chromeos/bin/answer.py?hl=en&answer=1331549";

const char kResetProfileSettingsLearnMoreURL[] =
    "https://support.google.com/chrome/?p=ui_reset_settings";

const char kAutomaticSettingsResetLearnMoreURL[] =
    "https://support.google.com/chrome/?p=ui_automatic_settings_reset";

const char kSupervisedUserManagementURL[] = "https://www.chrome.com/manage";

const char kSupervisedUserManagementDisplayURL[] = "www.chrome.com/manage";

const char kSettingsSearchHelpURL[] =
#if defined(OS_CHROMEOS)
    "https://support.google.com/chromeos/?p=settings_search_help";
#else
    "https://support.google.com/chrome/?p=settings_search_help";
#endif

const char kOmniboxLearnMoreURL[] =
#if defined(OS_CHROMEOS)
    "https://support.google.com/chromeos/?p=settings_omnibox";
#else
    "https://support.google.com/chrome/?p=settings_omnibox";
#endif

const char kPageInfoHelpCenterURL[] =
#if defined(OS_CHROMEOS)
    "https://support.google.com/chromeos/?p=ui_security_indicator";
#else
    "https://support.google.com/chrome/?p=ui_security_indicator";
#endif

const char kCrashReasonURL[] =
#if defined(OS_CHROMEOS)
    "https://support.google.com/chromeos/?p=e_awsnap";
#else
    "https://support.google.com/chrome/?p=e_awsnap";
#endif

const char kKillReasonURL[] =
#if defined(OS_CHROMEOS)
    "https://support.google.com/chromeos/?p=e_deadjim";
#else
    "https://support.google.com/chrome/?p=e_deadjim";
#endif

const char kPrivacyLearnMoreURL[] =
#if defined(OS_CHROMEOS)
    "https://support.google.com/chromeos/?p=settings_privacy";
#else
    "https://support.google.com/chrome/?p=settings_privacy";
#endif

const char kDoNotTrackLearnMoreURL[] =
#if defined(OS_CHROMEOS)
    "https://support.google.com/chromeos/?p=settings_do_not_track";
#else
    "https://support.google.com/chrome/?p=settings_do_not_track";
#endif

#if defined(OS_CHROMEOS)
const char kAttestationForContentProtectionLearnMoreURL[] =
    "https://support.google.com/chromebook/?p=verified_access";

const char kEnhancedPlaybackNotificationLearnMoreURL[] =
    "https://support.google.com/chromebook/?p=enhanced_playback";
#endif

const char kChromiumProjectURL[] = "http://www.chromium.org/";

const char kLearnMoreReportingURL[] =
    "https://support.google.com/chrome/?p=ui_usagestat";

const char kOutdatedPluginLearnMoreURL[] =
    "https://support.google.com/chrome/?p=ib_outdated_plugin";

const char kBlockedPluginLearnMoreURL[] =
    "https://support.google.com/chrome/?p=ib_blocked_plugin";

const char kHotwordLearnMoreURL[] =
    "https://support.google.com/chrome/?p=ui_hotword_search";

const char kLearnMoreRegisterProtocolHandlerURL[] =
    "https://support.google.com/chrome/?p=ib_protocol_handler";

const char kSyncLearnMoreURL[] =
    "https://support.google.com/chrome/?p=settings_sign_in";

const char kDownloadScanningLearnMoreURL[] =
    "https://support.google.com/chrome/?p=ib_download_blocked";

const char kDownloadInterruptedLearnMoreURL[] =
    "https://support.google.com/chrome/?p=ui_download_errors";

const char kSyncEverythingLearnMoreURL[] =
    "https://support.google.com/chrome/?p=settings_sync_all";

const char kCloudPrintLearnMoreURL[] =
#if defined(OS_CHROMEOS)
    "https://support.google.com/chromeos/?p=settings_cloud_print";
#else
    "https://support.google.com/chrome/?p=settings_cloud_print";
#endif

const char kCloudPrintNoDestinationsLearnMoreURL[] =
    "https://www.google.com/cloudprint/learn/";

const char kAppLauncherHelpURL[] =
    "https://support.google.com/chrome_webstore/?p=cws_app_launcher";

const char kSyncEncryptionHelpURL[] =
#if defined(OS_CHROMEOS)
    "https://support.google.com/chromeos/?p=settings_encryption";
#else
    "https://support.google.com/chrome/?p=settings_encryption";
#endif

const char kSyncErrorsHelpURL[] =
    "https://support.google.com/chrome/?p=settings_sync_error";

#if defined(OS_CHROMEOS)
const char kNaturalScrollHelpURL[] =
    "https://support.google.com/chromeos/?p=simple_scrolling";
#endif

#if defined(OS_CHROMEOS)
const char kLearnMoreEnterpriseURL[] =
    "https://support.google.com/chromeos/bin/answer.py?answer=2535613";
#endif

const char kRemoveNonCWSExtensionURL[] =
    "https://support.google.com/chrome/answer/2811969?"
    "p=ui_remove_non_cws_extensions&rd=1";

const char kCorruptExtensionURL[] =
    "https://support.google.com/chrome/?p=settings_corrupt_extension";

const char kNotificationsHelpURL[] =
    "https://support.google.com/chrome/?p=ui_notifications";

const char kNotificationWelcomeLearnMoreURL[] =
    "https://support.google.com/chrome/?p=ib_google_now_welcome";

// Add hosts here to be included in chrome://chrome-urls (about:about).
// These hosts will also be suggested by BuiltinProvider.
const char* const kChromeHostURLs[] = {
  kChromeUICacheHost,
  kChromeUIChromeURLsHost,
  kChromeUIComponentsHost,
  kChromeUICrashesHost,
  kChromeUICreditsHost,
  kChromeUIDNSHost,
  kChromeUIFlagsHost,
  kChromeUIHistoryHost,
  kChromeUIInvalidationsHost,
  kChromeUIMemoryHost,
  kChromeUIMemoryInternalsHost,
  kChromeUINetInternalsHost,
  kChromeUINewTabHost,
  kChromeUIOmniboxHost,
  kChromeUIPasswordManagerInternalsHost,
  kChromeUIPredictorsHost,
  kChromeUIProfilerHost,
  kChromeUISignInInternalsHost,
  kChromeUIStatsHost,
  kChromeUISuggestionsHost,
  kChromeUISyncInternalsHost,
  kChromeUITermsHost,
  kChromeUIThumbnailListHost,
  kChromeUITranslateInternalsHost,
  kChromeUIUserActionsHost,
  kChromeUIVersionHost,
  kChromeUIVoiceSearchHost,
  content::kChromeUIAccessibilityHost,
  content::kChromeUIAppCacheInternalsHost,
  content::kChromeUIBlobInternalsHost,
  content::kChromeUIGpuHost,
  content::kChromeUIHistogramHost,
  content::kChromeUIIndexedDBInternalsHost,
  content::kChromeUIMediaInternalsHost,
  content::kChromeUINetworkViewCacheHost,
  content::kChromeUIServiceWorkerInternalsHost,
  content::kChromeUITracingHost,
  content::kChromeUIWebRTCInternalsHost,
#if defined(OS_ANDROID)
  kChromeUIWelcomeHost,
#else
  kChromeUIAppLauncherPageHost,
  kChromeUIBookmarksHost,
  kChromeUIDownloadsHost,
  kChromeUIFlashHost,
  kChromeUIGCMInternalsHost,
  kChromeUIHelpHost,
  kChromeUIInspectHost,
  kChromeUIIPCHost,
  kChromeUIPluginsHost,
  kChromeUIQuotaInternalsHost,
  kChromeUISettingsHost,
  kChromeUISystemInfoHost,
  kChromeUIUberHost,
#endif
#if defined(OS_ANDROID) || defined(OS_IOS)
  kChromeUINetExportHost,
#endif
#if defined(OS_CHROMEOS)
  kChromeUICertificateManagerHost,
  kChromeUIChooseMobileNetworkHost,
  kChromeUICryptohomeHost,
  kChromeUIDiscardsHost,
  kChromeUIDriveInternalsHost,
  kChromeUIFirstRunHost,
  kChromeUIImageBurnerHost,
  kChromeUIKeyboardOverlayHost,
  kChromeUILoginHost,
  kChromeUINetworkHost,
  kChromeUIOobeHost,
  kChromeUIOSCreditsHost,
  kChromeUIPowerHost,
  kChromeUIProxySettingsHost,
  kChromeUITaskManagerHost,
#endif
#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
  kChromeUILinuxProxyConfigHost,
  kChromeUISandboxHost,
#endif
#if defined(OS_WIN)
  kChromeUIConflictsHost,
#endif
#if !defined(DISABLE_NACL)
  kChromeUINaClHost,
#endif
#if defined(ENABLE_CONFIGURATION_POLICY)
  kChromeUIPolicyHost,
#endif
#if defined(ENABLE_EXTENSIONS)
  kChromeUIExtensionsHost,
#endif
#if defined(ENABLE_FULL_PRINTING)
  kChromeUIPrintHost,
#endif
#if defined(ENABLE_SERVICE_DISCOVERY)
  kChromeUIDevicesHost,
#endif
#if defined(ENABLE_WEBRTC)
  kChromeUIWebRtcLogsHost,
#endif
};
const size_t kNumberOfChromeHostURLs = arraysize(kChromeHostURLs);

const char* const kChromeDebugURLs[] = {
  content::kChromeUICrashURL,
  content::kChromeUIDumpURL,
  content::kChromeUIKillURL,
  content::kChromeUIHangURL,
  content::kChromeUIShorthangURL,
  content::kChromeUIGpuCleanURL,
  content::kChromeUIGpuCrashURL,
  content::kChromeUIGpuHangURL,
  content::kChromeUIPpapiFlashCrashURL,
  content::kChromeUIPpapiFlashHangURL,
  chrome::kChromeUIQuitURL,
  chrome::kChromeUIRestartURL
};
const int kNumberOfChromeDebugURLs =
    static_cast<int>(arraysize(kChromeDebugURLs));

const char kChromeNativeScheme[] = "chrome-native";

const char kChromeSearchScheme[] = "chrome-search";
const char kChromeSearchLocalNtpHost[] = "local-ntp";
const char kChromeSearchLocalNtpUrl[] =
    "chrome-search://local-ntp/local-ntp.html";
const char kChromeSearchRemoteNtpHost[] = "remote-ntp";

const char kChromeSearchMostVisitedHost[] = "most-visited";
const char kChromeSearchMostVisitedUrl[] = "chrome-search://most-visited/";

const char kDomDistillerScheme[] = "chrome-distiller";

// Google SafeSearch query parameters.
const char kSafeSearchSafeParameter[] = "safe=active";
const char kSafeSearchSsuiParameter[] = "ssui=on";

const char kMediaAccessLearnMoreUrl[] =
    "https://support.google.com/chrome/?p=ib_access_cam_mic";

const char kLanguageSettingsLearnMoreUrl[] =
#if defined(OS_CHROMEOS)
    "https://support.google.com/chromebook/answer/1059490";
#else
    "https://support.google.com/chrome/topic/1678461";
#endif

#if defined(OS_MACOSX)
const char kMac32BitDeprecationURL[] =
#if !defined(ARCH_CPU_64_BITS)
    "https://support.google.com/chrome/?p=ui_mac_32bit_support";
#else
    "";
#endif
#endif

// TODO(tengs): Replace with real URL when ready.
const char kEasyUnlockLearnMoreUrl[] =
    "https://support.google.com/chromebook/?p=easy_unlock";
const char kEasyUnlockManagementUrl[] = "https://chrome.com";

}  // namespace chrome
