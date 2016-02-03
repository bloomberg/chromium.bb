// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/md_settings_localized_strings_provider.h"

#include <string>

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
#include "content/public/browser/web_ui_data_source.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/ui/webui/chromeos/ui_account_tweaks.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"
#endif

namespace {

// Note that settings.html contains a <script> tag which imports a script of
// the following name. These names must be kept in sync.
const char kLocalizedStringsFile[] = "strings.js";

void AddCommonStrings(content::WebUIDataSource* html_source) {
  html_source->AddLocalizedString("basicPageTitle", IDS_SETTINGS_BASIC);
  html_source->AddLocalizedString("advancedPageTitle", IDS_SETTINGS_ADVANCED);
  html_source->AddLocalizedString("addLabel", IDS_ADD);
  html_source->AddLocalizedString("learnMore", IDS_LEARN_MORE);
  html_source->AddLocalizedString("cancel", IDS_CANCEL);
  html_source->AddLocalizedString("ok", IDS_OK);
}

#if defined(OS_CHROMEOS)
void AddA11yStrings(content::WebUIDataSource* html_source) {
  html_source->AddLocalizedString(
      "a11yPageTitle", IDS_SETTINGS_ACCESSIBILITY);
  html_source->AddLocalizedString(
      "moreFeaturesLink", IDS_SETTINGS_MORE_FEATURES_LINK);
  html_source->AddLocalizedString(
      "optionsInMenuLabel", IDS_SETTINGS_OPTIONS_IN_MENU_LABEL);
  html_source->AddLocalizedString(
      "largeMouseCursorLabel", IDS_SETTINGS_LARGE_MOUSE_CURSOR_LABEL);
  html_source->AddLocalizedString(
      "highContrastLabel", IDS_SETTINGS_HIGH_CONTRAST_LABEL);
  html_source->AddLocalizedString(
      "stickyKeysLabel", IDS_SETTINGS_STICKY_KEYS_LABEL);
  html_source->AddLocalizedString(
      "chromeVoxLabel", IDS_SETTINGS_CHROMEVOX_LABEL);
  html_source->AddLocalizedString(
      "screenMagnifierLabel", IDS_SETTINGS_SCREEN_MAGNIFIER_LABEL);
  html_source->AddLocalizedString(
      "tapDraggingLabel", IDS_SETTINGS_TAP_DRAGGING_LABEL);
  html_source->AddLocalizedString(
      "clickOnStopLabel", IDS_SETTINGS_CLICK_ON_STOP_LABEL);
  html_source->AddLocalizedString(
      "delayBeforeClickLabel", IDS_SETTINGS_DELAY_BEFORE_CLICK_LABEL);
  html_source->AddLocalizedString(
      "delayBeforeClickExtremelyShort",
      IDS_SETTINGS_DELAY_BEFORE_CLICK_EXTREMELY_SHORT);
  html_source->AddLocalizedString(
      "delayBeforeClickVeryShort", IDS_SETTINGS_DELAY_BEFORE_CLICK_VERY_SHORT);
  html_source->AddLocalizedString(
      "delayBeforeClickShort", IDS_SETTINGS_DELAY_BEFORE_CLICK_SHORT);
  html_source->AddLocalizedString(
      "delayBeforeClickLong", IDS_SETTINGS_DELAY_BEFORE_CLICK_LONG);
  html_source->AddLocalizedString(
      "delayBeforeClickVeryLong", IDS_SETTINGS_DELAY_BEFORE_CLICK_VERY_LONG);
  html_source->AddLocalizedString(
      "onScreenKeyboardLabel", IDS_SETTINGS_ON_SCREEN_KEYBOARD_LABEL);
  html_source->AddLocalizedString(
      "a11yExplanation", IDS_SETTINGS_ACCESSIBILITY_EXPLANATION);
  html_source->AddString(
      "a11yLearnMoreUrl", chrome::kChromeAccessibilityHelpURL);
}
#endif

#if defined(OS_CHROMEOS)
void AddAccountUITweaksStrings(content::WebUIDataSource* html_source,
                               Profile* profile) {
  base::DictionaryValue localized_values;
  chromeos::AddAccountUITweaksLocalizedValues(&localized_values, profile);
  html_source->AddLocalizedStrings(localized_values);
}
#endif

void AddAppearanceStrings(content::WebUIDataSource* html_source) {
  html_source->AddLocalizedString(
      "appearancePageTitle", IDS_SETTINGS_APPEARANCE);
  html_source->AddLocalizedString(
      "exampleDotCom", IDS_SETTINGS_EXAMPLE_DOT_COM);
  html_source->AddLocalizedString(
      "setWallpaper", IDS_SETTINGS_SET_WALLPAPER);
  html_source->AddLocalizedString(
      "getThemes", IDS_SETTINGS_THEMES);
  html_source->AddLocalizedString(
      "resetToDefaultTheme", IDS_SETTINGS_RESET_TO_DEFAULT_THEME);
  html_source->AddLocalizedString(
      "showHomeButton", IDS_SETTINGS_SHOW_HOME_BUTTON);
  html_source->AddLocalizedString(
      "showBookmarksBar", IDS_SETTINGS_SHOW_BOOKMARKS_BAR);
  html_source->AddLocalizedString(
      "homePageNtp", IDS_SETTINGS_HOME_PAGE_NTP);
  html_source->AddLocalizedString(
      "other", IDS_SETTINGS_OTHER);
  html_source->AddLocalizedString(
      "changeHomePage", IDS_SETTINGS_CHANGE_HOME_PAGE);
  html_source->AddLocalizedString(
      "themesGalleryUrl", IDS_THEMES_GALLERY_URL);
  html_source->AddLocalizedString(
      "chooseFromWebStore", IDS_SETTINGS_WEB_STORE);
}

#if defined(OS_CHROMEOS)
void AddBluetoothStrings(content::WebUIDataSource* html_source) {
  html_source->AddLocalizedString(
      "bluetoothPageTitle", IDS_SETTINGS_BLUETOOTH);
  html_source->AddLocalizedString(
      "bluetoothAddDevicePageTitle", IDS_SETTINGS_BLUETOOTH_ADD_DEVICE);
  html_source->AddLocalizedString(
      "bluetoothPairDevicePageTitle", IDS_SETTINGS_BLUETOOTH_PAIR_DEVICE);
  html_source->AddLocalizedString(
      "bluetoothEnable", IDS_SETTINGS_BLUETOOTH_ENABLE);
  html_source->AddLocalizedString(
      "bluetoothConnect", IDS_OPTIONS_SETTINGS_BLUETOOTH_CONNECT);
  html_source->AddLocalizedString(
      "bluetoothAddDevice", IDS_OPTIONS_SETTINGS_ADD_BLUETOOTH_DEVICE);
  html_source->AddLocalizedString(
      "bluetoothNoDevices", IDS_OPTIONS_SETTINGS_BLUETOOTH_NO_DEVICES);
  html_source->AddLocalizedString(
      "bluetoothConnect", IDS_OPTIONS_SETTINGS_BLUETOOTH_CONNECT);
  html_source->AddLocalizedString(
      "bluetoothDisconnect", IDS_OPTIONS_SETTINGS_BLUETOOTH_DISCONNECT);
  html_source->AddLocalizedString(
      "bluetoothConnecting", IDS_OPTIONS_SETTINGS_BLUETOOTH_CONNECTING);
  html_source->AddLocalizedString(
      "bluetoothRemove", IDS_SETTINGS_BLUETOOTH_REMOVE);
  html_source->AddLocalizedString(
      "bluetoothScanning", IDS_OPTIONS_SETTINGS_BLUETOOTH_SCANNING);
  html_source->AddLocalizedString(
      "bluetoothAccept", IDS_OPTIONS_SETTINGS_BLUETOOTH_ACCEPT_PASSKEY);
  html_source->AddLocalizedString(
      "bluetoothReject", IDS_OPTIONS_SETTINGS_BLUETOOTH_REJECT_PASSKEY);
  html_source->AddLocalizedString(
      "bluetoothConnect", IDS_OPTIONS_SETTINGS_BLUETOOTH_CONNECT);
  html_source->AddLocalizedString(
      "bluetoothDismiss", IDS_OPTIONS_SETTINGS_BLUETOOTH_DISMISS_ERROR);

  // Device connecting and pairing.
  html_source->AddLocalizedString("bluetoothStartConnecting",
                                  IDS_SETTINGS_BLUETOOTH_START_CONNECTING);
  html_source->AddLocalizedString("bluetoothEnterKey",
                                  IDS_OPTIONS_SETTINGS_BLUETOOTH_ENTER_KEY);
  // These ids are generated in JS using 'bluetooth_' + a value from
  // bluetoothPrivate.PairingEventType (see bluetooth_private.idl).
  // 'keysEntered', and 'requestAuthorization' have no associated message.
  html_source->AddLocalizedString("bluetooth_requestPincode",
                                  IDS_SETTINGS_BLUETOOTH_REQUEST_PINCODE);
  html_source->AddLocalizedString("bluetooth_displayPincode",
                                  IDS_SETTINGS_BLUETOOTH_DISPLAY_PINCODE);
  html_source->AddLocalizedString("bluetooth_requestPasskey",
                                  IDS_SETTINGS_BLUETOOTH_REQUEST_PASSKEY);
  html_source->AddLocalizedString("bluetooth_displayPasskey",
                                  IDS_SETTINGS_BLUETOOTH_DISPLAY_PASSKEY);
  html_source->AddLocalizedString("bluetooth_confirmPasskey",
                                  IDS_SETTINGS_BLUETOOTH_CONFIRM_PASSKEY);
}
#endif

void AddCertificateManagerStrings(content::WebUIDataSource* html_source) {
  html_source->AddLocalizedString("certificateManagerPageTitle",
                                  IDS_SETTINGS_CERTIFICATE_MANAGER);
  html_source->AddLocalizedString(
      "certificateManagerYourCertificates",
      IDS_SETTINGS_CERTIFICATE_MANAGER_YOUR_CERTIFICATES);
  html_source->AddLocalizedString(
      "certificateManagerYourCertificatesSubtitle",
      IDS_SETTINGS_CERTIFICATE_MANAGER_YOU_HAVE_CERTIFICATES);
  html_source->AddLocalizedString("certificateManagerServers",
                                  IDS_SETTINGS_CERTIFICATE_MANAGER_SERVERS);
  html_source->AddLocalizedString(
      "certificateManagerServersSubtitle",
      IDS_SETTINGS_CERTIFICATE_MANAGER_SERVERS_IDENTIFY);
  html_source->AddLocalizedString("certificateManagerAuthorities",
                                  IDS_SETTINGS_CERTIFICATE_MANAGER_AUTHORITIES);
  html_source->AddLocalizedString(
      "certificateManagerAuthoritiesSubtitle",
      IDS_SETTINGS_CERTIFICATE_MANAGER_AUTHORITIES_YOU_HAVE_AUTHORITIES);
  html_source->AddLocalizedString("certificateManagerOthers",
                                  IDS_SETTINGS_CERTIFICATE_MANAGER_OTHERS);
  html_source->AddLocalizedString(
      "certificateManagerOthersSubtitle",
      IDS_SETTINGS_CERTIFICATE_MANAGER_OTHERS_YOU_HAVE_OTHERS);
  html_source->AddLocalizedString("certificateManagerView",
                                  IDS_SETTINGS_CERTIFICATE_MANAGER_VIEW);
  html_source->AddLocalizedString("certificateManagerImport",
                                  IDS_SETTINGS_CERTIFICATE_MANAGER_IMPORT);
  html_source->AddLocalizedString(
      "certificateManagerImportAndBind",
      IDS_SETTINGS_CERTIFICATE_MANAGER_IMPORT_AND_BIND);
  html_source->AddLocalizedString("certificateManagerExport",
                                  IDS_SETTINGS_CERTIFICATE_MANAGER_EXPORT);
  html_source->AddLocalizedString("certificateManagerDelete",
                                  IDS_SETTINGS_CERTIFICATE_MANAGER_DELETE);
  html_source->AddLocalizedString("certificateManagerDone",
                                  IDS_SETTINGS_CERTIFICATE_MANAGER_DONE);
}

void AddClearBrowsingDataStrings(content::WebUIDataSource* html_source) {
  html_source->AddLocalizedString("clearFollowingItemsFrom",
                                  IDS_SETTINGS_CLEAR_FOLLOWING_ITEMS_FROM);
  html_source->AddLocalizedString("clearBrowsingHistory",
                                  IDS_SETTINGS_CLEAR_BROWSING_HISTORY);
  html_source->AddLocalizedString("clearDownloadHistory",
                                  IDS_SETTINGS_CLEAR_DOWNLOAD_HISTORY);
  html_source->AddLocalizedString("clearCache",
                                  IDS_SETTINGS_CLEAR_CACHE);
  html_source->AddLocalizedString("clearCookies",
                                  IDS_SETTINGS_CLEAR_COOKIES);
  html_source->AddLocalizedString("clearCookiesFlash",
                                  IDS_SETTINGS_CLEAR_COOKIES_FLASH);
  html_source->AddLocalizedString("clearPasswords",
                                  IDS_SETTINGS_CLEAR_PASSWORDS);
  html_source->AddLocalizedString("clearFormData",
                                  IDS_SETTINGS_CLEAR_FORM_DATA);
  html_source->AddLocalizedString("clearHostedAppData",
                                  IDS_SETTINGS_CLEAR_HOSTED_APP_DATA);
  html_source->AddLocalizedString("clearDeauthorizeContentLicenses",
                                  IDS_SETTINGS_DEAUTHORIZE_CONTENT_LICENSES);
  html_source->AddLocalizedString("clearDataHour",
                                  IDS_SETTINGS_CLEAR_DATA_HOUR);
  html_source->AddLocalizedString("clearDataDay",
                                  IDS_SETTINGS_CLEAR_DATA_DAY);
  html_source->AddLocalizedString("clearDataWeek",
                                  IDS_SETTINGS_CLEAR_DATA_WEEK);
  html_source->AddLocalizedString("clearData4Weeks",
                                  IDS_SETTINGS_CLEAR_DATA_4WEEKS);
  html_source->AddLocalizedString("clearDataEverything",
                                  IDS_SETTINGS_CLEAR_DATA_EVERYTHING);
  html_source->AddLocalizedString(
      "warnAboutNonClearedData",
      IDS_SETTINGS_CLEAR_DATA_SOME_STUFF_REMAINS);
  html_source->AddLocalizedString(
      "clearsSyncedData",
      IDS_SETTINGS_CLEAR_DATA_CLEARS_SYNCED_DATA);
}

#if !defined(OS_CHROMEOS)
void AddDefaultBrowserStrings(content::WebUIDataSource* html_source) {
  html_source->AddLocalizedString(
      "defaultBrowser", IDS_SETTINGS_DEFAULT_BROWSER);
  html_source->AddLocalizedString(
      "defaultBrowserDefault", IDS_SETTINGS_DEFAULT_BROWSER_DEFAULT);
  html_source->AddLocalizedString(
      "defaultBrowserNotDefault", IDS_SETTINGS_DEFAULT_BROWSER_NOT_DEFAULT);
  html_source->AddLocalizedString(
      "defaultBrowserMakeDefault", IDS_SETTINGS_DEFAULT_BROWSER_MAKE_DEFAULT);
  html_source->AddLocalizedString(
      "defaultBrowserUnknown", IDS_SETTINGS_DEFAULT_BROWSER_UNKNOWN);
  html_source->AddLocalizedString(
      "defaultBrowserSecondary", IDS_SETTINGS_DEFAULT_BROWSER_SECONDARY);
  html_source->AddLocalizedString(
      "unableToSetDefaultBrowser", IDS_SETTINGS_DEFAULT_BROWSER_ERROR);
}
#endif

void AddDownloadsStrings(content::WebUIDataSource* html_source) {
  html_source->AddLocalizedString(
      "downloadsPageTitle", IDS_SETTINGS_DOWNLOADS);
  html_source->AddLocalizedString(
      "downloadLocation", IDS_SETTINGS_DOWNLOAD_LOCATION);
  html_source->AddLocalizedString(
      "changeDownloadLocation", IDS_SETTINGS_CHANGE_DOWNLOAD_LOCATION);
  html_source->AddLocalizedString(
      "promptForDownload", IDS_SETTINGS_PROMPT_FOR_DOWNLOAD);
  html_source->AddLocalizedString(
      "disconnectGoogleDriveAccount", IDS_SETTINGS_DISCONNECT_GOOGLE_DRIVE);
}

void AddResetStrings(content::WebUIDataSource* html_source) {
  html_source->AddLocalizedString(
      "resetPageTitle", IDS_RESET_PROFILE_SETTINGS_SECTION_TITLE);
  html_source->AddLocalizedString(
      "resetPageDescription", IDS_RESET_PROFILE_SETTINGS_DESCRIPTION);
  html_source->AddLocalizedString(
      "resetPageExplanation", IDS_RESET_PROFILE_SETTINGS_EXPLANATION);
  html_source->AddLocalizedString(
      "resetPageCommit", IDS_RESET_PROFILE_SETTINGS_COMMIT_BUTTON);
  html_source->AddLocalizedString(
      "resetPageFeedback", IDS_RESET_PROFILE_SETTINGS_FEEDBACK);
  html_source->AddString(
      "resetPageLearnMoreUrl",
      chrome::kResetProfileSettingsLearnMoreURL);

#if defined(OS_CHROMEOS)
  html_source->AddLocalizedString(
      "powerwashTitle", IDS_OPTIONS_FACTORY_RESET);
  html_source->AddString(
      "powerwashDescription",
      l10n_util::GetStringFUTF16(
          IDS_OPTIONS_FACTORY_RESET_DESCRIPTION,
          l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)));
  html_source->AddLocalizedString(
      "powerwashDialogTitle", IDS_OPTIONS_FACTORY_RESET_HEADING);
  html_source->AddLocalizedString(
      "powerwashDialogExplanation", IDS_OPTIONS_FACTORY_RESET_WARNING);
  html_source->AddLocalizedString(
      "powerwashDialogButton", IDS_RELAUNCH_BUTTON);
  html_source->AddLocalizedString(
      "powerwashLearnMoreUrl", IDS_FACTORY_RESET_HELP_URL);
#endif

  // Automatic reset banner.
  html_source->AddLocalizedString(
      "resetProfileBannerButton",
      IDS_AUTOMATIC_SETTINGS_RESET_BANNER_RESET_BUTTON_TEXT);
  html_source->AddLocalizedString(
      "resetProfileBannerDescription",
      IDS_AUTOMATIC_SETTINGS_RESET_BANNER_TEXT);
  html_source->AddString(
      "resetProfileBannerLearnMoreUrl",
      chrome::kAutomaticSettingsResetLearnMoreURL);
}

void AddDateTimeStrings(content::WebUIDataSource* html_source) {
  html_source->AddLocalizedString(
      "dateTimePageTitle", IDS_SETTINGS_DATE_TIME);
  html_source->AddLocalizedString(
      "timeZone", IDS_SETTINGS_TIME_ZONE);
  html_source->AddLocalizedString(
      "use24HourClock", IDS_SETTINGS_USE_24_HOUR_CLOCK);
  html_source->AddLocalizedString(
      "dateTimeSetAutomatically", IDS_SETTINGS_DATE_TIME_SET_AUTOMATICALLY);
}

#if defined(OS_CHROMEOS)
void AddInternetStrings(content::WebUIDataSource* html_source) {
  html_source->AddLocalizedString(
      "internetPageTitle", IDS_SETTINGS_INTERNET);
  html_source->AddLocalizedString(
      "internetDetailPageTitle", IDS_SETTINGS_INTERNET_DETAIL);
  html_source->AddLocalizedString("internetKnownNetworksPageTitle",
                                  IDS_SETTINGS_INTERNET_KNOWN_NETWORKS);

  // Required by cr_network_list_item.js. TODO(stevenjb): Add to
  // settings_strings.grdp or provide an alternative translation method.
  // crbug.com/512214.
  html_source->AddLocalizedString("networkConnected",
                                  IDS_ASH_STATUS_TRAY_NETWORK_CONNECTED);
  html_source->AddLocalizedString("networkConnecting",
                                  IDS_ASH_STATUS_TRAY_NETWORK_CONNECTING);
  html_source->AddLocalizedString("networkDisabled",
                                  IDS_OPTIONS_SETTINGS_NETWORK_DISABLED);
  html_source->AddLocalizedString("networkNotConnected",
                                  IDS_ASH_STATUS_TRAY_NETWORK_NOT_CONNECTED);
  html_source->AddLocalizedString("OncTypeCellular", IDS_NETWORK_TYPE_CELLULAR);
  html_source->AddLocalizedString("OncTypeEthernet", IDS_NETWORK_TYPE_ETHERNET);
  html_source->AddLocalizedString("OncTypeVPN", IDS_NETWORK_TYPE_VPN);
  html_source->AddLocalizedString("OncTypeWiFi", IDS_NETWORK_TYPE_WIFI);
  html_source->AddLocalizedString("OncTypeWimax", IDS_NETWORK_TYPE_WIMAX);
  html_source->AddLocalizedString(
      "vpnNameTemplate",
      IDS_OPTIONS_SETTINGS_SECTION_THIRD_PARTY_VPN_NAME_TEMPLATE);
}
#endif

void AddLanguagesStrings(content::WebUIDataSource* html_source) {
  html_source->AddLocalizedString(
      "languagesPageTitle", IDS_SETTINGS_LANGUAGES_PAGE_TITLE);
  html_source->AddLocalizedString(
      "languagesListTitle", IDS_SETTINGS_LANGUAGES_LANGUAGES_LIST_TITLE);
  html_source->AddLocalizedString(
      "manageLanguages", IDS_SETTINGS_LANGUAGES_LANGUAGES_MANAGE);
  html_source->AddLocalizedString(
      "inputMethodsListTitle", IDS_SETTINGS_LANGUAGES_INPUT_METHODS_LIST_TITLE);
  html_source->AddLocalizedString(
      "manageInputMethods", IDS_SETTINGS_LANGUAGES_INPUT_METHODS_MANAGE);
  html_source->AddLocalizedString(
      "spellCheckListTitle", IDS_SETTINGS_LANGUAGES_SPELL_CHECK_LIST_TITLE);
  html_source->AddLocalizedString(
      "manageSpellCheck", IDS_SETTINGS_LANGUAGES_SPELL_CHECK_MANAGE);
  html_source->AddLocalizedString(
      "manageLanguagesPageTitle",
      IDS_SETTINGS_LANGUAGES_MANAGE_LANGUAGES_TITLE);
  html_source->AddLocalizedString(
      "allLanguages", IDS_SETTINGS_LANGUAGES_ALL_LANGUAGES);
  html_source->AddLocalizedString(
      "enabledLanguages", IDS_SETTINGS_LANGUAGES_ENABLED_LANGUAGES);
  html_source->AddLocalizedString(
      "cannotBeDisplayedInThisLanguage",
      IDS_SETTINGS_LANGUAGES_CANNOT_BE_DISPLAYED_IN_THIS_LANGUAGE);
  html_source->AddLocalizedString(
      "isDisplayedInThisLanguage",
      IDS_SETTINGS_LANGUAGES_IS_DISPLAYED_IN_THIS_LANGUAGE);
  html_source->AddLocalizedString(
      "displayInThisLanguage",
      IDS_SETTINGS_LANGUAGES_DISPLAY_IN_THIS_LANGUAGE);
  html_source->AddLocalizedString(
      "offerToTranslateInThisLanguage",
      IDS_OPTIONS_LANGUAGES_OFFER_TO_TRANSLATE_IN_THIS_LANGUAGE);
  html_source->AddLocalizedString(
      "cannotTranslateInThisLanguage",
      IDS_OPTIONS_LANGUAGES_CANNOT_TRANSLATE_IN_THIS_LANGUAGE);
  html_source->AddLocalizedString(
      "restart",
      IDS_OPTIONS_SETTINGS_LANGUAGES_RELAUNCH_BUTTON);
  html_source->AddLocalizedString(
      "editDictionaryPageTitle",
      IDS_SETTINGS_LANGUAGES_EDIT_DICTIONARY_TITLE);
  html_source->AddLocalizedString(
      "addDictionaryWordLabel",
      IDS_SETTINGS_LANGUAGES_ADD_DICTIONARY_WORD);
  html_source->AddLocalizedString(
      "addDictionaryWordButton",
      IDS_SETTINGS_LANGUAGES_ADD_DICTIONARY_WORD_BUTTON);
  html_source->AddLocalizedString(
      "customDictionaryWords",
      IDS_SETTINGS_LANGUAGES_DICTIONARY_WORDS);
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
  html_source->AddLocalizedString(
      "onStartup",
      IDS_SETTINGS_ON_STARTUP);
  html_source->AddLocalizedString(
      "onStartupOpenNewTab",
      IDS_SETTINGS_ON_STARTUP_OPEN_NEW_TAB);
  html_source->AddLocalizedString(
      "onStartupContinue",
      IDS_SETTINGS_ON_STARTUP_CONTINUE);
  html_source->AddLocalizedString(
      "onStartupOpenSpecific",
      IDS_SETTINGS_ON_STARTUP_OPEN_SPECIFIC);
  html_source->AddLocalizedString(
      "onStartupUseCurrent",
      IDS_SETTINGS_ON_STARTUP_USE_CURRENT);
  html_source->AddLocalizedString(
      "onStartupAddNewPage",
      IDS_SETTINGS_ON_STARTUP_ADD_NEW_PAGE);
  html_source->AddLocalizedString(
      "onStartupSiteUrl",
      IDS_SETTINGS_ON_STARTUP_SITE_URL);
}

void AddPasswordsAndFormsStrings(content::WebUIDataSource* html_source) {
  html_source->AddLocalizedString(
      "passwordsAndAutofillPageTitle",
      IDS_SETTINGS_PASSWORDS_AND_AUTOFILL_PAGE_TITLE);
  html_source->AddLocalizedString("autofill", IDS_SETTINGS_AUTOFILL);
  html_source->AddLocalizedString("autofillDetail",
                                  IDS_SETTINGS_AUTOFILL_DETAIL);
  html_source->AddLocalizedString("passwords", IDS_SETTINGS_PASSWORDS);
  html_source->AddLocalizedString("passwordsDetail",
                                  IDS_SETTINGS_PASSWORDS_DETAIL);
  html_source->AddLocalizedString("savedPasswordsHeading",
                                  IDS_SETTINGS_PASSWORDS_SAVED_HEADING);
}

void AddPeopleStrings(content::WebUIDataSource* html_source) {
  html_source->AddLocalizedString("peoplePageTitle", IDS_SETTINGS_PEOPLE);
  html_source->AddLocalizedString("manageOtherPeople",
                                  IDS_SETTINGS_PEOPLE_MANAGE_OTHER_PEOPLE);
#if defined(OS_CHROMEOS)
  html_source->AddLocalizedString("changePictureTitle",
                                  IDS_SETTINGS_CHANGE_PICTURE_DIALOG_TITLE);
  html_source->AddLocalizedString("changePicturePageDescription",
                                  IDS_SETTINGS_CHANGE_PICTURE_DIALOG_TEXT);
  html_source->AddLocalizedString("takePhoto",
                                  IDS_SETTINGS_CHANGE_PICTURE_TAKE_PHOTO);
  html_source->AddLocalizedString("discardPhoto",
                                  IDS_SETTINGS_CHANGE_PICTURE_DISCARD_PHOTO);
  html_source->AddLocalizedString("flipPhoto",
                                  IDS_SETTINGS_CHANGE_PICTURE_FLIP_PHOTO);
  html_source->AddLocalizedString("chooseFile",
                                  IDS_SETTINGS_CHANGE_PICTURE_CHOOSE_FILE);
  html_source->AddLocalizedString("profilePhoto",
                                  IDS_SETTINGS_CHANGE_PICTURE_PROFILE_PHOTO);
  html_source->AddLocalizedString(
      "profilePhotoLoading", IDS_SETTINGS_CHANGE_PICTURE_PROFILE_LOADING_PHOTO);
  html_source->AddLocalizedString("previewAltText",
                                  IDS_SETTINGS_CHANGE_PICTURE_PREVIEW_ALT);
  html_source->AddLocalizedString("authorCredit",
                                  IDS_SETTINGS_CHANGE_PICTURE_AUTHOR_TEXT);
  html_source->AddLocalizedString(
      "photoFromCamera", IDS_SETTINGS_CHANGE_PICTURE_PHOTO_FROM_CAMERA);
  html_source->AddLocalizedString("photoFlippedAccessibleText",
                                  IDS_SETTINGS_PHOTO_FLIP_ACCESSIBLE_TEXT);
  html_source->AddLocalizedString("photoFlippedBackAccessibleText",
                                  IDS_SETTINGS_PHOTO_FLIPBACK_ACCESSIBLE_TEXT);
  html_source->AddLocalizedString("photoCaptureAccessibleText",
                                  IDS_SETTINGS_PHOTO_CAPTURE_ACCESSIBLE_TEXT);
  html_source->AddLocalizedString("photoDiscardAccessibleText",
                                  IDS_SETTINGS_PHOTO_DISCARD_ACCESSIBLE_TEXT);
#else   // !defined(OS_CHROMEOS)
  html_source->AddLocalizedString("editPerson", IDS_SETTINGS_EDIT_PERSON);
#endif  // defined(OS_CHROMEOS)

  html_source->AddLocalizedString("syncOverview", IDS_SETTINGS_SYNC_OVERVIEW);
  html_source->AddLocalizedString("syncSignin", IDS_SETTINGS_SYNC_SIGNIN);
  html_source->AddLocalizedString("syncDisconnect",
                                  IDS_SETTINGS_SYNC_DISCONNECT);
  html_source->AddLocalizedString("syncDisconnectTitle",
                                  IDS_SETTINGS_SYNC_DISCONNECT_TITLE);
  std::string disconnect_help_url =
      google_util::AppendGoogleLocaleParam(
          GURL(chrome::kSyncGoogleDashboardURL),
          g_browser_process->GetApplicationLocale())
          .spec();
  html_source->AddString(
      "syncDisconnectExplanation",
      l10n_util::GetStringFUTF16(IDS_SETTINGS_SYNC_DISCONNECT_EXPLANATION,
                                 base::ASCIIToUTF16(disconnect_help_url)));
  html_source->AddLocalizedString("syncDisconnectDeleteProfile",
                                  IDS_SETTINGS_SYNC_DISCONNECT_DELETE_PROFILE);
  html_source->AddLocalizedString("syncDisconnectConfirm",
                                  IDS_SETTINGS_SYNC_DISCONNECT_CONFIRM);

  html_source->AddLocalizedString("syncPageTitle", IDS_SETTINGS_SYNC);
  html_source->AddLocalizedString("syncLoading", IDS_SETTINGS_SYNC_LOADING);
  html_source->AddLocalizedString("syncTimeout", IDS_SETTINGS_SYNC_TIMEOUT);
  html_source->AddLocalizedString("syncEverythingCheckboxLabel",
                                  IDS_SETTINGS_SYNC_EVERYTHING_CHECKBOX_LABEL);
  html_source->AddLocalizedString("appCheckboxLabel",
                                  IDS_SETTINGS_APPS_CHECKBOX_LABEL);
  html_source->AddLocalizedString("extensionsCheckboxLabel",
                                  IDS_SETTINGS_EXTENSIONS_CHECKBOX_LABEL);
  html_source->AddLocalizedString("settingsCheckboxLabel",
                                  IDS_SETTINGS_SETTINGS_CHECKBOX_LABEL);
  html_source->AddLocalizedString("autofillCheckboxLabel",
                                  IDS_SETTINGS_AUTOFILL_CHECKBOX_LABEL);
  html_source->AddLocalizedString("historyCheckboxLabel",
                                  IDS_SETTINGS_HISTORY_CHECKBOX_LABEL);
  html_source->AddLocalizedString(
      "themesAndWallpapersCheckboxLabel",
      IDS_SETTINGS_THEMES_AND_WALLPAPERS_CHECKBOX_LABEL);
  html_source->AddLocalizedString("bookmarksCheckboxLabel",
                                  IDS_SETTINGS_BOOKMARKS_CHECKBOX_LABEL);
  html_source->AddLocalizedString("passwordsCheckboxLabel",
                                  IDS_SETTINGS_PASSWORDS_CHECKBOX_LABEL);
  html_source->AddLocalizedString("openTabsCheckboxLabel",
                                  IDS_SETTINGS_OPEN_TABS_CHECKBOX_LABEL);
  html_source->AddLocalizedString("encryptionOptionsTitle",
                                  IDS_SETTINGS_ENCRYPTION_OPTIONS);
  html_source->AddLocalizedString("syncDataEncryptedText",
                                  IDS_SETTINGS_SYNC_DATA_ENCRYPTED_TEXT);
  html_source->AddLocalizedString(
      "encryptWithGoogleCredentialsLabel",
      IDS_SETTINGS_ENCRYPT_WITH_GOOGLE_CREDENTIALS_LABEL);
  html_source->AddLocalizedString(
      "encryptWithSyncPassphraseLabel",
      IDS_SETTINGS_ENCRYPT_WITH_SYNC_PASSPHRASE_LABEL);
  html_source->AddLocalizedString(
      "encryptWithSyncPassphraseLearnMoreLink",
      IDS_SETTINGS_ENCRYPT_WITH_SYNC_PASSPHRASE_LEARN_MORE_LINK);
  html_source->AddLocalizedString("useDefaultSettingsButton",
                                  IDS_SETTINGS_USE_DEFAULT_SETTINGS_BUTTON);
  html_source->AddLocalizedString("passphraseExplanationText",
                                  IDS_SETTINGS_PASSPHRASE_EXPLANATION_TEXT);
  html_source->AddLocalizedString("emptyPassphraseError",
                                  IDS_SETTINGS_EMPTY_PASSPHRASE_ERROR);
  html_source->AddLocalizedString("mismatchedPassphraseError",
                                  IDS_SETTINGS_MISMATCHED_PASSPHRASE_ERROR);
  html_source->AddLocalizedString("incorrectPassphraseError",
                                  IDS_SETTINGS_INCORRECT_PASSPHRASE_ERROR);
  html_source->AddLocalizedString("passphrasePlaceholder",
                                  IDS_SETTINGS_PASSPHRASE_PLACEHOLDER);
  html_source->AddLocalizedString(
      "passphraseConfirmationPlaceholder",
      IDS_SETTINGS_PASSPHRASE_CONFIRMATION_PLACEHOLDER);
}

void AddPrivacyStrings(content::WebUIDataSource* html_source) {
  html_source->AddLocalizedString("privacyPageTitle",
                                  IDS_SETTINGS_PRIVACY);
  html_source->AddString("improveBrowsingExperience",
                         l10n_util::GetStringFUTF16(
                             IDS_SETTINGS_IMPROVE_BROWSING_EXPERIENCE,
                             base::ASCIIToUTF16(chrome::kPrivacyLearnMoreURL)));
  html_source->AddLocalizedString("linkDoctorPref",
                                  IDS_SETTINGS_LINKDOCTOR_PREF);
  html_source->AddLocalizedString("searchSuggestPref",
                                  IDS_SETTINGS_SUGGEST_PREF);
  html_source->AddLocalizedString(
      "networkPredictionEnabled",
      IDS_SETTINGS_NETWORK_PREDICTION_ENABLED_DESCRIPTION);
  html_source->AddLocalizedString("safeBrowsingEnableProtection",
                                  IDS_SETTINGS_SAFEBROWSING_ENABLEPROTECTION);
  html_source->AddLocalizedString(
      "safeBrowsingEnableExtendedReporting",
      IDS_SETTINGS_SAFEBROWSING_ENABLE_EXTENDED_REPORTING);
  html_source->AddLocalizedString("spellingPref",
                                  IDS_SETTINGS_SPELLING_PREF);
  html_source->AddLocalizedString("enableLogging",
                                  IDS_SETTINGS_ENABLE_LOGGING);
  html_source->AddLocalizedString("doNotTrack",
                                  IDS_SETTINGS_ENABLE_DO_NOT_TRACK);
  html_source->AddLocalizedString(
      "enableContentProtectionAttestation",
      IDS_SETTINGS_ENABLE_CONTENT_PROTECTION_ATTESTATION);
  html_source->AddLocalizedString("wakeOnWifi",
                                  IDS_SETTINGS_WAKE_ON_WIFI_DESCRIPTION);
  html_source->AddLocalizedString("manageCertificates",
                                  IDS_SETTINGS_MANAGE_CERTIFICATES);
  html_source->AddLocalizedString("siteSettings",
                                  IDS_SETTINGS_SITE_SETTINGS);
  html_source->AddLocalizedString("clearBrowsingData",
                                  IDS_SETTINGS_CLEAR_DATA);
  html_source->AddLocalizedString("titleAndCount",
                                  IDS_SETTINGS_TITLE_AND_COUNT);
}

void AddSearchStrings(content::WebUIDataSource* html_source) {
  html_source->AddLocalizedString("searchPageTitle",
                                  IDS_SETTINGS_SEARCH);
  html_source->AddLocalizedString("searchExplanation",
                                  IDS_SETTINGS_SEARCH_EXPLANATION);
  html_source->AddLocalizedString("searchManageButtonLabel",
                                  IDS_SETTINGS_SEARCH_MANAGE_BUTTON_LABEL);
  html_source->AddLocalizedString("searchOkGoogleLabel",
                                  IDS_SETTINGS_SEARCH_OK_GOOGLE_LABEL);
  html_source->AddLocalizedString(
      "searchOkGoogleLearnMoreLink",
      IDS_SETTINGS_SEARCH_OK_GOOGLE_LEARN_MORE_LINK);
  html_source->AddLocalizedString(
      "searchOkGoogleDescriptionLabel",
      IDS_SETTINGS_SEARCH_OK_GOOGLE_DESCRIPTION_LABEL);
}

void AddSearchEnginesStrings(content::WebUIDataSource* html_source) {
  html_source->AddLocalizedString("searchEnginesPageTitle",
                                  IDS_SETTINGS_SEARCH_ENGINES);
  html_source->AddLocalizedString(
      "searchEnginesAddSearchEngineLabel",
      IDS_SETTINGS_SEARCH_ENGINES_ADD_SEARCH_ENGINE_LABEL);
  html_source->AddLocalizedString("searchEnginesLabel",
                                  IDS_SETTINGS_SEARCH_ENGINES_LABEL);
  html_source->AddLocalizedString(
      "searchEnginesOtherLabel",
      IDS_SETTINGS_SEARCH_ENGINES_OTHER_ENGINES_LABEL);
  html_source->AddLocalizedString(
      "searchEnginesSearchEngineLabel",
      IDS_SETTINGS_SEARCH_ENGINES_SEARCH_ENGINE_LABEL);
  html_source->AddLocalizedString("searchEnginesKeywordLabel",
                                  IDS_SETTINGS_SEARCH_ENGINES_KEYWORD_LABEL);
  html_source->AddLocalizedString("searchEnginesQueryURLLabel",
                                  IDS_SETTINGS_SEARCH_ENGINES_QUERY_URL_LABEL);
  html_source->AddLocalizedString("searchEnginesAddButtonLabel",
                                  IDS_SETTINGS_SEARCH_ENGINES_ADD_BUTTON_LABEL);
}

void AddSiteSettingsStrings(content::WebUIDataSource* html_source) {
  html_source->AddLocalizedString("siteSettingsCategoryPageTitle",
                                  IDS_SETTINGS_SITE_SETTINGS_CATEGORY);
  html_source->AddLocalizedString("siteSettingsCategoryCamera",
                                  IDS_SETTINGS_SITE_SETTINGS_CAMERA);
  html_source->AddLocalizedString("siteSettingsCategoryCookies",
                                  IDS_SETTINGS_SITE_SETTINGS_COOKIES);
  html_source->AddLocalizedString("siteSettingsCategoryFullscreen",
                                  IDS_SETTINGS_SITE_SETTINGS_FULLSCREEN);
  html_source->AddLocalizedString("siteSettingsCategoryImages",
                                  IDS_SETTINGS_SITE_SETTINGS_IMAGES);
  html_source->AddLocalizedString("siteSettingsCategoryLocation",
                                  IDS_SETTINGS_SITE_SETTINGS_LOCATION);
  html_source->AddLocalizedString("siteSettingsCategoryJavascript",
                                  IDS_SETTINGS_SITE_SETTINGS_JAVASCRIPT);
  html_source->AddLocalizedString("siteSettingsCategoryMicrophone",
                                  IDS_SETTINGS_SITE_SETTINGS_MIC);
  html_source->AddLocalizedString("siteSettingsCategoryNotifications",
                                  IDS_SETTINGS_SITE_SETTINGS_NOTIFICATIONS);
  html_source->AddLocalizedString("siteSettingsCategoryPopups",
                                  IDS_SETTINGS_SITE_SETTINGS_POPUPS);
  html_source->AddLocalizedString("siteSettingsSiteDetailsPageTitle",
                                  IDS_SETTINGS_SITE_SETTINGS_SITE_DETAILS);
  html_source->AddLocalizedString("siteSettingsAllSites",
                                  IDS_SETTINGS_SITE_SETTINGS_ALL_SITES);
  html_source->AddLocalizedString("siteSettingsCamera",
                                  IDS_SETTINGS_SITE_SETTINGS_CAMERA);
  html_source->AddLocalizedString("siteSettingsCookies",
                                  IDS_SETTINGS_SITE_SETTINGS_COOKIES);
  html_source->AddLocalizedString("siteSettingsLocation",
                                  IDS_SETTINGS_SITE_SETTINGS_LOCATION);
  html_source->AddLocalizedString("siteSettingsMic",
                                  IDS_SETTINGS_SITE_SETTINGS_MIC);
  html_source->AddLocalizedString("siteSettingsNotifications",
                                  IDS_SETTINGS_SITE_SETTINGS_NOTIFICATIONS);
  html_source->AddLocalizedString("siteSettingsImages",
                                  IDS_SETTINGS_SITE_SETTINGS_IMAGES);
  html_source->AddLocalizedString("siteSettingsJavascript",
                                  IDS_SETTINGS_SITE_SETTINGS_JAVASCRIPT);
  html_source->AddLocalizedString("siteSettingsPopups",
                                  IDS_SETTINGS_SITE_SETTINGS_POPUPS);
  html_source->AddLocalizedString("siteSettingsFullscreen",
                                  IDS_SETTINGS_SITE_SETTINGS_FULLSCREEN);
  html_source->AddLocalizedString("siteSettingsMaySaveCookies",
                                  IDS_SETTINGS_SITE_SETTINGS_MAY_SAVE_COOKIES);
  html_source->AddLocalizedString("siteSettingsAskFirst",
                                  IDS_SETTINGS_SITE_SETTINGS_ASK_FIRST);
  html_source->AddLocalizedString("siteSettingsAskFirst",
                                  IDS_SETTINGS_SITE_SETTINGS_ASK_FIRST);
  html_source->AddLocalizedString(
      "siteSettingsAskFirstRecommended",
      IDS_SETTINGS_SITE_SETTINGS_ASK_FIRST_RECOMMENDED);
  html_source->AddLocalizedString(
      "siteSettingsAskBeforeAccessing",
      IDS_SETTINGS_SITE_SETTINGS_ASK_BEFORE_ACCESSING);
  html_source->AddLocalizedString(
      "siteSettingsAskBeforeAccessingRecommended",
      IDS_SETTINGS_SITE_SETTINGS_ASK_BEFORE_ACCESSING_RECOMMENDED);
  html_source->AddLocalizedString(
      "siteSettingsAskBeforeSending",
      IDS_SETTINGS_SITE_SETTINGS_ASK_BEFORE_SENDING);
  html_source->AddLocalizedString(
      "siteSettingsAskBeforeSendingRecommended",
      IDS_SETTINGS_SITE_SETTINGS_ASK_BEFORE_SENDING_RECOMMENDED);
  html_source->AddLocalizedString(
      "siteSettingsDontShowImages",
      IDS_SETTINGS_SITE_SETTINGS_DONT_SHOW_IMAGES);
  html_source->AddLocalizedString(
      "siteSettingsShowAll",
      IDS_SETTINGS_SITE_SETTINGS_SHOW_ALL);
  html_source->AddLocalizedString(
      "siteSettingsShowAllRecommended",
      IDS_SETTINGS_SITE_SETTINGS_SHOW_ALL_RECOMMENDED);
  html_source->AddLocalizedString(
      "siteSettingsCookiesAllowed",
      IDS_SETTINGS_SITE_SETTINGS_COOKIES_ALLOW_SITES);
  html_source->AddLocalizedString(
      "siteSettingsCookiesAllowedRecommended",
      IDS_SETTINGS_SITE_SETTINGS_COOKIES_ALLOW_SITES_RECOMMENDED);
  html_source->AddLocalizedString("siteSettingsAllow",
                                  IDS_SETTINGS_SITE_SETTINGS_ALLOW);
  html_source->AddLocalizedString("siteSettingsBlock",
                                  IDS_SETTINGS_SITE_SETTINGS_BLOCK);
  html_source->AddLocalizedString("siteSettingsAllowed",
                                  IDS_SETTINGS_SITE_SETTINGS_ALLOWED);
  html_source->AddLocalizedString(
      "siteSettingsAllowed",
      IDS_SETTINGS_SITE_SETTINGS_ALLOWED);
  html_source->AddLocalizedString(
      "siteSettingsAllowedRecommended",
      IDS_SETTINGS_SITE_SETTINGS_ALLOWED_RECOMMENDED);
  html_source->AddLocalizedString("siteSettingsBlocked",
                                  IDS_SETTINGS_SITE_SETTINGS_BLOCKED);
  html_source->AddLocalizedString(
      "siteSettingsBlocked",
      IDS_SETTINGS_SITE_SETTINGS_BLOCKED);
  html_source->AddLocalizedString(
      "siteSettingsBlockedRecommended",
      IDS_SETTINGS_SITE_SETTINGS_BLOCKED_RECOMMENDED);
  html_source->AddLocalizedString("siteSettingsExceptions",
                                  IDS_SETTINGS_SITE_SETTINGS_EXCEPTIONS);
  html_source->AddLocalizedString("siteSettingsAddSite",
                                  IDS_SETTINGS_SITE_SETTINGS_ADD_SITE);
  html_source->AddLocalizedString("siteSettingsSiteUrl",
                                  IDS_SETTINGS_SITE_SETTINGS_SITE_URL);

  html_source->AddLocalizedString("siteSettingsActionAllow",
                                  IDS_SETTINGS_SITE_SETTINGS_ALLOW_MENU);
  html_source->AddLocalizedString("siteSettingsActionBlock",
                                  IDS_SETTINGS_SITE_SETTINGS_BLOCK_MENU);
  html_source->AddLocalizedString("siteSettingsActionReset",
                                  IDS_SETTINGS_SITE_SETTINGS_RESET_MENU);
  html_source->AddLocalizedString("siteSettingsUsage",
                                  IDS_SETTINGS_SITE_SETTINGS_USAGE);
  html_source->AddLocalizedString("siteSettingsPermissions",
                                  IDS_SETTINGS_SITE_SETTINGS_PERMISSIONS);
  html_source->AddLocalizedString("siteSettingsClearAndReset",
                                  IDS_SETTINGS_SITE_SETTINGS_CLEAR_BUTTON);
  html_source->AddLocalizedString("siteSettingsDelete",
                                  IDS_SETTINGS_SITE_SETTINGS_DELETE);
}

void AddUsersStrings(content::WebUIDataSource* html_source) {
  html_source->AddLocalizedString("usersPageTitle",
                                  IDS_SETTINGS_USERS);
  html_source->AddLocalizedString("usersModifiedByOwnerLabel",
                                  IDS_SETTINGS_USERS_MODIFIED_BY_OWNER_LABEL);
  html_source->AddLocalizedString("guestBrowsingLabel",
                                  IDS_SETTINGS_USERS_GUEST_BROWSING_LABEL);
  html_source->AddLocalizedString("settingsManagedLabel",
                                  IDS_SETTINGS_USERS_MANAGED_LABEL);
  html_source->AddLocalizedString("supervisedUsersLabel",
                                  IDS_SETTINGS_USERS_SUPERVISED_USERS_LABEL);
  html_source->AddLocalizedString("showOnSigninLabel",
                                  IDS_SETTINGS_USERS_SHOW_ON_SIGNIN_LABEL);
  html_source->AddLocalizedString("restrictSigninLabel",
                                  IDS_SETTINGS_USERS_RESTRICT_SIGNIN_LABEL);
  html_source->AddLocalizedString("addUsersLabel",
                                  IDS_SETTINGS_USERS_ADD_USERS_LABEL);
}

void AddWebContentStrings(content::WebUIDataSource* html_source) {
  html_source->AddLocalizedString("webContent", IDS_SETTINGS_WEB_CONTENT);
  html_source->AddLocalizedString("pageZoom", IDS_SETTINGS_PAGE_ZOOM_LABEL);
  html_source->AddLocalizedString("fontSize", IDS_SETTINGS_FONT_SIZE_LABEL);
  html_source->AddLocalizedString("verySmall", IDS_SETTINGS_VERY_SMALL_FONT);
  html_source->AddLocalizedString("small", IDS_SETTINGS_SMALL_FONT);
  html_source->AddLocalizedString("medium", IDS_SETTINGS_MEDIUM_FONT);
  html_source->AddLocalizedString("large", IDS_SETTINGS_LARGE_FONT);
  html_source->AddLocalizedString("veryLarge", IDS_SETTINGS_VERY_LARGE_FONT);
  html_source->AddLocalizedString("custom", IDS_SETTINGS_CUSTOM);
  html_source->AddLocalizedString("customizeFonts",
                                  IDS_SETTINGS_CUSTOMIZE_FONTS);
  html_source->AddLocalizedString("fontsAndEncoding",
                                  IDS_SETTINGS_FONTS_AND_ENCODING);
  html_source->AddLocalizedString("standardFont",
                                  IDS_SETTINGS_STANDARD_FONT_LABEL);
  html_source->AddLocalizedString("serifFont", IDS_SETTINGS_SERIF_FONT_LABEL);
  html_source->AddLocalizedString("sansSerifFont",
                                  IDS_SETTINGS_SANS_SERIF_FONT_LABEL);
  html_source->AddLocalizedString("fixedWidthFont",
                                  IDS_SETTINGS_FIXED_WIDTH_FONT_LABEL);
  html_source->AddLocalizedString("minimumFont",
                                  IDS_SETTINGS_MINIMUM_FONT_SIZE_LABEL);
  html_source->AddLocalizedString("encoding", IDS_SETTINGS_ENCODING_LABEL);
  html_source->AddLocalizedString("tiny", IDS_SETTINGS_TINY_FONT_SIZE);
  html_source->AddLocalizedString("huge", IDS_SETTINGS_HUGE_FONT_SIZE);
  html_source->AddLocalizedString("loremIpsum", IDS_SETTINGS_LOREM_IPSUM);
  html_source->AddLocalizedString("loading", IDS_SETTINGS_LOADING);
  html_source->AddLocalizedString("advancedFontSettings",
                                  IDS_SETTINGS_ADVANCED_FONT_SETTINGS);
}

}  // namespace

namespace settings {

void AddLocalizedStrings(content::WebUIDataSource* html_source,
                         Profile* profile) {
  AddCommonStrings(html_source);

#if defined(OS_CHROMEOS)
  AddA11yStrings(html_source);
  AddAccountUITweaksStrings(html_source, profile);
#endif
  AddAppearanceStrings(html_source);
#if defined(OS_CHROMEOS)
  AddBluetoothStrings(html_source);
#endif
  AddCertificateManagerStrings(html_source);
  AddClearBrowsingDataStrings(html_source);
#if !defined(OS_CHROMEOS)
  AddDefaultBrowserStrings(html_source);
#endif
  AddDateTimeStrings(html_source);
  AddDownloadsStrings(html_source);
#if defined(OS_CHROMEOS)
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
  AddUsersStrings(html_source);
  AddWebContentStrings(html_source);

  policy_indicator::AddLocalizedStrings(html_source);

  html_source->SetJsonPath(kLocalizedStringsFile);
}

}  // namespace settings
