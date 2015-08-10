// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/md_settings_localized_strings_provider.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/google_chrome_strings.h"
#include "chrome/grit/locale_settings.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"
#endif

namespace {

// Note that md_settings.html contains a <script> tag which imports a script of
// the following name. These names must be kept in sync.
const char kLocalizedStringsFile[] = "strings.js";

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
      "stickyKeysSublabel", IDS_SETTINGS_STICKY_KEYS_SUBLABEL);
  html_source->AddLocalizedString(
      "chromeVoxLabel", IDS_SETTINGS_CHROMEVOX_LABEL);
  html_source->AddLocalizedString(
      "chromeVoxSublabel", IDS_SETTINGS_CHROMEVOX_SUBLABEL);
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
}

void AddAppearanceStrings(content::WebUIDataSource* html_source) {
  html_source->AddLocalizedString(
      "appearancePageTitle", IDS_SETTINGS_APPEARANCE);
  html_source->AddLocalizedString(
      "setWallpaper", IDS_SETTINGS_SET_WALLPAPER);
  html_source->AddLocalizedString(
      "getThemes", IDS_SETTINGS_GET_THEMES);
  html_source->AddLocalizedString(
      "resetToDefaultTheme", IDS_SETTINGS_RESET_TO_DEFAULT_THEME);
  html_source->AddLocalizedString(
      "showHomeButton", IDS_SETTINGS_SHOW_HOME_BUTTON);
  html_source->AddLocalizedString(
      "showBookmarksBar", IDS_SETTINGS_SHOW_BOOKMARKS_BAR);
  html_source->AddLocalizedString(
      "homePageNtp", IDS_SETTINGS_HOME_PAGE_NTP);
  html_source->AddLocalizedString(
      "changeHomePage", IDS_SETTINGS_CHANGE_HOME_PAGE);
  html_source->AddLocalizedString(
      "themesGalleryUrl", IDS_THEMES_GALLERY_URL);
}

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

void AddCommonStrings(content::WebUIDataSource* html_source) {
  html_source->AddLocalizedString("addLabel", IDS_ADD);
}

void AddDownloadsStrings(content::WebUIDataSource* html_source) {
  html_source->AddLocalizedString(
      "downloadsPageTitle", IDS_SETTINGS_DOWNLOADS);
  html_source->AddLocalizedString(
      "downloadLocation", IDS_SETTINGS_DOWNLOAD_LOCATION);
  html_source->AddLocalizedString(
      "changeDownloadLocation", IDS_SETTINGS_CHANGE_DOWNLOAD_LOCATION);
  html_source->AddLocalizedString(
      "promptForDownload", IDS_SETTINGS_PROMPT_FOR_DOWNLOAD);
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
}
#endif

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
  html_source->AddLocalizedString("searchEnginesDomainLabel",
                                  IDS_SETTINGS_SEARCH_ENGINES_DOMAIN_LABEL);
  html_source->AddLocalizedString("searchEnginesKeywordLabel",
                                  IDS_SETTINGS_SEARCH_ENGINES_KEYWORD_LABEL);
  html_source->AddLocalizedString("searchEnginesQueryURLLabel",
                                  IDS_SETTINGS_SEARCH_ENGINES_QUERY_URL_LABEL);
  html_source->AddLocalizedString("searchEnginesAddButtonLabel",
                                  IDS_SETTINGS_SEARCH_ENGINES_ADD_BUTTON_LABEL);
}

void AddSiteSettingsStrings(content::WebUIDataSource* html_source) {
  html_source->AddLocalizedString("siteSettingsPageTitle",
                                  IDS_SETTINGS_SITE_SETTINGS);
  html_source->AddLocalizedString("siteSettingsAllSites",
                                  IDS_SETTINGS_SITE_SETTINGS_ALL_SITES);
  html_source->AddLocalizedString("siteSettingsCookies",
                                  IDS_SETTINGS_SITE_SETTINGS_COOKIES);
  html_source->AddLocalizedString("siteSettingsLocation",
                                  IDS_SETTINGS_SITE_SETTINGS_LOCATION);
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
  html_source->AddLocalizedString("siteSettingsCookiesOption",
                                  IDS_SETTINGS_SITE_SETTINGS_COOKIES_OPTION);
  html_source->AddLocalizedString("siteSettingsAskFirstOption",
                                  IDS_SETTINGS_SITE_SETTINGS_ASK_FIRST_OPTION);
  html_source->AddLocalizedString("siteSettingsLocationOption",
                                  IDS_SETTINGS_SITE_SETTINGS_LOCATION_OPTION);
  html_source->AddLocalizedString("siteSettingsAllow",
                                  IDS_SETTINGS_SITE_SETTINGS_ALLOW);
  html_source->AddLocalizedString("siteSettingsBlock",
                                  IDS_SETTINGS_SITE_SETTINGS_BLOCK);
  html_source->AddLocalizedString("siteSettingsExceptions",
                                  IDS_SETTINGS_SITE_SETTINGS_EXCEPTIONS);
  html_source->AddLocalizedString("siteSettingsAddSite",
                                  IDS_SETTINGS_SITE_SETTINGS_ADD_SITE);
  html_source->AddLocalizedString("siteSettingsSiteUrl",
                                  IDS_SETTINGS_SITE_SETTINGS_SITE_URL);
}

void AddSyncStrings(content::WebUIDataSource* html_source) {
  html_source->AddLocalizedString("syncPageTitle",
                                  IDS_SETTINGS_SYNC);
  html_source->AddLocalizedString("syncEverythingMenuOption",
                                  IDS_SETTINGS_SYNC_EVERYTHING_MENU_OPTION);
  html_source->AddLocalizedString("chooseWhatToSyncMenuOption",
                                  IDS_SETTINGS_CHOOSE_WHAT_TO_SYNC_MENU_OPTION);
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
  html_source->AddLocalizedString("cancelButton",
                                  IDS_SETTINGS_CANCEL_BUTTON);
  html_source->AddLocalizedString("okButton",
                                  IDS_SETTINGS_OK_BUTTON);
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

}  // namespace

namespace settings {

void AddLocalizedStrings(content::WebUIDataSource* html_source) {
  html_source->AddLocalizedString("basicPageTitle",
                                  IDS_SETTINGS_BASIC);
  html_source->AddLocalizedString("advancedPageTitle",
                                  IDS_SETTINGS_ADVANCED);

  AddA11yStrings(html_source);
  AddAppearanceStrings(html_source);
  AddCertificateManagerStrings(html_source);
  AddCommonStrings(html_source);
  AddDownloadsStrings(html_source);
  AddDateTimeStrings(html_source);
#if defined(OS_CHROMEOS)
  AddInternetStrings(html_source);
#endif
  AddPrivacyStrings(html_source);
  AddSearchStrings(html_source);
  AddSearchEnginesStrings(html_source);
  AddSiteSettingsStrings(html_source);
  AddSyncStrings(html_source);
  AddUsersStrings(html_source);
  html_source->SetJsonPath(kLocalizedStringsFile);
}

}  // namespace settings
