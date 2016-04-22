// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/md_settings_localized_strings_provider.h"

#include <string>

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/policy_indicator_localized_strings_provider.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/google_chrome_strings.h"
#include "chrome/grit/locale_settings.h"
#include "chrome/grit/settings_chromium_strings.h"
#include "chrome/grit/settings_google_chrome_strings.h"
#include "chrome/grit/settings_strings.h"
#include "components/google/core/browser/google_util.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "content/public/browser/web_ui_data_source.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "ash/system/chromeos/devicetype_utils.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/ui/webui/chromeos/ui_account_tweaks.h"
#include "chromeos/chromeos_switches.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"
#else
#include "chrome/browser/ui/webui/settings/system_handler.h"
#endif

namespace settings {
namespace {

// Note that settings.html contains a <script> tag which imports a script of
// the following name. These names must be kept in sync.
const char kLocalizedStringsFile[] = "strings.js";

struct LocalizedString {
  const char* name;
  int id;
};

void AddLocalizedStringsBulk(content::WebUIDataSource* html_source,
                             LocalizedString localized_strings[],
                             size_t num_strings) {
  for (size_t i = 0; i < num_strings; i++) {
    html_source->AddLocalizedString(localized_strings[i].name,
                                    localized_strings[i].id);
  }
}

void AddCommonStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
      {"add", IDS_ADD},
      {"cancel", IDS_CANCEL},
      {"disable", IDS_DISABLE},
      {"learnMore", IDS_LEARN_MORE},
      {"ok", IDS_OK},
      {"save", IDS_SAVE},
      {"advancedPageTitle", IDS_SETTINGS_ADVANCED},
      {"basicPageTitle", IDS_SETTINGS_BASIC},
      {"internalSearch", IDS_SETTINGS_INTERNAL_SEARCH},
      {"settings", IDS_SETTINGS_SETTINGS},
      {"restart", IDS_SETTINGS_RESTART},
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));
}

void AddA11yStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
    {"a11yPageTitle", IDS_SETTINGS_ACCESSIBILITY},
    {"moreFeaturesLink", IDS_SETTINGS_MORE_FEATURES_LINK},
#if defined(OS_CHROMEOS)
    {"optionsInMenuLabel", IDS_SETTINGS_OPTIONS_IN_MENU_LABEL},
    {"largeMouseCursorLabel", IDS_SETTINGS_LARGE_MOUSE_CURSOR_LABEL},
    {"highContrastLabel", IDS_SETTINGS_HIGH_CONTRAST_LABEL},
    {"stickyKeysLabel", IDS_SETTINGS_STICKY_KEYS_LABEL},
    {"chromeVoxLabel", IDS_SETTINGS_CHROMEVOX_LABEL},
    {"screenMagnifierLabel", IDS_SETTINGS_SCREEN_MAGNIFIER_LABEL},
    {"tapDraggingLabel", IDS_SETTINGS_TAP_DRAGGING_LABEL},
    {"clickOnStopLabel", IDS_SETTINGS_CLICK_ON_STOP_LABEL},
    {"delayBeforeClickLabel", IDS_SETTINGS_DELAY_BEFORE_CLICK_LABEL},
    {"delayBeforeClickExtremelyShort",
     IDS_SETTINGS_DELAY_BEFORE_CLICK_EXTREMELY_SHORT},
    {"delayBeforeClickVeryShort", IDS_SETTINGS_DELAY_BEFORE_CLICK_VERY_SHORT},
    {"delayBeforeClickShort", IDS_SETTINGS_DELAY_BEFORE_CLICK_SHORT},
    {"delayBeforeClickLong", IDS_SETTINGS_DELAY_BEFORE_CLICK_LONG},
    {"delayBeforeClickVeryLong", IDS_SETTINGS_DELAY_BEFORE_CLICK_VERY_LONG},
    {"onScreenKeyboardLabel", IDS_SETTINGS_ON_SCREEN_KEYBOARD_LABEL},
    {"monoAudioLabel", IDS_SETTINGS_MONO_AUDIO_LABEL},
    {"a11yExplanation", IDS_SETTINGS_ACCESSIBILITY_EXPLANATION},
    {"caretHighlightLabel",
     IDS_OPTIONS_SETTINGS_ACCESSIBILITY_CARET_HIGHLIGHT_DESCRIPTION},
    {"cursorHighlightLabel",
     IDS_OPTIONS_SETTINGS_ACCESSIBILITY_CURSOR_HIGHLIGHT_DESCRIPTION},
    {"focusHighlightLabel",
     IDS_OPTIONS_SETTINGS_ACCESSIBILITY_FOCUS_HIGHLIGHT_DESCRIPTION},
    {"selectToSpeakLabel",
     IDS_OPTIONS_SETTINGS_ACCESSIBILITY_SELECT_TO_SPEAK_DESCRIPTION},
    {"switchAccessLabel",
     IDS_OPTIONS_SETTINGS_ACCESSIBILITY_SWITCH_ACCESS_DESCRIPTION},
#endif
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));

#if defined(OS_CHROMEOS)
  html_source->AddString("a11yLearnMoreUrl",
                         chrome::kChromeAccessibilityHelpURL);

  html_source->AddBoolean("showExperimentalA11yFeatures",
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kEnableExperimentalAccessibilityFeatures));
#endif
}

void AddAboutStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
      {"aboutProgram", IDS_SETTINGS_ABOUT_PROGRAM},
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));
}

#if defined(OS_CHROMEOS)
void AddAccountUITweaksStrings(content::WebUIDataSource* html_source,
                               Profile* profile) {
  base::DictionaryValue localized_values;
  chromeos::AddAccountUITweaksLocalizedValues(&localized_values, profile);
  html_source->AddLocalizedStrings(localized_values);
}
#endif

void AddAppearanceStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
      {"appearancePageTitle", IDS_SETTINGS_APPEARANCE},
      {"exampleDotCom", IDS_SETTINGS_EXAMPLE_DOT_COM},
      {"getThemes", IDS_SETTINGS_THEMES},
      {"resetToDefaultTheme", IDS_SETTINGS_RESET_TO_DEFAULT_THEME},
      {"showHomeButton", IDS_SETTINGS_SHOW_HOME_BUTTON},
      {"showBookmarksBar", IDS_SETTINGS_SHOW_BOOKMARKS_BAR},
      {"homePageNtp", IDS_SETTINGS_HOME_PAGE_NTP},
      {"other", IDS_SETTINGS_OTHER},
      {"changeHomePage", IDS_SETTINGS_CHANGE_HOME_PAGE},
      {"themesGalleryUrl", IDS_THEMES_GALLERY_URL},
      {"chooseFromWebStore", IDS_SETTINGS_WEB_STORE},
      {"chooseFontsAndEncoding", IDS_SETTINGS_CHOOSE_FONTS_AND_ENCODING},
#if defined(OS_CHROMEOS)
      {"openWallpaperApp", IDS_SETTINGS_OPEN_WALLPAPER_APP},
      {"setWallpaper", IDS_SETTINGS_SET_WALLPAPER},
#endif
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));
}

#if defined(OS_CHROMEOS)
void AddBluetoothStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
      {"bluetoothPageTitle", IDS_SETTINGS_BLUETOOTH},
      {"bluetoothAddDevicePageTitle", IDS_SETTINGS_BLUETOOTH_ADD_DEVICE},
      {"bluetoothPairDevicePageTitle", IDS_SETTINGS_BLUETOOTH_PAIR_DEVICE},
      {"bluetoothEnable", IDS_SETTINGS_BLUETOOTH_ENABLE},
      {"bluetoothConnect", IDS_OPTIONS_SETTINGS_BLUETOOTH_CONNECT},
      {"bluetoothAddDevice", IDS_OPTIONS_SETTINGS_ADD_BLUETOOTH_DEVICE},
      {"bluetoothNoDevices", IDS_OPTIONS_SETTINGS_BLUETOOTH_NO_DEVICES},
      {"bluetoothConnect", IDS_OPTIONS_SETTINGS_BLUETOOTH_CONNECT},
      {"bluetoothDisconnect", IDS_OPTIONS_SETTINGS_BLUETOOTH_DISCONNECT},
      {"bluetoothConnecting", IDS_OPTIONS_SETTINGS_BLUETOOTH_CONNECTING},
      {"bluetoothRemove", IDS_SETTINGS_BLUETOOTH_REMOVE},
      {"bluetoothScanning", IDS_OPTIONS_SETTINGS_BLUETOOTH_SCANNING},
      {"bluetoothAccept", IDS_OPTIONS_SETTINGS_BLUETOOTH_ACCEPT_PASSKEY},
      {"bluetoothReject", IDS_OPTIONS_SETTINGS_BLUETOOTH_REJECT_PASSKEY},
      {"bluetoothConnect", IDS_OPTIONS_SETTINGS_BLUETOOTH_CONNECT},
      {"bluetoothDismiss", IDS_OPTIONS_SETTINGS_BLUETOOTH_DISMISS_ERROR},
      // Device connecting and pairing.
      {"bluetoothStartConnecting", IDS_SETTINGS_BLUETOOTH_START_CONNECTING},
      {"bluetoothEnterKey", IDS_OPTIONS_SETTINGS_BLUETOOTH_ENTER_KEY},
      // These ids are generated in JS using 'bluetooth_' + a value from
      // bluetoothPrivate.PairingEventType (see bluetooth_private.idl).
      // 'keysEntered', and 'requestAuthorization' have no associated message.
      {"bluetooth_requestPincode", IDS_SETTINGS_BLUETOOTH_REQUEST_PINCODE},
      {"bluetooth_displayPincode", IDS_SETTINGS_BLUETOOTH_DISPLAY_PINCODE},
      {"bluetooth_requestPasskey", IDS_SETTINGS_BLUETOOTH_REQUEST_PASSKEY},
      {"bluetooth_displayPasskey", IDS_SETTINGS_BLUETOOTH_DISPLAY_PASSKEY},
      {"bluetooth_confirmPasskey", IDS_SETTINGS_BLUETOOTH_CONFIRM_PASSKEY},
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));
}
#endif

#if defined(USE_NSS_CERTS)
void AddCertificateManagerStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
      {"certificateManagerPageTitle", IDS_SETTINGS_CERTIFICATE_MANAGER},
      {"certificateManagerNoCertificates",
        IDS_SETTINGS_CERTIFICATE_MANAGER_NO_CERTIFICATES},
      {"certificateManagerYourCertificates",
       IDS_SETTINGS_CERTIFICATE_MANAGER_YOUR_CERTIFICATES},
      {"certificateManagerYourCertificatesDescription",
       IDS_SETTINGS_CERTIFICATE_MANAGER_YOUR_CERTIFICATES_DESCRIPTION},
      {"certificateManagerServers", IDS_SETTINGS_CERTIFICATE_MANAGER_SERVERS},
      {"certificateManagerServersDescription",
       IDS_SETTINGS_CERTIFICATE_MANAGER_SERVERS_DESCRIPTION},
      {"certificateManagerAuthorities",
       IDS_SETTINGS_CERTIFICATE_MANAGER_AUTHORITIES},
      {"certificateManagerAuthoritiesDescription",
       IDS_SETTINGS_CERTIFICATE_MANAGER_AUTHORITIES_DESCRIPTION},
      {"certificateManagerOthers", IDS_SETTINGS_CERTIFICATE_MANAGER_OTHERS},
      {"certificateManagerOthersDescription",
       IDS_SETTINGS_CERTIFICATE_MANAGER_OTHERS_DESCRIPTION},
      {"certificateManagerView", IDS_SETTINGS_CERTIFICATE_MANAGER_VIEW},
      {"certificateManagerEdit", IDS_SETTINGS_CERTIFICATE_MANAGER_EDIT},
      {"certificateManagerImport", IDS_SETTINGS_CERTIFICATE_MANAGER_IMPORT},
      {"certificateManagerImportAndBind",
       IDS_SETTINGS_CERTIFICATE_MANAGER_IMPORT_AND_BIND},
      {"certificateManagerExport", IDS_SETTINGS_CERTIFICATE_MANAGER_EXPORT},
      {"certificateManagerDelete", IDS_SETTINGS_CERTIFICATE_MANAGER_DELETE},
      {"certificateManagerDone", IDS_SETTINGS_CERTIFICATE_MANAGER_DONE},
      {"certificateManagerUntrusted",
       IDS_SETTINGS_CERTIFICATE_MANAGER_UNTRUSTED},
       // CA trust edit dialog.
      {"certificateManagerCaTrustEditDialogTitle",
       IDS_SETTINGS_CERTIFICATE_MANAGER_CA_TRUST_EDIT_DIALOG_TITLE},
      {"certificateManagerCaTrustEditDialogDescription",
       IDS_SETTINGS_CERTIFICATE_MANAGER_CA_TRUST_EDIT_DIALOG_DESCRIPTION},
      {"certificateManagerCaTrustEditDialogExplanation",
       IDS_SETTINGS_CERTIFICATE_MANAGER_CA_TRUST_EDIT_DIALOG_EXPLANATION},
      {"certificateManagerCaTrustEditDialogSsl",
       IDS_SETTINGS_CERTIFICATE_MANAGER_CA_TRUST_EDIT_DIALOG_SSL},
      {"certificateManagerCaTrustEditDialogEmail",
       IDS_SETTINGS_CERTIFICATE_MANAGER_CA_TRUST_EDIT_DIALOG_EMAIL},
      {"certificateManagerCaTrustEditDialogObjSign",
       IDS_SETTINGS_CERTIFICATE_MANAGER_CA_TRUST_EDIT_DIALOG_OBJ_SIGN},
       // Certificate delete confirmation dialog.
      {"certificateManagerDeleteUserTitle",
       IDS_SETTINGS_CERTIFICATE_MANAGER_DELETE_USER_TITLE},
      {"certificateManagerDeleteUserDescription",
       IDS_SETTINGS_CERTIFICATE_MANAGER_DELETE_USER_DESCRIPTION},
      {"certificateManagerDeleteServerTitle",
       IDS_SETTINGS_CERTIFICATE_MANAGER_DELETE_SERVER_TITLE},
      {"certificateManagerDeleteServerDescription",
       IDS_SETTINGS_CERTIFICATE_MANAGER_DELETE_SERVER_DESCRIPTION},
      {"certificateManagerDeleteCaTitle",
       IDS_SETTINGS_CERTIFICATE_MANAGER_DELETE_CA_TITLE},
      {"certificateManagerDeleteCaDescription",
       IDS_SETTINGS_CERTIFICATE_MANAGER_DELETE_CA_DESCRIPTION},
      {"certificateManagerDeleteOtherTitle",
       IDS_SETTINGS_CERTIFICATE_MANAGER_DELETE_OTHER_TITLE},
       // Encrypt/decrypt password dialogs.
      {"certificateManagerEncryptPasswordTitle",
       IDS_SETTINGS_CERTIFICATE_MANAGER_ENCRYPT_PASSWORD_TITLE},
      {"certificateManagerDecryptPasswordTitle",
       IDS_SETTINGS_CERTIFICATE_MANAGER_DECRYPT_PASSWORD_TITLE},
      {"certificateManagerEncryptPasswordDescription",
       IDS_SETTINGS_CERTIFICATE_MANAGER_ENCRYPT_PASSWORD_DESCRIPTION},
      {"certificateManagerPassword",
       IDS_SETTINGS_CERTIFICATE_MANAGER_PASSWORD},
      {"certificateManagerConfirmPassword",
       IDS_SETTINGS_CERTIFICATE_MANAGER_CONFIRM_PASSWORD},
      {"certificateImportErrorFormat",
       IDS_SETTINGS_CERTIFICATE_MANAGER_IMPORT_ERROR_FORMAT},
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));
}
#endif

void AddClearBrowsingDataStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
      {"clearFollowingItemsFrom", IDS_SETTINGS_CLEAR_FOLLOWING_ITEMS_FROM},
      {"clearBrowsingHistory", IDS_SETTINGS_CLEAR_BROWSING_HISTORY},
      {"clearDownloadHistory", IDS_SETTINGS_CLEAR_DOWNLOAD_HISTORY},
      {"clearCache", IDS_SETTINGS_CLEAR_CACHE},
      {"clearCookies", IDS_SETTINGS_CLEAR_COOKIES},
      {"clearCookiesFlash", IDS_SETTINGS_CLEAR_COOKIES_FLASH},
      {"clearPasswords", IDS_SETTINGS_CLEAR_PASSWORDS},
      {"clearFormData", IDS_SETTINGS_CLEAR_FORM_DATA},
      {"clearHostedAppData", IDS_SETTINGS_CLEAR_HOSTED_APP_DATA},
      {"clearDeauthorizeContentLicenses",
       IDS_SETTINGS_DEAUTHORIZE_CONTENT_LICENSES},
      {"clearDataHour", IDS_SETTINGS_CLEAR_DATA_HOUR},
      {"clearDataDay", IDS_SETTINGS_CLEAR_DATA_DAY},
      {"clearDataWeek", IDS_SETTINGS_CLEAR_DATA_WEEK},
      {"clearData4Weeks", IDS_SETTINGS_CLEAR_DATA_4WEEKS},
      {"clearDataEverything", IDS_SETTINGS_CLEAR_DATA_EVERYTHING},
      {"warnAboutNonClearedData", IDS_SETTINGS_CLEAR_DATA_SOME_STUFF_REMAINS},
      {"clearsSyncedData", IDS_SETTINGS_CLEAR_DATA_CLEARS_SYNCED_DATA},
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));
}

void AddCloudPrintStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
      {"googleCloudPrint", IDS_SETTINGS_GOOGLE_CLOUD_PRINT},
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));
}

#if !defined(OS_CHROMEOS)
void AddDefaultBrowserStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
      {"defaultBrowser", IDS_SETTINGS_DEFAULT_BROWSER},
      {"defaultBrowserDefault", IDS_SETTINGS_DEFAULT_BROWSER_DEFAULT},
      {"defaultBrowserMakeDefault", IDS_SETTINGS_DEFAULT_BROWSER_MAKE_DEFAULT},
      {"defaultBrowserUnknown", IDS_SETTINGS_DEFAULT_BROWSER_UNKNOWN},
      {"defaultBrowserSecondary", IDS_SETTINGS_DEFAULT_BROWSER_SECONDARY},
      {"unableToSetDefaultBrowser", IDS_SETTINGS_DEFAULT_BROWSER_ERROR},
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));
}
#endif

#if defined(OS_CHROMEOS)
void AddDeviceStrings(content::WebUIDataSource* html_source) {
  LocalizedString device_strings[] = {
      {"devicePageTitle", IDS_SETTINGS_DEVICE_TITLE},
      {"scrollLabel", IDS_SETTINGS_SCROLL_LABEL},
      {"traditionalScrollLabel", IDS_SETTINGS_TRADITIONAL_SCROLL_LABEL},
  };
  AddLocalizedStringsBulk(html_source, device_strings,
                          arraysize(device_strings));

  LocalizedString touchpad_strings[] = {
      {"touchpadTitle", IDS_SETTINGS_TOUCHPAD_TITLE},
      {"touchpadTapToClickEnabledLabel",
       IDS_SETTINGS_TOUCHPAD_TAP_TO_CLICK_ENABLED_LABEL},
  };
  AddLocalizedStringsBulk(html_source, touchpad_strings,
                          arraysize(touchpad_strings));

  LocalizedString keyboard_strings[] = {
      {"keyboardTitle", IDS_SETTINGS_KEYBOARD_TITLE},
      {"keyboardKeySearch", IDS_SETTINGS_KEYBOARD_KEY_SEARCH},
      {"keyboardKeyCtrl", IDS_SETTINGS_KEYBOARD_KEY_LEFT_CTRL},
      {"keyboardKeyAlt", IDS_SETTINGS_KEYBOARD_KEY_LEFT_ALT},
      {"keyboardKeyCapsLock", IDS_SETTINGS_KEYBOARD_KEY_CAPS_LOCK},
      {"keyboardKeyDiamond", IDS_SETTINGS_KEYBOARD_KEY_DIAMOND},
      {"keyboardKeyEscape", IDS_SETTINGS_KEYBOARD_KEY_ESCAPE},
      {"keyboardKeyDisabled", IDS_SETTINGS_KEYBOARD_KEY_DISABLED},
      {"keyboardSendFunctionKeys", IDS_SETTINGS_KEYBOARD_SEND_FUNCTION_KEYS},
      {"keyboardSendFunctionKeysDescription",
       IDS_SETTINGS_KEYBOARD_SEND_FUNCTION_KEYS_DESCRIPTION},
  };
  AddLocalizedStringsBulk(html_source, keyboard_strings,
                          arraysize(keyboard_strings));

  LocalizedString display_strings[] = {
      {"devicePageTitle", IDS_SETTINGS_DEVICE_TITLE},
      {"displayTitle", IDS_SETTINGS_DISPLAY_TITLE},
      {"displayArrangement", IDS_SETTINGS_DISPLAY_ARRANGEMENT},
      {"displayMirror", IDS_SETTINGS_DISPLAY_MIRROR},
      {"displayMakePrimary", IDS_SETTINGS_DISPLAY_MAKE_PRIMARY},
      {"displayResolutionTitle", IDS_SETTINGS_DISPLAY_RESOLUTION_TITLE},
      {"displayResolutionText", IDS_SETTINGS_DISPLAY_RESOLUTION_TEXT},
      {"displayResolutionTextBest", IDS_SETTINGS_DISPLAY_RESOLUTION_TEXT_BEST},
      {"displayResolutionTextNative",
       IDS_SETTINGS_DISPLAY_RESOLUTION_TEXT_NATIVE},
      {"displayOrientation", IDS_SETTINGS_DISPLAY_ORIENTATION},
      {"displayOrientationStandard", IDS_SETTINGS_DISPLAY_ORIENTATION_STANDARD},
  };
  AddLocalizedStringsBulk(html_source, display_strings,
                          arraysize(display_strings));

  html_source->AddString(
      "naturalScrollLabel",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_NATURAL_SCROLL_LABEL,
          base::ASCIIToUTF16(chrome::kNaturalScrollHelpURL)));
}
#endif

void AddDownloadsStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
      {"downloadsPageTitle", IDS_SETTINGS_DOWNLOADS},
      {"downloadLocation", IDS_SETTINGS_DOWNLOAD_LOCATION},
      {"changeDownloadLocation", IDS_SETTINGS_CHANGE_DOWNLOAD_LOCATION},
      {"promptForDownload", IDS_SETTINGS_PROMPT_FOR_DOWNLOAD},
      {"disconnectGoogleDriveAccount", IDS_SETTINGS_DISCONNECT_GOOGLE_DRIVE},
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));
}

void AddResetStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
    {"resetPageTitle", IDS_SETTINGS_RESET},
    {"resetPageDescription", IDS_RESET_PROFILE_SETTINGS_DESCRIPTION},
    {"resetPageExplanation", IDS_RESET_PROFILE_SETTINGS_EXPLANATION},
    {"resetPageCommit", IDS_RESET_PROFILE_SETTINGS_COMMIT_BUTTON},
    {"resetPageFeedback", IDS_RESET_PROFILE_SETTINGS_FEEDBACK},
#if defined(OS_CHROMEOS)
    {"powerwashTitle", IDS_OPTIONS_FACTORY_RESET},
    {"powerwashDialogTitle", IDS_OPTIONS_FACTORY_RESET_HEADING},
    {"powerwashDialogExplanation", IDS_OPTIONS_FACTORY_RESET_WARNING},
    {"powerwashDialogButton", IDS_SETTINGS_RESTART},
    {"powerwashLearnMoreUrl", IDS_FACTORY_RESET_HELP_URL},
#endif
    // Automatic reset banner.
    {"resetProfileBannerButton",
     IDS_AUTOMATIC_SETTINGS_RESET_BANNER_RESET_BUTTON_TEXT},
    {"resetProfileBannerDescription", IDS_AUTOMATIC_SETTINGS_RESET_BANNER_TEXT},
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));

  html_source->AddString("resetPageLearnMoreUrl",
                         chrome::kResetProfileSettingsLearnMoreURL);
  html_source->AddString("resetProfileBannerLearnMoreUrl",
                         chrome::kAutomaticSettingsResetLearnMoreURL);
#if defined(OS_CHROMEOS)
  html_source->AddString(
      "powerwashDescription",
      l10n_util::GetStringFUTF16(IDS_OPTIONS_FACTORY_RESET_DESCRIPTION,
                                 l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)));
#endif
}

void AddDateTimeStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
      {"dateTimePageTitle", IDS_SETTINGS_DATE_TIME},
      {"timeZone", IDS_SETTINGS_TIME_ZONE},
      {"use24HourClock", IDS_SETTINGS_USE_24_HOUR_CLOCK},
      {"dateTimeSetAutomatically", IDS_SETTINGS_DATE_TIME_SET_AUTOMATICALLY},
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));
}

#if defined(OS_CHROMEOS)
void AddEasyUnlockStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
      {"easyUnlockSectionTitle", IDS_SETTINGS_EASY_UNLOCK_SECTION_TITLE},
      {"easyUnlockSetupButton", IDS_SETTINGS_EASY_UNLOCK_SETUP_BUTTON},
      // Easy Unlock turn-off dialog.
      {"easyUnlockTurnOffButton", IDS_SETTINGS_EASY_UNLOCK_TURN_OFF_BUTTON},
      {"easyUnlockTurnOffOfflineTitle",
       IDS_SETTINGS_EASY_UNLOCK_TURN_OFF_OFFLINE_TITLE},
      {"easyUnlockTurnOffOfflineMessage",
       IDS_SETTINGS_EASY_UNLOCK_TURN_OFF_OFFLINE_MESSAGE},
      {"easyUnlockTurnOffErrorTitle",
       IDS_SETTINGS_EASY_UNLOCK_TURN_OFF_ERROR_TITLE},
      {"easyUnlockTurnOffErrorMessage",
       IDS_SETTINGS_EASY_UNLOCK_TURN_OFF_ERROR_MESSAGE},
      {"easyUnlockTurnOffRetryButton",
       IDS_SETTINGS_EASY_UNLOCK_TURN_OFF_RETRY_BUTTON},
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));

  base::string16 device_name =
      l10n_util::GetStringUTF16(ash::GetChromeOSDeviceTypeResourceId());
  html_source->AddString(
      "easyUnlockSetupIntro",
      l10n_util::GetStringFUTF16(IDS_SETTINGS_EASY_UNLOCK_SETUP_INTRO,
                                 device_name));
  html_source->AddString(
      "easyUnlockDescription",
      l10n_util::GetStringFUTF16(IDS_SETTINGS_EASY_UNLOCK_DESCRIPTION,
                                 device_name));
  html_source->AddString(
      "easyUnlockTurnOffTitle",
      l10n_util::GetStringFUTF16(IDS_SETTINGS_EASY_UNLOCK_TURN_OFF_TITLE,
                                 device_name));
  html_source->AddString(
      "easyUnlockTurnOffDescription",
      l10n_util::GetStringFUTF16(IDS_SETTINGS_EASY_UNLOCK_TURN_OFF_DESCRIPTION,
                                 device_name));
  html_source->AddString(
      "easyUnlockRequireProximityLabel",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_EASY_UNLOCK_REQUIRE_PROXIMITY_LABEL, device_name));

  html_source->AddString("easyUnlockLearnMoreURL",
                         chrome::kEasyUnlockLearnMoreUrl);
}

void AddInternetStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
      {"internetPageTitle", IDS_SETTINGS_INTERNET},
      {"internetDetailPageTitle", IDS_SETTINGS_INTERNET_DETAIL},
      {"internetKnownNetworksPageTitle", IDS_SETTINGS_INTERNET_KNOWN_NETWORKS},
      // Required by cr_network_list_item.js. TODO(stevenjb): Add to
      // settings_strings.grdp or provide an alternative translation method.
      // crbug.com/512214.
      {"networkConnected", IDS_ASH_STATUS_TRAY_NETWORK_CONNECTED},
      {"networkConnecting", IDS_ASH_STATUS_TRAY_NETWORK_CONNECTING},
      {"networkDisabled", IDS_OPTIONS_SETTINGS_NETWORK_DISABLED},
      {"networkNotConnected", IDS_ASH_STATUS_TRAY_NETWORK_NOT_CONNECTED},
      {"OncTypeCellular", IDS_NETWORK_TYPE_CELLULAR},
      {"OncTypeEthernet", IDS_NETWORK_TYPE_ETHERNET},
      {"OncTypeVPN", IDS_NETWORK_TYPE_VPN},
      {"OncTypeWiFi", IDS_NETWORK_TYPE_WIFI},
      {"OncTypeWimax", IDS_NETWORK_TYPE_WIMAX},
      {"vpnNameTemplate",
       IDS_OPTIONS_SETTINGS_SECTION_THIRD_PARTY_VPN_NAME_TEMPLATE},
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));
}
#endif

void AddLanguagesStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
      {"languagesPageTitle", IDS_SETTINGS_LANGUAGES_PAGE_TITLE},
      {"languagesListTitle", IDS_SETTINGS_LANGUAGES_LANGUAGES_LIST_TITLE},
      {"manageLanguages", IDS_SETTINGS_LANGUAGES_LANGUAGES_MANAGE},
      {"inputMethodsListTitle",
       IDS_SETTINGS_LANGUAGES_INPUT_METHODS_LIST_TITLE},
      {"manageInputMethods", IDS_SETTINGS_LANGUAGES_INPUT_METHODS_MANAGE},
      {"spellCheckListTitle", IDS_SETTINGS_LANGUAGES_SPELL_CHECK_LIST_TITLE},
      {"manageSpellCheck", IDS_SETTINGS_LANGUAGES_SPELL_CHECK_MANAGE},
      {"manageLanguagesPageTitle",
       IDS_SETTINGS_LANGUAGES_MANAGE_LANGUAGES_TITLE},
      {"allLanguages", IDS_SETTINGS_LANGUAGES_ALL_LANGUAGES},
      {"enabledLanguages", IDS_SETTINGS_LANGUAGES_ENABLED_LANGUAGES},
      {"cannotBeDisplayedInThisLanguage",
       IDS_SETTINGS_LANGUAGES_CANNOT_BE_DISPLAYED_IN_THIS_LANGUAGE},
      {"isDisplayedInThisLanguage",
       IDS_SETTINGS_LANGUAGES_IS_DISPLAYED_IN_THIS_LANGUAGE},
      {"displayInThisLanguage",
       IDS_SETTINGS_LANGUAGES_DISPLAY_IN_THIS_LANGUAGE},
      {"offerToTranslateInThisLanguage",
       IDS_SETTINGS_LANGUAGES_OFFER_TO_TRANSLATE_IN_THIS_LANGUAGE},
      {"cannotTranslateInThisLanguage",
       IDS_SETTINGS_LANGUAGES_CANNOT_TRANSLATE_IN_THIS_LANGUAGE},
      {"editDictionaryPageTitle", IDS_SETTINGS_LANGUAGES_EDIT_DICTIONARY_TITLE},
      {"addDictionaryWordLabel", IDS_SETTINGS_LANGUAGES_ADD_DICTIONARY_WORD},
      {"addDictionaryWordButton",
       IDS_SETTINGS_LANGUAGES_ADD_DICTIONARY_WORD_BUTTON},
      {"customDictionaryWords", IDS_SETTINGS_LANGUAGES_DICTIONARY_WORDS},
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));
}

#if defined(OS_CHROMEOS)
void AddMultiProfilesStrings(content::WebUIDataSource* html_source,
                             Profile* profile) {
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();

  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  std::string primary_user_email = user_manager->GetPrimaryUser()->email();
  html_source->AddString("primaryUserEmail", primary_user_email);
  html_source->AddBoolean("isSecondaryUser",
                          user && user->email() != primary_user_email);
}
#endif

void AddOnStartupStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
      {"onStartup", IDS_SETTINGS_ON_STARTUP},
      {"onStartupOpenNewTab", IDS_SETTINGS_ON_STARTUP_OPEN_NEW_TAB},
      {"onStartupContinue", IDS_SETTINGS_ON_STARTUP_CONTINUE},
      {"onStartupOpenSpecific", IDS_SETTINGS_ON_STARTUP_OPEN_SPECIFIC},
      {"onStartupUseCurrent", IDS_SETTINGS_ON_STARTUP_USE_CURRENT},
      {"onStartupAddNewPage", IDS_SETTINGS_ON_STARTUP_ADD_NEW_PAGE},
      {"onStartupEditPage", IDS_SETTINGS_ON_STARTUP_EDIT_PAGE},
      {"onStartupSiteUrl", IDS_SETTINGS_ON_STARTUP_SITE_URL},
      {"onStartupRemove", IDS_SETTINGS_ON_STARTUP_REMOVE},
      {"onStartupEdit", IDS_SETTINGS_ON_STARTUP_EDIT},
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));
}

void AddPasswordsAndFormsStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
      {"passwordsAndAutofillPageTitle",
       IDS_SETTINGS_PASSWORDS_AND_AUTOFILL_PAGE_TITLE},
      {"autofill", IDS_SETTINGS_AUTOFILL},
      {"autofillDetail", IDS_SETTINGS_AUTOFILL_DETAIL},
      {"passwords", IDS_SETTINGS_PASSWORDS},
      {"passwordsDetail", IDS_SETTINGS_PASSWORDS_DETAIL},
      {"savedPasswordsHeading", IDS_SETTINGS_PASSWORDS_SAVED_HEADING},
      {"passwordExceptionsHeading", IDS_SETTINGS_PASSWORDS_EXCEPTIONS_HEADING},
      {"deletePasswordException", IDS_SETTINGS_PASSWORDS_DELETE_EXCEPTION},
      {"passwordMenu", IDS_SETTINGS_PASSWORDS_MENU},
      {"editPassword", IDS_SETTINGS_PASSWORDS_EDIT},
      {"removePassword", IDS_SETTINGS_PASSWORDS_REMOVE},
      {"editPasswordTitle", IDS_SETTINGS_PASSWORDS_EDIT_TITLE},
      {"editPasswordWebsiteLabel", IDS_SETTINGS_PASSWORDS_WEBSITE},
      {"editPasswordUsernameLabel", IDS_SETTINGS_PASSWORDS_USERNAME},
      {"editPasswordPasswordLabel", IDS_SETTINGS_PASSWORDS_PASSWORD},
  };
  html_source->AddString(
      "managePasswordsLabel",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_PASSWORDS_MANAGE_PASSWORDS,
          base::ASCIIToUTF16(
              password_manager::kPasswordManagerAccountDashboardURL)));
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));
}

void AddPeopleStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
    {"peoplePageTitle", IDS_SETTINGS_PEOPLE},
    {"manageOtherPeople", IDS_SETTINGS_PEOPLE_MANAGE_OTHER_PEOPLE},
#if defined(OS_CHROMEOS)
    {"enableScreenlock", IDS_SETTINGS_PEOPLE_ENABLE_SCREENLOCK},
    {"changePictureTitle", IDS_SETTINGS_CHANGE_PICTURE_DIALOG_TITLE},
    {"changePicturePageDescription", IDS_SETTINGS_CHANGE_PICTURE_DIALOG_TEXT},
    {"takePhoto", IDS_SETTINGS_CHANGE_PICTURE_TAKE_PHOTO},
    {"discardPhoto", IDS_SETTINGS_CHANGE_PICTURE_DISCARD_PHOTO},
    {"flipPhoto", IDS_SETTINGS_CHANGE_PICTURE_FLIP_PHOTO},
    {"chooseFile", IDS_SETTINGS_CHANGE_PICTURE_CHOOSE_FILE},
    {"profilePhoto", IDS_SETTINGS_CHANGE_PICTURE_PROFILE_PHOTO},
    {"oldPhoto", IDS_SETTINGS_CHANGE_PICTURE_OLD_PHOTO},
    {"profilePhotoLoading", IDS_SETTINGS_CHANGE_PICTURE_PROFILE_LOADING_PHOTO},
    {"previewAltText", IDS_SETTINGS_CHANGE_PICTURE_PREVIEW_ALT},
    {"authorCredit", IDS_SETTINGS_CHANGE_PICTURE_AUTHOR_TEXT},
    {"photoFromCamera", IDS_SETTINGS_CHANGE_PICTURE_PHOTO_FROM_CAMERA},
    {"photoFlippedAccessibleText", IDS_SETTINGS_PHOTO_FLIP_ACCESSIBLE_TEXT},
    {"photoFlippedBackAccessibleText",
     IDS_SETTINGS_PHOTO_FLIPBACK_ACCESSIBLE_TEXT},
    {"photoCaptureAccessibleText", IDS_SETTINGS_PHOTO_CAPTURE_ACCESSIBLE_TEXT},
    {"photoDiscardAccessibleText", IDS_SETTINGS_PHOTO_DISCARD_ACCESSIBLE_TEXT},
#else   // !defined(OS_CHROMEOS)
    {"editPerson", IDS_SETTINGS_EDIT_PERSON},
#endif  // defined(OS_CHROMEOS)
    {"syncOverview", IDS_SETTINGS_SYNC_OVERVIEW},
    {"syncSignin", IDS_SETTINGS_SYNC_SIGNIN},
    {"syncDisconnect", IDS_SETTINGS_SYNC_DISCONNECT},
    {"syncDisconnectTitle", IDS_SETTINGS_SYNC_DISCONNECT_TITLE},
    {"syncDisconnectDeleteProfile",
     IDS_SETTINGS_SYNC_DISCONNECT_DELETE_PROFILE},
    {"syncDisconnectConfirm", IDS_SETTINGS_SYNC_DISCONNECT_CONFIRM},
    {"syncPageTitle", IDS_SETTINGS_SYNC},
    {"syncLoading", IDS_SETTINGS_SYNC_LOADING},
    {"syncTimeout", IDS_SETTINGS_SYNC_TIMEOUT},
    {"syncEverythingCheckboxLabel",
     IDS_SETTINGS_SYNC_EVERYTHING_CHECKBOX_LABEL},
    {"appCheckboxLabel", IDS_SETTINGS_APPS_CHECKBOX_LABEL},
    {"extensionsCheckboxLabel", IDS_SETTINGS_EXTENSIONS_CHECKBOX_LABEL},
    {"settingsCheckboxLabel", IDS_SETTINGS_SETTINGS_CHECKBOX_LABEL},
    {"autofillCheckboxLabel", IDS_SETTINGS_AUTOFILL_CHECKBOX_LABEL},
    {"historyCheckboxLabel", IDS_SETTINGS_HISTORY_CHECKBOX_LABEL},
    {"themesAndWallpapersCheckboxLabel",
     IDS_SETTINGS_THEMES_AND_WALLPAPERS_CHECKBOX_LABEL},
    {"bookmarksCheckboxLabel", IDS_SETTINGS_BOOKMARKS_CHECKBOX_LABEL},
    {"passwordsCheckboxLabel", IDS_SETTINGS_PASSWORDS_CHECKBOX_LABEL},
    {"openTabsCheckboxLabel", IDS_SETTINGS_OPEN_TABS_CHECKBOX_LABEL},
    {"encryptionOptionsTitle", IDS_SETTINGS_ENCRYPTION_OPTIONS},
    {"syncDataEncryptedText", IDS_SETTINGS_SYNC_DATA_ENCRYPTED_TEXT},
    {"encryptWithGoogleCredentialsLabel",
     IDS_SETTINGS_ENCRYPT_WITH_GOOGLE_CREDENTIALS_LABEL},
    {"encryptWithSyncPassphraseLabel",
     IDS_SETTINGS_ENCRYPT_WITH_SYNC_PASSPHRASE_LABEL},
    {"encryptWithSyncPassphraseLearnMoreLink",
     IDS_SETTINGS_ENCRYPT_WITH_SYNC_PASSPHRASE_LEARN_MORE_LINK},
    {"useDefaultSettingsButton", IDS_SETTINGS_USE_DEFAULT_SETTINGS_BUTTON},
    {"passphraseExplanationText", IDS_SETTINGS_PASSPHRASE_EXPLANATION_TEXT},
    {"emptyPassphraseError", IDS_SETTINGS_EMPTY_PASSPHRASE_ERROR},
    {"mismatchedPassphraseError", IDS_SETTINGS_MISMATCHED_PASSPHRASE_ERROR},
    {"incorrectPassphraseError", IDS_SETTINGS_INCORRECT_PASSPHRASE_ERROR},
    {"passphrasePlaceholder", IDS_SETTINGS_PASSPHRASE_PLACEHOLDER},
    {"passphraseConfirmationPlaceholder",
     IDS_SETTINGS_PASSPHRASE_CONFIRMATION_PLACEHOLDER},
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));

  std::string disconnect_help_url =
      google_util::AppendGoogleLocaleParam(
          GURL(chrome::kSyncGoogleDashboardURL),
          g_browser_process->GetApplicationLocale())
          .spec();
  html_source->AddString(
      "syncDisconnectExplanation",
      l10n_util::GetStringFUTF16(IDS_SETTINGS_SYNC_DISCONNECT_EXPLANATION,
                                 base::ASCIIToUTF16(disconnect_help_url)));
}

void AddPrivacyStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
      {"privacyPageTitle", IDS_SETTINGS_PRIVACY},
      {"linkDoctorPref", IDS_SETTINGS_LINKDOCTOR_PREF},
      {"searchSuggestPref", IDS_SETTINGS_SUGGEST_PREF},
      {"networkPredictionEnabled",
       IDS_SETTINGS_NETWORK_PREDICTION_ENABLED_DESCRIPTION},
      {"safeBrowsingEnableProtection",
       IDS_SETTINGS_SAFEBROWSING_ENABLEPROTECTION},
      {"safeBrowsingEnableExtendedReporting",
       IDS_SETTINGS_SAFEBROWSING_ENABLE_EXTENDED_REPORTING},
      {"spellingPref", IDS_SETTINGS_SPELLING_PREF},
      {"enableLogging", IDS_SETTINGS_ENABLE_LOGGING},
      {"doNotTrack", IDS_SETTINGS_ENABLE_DO_NOT_TRACK},
      {"enableContentProtectionAttestation",
       IDS_SETTINGS_ENABLE_CONTENT_PROTECTION_ATTESTATION},
      {"wakeOnWifi", IDS_SETTINGS_WAKE_ON_WIFI_DESCRIPTION},
      {"manageCertificates", IDS_SETTINGS_MANAGE_CERTIFICATES},
      {"siteSettings", IDS_SETTINGS_SITE_SETTINGS},
      {"clearBrowsingData", IDS_SETTINGS_CLEAR_DATA},
      {"titleAndCount", IDS_SETTINGS_TITLE_AND_COUNT},
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));

  html_source->AddString("improveBrowsingExperience",
                         l10n_util::GetStringFUTF16(
                             IDS_SETTINGS_IMPROVE_BROWSING_EXPERIENCE,
                             base::ASCIIToUTF16(chrome::kPrivacyLearnMoreURL)));
}

void AddSearchStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
      {"searchPageTitle", IDS_SETTINGS_SEARCH},
      {"searchExplanation", IDS_SETTINGS_SEARCH_EXPLANATION},
      {"searchEnginesManage", IDS_SETTINGS_SEARCH_MANAGE_SEARCH_ENGINES},
      {"searchOkGoogleLabel", IDS_SETTINGS_SEARCH_OK_GOOGLE_LABEL},
      {"searchOkGoogleLearnMoreLink",
       IDS_SETTINGS_SEARCH_OK_GOOGLE_LEARN_MORE_LINK},
      {"searchOkGoogleDescriptionLabel",
       IDS_SETTINGS_SEARCH_OK_GOOGLE_DESCRIPTION_LABEL},
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));
}

void AddSearchEnginesStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
      {"searchEnginesPageTitle", IDS_SETTINGS_SEARCH_ENGINES},
      {"searchEnginesAddSearchEngine",
       IDS_SETTINGS_SEARCH_ENGINES_ADD_SEARCH_ENGINE},
      {"searchEnginesEditSearchEngine",
       IDS_SETTINGS_SEARCH_ENGINES_EDIT_SEARCH_ENGINE},
      {"searchEnginesNotValid", IDS_SETTINGS_SEARCH_ENGINES_NOT_VALID},
      {"searchEngines", IDS_SETTINGS_SEARCH_ENGINES},
      {"searchEnginesDefault", IDS_SETTINGS_SEARCH_ENGINES_DEFAULT_ENGINES},
      {"searchEnginesOther", IDS_SETTINGS_SEARCH_ENGINES_OTHER_ENGINES},
      {"searchEnginesExtension", IDS_SETTINGS_SEARCH_ENGINES_EXTENSION_ENGINES},
      {"searchEnginesSearchEngine", IDS_SETTINGS_SEARCH_ENGINES_SEARCH_ENGINE},
      {"searchEnginesKeyword", IDS_SETTINGS_SEARCH_ENGINES_KEYWORD},
      {"searchEnginesQueryURL", IDS_SETTINGS_SEARCH_ENGINES_QUERY_URL},
      {"searchEnginesQueryURLExplanation",
       IDS_SETTINGS_SEARCH_ENGINES_QUERY_URL_EXPLANATION},
      {"searchEnginesMakeDefault", IDS_SETTINGS_SEARCH_ENGINES_MAKE_DEFAULT},
      {"searchEnginesEdit", IDS_SETTINGS_SEARCH_ENGINES_EDIT},
      {"searchEnginesRemoveFromList",
       IDS_SETTINGS_SEARCH_ENGINES_REMOVE_FROM_LIST},
      {"searchEnginesManageExtension",
       IDS_SETTINGS_SEARCH_ENGINES_MANAGE_EXTENSION},
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));
}

void AddSiteSettingsStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
      {"embeddedOnHost", IDS_EXCEPTIONS_GEOLOCATION_EMBEDDED_ON_HOST},
      {"siteSettingsCategoryPageTitle", IDS_SETTINGS_SITE_SETTINGS_CATEGORY},
      {"siteSettingsCategoryAllSites", IDS_SETTINGS_SITE_SETTINGS_ALL_SITES},
      {"siteSettingsCategoryCamera", IDS_SETTINGS_SITE_SETTINGS_CAMERA},
      {"siteSettingsCategoryCookies", IDS_SETTINGS_SITE_SETTINGS_COOKIES},
      {"siteSettingsCategoryFullscreen", IDS_SETTINGS_SITE_SETTINGS_FULLSCREEN},
      {"siteSettingsCategoryImages", IDS_SETTINGS_SITE_SETTINGS_IMAGES},
      {"siteSettingsCategoryLocation", IDS_SETTINGS_SITE_SETTINGS_LOCATION},
      {"siteSettingsCategoryJavascript", IDS_SETTINGS_SITE_SETTINGS_JAVASCRIPT},
      {"siteSettingsCategoryMicrophone", IDS_SETTINGS_SITE_SETTINGS_MIC},
      {"siteSettingsCategoryNotifications",
       IDS_SETTINGS_SITE_SETTINGS_NOTIFICATIONS},
      {"siteSettingsCategoryPopups", IDS_SETTINGS_SITE_SETTINGS_POPUPS},
      {"siteSettingsSiteDetailsPageTitle",
       IDS_SETTINGS_SITE_SETTINGS_SITE_DETAILS},
      {"siteSettingsAllSites", IDS_SETTINGS_SITE_SETTINGS_ALL_SITES},
      {"siteSettingsCamera", IDS_SETTINGS_SITE_SETTINGS_CAMERA},
      {"siteSettingsCookies", IDS_SETTINGS_SITE_SETTINGS_COOKIES},
      {"siteSettingsLocation", IDS_SETTINGS_SITE_SETTINGS_LOCATION},
      {"siteSettingsMic", IDS_SETTINGS_SITE_SETTINGS_MIC},
      {"siteSettingsNotifications", IDS_SETTINGS_SITE_SETTINGS_NOTIFICATIONS},
      {"siteSettingsImages", IDS_SETTINGS_SITE_SETTINGS_IMAGES},
      {"siteSettingsJavascript", IDS_SETTINGS_SITE_SETTINGS_JAVASCRIPT},
      {"siteSettingsPopups", IDS_SETTINGS_SITE_SETTINGS_POPUPS},
      {"siteSettingsFullscreen", IDS_SETTINGS_SITE_SETTINGS_FULLSCREEN},
      {"siteSettingsMaySaveCookies",
       IDS_SETTINGS_SITE_SETTINGS_MAY_SAVE_COOKIES},
      {"siteSettingsAskFirst", IDS_SETTINGS_SITE_SETTINGS_ASK_FIRST},
      {"siteSettingsAskFirst", IDS_SETTINGS_SITE_SETTINGS_ASK_FIRST},
      {"siteSettingsAskFirstRecommended",
       IDS_SETTINGS_SITE_SETTINGS_ASK_FIRST_RECOMMENDED},
      {"siteSettingsAskBeforeAccessing",
       IDS_SETTINGS_SITE_SETTINGS_ASK_BEFORE_ACCESSING},
      {"siteSettingsAskBeforeAccessingRecommended",
       IDS_SETTINGS_SITE_SETTINGS_ASK_BEFORE_ACCESSING_RECOMMENDED},
      {"siteSettingsAskBeforeSending",
       IDS_SETTINGS_SITE_SETTINGS_ASK_BEFORE_SENDING},
      {"siteSettingsAskBeforeSendingRecommended",
       IDS_SETTINGS_SITE_SETTINGS_ASK_BEFORE_SENDING_RECOMMENDED},
      {"siteSettingsDontShowImages",
       IDS_SETTINGS_SITE_SETTINGS_DONT_SHOW_IMAGES},
      {"siteSettingsShowAll", IDS_SETTINGS_SITE_SETTINGS_SHOW_ALL},
      {"siteSettingsShowAllRecommended",
       IDS_SETTINGS_SITE_SETTINGS_SHOW_ALL_RECOMMENDED},
      {"siteSettingsCookiesAllowed",
       IDS_SETTINGS_SITE_SETTINGS_COOKIES_ALLOW_SITES},
      {"siteSettingsCookiesAllowedRecommended",
       IDS_SETTINGS_SITE_SETTINGS_COOKIES_ALLOW_SITES_RECOMMENDED},
      {"siteSettingsAllow", IDS_SETTINGS_SITE_SETTINGS_ALLOW},
      {"siteSettingsBlock", IDS_SETTINGS_SITE_SETTINGS_BLOCK},
      {"siteSettingsAllowed", IDS_SETTINGS_SITE_SETTINGS_ALLOWED},
      {"siteSettingsAllowed", IDS_SETTINGS_SITE_SETTINGS_ALLOWED},
      {"siteSettingsAllowedRecommended",
       IDS_SETTINGS_SITE_SETTINGS_ALLOWED_RECOMMENDED},
      {"siteSettingsBlocked", IDS_SETTINGS_SITE_SETTINGS_BLOCKED},
      {"siteSettingsBlocked", IDS_SETTINGS_SITE_SETTINGS_BLOCKED},
      {"siteSettingsBlockedRecommended",
       IDS_SETTINGS_SITE_SETTINGS_BLOCKED_RECOMMENDED},
      {"siteSettingsExceptions", IDS_SETTINGS_SITE_SETTINGS_EXCEPTIONS},
      {"siteSettingsAddSite", IDS_SETTINGS_SITE_SETTINGS_ADD_SITE},
      {"siteSettingsSiteUrl", IDS_SETTINGS_SITE_SETTINGS_SITE_URL},
      {"siteSettingsActionAllow", IDS_SETTINGS_SITE_SETTINGS_ALLOW_MENU},
      {"siteSettingsActionBlock", IDS_SETTINGS_SITE_SETTINGS_BLOCK_MENU},
      {"siteSettingsActionReset", IDS_SETTINGS_SITE_SETTINGS_RESET_MENU},
      {"siteSettingsUsage", IDS_SETTINGS_SITE_SETTINGS_USAGE},
      {"siteSettingsPermissions", IDS_SETTINGS_SITE_SETTINGS_PERMISSIONS},
      {"siteSettingsClearAndReset", IDS_SETTINGS_SITE_SETTINGS_CLEAR_BUTTON},
      {"siteSettingsDelete", IDS_SETTINGS_SITE_SETTINGS_DELETE},
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));
}

void AddUsersStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
      {"usersPageTitle", IDS_SETTINGS_USERS},
      {"usersModifiedByOwnerLabel", IDS_SETTINGS_USERS_MODIFIED_BY_OWNER_LABEL},
      {"guestBrowsingLabel", IDS_SETTINGS_USERS_GUEST_BROWSING_LABEL},
      {"settingsManagedLabel", IDS_SETTINGS_USERS_MANAGED_LABEL},
      {"supervisedUsersLabel", IDS_SETTINGS_USERS_SUPERVISED_USERS_LABEL},
      {"showOnSigninLabel", IDS_SETTINGS_USERS_SHOW_ON_SIGNIN_LABEL},
      {"restrictSigninLabel", IDS_SETTINGS_USERS_RESTRICT_SIGNIN_LABEL},
      {"addUsersLabel", IDS_SETTINGS_USERS_ADD_USERS_LABEL},
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));
}

#if !defined(OS_CHROMEOS)
void AddSystemStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
      {"systemPageTitle", IDS_SETTINGS_SYSTEM},
#if !defined(OS_MACOSX)
      {"backgroundAppsLabel", IDS_SETTINGS_SYSTEM_BACKGROUND_APPS_LABEL},
#endif
      {"hardwareAccelerationLabel",
          IDS_SETTINGS_SYSTEM_HARDWARE_ACCELERATION_LABEL},
      {"changeProxySettings", IDS_SETTINGS_SYSTEM_PROXY_SETTINGS_BUTTON},
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));

  // TODO(dbeam): we should probably rename anything involving "localized
  // strings" to "load time data" as all primitive types are used now.
  SystemHandler::AddLoadTimeData(html_source);
}
#endif

void AddWebContentStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
      {"webContent", IDS_SETTINGS_WEB_CONTENT},
      {"pageZoom", IDS_SETTINGS_PAGE_ZOOM_LABEL},
      {"fontSize", IDS_SETTINGS_FONT_SIZE_LABEL},
      {"verySmall", IDS_SETTINGS_VERY_SMALL_FONT},
      {"small", IDS_SETTINGS_SMALL_FONT},
      {"medium", IDS_SETTINGS_MEDIUM_FONT},
      {"large", IDS_SETTINGS_LARGE_FONT},
      {"veryLarge", IDS_SETTINGS_VERY_LARGE_FONT},
      {"custom", IDS_SETTINGS_CUSTOM},
      {"customizeFonts", IDS_SETTINGS_CUSTOMIZE_FONTS},
      {"fontsAndEncoding", IDS_SETTINGS_FONTS_AND_ENCODING},
      {"standardFont", IDS_SETTINGS_STANDARD_FONT_LABEL},
      {"serifFont", IDS_SETTINGS_SERIF_FONT_LABEL},
      {"sansSerifFont", IDS_SETTINGS_SANS_SERIF_FONT_LABEL},
      {"fixedWidthFont", IDS_SETTINGS_FIXED_WIDTH_FONT_LABEL},
      {"minimumFont", IDS_SETTINGS_MINIMUM_FONT_SIZE_LABEL},
      {"encoding", IDS_SETTINGS_ENCODING_LABEL},
      {"tiny", IDS_SETTINGS_TINY_FONT_SIZE},
      {"huge", IDS_SETTINGS_HUGE_FONT_SIZE},
      {"loremIpsum", IDS_SETTINGS_LOREM_IPSUM},
      {"loading", IDS_SETTINGS_LOADING},
      {"advancedFontSettings", IDS_SETTINGS_ADVANCED_FONT_SETTINGS},
      {"openAdvancedFontSettings", IDS_SETTINGS_OPEN_ADVANCED_FONT_SETTINGS},
      {"requiresWebStoreExtension", IDS_SETTINGS_REQUIRES_WEB_STORE_EXTENSION},
      {"quickBrownFox", IDS_SETTINGS_QUICK_BROWN_FOX},
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));
}

}  // namespace

void AddLocalizedStrings(content::WebUIDataSource* html_source,
                         Profile* profile) {
  AddCommonStrings(html_source);

  AddA11yStrings(html_source);
  AddAboutStrings(html_source);
#if defined(OS_CHROMEOS)
  AddAccountUITweaksStrings(html_source, profile);
#endif
  AddAppearanceStrings(html_source);
#if defined(OS_CHROMEOS)
  AddBluetoothStrings(html_source);
#endif
#if defined(USE_NSS_CERTS)
  AddCertificateManagerStrings(html_source);
#endif
  AddClearBrowsingDataStrings(html_source);
  AddCloudPrintStrings(html_source);
#if !defined(OS_CHROMEOS)
  AddDefaultBrowserStrings(html_source);
#endif
  AddDateTimeStrings(html_source);
#if defined(OS_CHROMEOS)
  AddDeviceStrings(html_source);
#endif
  AddDownloadsStrings(html_source);

#if defined(OS_CHROMEOS)
  AddEasyUnlockStrings(html_source);
  AddInternetStrings(html_source);
#endif
  AddLanguagesStrings(html_source);
#if defined(OS_CHROMEOS)
  AddMultiProfilesStrings(html_source, profile);
#endif
  AddOnStartupStrings(html_source);
  AddPasswordsAndFormsStrings(html_source);
  AddPeopleStrings(html_source);
  AddPrivacyStrings(html_source);
  AddResetStrings(html_source);
  AddSearchEnginesStrings(html_source);
  AddSearchStrings(html_source);
  AddSiteSettingsStrings(html_source);
#if !defined(OS_CHROMEOS)
  AddSystemStrings(html_source);
#endif
  AddUsersStrings(html_source);
  AddWebContentStrings(html_source);

  policy_indicator::AddLocalizedStrings(html_source);

  html_source->SetJsonPath(kLocalizedStringsFile);
}

}  // namespace settings
