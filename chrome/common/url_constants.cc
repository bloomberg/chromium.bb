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

const char kAboutPluginsURL[] = "about:plugins";
const char kAboutVersionURL[] = "about:version";

// Add Chrome UI URLs as necessary, in alphabetical order.
// Be sure to add the corresponding kChromeUI*Host constant below.
// This is a WebUI page that lists other WebUI pages.
const char kChromeUIAboutURL[] = "chrome://about/";
const char kChromeUIAppsURL[] = "chrome://apps/";
const char kChromeUIBookmarksURL[] = "chrome://bookmarks/";
const char kChromeUICertificateViewerURL[] = "chrome://view-cert/";
const char kChromeUIChromeURLsURL[] = "chrome://chrome-urls/";
const char kChromeUICloudPrintResourcesURL[] = "chrome://cloudprintresources/";
const char kChromeUIConflictsURL[] = "chrome://conflicts/";
const char kChromeUIConstrainedHTMLTestURL[] = "chrome://constrained-test/";
const char kChromeUICrashesURL[] = "chrome://crashes/";
const char kChromeUICreditsURL[] = "chrome://credits/";
const char kChromeUIDevicesURL[] = "chrome://devices/";
const char kChromeUIDevToolsURL[] =
    "chrome-devtools://devtools/bundled/devtools.html";
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
const char kChromeUIHelpFrameURL[] = "chrome://help-frame/";
const char kChromeUIHistoryURL[] = "chrome://history/";
const char kChromeUIHistoryFrameURL[] = "chrome://history-frame/";
const char kChromeUIIdentityInternalsURL[] = "chrome://identity-internals/";
const char kChromeUIInlineLoginURL[] = "chrome://inline-login/";
const char kChromeUIInspectURL[] = "chrome://inspect/";
const char kChromeUIInstantURL[] = "chrome://instant/";
const char kChromeUIIPCURL[] = "chrome://ipc/";
const char kChromeUIManagedUserPassphrasePageURL[] =
    "chrome://managed-user-passphrase/";
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
const char kChromeUIRestartURL[] = "chrome://restart/";
const char kChromeUISettingsURL[] = "chrome://settings/";
const char kChromeUISettingsFrameURL[] = "chrome://settings-frame/";
const char kChromeUISuggestionsInternalsURL[] =
    "chrome://suggestions-internals/";
const char kChromeUISSLClientCertificateSelectorURL[] = "chrome://select-cert/";
const char kChromeUITaskManagerURL[] = "chrome://tasks/";
const char kChromeUITermsURL[] = "chrome://terms/";
const char kChromeUIThemeURL[] = "chrome://theme/";
const char kChromeUIThumbnailURL[] = "chrome://thumb/";
const char kChromeUIUberURL[] = "chrome://chrome/";
const char kChromeUIUberFrameURL[] = "chrome://uber-frame/";
const char kChromeUIUserActionsURL[] = "chrome://user-actions/";
const char kChromeUIVersionURL[] = "chrome://version/";

#if defined(OS_ANDROID)
const char kChromeUIWelcomeURL[] = "chrome://welcome/";
#endif

#if defined(OS_CHROMEOS)
const char kChromeUIActivationMessage[] = "chrome://activationmessage/";
const char kChromeUIAppLaunchURL[] = "chrome://app-launch/";
const char kChromeUIBluetoothPairingURL[] = "chrome://bluetooth-pairing/";
const char kChromeUIChooseMobileNetworkURL[] =
    "chrome://choose-mobile-network/";
const char kChromeUIDiagnosticsURL[] = "chrome://diagnostics/";
const char kChromeUIDiscardsURL[] = "chrome://discards/";
const char kChromeUIIdleLogoutDialogURL[] = "chrome://idle-logout/";
const char kChromeUIImageBurnerURL[] = "chrome://imageburner/";
const char kChromeUIKeyboardOverlayURL[] = "chrome://keyboardoverlay/";
const char kChromeUILockScreenURL[] = "chrome://lock/";
const char kChromeUIMediaplayerURL[] = "chrome://mediaplayer/";
const char kChromeUIMobileSetupURL[] = "chrome://mobilesetup/";
const char kChromeUIOobeURL[] = "chrome://oobe/";
const char kChromeUIOSCreditsURL[] = "chrome://os-credits/";
const char kChromeUIProxySettingsURL[] = "chrome://proxy-settings/";
const char kChromeUISimUnlockURL[] = "chrome://sim-unlock/";
const char kChromeUISlideshowURL[] = "chrome://slideshow/";
const char kChromeUISlowURL[] = "chrome://slow/";
const char kChromeUISystemInfoURL[] = "chrome://system/";
const char kChromeUITermsOemURL[] = "chrome://terms/oem";
const char kChromeUIUserImageURL[] = "chrome://userimage/";
#endif

#if defined(USE_ASH)
const char kChromeUITransparencyURL[] = "chrome://transparency/";
#endif

#if defined(FILE_MANAGER_EXTENSION)
const char kChromeUIFileManagerURL[] = "chrome://files/";
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
const char kChromeUIBookmarksHost[] = "bookmarks";
const char kChromeUICacheHost[] = "cache";
const char kChromeUICertificateViewerHost[] = "view-cert";
const char kChromeUIChromeURLsHost[] = "chrome-urls";
const char kChromeUICloudPrintResourcesHost[] = "cloudprintresources";
const char kChromeUICloudPrintSetupHost[] = "cloudprintsetup";
const char kChromeUIConflictsHost[] = "conflicts";
const char kChromeUIConstrainedHTMLTestHost[] = "constrained-test";
const char kChromeUICrashesHost[] = "crashes";
const char kChromeUICrashHost[] = "crash";
const char kChromeUICreditsHost[] = "credits";
const char kChromeUIDefaultHost[] = "version";
const char kChromeUIDevicesHost[] = "devices";
const char kChromeUIDevToolsHost[] = "devtools";
const char kChromeUIDevToolsBundledPath[] = "bundled";
const char kChromeUIDevToolsRemotePath[] = "remote";
const char kChromeUIDNSHost[] = "dns";
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
const char kChromeUIHangHost[] = "hang";
const char kChromeUIHelpFrameHost[] = "help-frame";
const char kChromeUIHelpHost[] = "help";
const char kChromeUIHistoryHost[] = "history";
const char kChromeUIHistoryFrameHost[] = "history-frame";
const char kChromeUIIdentityInternalsHost[] = "identity-internals";
const char kChromeUIInlineLoginHost[] = "inline-login";
const char kChromeUIInspectHost[] = "inspect";
const char kChromeUIInstantHost[] = "instant";
const char kChromeUIIPCHost[] = "ipc";
const char kChromeUIKillHost[] = "kill";
const char kChromeUIManagedUserPassphrasePageHost[] = "managed-user-passphrase";
const char kChromeUIMemoryHost[] = "memory";
const char kChromeUIMemoryInternalsHost[] = "memory-internals";
const char kChromeUIMemoryRedirectHost[] = "memory-redirect";
const char kChromeUIMetroFlowHost[] = "make-metro";
const char kChromeUINaClHost[] = "nacl";
const char kChromeUINetExportHost[] = "net-export";
const char kChromeUINetInternalsHost[] = "net-internals";
const char kChromeUINewTabHost[] = "newtab";
const char kChromeUIOmniboxHost[] = "omnibox";
const char kChromeUIPerformanceMonitorHost[] = "performance";
const char kChromeUIPluginsHost[] = "plugins";
const char kChromeUIPolicyHost[] = "policy";
const char kChromeUIProfileSigninConfirmationHost[] =
    "profile-signin-confirmation";
const char kChromeUIUserManagerHost[] = "user-manager";
const char kChromeUIPredictorsHost[] = "predictors";
const char kChromeUIPrintHost[] = "print";
const char kChromeUIProfilerHost[] = "profiler";
const char kChromeUIQuotaInternalsHost[] = "quota-internals";
const char kChromeUIRestartHost[] = "restart";
const char kChromeUISettingsHost[] = "settings";
const char kChromeUISettingsFrameHost[] = "settings-frame";
const char kChromeUIShorthangHost[] = "shorthang";
const char kChromeUISignInInternalsHost[] = "signin-internals";
const char kChromeUISuggestionsInternalsHost[] = "suggestions-internals";
const char kChromeUISSLClientCertificateSelectorHost[] = "select-cert";
const char kChromeUIStatsHost[] = "stats";
const char kChromeUISyncHost[] = "sync";
const char kChromeUISyncFileSystemInternalsHost[] = "syncfs-internals";
const char kChromeUISyncInternalsHost[] = "sync-internals";
const char kChromeUISyncResourcesHost[] = "syncresources";
const char kChromeUITaskManagerHost[] = "tasks";
const char kChromeUITermsHost[] = "terms";
const char kChromeUIThemeHost[] = "theme";
const char kChromeUIThumbnailHost[] = "thumb";
const char kChromeUITouchIconHost[] = "touch-icon";
const char kChromeUITranslateInternalsHost[] = "translate-internals";
const char kChromeUIUberFrameHost[] = "uber-frame";
const char kChromeUIUberHost[] = "chrome";
const char kChromeUIUserActionsHost[] = "user-actions";
const char kChromeUIVersionHost[] = "version";
const char kChromeUIWorkersHost[] = "workers";

const char kChromeUIScreenshotPath[] = "screenshots";
const char kChromeUIThemePath[] = "theme";

#if defined(OS_ANDROID)
const char kChromeUIWelcomeHost[] = "welcome";
#endif

#if defined(OS_LINUX) || defined(OS_OPENBSD)
const char kChromeUILinuxProxyConfigHost[] = "linux-proxy-config";
const char kChromeUISandboxHost[] = "sandbox";
#endif

#if defined(OS_CHROMEOS)
const char kChromeUIActivationMessageHost[] = "activationmessage";
const char kChromeUIAppLaunchHost[] = "app-launch";
const char kChromeUIBluetoothPairingHost[] = "bluetooth-pairing";
const char kChromeUIChooseMobileNetworkHost[] = "choose-mobile-network";
const char kChromeUICryptohomeHost[] = "cryptohome";
const char kChromeUIDiagnosticsHost[] = "diagnostics";
const char kChromeUIDiscardsHost[] = "discards";
const char kChromeUIIdleLogoutDialogHost[] = "idle-logout";
const char kChromeUIImageBurnerHost[] = "imageburner";
const char kChromeUIKeyboardOverlayHost[] = "keyboardoverlay";
const char kChromeUILockScreenHost[] = "lock";
const char kChromeUILoginContainerHost[] = "login-container";
const char kChromeUILoginHost[] = "login";
const char kChromeUIMediaplayerHost[] = "mediaplayer";
const char kChromeUIMobileSetupHost[] = "mobilesetup";
const char kChromeUINetworkHost[] = "network";
const char kChromeUIOobeHost[] = "oobe";
const char kChromeUIOSCreditsHost[] = "os-credits";
const char kChromeUIProxySettingsHost[] = "proxy-settings";
const char kChromeUIRotateHost[] = "rotate";
const char kChromeUISimUnlockHost[] = "sim-unlock";
const char kChromeUISlideshowHost[] = "slideshow";
const char kChromeUISlowHost[] = "slow";
const char kChromeUISystemInfoHost[] = "system";
const char kChromeUIUserImageHost[] = "userimage";

const char kChromeUIMenu[] = "menu";
const char kChromeUINetworkMenu[] = "network-menu";
const char kChromeUIWrenchMenu[] = "wrench-menu";

const char kEULAPathFormat[] = "/usr/share/chromeos-assets/eula/%s/eula.html";
const char kOemEulaURLPath[] = "oem";
const char kOnlineEulaURLPath[] =
    "https://www.google.com/intl/%s/chrome/eula_text.html";
#endif

#if defined(USE_ASH)
const char kChromeUITransparencyHost[] = "transparency";
#endif

#if defined(FILE_MANAGER_EXTENSION)
const char kChromeUIFileManagerHost[] = "files";
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
const char kManagedUserSettingsSubPage[] = "managedUser";
const char kManageProfileSubPage[] = "manageProfile";
const char kPasswordManagerSubPage[] = "passwords";
const char kSearchEnginesSubPage[] = "searchEngines";
const char kSearchSubPage[] = "search";
const char kSearchUsersSubPage[] = "search#Users";
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
#endif  // defined(OS_CHROMEOS)

const char kChromeSyncLearnMoreURL[] =
#if defined(OS_CHROMEOS)
    "http://support.google.com/chromeos/bin/answer.py?answer=165139";
#else
    "http://support.google.com/chrome/bin/answer.py?answer=165139";
#endif

const char kChromeEnterpriseSignInLearnMoreURL[] =
  "http://support.google.com/chromeos/bin/answer.py?hl=en&answer=1331549";

const char kSupervisedUserManagementURL[] = "https://www.chrome.com/manage";

const char kSupervisedUserManagementDisplayURL[] = "www.chrome.com/manage";

const char kSettingsSearchHelpURL[] =
#if defined(OS_CHROMEOS)
    "https://support.google.com/chromeos/?p=settings_search_help";
#else
    "https://support.google.com/chrome/?p=settings_search_help";
#endif

const char kAboutGoogleTranslateURL[] =
#if defined(OS_CHROMEOS)
    "https://support.google.com/chromeos/?p=ib_translation_bar";
#else
    "https://support.google.com/chrome/?p=ib_translation_bar";
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

const char kChromiumProjectURL[] = "http://www.chromium.org/";

const char kLearnMoreReportingURL[] =
    "https://support.google.com/chrome/?p=ui_usagestat";

const char kOutdatedPluginLearnMoreURL[] =
    "https://support.google.com/chrome/?p=ib_outdated_plugin";

const char kBlockedPluginLearnMoreURL[] =
    "https://support.google.com/chrome/?p=ib_blocked_plugin";

const char kSpeechInputAboutURL[] =
    "https://support.google.com/chrome/?p=ui_speech_input";

const char kLearnMoreRegisterProtocolHandlerURL[] =
    "https://support.google.com/chrome/?p=ib_protocol_handler";

const char kSyncLearnMoreURL[] =
    "https://support.google.com/chrome/?p=settings_sign_in";

const char kDownloadScanningLearnMoreURL[] =
    "https://support.google.com/chrome/?p=ib_download_scan";

const char kDownloadPotentiallyUnwantedLearnMoreURL[] =
    "https://support.google.com/chrome/?p=ui_protect_settings";

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

const char kSideloadWipeoutHelpURL[] =
    "https://support.google.com/chrome/?p=ui_remove_non_cws_extensions";

#if defined(OS_CHROMEOS)
const char kNaturalScrollHelpURL[] =
    "https://support.google.com/chromeos/?p=simple_scrolling";
#endif

#if defined(OS_CHROMEOS)
const char kLearnMoreEnterpriseURL[] =
    "https://support.google.com/chromeos/bin/answer.py?answer=2535613";
#endif

const char kNotificationsHelpURL[] =
    "https://support.google.com/chrome/?p=ui_notifications";

// Add hosts here to be included in chrome://chrome-urls (about:about).
// These hosts will also be suggested by BuiltinProvider.
const char* const kChromeHostURLs[] = {
  kChromeUICacheHost,
  kChromeUIChromeURLsHost,
  kChromeUICrashesHost,
  kChromeUICreditsHost,
  kChromeUIDNSHost,
  kChromeUIFlagsHost,
  kChromeUIHistoryHost,
  kChromeUIIPCHost,
  kChromeUIMemoryHost,
  kChromeUIMemoryInternalsHost,
#if defined(OS_ANDROID) || defined(OS_IOS)
  kChromeUINetExportHost,
#endif
  kChromeUINetInternalsHost,
  kChromeUINewTabHost,
  kChromeUIOmniboxHost,
  kChromeUIPredictorsHost,
  kChromeUIProfilerHost,
  kChromeUIQuotaInternalsHost,
  kChromeUISignInInternalsHost,
  kChromeUIStatsHost,
  kChromeUISyncInternalsHost,
  kChromeUITermsHost,
  kChromeUITranslateInternalsHost,
  kChromeUIUserActionsHost,
  kChromeUIVersionHost,
#if defined(OS_ANDROID)
  kChromeUIWelcomeHost,
#else
  kChromeUIBookmarksHost,
  kChromeUIDownloadsHost,
  kChromeUIFlashHost,
  kChromeUIInspectHost,
  kChromeUIPluginsHost,
  kChromeUISettingsHost,
#endif
#if defined(OS_WIN)
  kChromeUIConflictsHost,
#endif
#if defined(OS_LINUX) || defined(OS_OPENBSD)
  kChromeUILinuxProxyConfigHost,
  kChromeUISandboxHost,
#endif
#if defined(OS_CHROMEOS)
  kChromeUIChooseMobileNetworkHost,
  kChromeUICryptohomeHost,
  kChromeUIDiagnosticsHost,
  kChromeUIDiscardsHost,
  kChromeUIDriveInternalsHost,
  kChromeUIImageBurnerHost,
  kChromeUIKeyboardOverlayHost,
  kChromeUILoginHost,
  kChromeUINetworkHost,
  kChromeUIOobeHost,
  kChromeUIOSCreditsHost,
  kChromeUIProxySettingsHost,
  kChromeUISystemInfoHost,
  kChromeUITaskManagerHost,
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
  content::kChromeUIAccessibilityHost,
  content::kChromeUIAppCacheInternalsHost,
  content::kChromeUIBlobInternalsHost,
  content::kChromeUIGpuHost,
  content::kChromeUIHistogramHost,
  content::kChromeUIIndexedDBInternalsHost,
  content::kChromeUIMediaInternalsHost,
  content::kChromeUINetworkViewCacheHost,
  content::kChromeUITracingHost,
  content::kChromeUIWebRTCInternalsHost,
#if defined(ENABLE_WEBRTC)
  kChromeUIWebRtcLogsHost,
#endif
};
const size_t kNumberOfChromeHostURLs = arraysize(kChromeHostURLs);

const char* const kChromeDebugURLs[] = {
  content::kChromeUICrashURL,
  content::kChromeUIKillURL,
  content::kChromeUIHangURL,
  content::kChromeUIShorthangURL,
  content::kChromeUIGpuCleanURL,
  content::kChromeUIGpuCrashURL,
  content::kChromeUIGpuHangURL,
  content::kChromeUIPpapiFlashCrashURL,
  content::kChromeUIPpapiFlashHangURL,
  chrome::kChromeUIRestartURL
};
const int kNumberOfChromeDebugURLs =
    static_cast<int>(arraysize(kChromeDebugURLs));

const char kExtensionResourceScheme[] = "chrome-extension-resource";
const char kChromeNativeScheme[] = "chrome-native";

const char kChromeSearchScheme[] = "chrome-search";
const char kChromeSearchLocalNtpHost[] = "local-ntp";
const char kChromeSearchLocalNtpUrl[] =
    "chrome-search://local-ntp/local-ntp.html";
const char kChromeSearchOnlineNtpHost[] = "online-ntp";

const char kChromeSearchMostVisitedHost[] = "most-visited";
const char kChromeSearchMostVisitedUrl[] = "chrome-search://most-visited/";

// Google SafeSearch query parameters.
const char kSafeSearchSafeParameter[] = "safe=active";
const char kSafeSearchSsuiParameter[] = "ssui=on";

const char kMediaAccessLearnMoreUrl[] =
    "https://support.google.com/chrome/?p=ib_access_cam_mic";

}  // namespace chrome
