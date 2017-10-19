// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webui_url_constants.h"

#include "base/macros.h"
#include "components/nacl/common/features.h"
#include "components/safe_browsing/web_ui/constants.h"
#include "extensions/features/features.h"

namespace chrome {

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
const char kChromeUIComponentsURL[] = "chrome://components/";
const char kChromeUIConflictsURL[] = "chrome://conflicts/";
const char kChromeUIConstrainedHTMLTestURL[] = "chrome://constrained-test/";
const char kChromeUICrashesURL[] = "chrome://crashes/";
const char kChromeUICreditsURL[] = "chrome://credits/";
const char kChromeUIDevicesURL[] = "chrome://devices/";
const char kChromeUIDevToolsCustomURL[] =
    "chrome-devtools://devtools/custom/inspector.html";
const char kChromeUIDevToolsURL[] =
    "chrome-devtools://devtools/bundled/inspector.html";
const char kChromeUIDomainReliabilityInternalsURL[] =
    "chrome://domain-reliability-internals/";
const char kChromeUIDownloadsURL[] = "chrome://downloads/";
const char kChromeUIExtensionIconURL[] = "chrome://extension-icon/";
const char kChromeUIExtensionsFrameURL[] = "chrome://extensions-frame/";
const char kChromeUIExtensionsURL[] = "chrome://extensions/";
const char kChromeUIFaviconURL[] = "chrome://favicon/";
const char kChromeUIFeedbackURL[] = "chrome://feedback/";
const char kChromeUIFlagsURL[] = "chrome://flags/";
const char kChromeUIFlashURL[] = "chrome://flash/";
const char kChromeUIGCMInternalsURL[] = "chrome://gcm-internals/";
const char kChromeUIHelpFrameURL[] = "chrome://help-frame/";
const char kChromeUIHelpURL[] = "chrome://help/";
const char kChromeUIHistoryURL[] = "chrome://history/";
const char kDeprecatedChromeUIHistoryFrameURL[] = "chrome://history-frame/";
const char kChromeUIIdentityInternalsURL[] = "chrome://identity-internals/";
const char kChromeUIInspectURL[] = "chrome://inspect/";
const char kChromeUIInstantURL[] = "chrome://instant/";
const char kChromeUIInterstitialURL[] = "chrome://interstitials/";
const char kChromeUIInterventionsInternalsURL[] =
    "chrome://interventions-internals/";
const char kChromeUIInvalidationsURL[] = "chrome://invalidations/";
const char kChromeUIMemoryInternalsURL[] = "chrome://memory-internals/";
const char kChromeUIPolicyToolURL[] = "chrome://policy-tool";
const char kChromeUINaClURL[] = "chrome://nacl/";
const char kChromeUINetInternalsURL[] = "chrome://net-internals/";
const char kChromeUINewProfileURL[] = "chrome://newprofile/";
const char kChromeUINewTabURL[] = "chrome://newtab/";
const char kChromeUINTPTilesInternalsURL[] = "chrome://ntp-tiles-internals/";
const char kChromeUIOmniboxURL[] = "chrome://omnibox/";
const char kChromeUIPolicyURL[] = "chrome://policy/";
const char kChromeUIMdUserManagerUrl[] = "chrome://md-user-manager/";
const char kChromeUIPrintURL[] = "chrome://print/";
const char kChromeUIQuitURL[] = "chrome://quit/";
const char kChromeUIRestartURL[] = "chrome://restart/";
const char kChromeUISettingsURL[] = "chrome://settings/";
const char kChromeUIContentSettingsURL[] = "chrome://settings/content";
const char kChromeUISiteDetailsPrefixURL[] =
    "chrome://settings/content/siteDetails?site=";
const char kChromeUISigninEmailConfirmationURL[] =
    "chrome://signin-email-confirmation";
const char kChromeUISigninErrorURL[] = "chrome://signin-error/";
const char kChromeUISuggestionsURL[] = "chrome://suggestions/";
const char kChromeUISupervisedUserPassphrasePageURL[] =
    "chrome://managed-user-passphrase/";
const char kChromeUISyncConfirmationURL[] = "chrome://sync-confirmation/";
const char kChromeUITermsURL[] = "chrome://terms/";
const char kChromeUIThemeURL[] = "chrome://theme/";
const char kChromeUIThumbnailURL[] = "chrome://thumb/";
const char kChromeUIThumbnailListURL[] = "chrome://thumbnails/";
const char kChromeUIUberFrameURL[] = "chrome://uber-frame/";
const char kChromeUIUserActionsURL[] = "chrome://user-actions/";
const char kChromeUIVersionURL[] = "chrome://version/";
const char kChromeUIWelcomeURL[] = "chrome://welcome/";
const char kChromeUIWelcomeWin10URL[] = "chrome://welcome-win10/";

#if defined(OS_ANDROID)
const char kChromeUIContextualSearchPromoURL[] =
    "chrome://contextual-search-promo";
const char kChromeUIJavaCrashURL[] = "chrome://java-crash/";
const char kChromeUINativeScheme[] = "chrome-native";
const char kChromeUINativeNewTabURL[] = "chrome-native://newtab/";
const char kChromeUINativeBookmarksURL[] = "chrome-native://bookmarks/";
const char kChromeUINativePhysicalWebDiagnosticsURL[] =
    "chrome-native://physical-web-diagnostics/";
const char kChromeUINativeRecentTabsURL[] = "chrome-native://recent-tabs/";
const char kChromeUINativeHistoryURL[] = "chrome-native://history/";
const char kChromeUIWebApksURL[] = "chrome://webapks/";
#endif

#if defined(OS_CHROMEOS)
const char kChromeUIBluetoothPairingURL[] = "chrome://bluetooth-pairing/";
const char kChromeUICertificateManagerDialogURL[] =
    "chrome://certificate-manager/";
const char kChromeUIDeviceEmulatorURL[] = "chrome://device-emulator/";
const char kChromeUIFirstRunURL[] = "chrome://first-run/";
const char kChromeUIKeyboardOverlayURL[] = "chrome://keyboardoverlay/";
const char kChromeUIMobileSetupURL[] = "chrome://mobilesetup/";
const char kChromeUIOobeURL[] = "chrome://oobe/";
const char kChromeUIOSCreditsURL[] = "chrome://os-credits/";
const char kChromeUIIntenetConfigDialogURL[] =
    "chrome://internet-config-dialog/";
const char kChromeUIIntenetDetailDialogURL[] =
    "chrome://internet-detail-dialog/";
const char kChromeUIScreenlockIconURL[] = "chrome://screenlock-icon/";
const char kChromeUISetTimeURL[] = "chrome://set-time/";
const char kChromeUISimUnlockURL[] = "chrome://sim-unlock/";
const char kChromeUISlowURL[] = "chrome://slow/";
const char kChromeUISysInternalsURL[] = "chrome://sys-internals/";
const char kChromeUISystemInfoURL[] = "chrome://system/";
const char kChromeUITermsOemURL[] = "chrome://terms/oem";
const char kChromeUIUserImageURL[] = "chrome://userimage/";
const char kChromeUIMdCupsSettingsURL[] = "chrome://settings/cupsPrinters";
#endif  // defined(OS_CHROMEOS)

#if defined(OS_WIN)
const char kChromeUIMetroFlowURL[] = "chrome://make-metro/";
#endif

#if (defined(OS_LINUX) && defined(TOOLKIT_VIEWS)) || defined(USE_AURA)
const char kChromeUITabModalConfirmDialogURL[] =
    "chrome://tab-modal-confirm-dialog/";
#endif

#if BUILDFLAG(ENABLE_WEBRTC)
const char kChromeUIWebRtcLogsURL[] = "chrome://webrtc-logs/";
#endif

const char kChromeUIMediaRouterURL[] = "chrome://media-router/";
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)
const char kChromeUICastURL[] = "chrome://cast/";
#endif

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
const char kChromeUIDiscardsURL[] = "chrome://discards/";
#endif

// Add Chrome UI hosts here, in alphabetical order.
// Add hosts to kChromePaths in browser_about_handler.cc to be listed by
// chrome://chrome-urls (about:about) and the built-in AutocompleteProvider.
const char kChromeUIAboutHost[] = "about";
const char kChromeUIBlankHost[] = "blank";
const char kChromeUIAppLauncherPageHost[] = "apps";
const char kChromeUIAppListStartPageHost[] = "app-list";
const char kChromeUIBluetoothInternalsHost[] = "bluetooth-internals";
const char kChromeUIBookmarksHost[] = "bookmarks";
const char kChromeUICacheHost[] = "cache";
const char kChromeUICertificateViewerHost[] = "view-cert";
const char kChromeUICertificateViewerDialogHost[] = "view-cert-dialog";
const char kChromeUIChromeSigninHost[] = "chrome-signin";
const char kChromeUIChromeURLsHost[] = "chrome-urls";
const char kChromeUIConflictsHost[] = "conflicts";
const char kChromeUIConstrainedHTMLTestHost[] = "constrained-test";
const char kChromeUICrashesHost[] = "crashes";
const char kChromeUICrashHost[] = "crash";
const char kChromeUICreditsHost[] = "credits";
const char kChromeUIDefaultHost[] = "version";
const char kChromeUIDelayedHangUIHost[] = "delayeduithreadhang";
const char kChromeUIDeviceLogHost[] = "device-log";
const char kChromeUIDevicesHost[] = "devices";
const char kChromeUIDevToolsHost[] = "devtools";
const char kChromeUIDevToolsBlankPath[] = "blank";
const char kChromeUIDevToolsBundledPath[] = "bundled";
const char kChromeUIDevToolsCustomPath[] = "custom";
const char kChromeUIDevToolsRemotePath[] = "remote";
const char kChromeUIDNSHost[] = "dns";
const char kChromeUIDomainReliabilityInternalsHost[] =
    "domain-reliability-internals";
const char kChromeUIDownloadsHost[] = "downloads";
const char kChromeUIDownloadInternalsHost[] = "download-internals";
const char kChromeUIDriveInternalsHost[] = "drive-internals";
const char kChromeUIExtensionIconHost[] = "extension-icon";
const char kChromeUIExtensionsFrameHost[] = "extensions-frame";
const char kChromeUIExtensionsHost[] = "extensions";
const char kChromeUIFaviconHost[] = "favicon";
const char kChromeUIFeedbackHost[] = "feedback";
const char kChromeUIFlagsHost[] = "flags";
const char kChromeUIFlashHost[] = "flash";
const char kChromeUIGCMInternalsHost[] = "gcm-internals";
const char kChromeUIHangHost[] = "hang";
const char kChromeUIHangUIHost[] = "uithreadhang";
const char kChromeUIHelpFrameHost[] = "help-frame";
const char kChromeUIHelpHost[] = "help";
const char kChromeUIHistoryHost[] = "history";
const char kDeprecatedChromeUIHistoryFrameHost[] = "history-frame";
const char kChromeUIIdentityInternalsHost[] = "identity-internals";
const char kChromeUIInspectHost[] = "inspect";
const char kChromeUIInstantHost[] = "instant";
const char kChromeUIInterstitialHost[] = "interstitials";
const char kChromeUIInterventionsInternalsHost[] = "interventions-internals";
const char kChromeUIInvalidationsHost[] = "invalidations";
const char kChromeUIKillHost[] = "kill";
const char kChromeUILargeIconHost[] = "large-icon";
const char kChromeUILocalStateHost[] = "local-state";
const char kChromeUIPolicyToolHost[] = "policy-tool";
const char kChromeUIMediaEngagementHost[] = "media-engagement";
const char kChromeUIMemoryInternalsHost[] = "memory-internals";
const char kChromeUINaClHost[] = "nacl";
const char kChromeUINetExportHost[] = "net-export";
const char kChromeUINetInternalsHost[] = "net-internals";
const char kChromeUINewTabHost[] = "newtab";
const char kChromeUINTPTilesInternalsHost[] = "ntp-tiles-internals";
const char kChromeUIOmniboxHost[] = "omnibox";
const char kChromeUIPasswordManagerInternalsHost[] =
    "password-manager-internals";
const char kChromeUIPhysicalWebHost[] = "physical-web";
const char kChromeUIPrefsInternalsHost[] = "prefs-internals";
const char kChromeUIComponentsHost[] = "components";
const char kChromeUIPolicyHost[] = "policy";
const char kChromeUIMdUserManagerHost[] = "md-user-manager";
const char kChromeUIPredictorsHost[] = "predictors";
const char kChromeUIQuotaInternalsHost[] = "quota-internals";
const char kChromeUIQuitHost[] = "quit";
const char kChromeUIRestartHost[] = "restart";
const char kChromeUISettingsHost[] = "settings";
const char kChromeUIShorthangHost[] = "shorthang";
const char kChromeUISigninEmailConfirmationHost[] = "signin-email-confirmation";
const char kChromeUISigninErrorHost[] = "signin-error";
const char kChromeUISignInInternalsHost[] = "signin-internals";
const char kChromeUISiteEngagementHost[] = "site-engagement";
const char kChromeUISuggestionsHost[] = "suggestions";
const char kChromeUISupervisedUserInternalsHost[] = "supervised-user-internals";
const char kChromeUISupervisedUserPassphrasePageHost[] =
    "managed-user-passphrase";
const char kChromeUISyncConfirmationHost[] = "sync-confirmation";
const char kChromeUISyncHost[] = "sync";
const char kChromeUISyncFileSystemInternalsHost[] = "syncfs-internals";
const char kChromeUISyncInternalsHost[] = "sync-internals";
const char kChromeUISyncResourcesHost[] = "syncresources";
const char kChromeUISystemInfoHost[] = "system";
const char kChromeUITaskSchedulerInternalsHost[] = "taskscheduler-internals";
const char kChromeUITermsHost[] = "terms";
const char kChromeUIThemeHost[] = "theme";
const char kChromeUIThumbnailHost[] = "thumb";
const char kChromeUIThumbnailHost2[] = "thumb2";
const char kChromeUIThumbnailListHost[] = "thumbnails";
const char kChromeUITranslateInternalsHost[] = "translate-internals";
const char kChromeUIUberFrameHost[] = "uber-frame";
const char kChromeUIUberHost[] = "chrome";
const char kChromeUIUsbInternalsHost[] = "usb-internals";
const char kChromeUIUserActionsHost[] = "user-actions";
const char kChromeUIVersionHost[] = "version";
const char kChromeUIWelcomeHost[] = "welcome";
const char kChromeUIWelcomeWin10Host[] = "welcome-win10";
const char kChromeUIWorkersHost[] = "workers";

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
const char kChromeUIPrintHost[] = "print";
#endif  // ENABLE_PRINT_PREVIEW

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
const char kChromeUIDiscardsHost[] = "discards";
#endif

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
const char kChromeUILinuxProxyConfigHost[] = "linux-proxy-config";
#endif

#if defined(OS_LINUX) || defined(OS_ANDROID)
const char kChromeUISandboxHost[] = "sandbox";
#endif

#if defined(OS_ANDROID)
const char kChromeUIContextualSearchPromoHost[] = "contextual-search-promo";
const char kChromeUIOfflineInternalsHost[] = "offline-internals";
const char kChromeUIPhysicalWebDiagnosticsHost[] = "physical-web-diagnostics";
const char kChromeUISnippetsInternalsHost[] = "snippets-internals";
const char kChromeUIWebApksHost[] = "webapks";
#endif

#if defined(OS_CHROMEOS)
const char kChromeUIActivationMessageHost[] = "activationmessage";
const char kChromeUIAppLaunchHost[] = "app-launch";
const char kChromeUIBluetoothPairingHost[] = "bluetooth-pairing";
const char kChromeUICertificateManagerHost[] = "certificate-manager";
const char kChromeUIChooseMobileNetworkHost[] = "choose-mobile-network";
const char kChromeUICryptohomeHost[] = "cryptohome";
const char kChromeUIDeviceEmulatorHost[] = "device-emulator";
const char kChromeUIFirstRunHost[] = "first-run";
const char kChromeUIKeyboardOverlayHost[] = "keyboardoverlay";
const char kChromeUILoginContainerHost[] = "login-container";
const char kChromeUILoginHost[] = "login";
const char kChromeUIMobileSetupHost[] = "mobilesetup";
const char kChromeUINetworkHost[] = "network";
const char kChromeUIOobeHost[] = "oobe";
const char kChromeUIOSCreditsHost[] = "os-credits";
const char kChromeUIPowerHost[] = "power";
const char kChromeUIInternetConfigDialogHost[] = "internet-config-dialog";
const char kChromeUIInternetDetailDialogHost[] = "internet-detail-dialog";
const char kChromeUIRotateHost[] = "rotate";
const char kChromeUIScreenlockIconHost[] = "screenlock-icon";
const char kChromeUISetTimeHost[] = "set-time";
const char kChromeUISimUnlockHost[] = "sim-unlock";
const char kChromeUISlowHost[] = "slow";
const char kChromeUISlowTraceHost[] = "slow_trace";
const char kChromeUISysInternalsHost[] = "sys-internals";
const char kChromeUIUserImageHost[] = "userimage";
const char kChromeUIVoiceSearchHost[] = "voicesearch";
#endif  // defined(OS_CHROMEOS)

#if defined(OS_WIN)
const char kChromeUIMetroFlowHost[] = "make-metro";
#endif

#if (defined(OS_LINUX) && defined(TOOLKIT_VIEWS)) || defined(USE_AURA)
const char kChromeUITabModalConfirmDialogHost[] = "tab-modal-confirm-dialog";
#endif

#if BUILDFLAG(ENABLE_WEBRTC)
const char kChromeUIWebRtcLogsHost[] = "webrtc-logs";
#endif

const char kChromeUIMediaRouterHost[] = "media-router";
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)
const char kChromeUICastHost[] = "cast";
#endif

// Settings sub pages.

// NOTE: Add sub page paths to kChromeSettingsSubPages in
// chrome_autocomplete_provider_client.cc to be listed by the built-in
// AutocompleteProvider.

const char kAutofillSubPage[] = "autofill";
const char kClearBrowserDataSubPage[] = "clearBrowserData";
const char kContentSettingsSubPage[] = "content";
const char kDeprecatedExtensionsSubPage[] = "extensions";
const char kHandlerSettingsSubPage[] = "handlers";
const char kImportDataSubPage[] = "importData";
const char kLanguageOptionsSubPage[] = "languages";
const char kPasswordManagerSubPage[] = "passwords";
const char kResetProfileSettingsSubPage[] = "resetProfileSettings";
const char kSearchEnginesSubPage[] = "searchEngines";
const char kSignOutSubPage[] = "signOut";
const char kSyncSetupSubPage[] = "syncSetup";
const char kTriggeredResetProfileSettingsSubPage[] =
    "triggeredResetProfileSettings";
#if defined(OS_CHROMEOS)
const char kAccessibilitySubPage[] = "accessibility";
const char kBluetoothSubPage[] = "bluetoothDevices";
const char kDateTimeSubPage[] = "dateTime";
const char kDisplaySubPage[] = "display";
const char kHelpSubPage[] = "help";
const char kInternetSubPage[] = "internet";
const char kNetworkDetailSubPage[] = "networkDetail";
const char kPowerSubPage[] = "power";
const char kStylusSubPage[] = "stylus";
#else
const char kCreateProfileSubPage[] = "createProfile";
const char kManageProfileSubPage[] = "manageProfile";
#endif

// Extension sub pages.
const char kExtensionConfigureCommandsSubPage[] = "configureCommands";

// Add hosts here to be included in chrome://chrome-urls (about:about).
// These hosts will also be suggested by BuiltinProvider.
const char* const kChromeHostURLs[] = {
    kChromeUIAboutHost,
    kChromeUIBluetoothInternalsHost,
    kChromeUICacheHost,
    kChromeUIChromeURLsHost,
    kChromeUIComponentsHost,
    kChromeUICrashesHost,
    kChromeUICreditsHost,
    kChromeUIDNSHost,
#if defined(OS_CHROMEOS) && !defined(OFFICIAL_BUILD)
    kChromeUIDeviceEmulatorHost,
#endif
    kChromeUIDeviceLogHost,
    kChromeUIDownloadInternalsHost,
    kChromeUIFlagsHost,
    kChromeUIGCMInternalsHost,
    kChromeUIHistoryHost,
    kChromeUIInterventionsInternalsHost,
    kChromeUIInvalidationsHost,
    kChromeUILocalStateHost,
    kChromeUIMediaEngagementHost,
    kChromeUINetExportHost,
    kChromeUINetInternalsHost,
    kChromeUINewTabHost,
    kChromeUIOmniboxHost,
    kChromeUIPasswordManagerInternalsHost,
    kChromeUIPolicyHost,
    kChromeUIPredictorsHost,
    kChromeUIQuotaInternalsHost,
    kChromeUISignInInternalsHost,
    kChromeUISiteEngagementHost,
    kChromeUINTPTilesInternalsHost,
    safe_browsing::kChromeUISafeBrowsingHost,
    kChromeUISuggestionsHost,
    kChromeUISupervisedUserInternalsHost,
    kChromeUISyncInternalsHost,
    kChromeUITaskSchedulerInternalsHost,
    kChromeUITermsHost,
    kChromeUIThumbnailListHost,
    kChromeUITranslateInternalsHost,
    kChromeUIUsbInternalsHost,
    kChromeUIUserActionsHost,
    kChromeUIVersionHost,
    content::kChromeUIAccessibilityHost,
    content::kChromeUIAppCacheInternalsHost,
    content::kChromeUIBlobInternalsHost,
    content::kChromeUIDinoHost,
    content::kChromeUIGpuHost,
    content::kChromeUIHistogramHost,
    content::kChromeUIIndexedDBInternalsHost,
    content::kChromeUIMediaInternalsHost,
    content::kChromeUINetworkErrorHost,
    content::kChromeUINetworkErrorsListingHost,
    content::kChromeUINetworkViewCacheHost,
    content::kChromeUIServiceWorkerInternalsHost,
    content::kChromeUITracingHost,
    content::kChromeUIWebRTCInternalsHost,
#if !defined(OS_ANDROID)
#if !defined(OS_CHROMEOS)
    kChromeUIAppLauncherPageHost,
#endif
    kChromeUIBookmarksHost,
    kChromeUIDownloadsHost,
    kChromeUIFlashHost,
    kChromeUIHelpHost,
    kChromeUIInspectHost,
    kChromeUISettingsHost,
    kChromeUISystemInfoHost,
    kChromeUIUberHost,
#endif
#if defined(OS_ANDROID)
    kChromeUIOfflineInternalsHost,
    kChromeUISnippetsInternalsHost,
    kChromeUIWebApksHost,
#endif
#if defined(OS_CHROMEOS)
    kChromeUICertificateManagerHost,
    kChromeUIChooseMobileNetworkHost,
    kChromeUICryptohomeHost,
    kChromeUIDriveInternalsHost,
    kChromeUIFirstRunHost,
    kChromeUIKeyboardOverlayHost,
    kChromeUILoginHost,
    kChromeUINetworkHost,
    kChromeUIOobeHost,
    kChromeUIOSCreditsHost,
    kChromeUIPowerHost,
    kChromeUIInternetConfigDialogHost,
    kChromeUIInternetDetailDialogHost,
    kChromeUIVoiceSearchHost,
#endif
#if defined(OS_WIN) || defined(OS_CHROMEOS)
    kChromeUIDiscardsHost,
#endif
#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
    kChromeUILinuxProxyConfigHost,
#endif
#if defined(OS_LINUX) || defined(OS_ANDROID)
    kChromeUISandboxHost,
#endif
#if defined(OS_WIN)
    kChromeUIConflictsHost,
#endif
#if BUILDFLAG(ENABLE_NACL)
    kChromeUINaClHost,
#endif
#if BUILDFLAG(ENABLE_EXTENSIONS)
    kChromeUIExtensionsHost,
#endif
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
    kChromeUIPrintHost,
#endif
#if BUILDFLAG(ENABLE_SERVICE_DISCOVERY)
    kChromeUIDevicesHost,
#endif
#if BUILDFLAG(ENABLE_WEBRTC)
    kChromeUIWebRtcLogsHost,
#endif
};
const size_t kNumberOfChromeHostURLs = arraysize(kChromeHostURLs);

const char* const kChromeDebugURLs[] = {content::kChromeUIBadCastCrashURL,
                                        content::kChromeUIBrowserCrashURL,
                                        content::kChromeUICrashURL,
                                        content::kChromeUIDumpURL,
                                        content::kChromeUIKillURL,
                                        content::kChromeUIHangURL,
                                        content::kChromeUIShorthangURL,
                                        content::kChromeUIGpuCleanURL,
                                        content::kChromeUIGpuCrashURL,
                                        content::kChromeUIGpuHangURL,
                                        content::kChromeUIMemoryExhaustURL,
                                        content::kChromeUIPpapiFlashCrashURL,
                                        content::kChromeUIPpapiFlashHangURL,
#if defined(OS_ANDROID)
                                        content::kChromeUIGpuJavaCrashURL,
                                        chrome::kChromeUIJavaCrashURL,
#endif
                                        chrome::kChromeUIQuitURL,
                                        chrome::kChromeUIRestartURL};
const size_t kNumberOfChromeDebugURLs = arraysize(kChromeDebugURLs);

}  // namespace chrome
