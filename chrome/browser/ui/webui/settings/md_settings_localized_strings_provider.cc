// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/md_settings_localized_strings_provider.h"

#include <string>

#include "base/command_line.h"
#include "base/i18n/number_formatting.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/plugins/plugin_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_shortcut_manager.h"
#include "chrome/browser/ui/webui/policy_indicator_localized_strings_provider.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/locale_settings.h"
#include "components/autofill/core/browser/payments/payments_service_url.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/google/core/browser/google_util.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/strings/grit/components_strings.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/interfaces/voice_interaction_controller.mojom.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/sys_info.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos_factory.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/ui/webui/chromeos/bluetooth_dialog_localized_strings_provider.h"
#include "chrome/browser/ui/webui/chromeos/network_element_localized_strings_provider.h"
#include "chromeos/chromeos_switches.h"
#include "components/arc/arc_util.h"
#include "components/user_manager/user_manager.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/chromeos/events/keyboard_layout_util.h"
#include "ui/display/display_switches.h"
#include "ui/display/manager/chromeos/touch_device_manager.h"
#else
#include "chrome/browser/ui/webui/settings/system_handler.h"
#endif

#if defined(OS_WIN)
#include "chrome/browser/safe_browsing/chrome_cleaner/srt_field_trial_win.h"
#endif

#if defined(USE_NSS_CERTS)
#include "chrome/browser/ui/webui/certificate_manager_localized_strings_provider.h"
#endif

#if defined(SAFE_BROWSING_DB_LOCAL)
#include "components/safe_browsing/password_protection/password_protection_service.h"
#endif

namespace settings {
namespace {

// Note that settings.html contains a <script> tag which imports a script of
// the following name. These names must be kept in sync.
constexpr char kLocalizedStringsFile[] = "strings.js";

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

void AddCommonStrings(content::WebUIDataSource* html_source, Profile* profile) {
  LocalizedString localized_strings[] = {
    {"add", IDS_ADD},
    {"advancedPageTitle", IDS_SETTINGS_ADVANCED},
    {"back", IDS_ACCNAME_BACK},
    {"basicPageTitle", IDS_SETTINGS_BASIC},
    {"cancel", IDS_CANCEL},
    {"clear", IDS_SETTINGS_CLEAR},
    {"close", IDS_CLOSE},
    {"confirm", IDS_CONFIRM},
    {"controlledByExtension", IDS_SETTINGS_CONTROLLED_BY_EXTENSION},
#if defined(OS_CHROMEOS)
    {"deviceOff", IDS_SETTINGS_DEVICE_OFF},
    {"deviceOn", IDS_SETTINGS_DEVICE_ON},
#endif
    {"disable", IDS_DISABLE},
    {"done", IDS_DONE},
    {"edit", IDS_SETTINGS_EDIT},
    {"learnMore", IDS_LEARN_MORE},
    {"menuButtonLabel", IDS_SETTINGS_MENU_BUTTON_LABEL},
    {"moreActions", IDS_SETTINGS_MORE_ACTIONS},
    {"ok", IDS_OK},
    {"restart", IDS_SETTINGS_RESTART},
    {"save", IDS_SAVE},
    {"settings", IDS_SETTINGS_SETTINGS},
    {"toggleOn", IDS_SETTINGS_TOGGLE_ON},
    {"toggleOff", IDS_SETTINGS_TOGGLE_OFF},
    {"notValid", IDS_SETTINGS_NOT_VALID},
    {"notValidWebAddress", IDS_SETTINGS_NOT_VALID_WEB_ADDRESS},
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));

  html_source->AddBoolean(
      "isGuest",
#if defined(OS_CHROMEOS)
      user_manager::UserManager::Get()->IsLoggedInAsGuest() ||
      user_manager::UserManager::Get()->IsLoggedInAsPublicAccount());
#else
      profile->IsOffTheRecord());
#endif

  html_source->AddBoolean("isSupervised", profile->IsSupervised());
}

void AddA11yStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
    {"a11yPageTitle", IDS_SETTINGS_ACCESSIBILITY},
    {"a11yWebStore", IDS_SETTINGS_ACCESSIBILITY_WEB_STORE},
    {"moreFeaturesLink", IDS_SETTINGS_MORE_FEATURES_LINK},
    {"moreFeaturesLinkDescription",
     IDS_SETTINGS_MORE_FEATURES_LINK_DESCRIPTION},
#if defined(OS_CHROMEOS)
    {"optionsInMenuLabel", IDS_SETTINGS_OPTIONS_IN_MENU_LABEL},
    {"largeMouseCursorLabel", IDS_SETTINGS_LARGE_MOUSE_CURSOR_LABEL},
    {"largeMouseCursorSizeLabel", IDS_SETTINGS_LARGE_MOUSE_CURSOR_SIZE_LABEL},
    {"largeMouseCursorSizeDefaultLabel",
     IDS_SETTINGS_LARGE_MOUSE_CURSOR_SIZE_DEFAULT_LABEL},
    {"largeMouseCursorSizeLargeLabel",
     IDS_SETTINGS_LARGE_MOUSE_CURSOR_SIZE_LARGE_LABEL},
    {"highContrastLabel", IDS_SETTINGS_HIGH_CONTRAST_LABEL},
    {"stickyKeysLabel", IDS_SETTINGS_STICKY_KEYS_LABEL},
    {"chromeVoxLabel", IDS_SETTINGS_CHROMEVOX_LABEL},
    {"chromeVoxOptionsLabel", IDS_SETTINGS_CHROMEVOX_OPTIONS_LABEL},
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
     IDS_SETTINGS_ACCESSIBILITY_CARET_HIGHLIGHT_DESCRIPTION},
    {"cursorHighlightLabel",
     IDS_SETTINGS_ACCESSIBILITY_CURSOR_HIGHLIGHT_DESCRIPTION},
    {"focusHighlightLabel",
     IDS_SETTINGS_ACCESSIBILITY_FOCUS_HIGHLIGHT_DESCRIPTION},
    {"selectToSpeakTitle", IDS_SETTINGS_ACCESSIBILITY_SELECT_TO_SPEAK_TITLE},
    {"selectToSpeakDescription",
     IDS_SETTINGS_ACCESSIBILITY_SELECT_TO_SPEAK_DESCRIPTION},
    {"selectToSpeakOptionsLabel",
     IDS_SETTINGS_ACCESSIBILITY_SELECT_TO_SPEAK_OPTIONS_LABEL},
    {"switchAccessLabel", IDS_SETTINGS_ACCESSIBILITY_SWITCH_ACCESS_DESCRIPTION},
    {"switchAccessOptionsLabel",
     IDS_SETTINGS_ACCESSIBILITY_SWITCH_ACCESS_OPTIONS_LABEL},
    {"manageAccessibilityFeatures",
     IDS_SETTINGS_ACCESSIBILITY_MANAGE_ACCESSIBILITY_FEATURES},
    {"textToSpeechHeading", IDS_SETTINGS_ACCESSIBILITY_TEXT_TO_SPEECH_HEADING},
    {"displayHeading", IDS_SETTINGS_ACCESSIBILITY_DISPLAY_HEADING},
    {"displaySettingsTitle", IDS_SETTINGS_ACCESSIBILITY_DISPLAY_SETTINGS_TITLE},
    {"displaySettingsDescription",
     IDS_SETTINGS_ACCESSIBILITY_DISPLAY_SETTINGS_DESCRIPTION},
    {"appearanceSettingsTitle",
     IDS_SETTINGS_ACCESSIBILITY_APPEARANCE_SETTINGS_TITLE},
    {"appearanceSettingsDescription",
     IDS_SETTINGS_ACCESSIBILITY_APPEARANCE_SETTINGS_DESCRIPTION},
    {"keyboardHeading", IDS_SETTINGS_ACCESSIBILITY_KEYBOARD_HEADING},
    {"keyboardSettingsTitle",
     IDS_SETTINGS_ACCESSIBILITY_KEYBOARD_SETTINGS_TITLE},
    {"keyboardSettingsDescription",
     IDS_SETTINGS_ACCESSIBILITY_KEYBOARD_SETTINGS_DESCRIPTION},
    {"mouseAndTouchpadHeading",
     IDS_SETTINGS_ACCESSIBILITY_MOUSE_AND_TOUCHPAD_HEADING},
    {"mouseSettingsTitle", IDS_SETTINGS_ACCESSIBILITY_MOUSE_SETTINGS_TITLE},
    {"mouseSettingsDescription",
     IDS_SETTINGS_ACCESSIBILITY_MOUSE_SETTINGS_DESCRIPTION},
    {"audioHeading", IDS_SETTINGS_ACCESSIBILITY_AUDIO_HEADING},
    {"additionalFeaturesTitle",
     IDS_SETTINGS_ACCESSIBILITY_ADDITIONAL_FEATURES_TITLE},
#endif
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));

#if defined(OS_CHROMEOS)
  html_source->AddString("a11yLearnMoreUrl",
                         chrome::kChromeAccessibilityHelpURL);

  html_source->AddBoolean(
      "showExperimentalA11yFeatures",
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kEnableExperimentalAccessibilityFeatures));
#endif
}

void AddAboutStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
    {"aboutProductLogoAlt", IDS_SHORT_PRODUCT_LOGO_ALT_TEXT},
    {"aboutPageTitle", IDS_SETTINGS_ABOUT_PROGRAM},
#if defined(OS_CHROMEOS)
    {"aboutProductTitle", IDS_PRODUCT_OS_NAME},
#else
    {"aboutProductTitle", IDS_PRODUCT_NAME},
#endif
    {"aboutGetHelpUsingChrome", IDS_SETTINGS_GET_HELP_USING_CHROME},

#if defined(GOOGLE_CHROME_BUILD)
    {"aboutReportAnIssue", IDS_SETTINGS_ABOUT_PAGE_REPORT_AN_ISSUE},
#endif

    {"aboutRelaunch", IDS_SETTINGS_ABOUT_PAGE_RELAUNCH},
    {"aboutUpgradeCheckStarted", IDS_SETTINGS_ABOUT_UPGRADE_CHECK_STARTED},
    {"aboutUpgradeRelaunch", IDS_SETTINGS_UPGRADE_SUCCESSFUL_RELAUNCH},
    {"aboutUpgradeUpdating", IDS_SETTINGS_UPGRADE_UPDATING},
    {"aboutUpgradeUpdatingPercent", IDS_SETTINGS_UPGRADE_UPDATING_PERCENT},

#if defined(OS_CHROMEOS)
    {"aboutArcVersionLabel", IDS_SETTINGS_ABOUT_PAGE_ARC_VERSION},
    {"aboutBuildDateLabel", IDS_VERSION_UI_BUILD_DATE},
    {"aboutChannelBeta", IDS_SETTINGS_ABOUT_PAGE_CURRENT_CHANNEL_BETA},
    {"aboutChannelCanary", IDS_SETTINGS_ABOUT_PAGE_CURRENT_CHANNEL_CANARY},
    {"aboutChannelDev", IDS_SETTINGS_ABOUT_PAGE_CURRENT_CHANNEL_DEV},
    {"aboutChannelLabel", IDS_SETTINGS_ABOUT_PAGE_CHANNEL},
    {"aboutChannelStable", IDS_SETTINGS_ABOUT_PAGE_CURRENT_CHANNEL_STABLE},
    {"aboutCheckForUpdates", IDS_SETTINGS_ABOUT_PAGE_CHECK_FOR_UPDATES},
    {"aboutCommandLineLabel", IDS_VERSION_UI_COMMAND_LINE},
    {"aboutCurrentlyOnChannel", IDS_SETTINGS_ABOUT_PAGE_CURRENT_CHANNEL},
    {"aboutDetailedBuildInfo", IDS_SETTINGS_ABOUT_PAGE_DETAILED_BUILD_INFO},
    {"aboutFirmwareLabel", IDS_SETTINGS_ABOUT_PAGE_FIRMWARE},
    {"aboutPlatformLabel", IDS_SETTINGS_ABOUT_PAGE_PLATFORM},
    {"aboutRelaunchAndPowerwash",
     IDS_SETTINGS_ABOUT_PAGE_RELAUNCH_AND_POWERWASH},
    {"aboutUpgradeUpdatingChannelSwitch",
     IDS_SETTINGS_UPGRADE_UPDATING_CHANNEL_SWITCH},
    {"aboutUpgradeSuccessChannelSwitch",
     IDS_SETTINGS_UPGRADE_SUCCESSFUL_CHANNEL_SWITCH},
    {"aboutUserAgentLabel", IDS_VERSION_UI_USER_AGENT},
    {"aboutTPMFirmwareUpdateTitle",
     IDS_SETTINGS_ABOUT_TPM_FIRMWARE_UPDATE_TITLE},
    {"aboutTPMFirmwareUpdateDescription",
     IDS_SETTINGS_ABOUT_TPM_FIRMWARE_UPDATE_DESCRIPTION},

    // About page, channel switcher dialog.
    {"aboutChangeChannel", IDS_SETTINGS_ABOUT_PAGE_CHANGE_CHANNEL},
    {"aboutChangeChannelAndPowerwash",
     IDS_SETTINGS_ABOUT_PAGE_CHANGE_CHANNEL_AND_POWERWASH},
    {"aboutDelayedWarningMessage",
     IDS_SETTINGS_ABOUT_PAGE_DELAYED_WARNING_MESSAGE},
    {"aboutDelayedWarningTitle", IDS_SETTINGS_ABOUT_PAGE_DELAYED_WARNING_TITLE},
    {"aboutPowerwashWarningMessage",
     IDS_SETTINGS_ABOUT_PAGE_POWERWASH_WARNING_MESSAGE},
    {"aboutPowerwashWarningTitle",
     IDS_SETTINGS_ABOUT_PAGE_POWERWASH_WARNING_TITLE},
    {"aboutUnstableWarningMessage",
     IDS_SETTINGS_ABOUT_PAGE_UNSTABLE_WARNING_MESSAGE},
    {"aboutUnstableWarningTitle",
     IDS_SETTINGS_ABOUT_PAGE_UNSTABLE_WARNING_TITLE},
    {"aboutChannelDialogBeta", IDS_SETTINGS_ABOUT_PAGE_DIALOG_CHANNEL_BETA},
    {"aboutChannelDialogDev", IDS_SETTINGS_ABOUT_PAGE_DIALOG_CHANNEL_DEV},
    {"aboutChannelDialogStable", IDS_SETTINGS_ABOUT_PAGE_DIALOG_CHANNEL_STABLE},

    // About page, update warning dialog.
    {"aboutUpdateWarningMessage",
     IDS_SETTINGS_ABOUT_PAGE_UPDATE_WARNING_MESSAGE},
    {"aboutUpdateWarningTitle", IDS_SETTINGS_ABOUT_PAGE_UPDATE_WARNING_TITLE},
    {"aboutUpdateWarningContinue",
     IDS_SETTINGS_ABOUT_PAGE_UPDATE_WARNING_CONTINUE_BUTTON},
#endif  // defined(OS_CHROMEOS)
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));

  html_source->AddString(
      "aboutUpgradeUpToDate",
#if defined(OS_CHROMEOS)
      ui::SubstituteChromeOSDeviceType(IDS_SETTINGS_UPGRADE_UP_TO_DATE));
#else
      l10n_util::GetStringUTF16(IDS_SETTINGS_UPGRADE_UP_TO_DATE));
#endif

#if defined(OS_CHROMEOS)
  html_source->AddString("aboutTPMFirmwareUpdateLearnMoreURL",
                         chrome::kTPMFirmwareUpdateLearnMoreURL);
#endif
}

#if defined(OS_CHROMEOS)
void AddAndroidAppStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
      {"androidAppsPageTitle", arc::IsPlayStoreAvailable()
                                   ? IDS_SETTINGS_ANDROID_APPS_TITLE
                                   : IDS_SETTINGS_ANDROID_SETTINGS_TITLE},
      {"androidAppsPageLabel", IDS_SETTINGS_ANDROID_APPS_LABEL},
      {"androidAppsEnable", IDS_SETTINGS_ANDROID_APPS_ENABLE},
      {"androidAppsManageApps", IDS_SETTINGS_ANDROID_APPS_MANAGE_APPS},
      {"androidAppsRemove", IDS_SETTINGS_ANDROID_APPS_REMOVE},
      {"androidAppsDisableDialogTitle",
       IDS_SETTINGS_ANDROID_APPS_DISABLE_DIALOG_TITLE},
      {"androidAppsDisableDialogMessage",
       IDS_SETTINGS_ANDROID_APPS_DISABLE_DIALOG_MESSAGE},
      {"androidAppsDisableDialogRemove",
       IDS_SETTINGS_ANDROID_APPS_DISABLE_DIALOG_REMOVE},
      {"androidAppsManageAppLinks", IDS_SETTINGS_ANDROID_APPS_MANAGE_APP_LINKS},
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));
  html_source->AddString(
      "androidAppsSubtext",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_ANDROID_APPS_SUBTEXT,
          base::ASCIIToUTF16(chrome::kAndroidAppsLearnMoreURL)));
}
#endif

void AddAppearanceStrings(content::WebUIDataSource* html_source,
                          Profile* profile) {
  LocalizedString localized_strings[] = {
    {"appearancePageTitle", IDS_SETTINGS_APPEARANCE},
    {"customWebAddress", IDS_SETTINGS_CUSTOM_WEB_ADDRESS},
    {"enterCustomWebAddress", IDS_SETTINGS_ENTER_CUSTOM_WEB_ADDRESS},
    {"homeButtonDisabled", IDS_SETTINGS_HOME_BUTTON_DISABLED},
    {"themes", IDS_SETTINGS_THEMES},
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
    {"systemTheme", IDS_SETTINGS_SYSTEM_THEME},
    {"useSystemTheme", IDS_SETTINGS_USE_SYSTEM_THEME},
    {"classicTheme", IDS_SETTINGS_CLASSIC_THEME},
    {"useClassicTheme", IDS_SETTINGS_USE_CLASSIC_THEME},
#else
    {"resetToDefaultTheme", IDS_SETTINGS_RESET_TO_DEFAULT_THEME},
#endif
    {"showHomeButton", IDS_SETTINGS_SHOW_HOME_BUTTON},
    {"showBookmarksBar", IDS_SETTINGS_SHOW_BOOKMARKS_BAR},
    {"homePageNtp", IDS_SETTINGS_HOME_PAGE_NTP},
    {"changeHomePage", IDS_SETTINGS_CHANGE_HOME_PAGE},
    {"themesGalleryUrl", IDS_THEMES_GALLERY_URL},
    {"chooseFromWebStore", IDS_SETTINGS_WEB_STORE},
#if defined(OS_CHROMEOS)
    {"openWallpaperApp", IDS_SETTINGS_OPEN_WALLPAPER_APP},
    {"setWallpaper", IDS_SETTINGS_SET_WALLPAPER},
#endif
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
    {"showWindowDecorations", IDS_SHOW_WINDOW_DECORATIONS},
#endif
#if defined(OS_MACOSX)
    {"tabsToLinks", IDS_SETTINGS_TABS_TO_LINKS_PREF},
#endif
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));
}

#if defined(OS_CHROMEOS)
void AddBluetoothStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
      {"bluetoothConnected", IDS_SETTINGS_BLUETOOTH_CONNECTED},
      {"bluetoothConnecting", IDS_SETTINGS_BLUETOOTH_CONNECTING},
      {"bluetoothDeviceListPaired", IDS_SETTINGS_BLUETOOTH_DEVICE_LIST_PAIRED},
      {"bluetoothDeviceListUnpaired",
       IDS_SETTINGS_BLUETOOTH_DEVICE_LIST_UNPAIRED},
      {"bluetoothConnect", IDS_SETTINGS_BLUETOOTH_CONNECT},
      {"bluetoothDisconnect", IDS_SETTINGS_BLUETOOTH_DISCONNECT},
      {"bluetoothToggleA11yLabel",
       IDS_SETTINGS_BLUETOOTH_TOGGLE_ACCESSIBILITY_LABEL},
      {"bluetoothExpandA11yLabel",
       IDS_SETTINGS_BLUETOOTH_EXPAND_ACCESSIBILITY_LABEL},
      {"bluetoothNoDevices", IDS_SETTINGS_BLUETOOTH_NO_DEVICES},
      {"bluetoothNoDevicesFound", IDS_SETTINGS_BLUETOOTH_NO_DEVICES_FOUND},
      {"bluetoothNotConnected", IDS_SETTINGS_BLUETOOTH_NOT_CONNECTED},
      {"bluetoothPageTitle", IDS_SETTINGS_BLUETOOTH},
      {"bluetoothPairDevicePageTitle",
       IDS_SETTINGS_BLUETOOTH_PAIR_DEVICE_TITLE},
      {"bluetoothRemove", IDS_SETTINGS_BLUETOOTH_REMOVE},
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));
  chromeos::bluetooth_dialog::AddLocalizedStrings(html_source);
}
#endif

void AddChangePasswordStrings(content::WebUIDataSource* html_source) {
#if defined(SAFE_BROWSING_DB_LOCAL)
  bool show_softer_warning =
      safe_browsing::PasswordProtectionService::ShouldShowSofterWarning();

  auto title_string_id = show_softer_warning
                             ? IDS_SETTINGS_CHANGE_PASSWORD_TITLE_SOFTER
                             : IDS_SETTINGS_CHANGE_PASSWORD_TITLE;
  LocalizedString localized_strings[] = {
      {"changePasswordPageTitle", title_string_id},
      {"changePasswordPageDetails", IDS_SETTINGS_CHANGE_PASSWORD_DETAIL},
      {"changePasswordPageButton", IDS_SETTINGS_CHANGE_PASSWORD_BUTTON},
  };

  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));

  const std::string icon_id =
      show_softer_warning ? "settings:security" : "cr:warning";
  html_source->AddString("changePasswordPageIcon", icon_id);
#endif
}

void AddClearBrowsingDataStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
      {"clearFollowingItemsFrom", IDS_SETTINGS_CLEAR_FOLLOWING_ITEMS_FROM},
      {"clearTimeRange", IDS_SETTINGS_CLEAR_PERIOD_TITLE},
      {"clearBrowsingHistory", IDS_SETTINGS_CLEAR_BROWSING_HISTORY},
      {"clearBrowsingHistorySummary",
       IDS_SETTINGS_CLEAR_BROWSING_HISTORY_SUMMARY},
      {"clearDownloadHistory", IDS_SETTINGS_CLEAR_DOWNLOAD_HISTORY},
      {"clearCache", IDS_SETTINGS_CLEAR_CACHE},
      {"clearCookies", IDS_SETTINGS_CLEAR_COOKIES},
      {"clearCookiesSummary",
       IDS_SETTINGS_CLEAR_COOKIES_AND_SITE_DATA_SUMMARY_BASIC},
      {"clearCookiesCounter", IDS_DEL_COOKIES_COUNTER},
      {"clearCookiesFlash", IDS_SETTINGS_CLEAR_COOKIES_FLASH},
      {"clearPasswords", IDS_SETTINGS_CLEAR_PASSWORDS},
      {"clearFormData", IDS_SETTINGS_CLEAR_FORM_DATA},
      {"clearHostedAppData", IDS_SETTINGS_CLEAR_HOSTED_APP_DATA},
      {"clearMediaLicenses", IDS_SETTINGS_CLEAR_MEDIA_LICENSES},
      {"clearDataHour", IDS_SETTINGS_CLEAR_DATA_HOUR},
      {"clearDataDay", IDS_SETTINGS_CLEAR_DATA_DAY},
      {"clearDataWeek", IDS_SETTINGS_CLEAR_DATA_WEEK},
      {"clearData4Weeks", IDS_SETTINGS_CLEAR_DATA_4WEEKS},
      {"clearDataEverything", IDS_SETTINGS_CLEAR_DATA_EVERYTHING},
      {"clearPeriodHour", IDS_SETTINGS_CLEAR_PERIOD_HOUR},
      {"clearPeriod24Hours", IDS_SETTINGS_CLEAR_PERIOD_24_HOURS},
      {"clearPeriod7Days", IDS_SETTINGS_CLEAR_PERIOD_7_DAYS},
      {"clearPeriod4Weeks", IDS_SETTINGS_CLEAR_PERIOD_FOUR_WEEKS},
      {"clearPeriodEverything", IDS_SETTINGS_CLEAR_PERIOD_EVERYTHING},
      {"warnAboutNonClearedData", IDS_SETTINGS_CLEAR_DATA_SOME_STUFF_REMAINS},
      {"clearsSyncedData", IDS_SETTINGS_CLEAR_DATA_CLEARS_SYNCED_DATA},
      {"clearBrowsingDataLearnMoreUrl", IDS_SETTINGS_CLEAR_DATA_LEARN_MORE_URL},
      {"historyDeletionDialogTitle",
       IDS_CLEAR_BROWSING_DATA_HISTORY_NOTICE_TITLE},
      {"historyDeletionDialogOK", IDS_CLEAR_BROWSING_DATA_HISTORY_NOTICE_OK},
      {"importantSitesSubtitleCookies",
       IDS_SETTINGS_IMPORTANT_SITES_SUBTITLE_COOKIES},
      {"importantSitesSubtitleCookiesAndCache",
       IDS_SETTINGS_IMPORTANT_SITES_SUBTITLE_COOKIES_AND_CACHE},
      {"importantSitesConfirm", IDS_SETTINGS_IMPORTANT_SITES_CONFIRM},
      {"notificationWarning", IDS_SETTINGS_NOTIFICATION_WARNING},
  };

  html_source->AddString(
      "clearBrowsingHistorySummarySignedIn",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_CLEAR_BROWSING_HISTORY_SUMMARY_SIGNED_IN,
          base::ASCIIToUTF16(chrome::kMyActivityUrlInClearBrowsingData)));
  html_source->AddString(
      "clearBrowsingHistorySummarySynced",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_CLEAR_BROWSING_HISTORY_SUMMARY_SYNCED,
          base::ASCIIToUTF16(chrome::kMyActivityUrlInClearBrowsingData)));
  html_source->AddString(
      "otherFormsOfBrowsingHistory",
      l10n_util::GetStringFUTF16(
          IDS_CLEAR_BROWSING_DATA_HISTORY_FOOTER,
          l10n_util::GetStringUTF16(
              IDS_SETTINGS_CLEAR_DATA_WEB_HISTORY_URL_IN_FOOTER)));
  html_source->AddString(
      "historyDeletionDialogBody",
      l10n_util::GetStringFUTF16(
          IDS_CLEAR_BROWSING_DATA_HISTORY_NOTICE,
          l10n_util::GetStringUTF16(
              IDS_SETTINGS_CLEAR_DATA_MYACTIVITY_URL_IN_DIALOG)));

  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));
}

#if !defined(OS_CHROMEOS)
void AddDefaultBrowserStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
      {"defaultBrowser", IDS_SETTINGS_DEFAULT_BROWSER},
      {"defaultBrowserDefault", IDS_SETTINGS_DEFAULT_BROWSER_DEFAULT},
      {"defaultBrowserMakeDefault", IDS_SETTINGS_DEFAULT_BROWSER_MAKE_DEFAULT},
      {"defaultBrowserMakeDefaultButton",
       IDS_SETTINGS_DEFAULT_BROWSER_MAKE_DEFAULT_BUTTON},
      {"defaultBrowserError", IDS_SETTINGS_DEFAULT_BROWSER_ERROR},
      {"defaultBrowserSecondary", IDS_SETTINGS_DEFAULT_BROWSER_SECONDARY},
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
      {"naturalScrollLabel", IDS_SETTINGS_NATURAL_SCROLL_LABEL},
      {"naturalScrollLearnMore", IDS_SETTINGS_NATURAL_SCROLL_LEARN_MORE},
  };
  AddLocalizedStringsBulk(html_source, device_strings,
                          arraysize(device_strings));

  LocalizedString pointers_strings[] = {
      {"mouseTitle", IDS_SETTINGS_MOUSE_TITLE},
      {"touchpadTitle", IDS_SETTINGS_TOUCHPAD_TITLE},
      {"mouseAndTouchpadTitle", IDS_SETTINGS_MOUSE_AND_TOUCHPAD_TITLE},
      {"touchpadTapToClickEnabledLabel",
       IDS_SETTINGS_TOUCHPAD_TAP_TO_CLICK_ENABLED_LABEL},
      {"touchpadSpeed", IDS_SETTINGS_TOUCHPAD_SPEED_LABEL},
      {"pointerSlow", IDS_SETTINGS_POINTER_SPEED_SLOW_LABEL},
      {"pointerFast", IDS_SETTINGS_POINTER_SPEED_FAST_LABEL},
      {"mouseSpeed", IDS_SETTINGS_MOUSE_SPEED_LABEL},
      {"mouseSwapButtons", IDS_SETTINGS_MOUSE_SWAP_BUTTONS_LABEL},
      {"mouseReverseScroll", IDS_SETTINGS_MOUSE_REVERSE_SCROLL_LABEL},
  };
  AddLocalizedStringsBulk(html_source, pointers_strings,
                          arraysize(pointers_strings));

  LocalizedString keyboard_strings[] = {
      {"keyboardTitle", IDS_SETTINGS_KEYBOARD_TITLE},
      {"keyboardKeySearch", ui::DeviceUsesKeyboardLayout2()
                                ? IDS_SETTINGS_KEYBOARD_KEY_LAUNCHER
                                : IDS_SETTINGS_KEYBOARD_KEY_SEARCH},
      {"keyboardKeyCtrl", IDS_SETTINGS_KEYBOARD_KEY_LEFT_CTRL},
      {"keyboardKeyAlt", IDS_SETTINGS_KEYBOARD_KEY_LEFT_ALT},
      {"keyboardKeyCapsLock", IDS_SETTINGS_KEYBOARD_KEY_CAPS_LOCK},
      {"keyboardKeyDiamond", IDS_SETTINGS_KEYBOARD_KEY_DIAMOND},
      {"keyboardKeyEscape", IDS_SETTINGS_KEYBOARD_KEY_ESCAPE},
      {"keyboardKeyBackspace", IDS_SETTINGS_KEYBOARD_KEY_BACKSPACE},
      {"keyboardKeyDisabled", IDS_SETTINGS_KEYBOARD_KEY_DISABLED},
      {"keyboardSendFunctionKeys", IDS_SETTINGS_KEYBOARD_SEND_FUNCTION_KEYS},
      {"keyboardSendFunctionKeysDescription",
       ui::DeviceUsesKeyboardLayout2()
           ? IDS_SETTINGS_KEYBOARD_SEND_FUNCTION_KEYS_LAYOUT2_DESCRIPTION
           : IDS_SETTINGS_KEYBOARD_SEND_FUNCTION_KEYS_DESCRIPTION},
      {"keyboardEnableAutoRepeat", IDS_SETTINGS_KEYBOARD_AUTO_REPEAT_ENABLE},
      {"keyRepeatDelay", IDS_SETTINGS_KEYBOARD_AUTO_REPEAT_DELAY},
      {"keyRepeatDelayLong", IDS_SETTINGS_KEYBOARD_AUTO_REPEAT_DELAY_LONG},
      {"keyRepeatDelayShort", IDS_SETTINGS_KEYBOARD_AUTO_REPEAT_DELAY_SHORT},
      {"keyRepeatRate", IDS_SETTINGS_KEYBOARD_AUTO_REPEAT_RATE},
      {"keyRepeatRateSlow", IDS_SETTINGS_KEYBOARD_AUTO_REPEAT_RATE_SLOW},
      {"keyRepeatRateFast", IDS_SETTINGS_KEYBOARD_AUTO_REPEAT_FAST},
      {"showKeyboardShortcutsOverlay",
       IDS_SETTINGS_KEYBOARD_SHOW_KEYBOARD_SHORTCUTS_OVERLAY},
      {"keyboardShowLanguageAndInput",
       IDS_SETTINGS_KEYBOARD_SHOW_LANGUAGE_AND_INPUT},
  };
  AddLocalizedStringsBulk(html_source, keyboard_strings,
                          arraysize(keyboard_strings));

  LocalizedString stylus_strings[] = {
      {"stylusTitle", IDS_SETTINGS_STYLUS_TITLE},
      {"stylusEnableStylusTools", IDS_SETTINGS_STYLUS_ENABLE_STYLUS_TOOLS},
      {"stylusAutoOpenStylusTools", IDS_SETTINGS_STYLUS_AUTO_OPEN_STYLUS_TOOLS},
      {"stylusFindMoreAppsPrimary", IDS_SETTINGS_STYLUS_FIND_MORE_APPS_PRIMARY},
      {"stylusFindMoreAppsSecondary",
       IDS_SETTINGS_STYLUS_FIND_MORE_APPS_SECONDARY},
      {"stylusNoteTakingApp", IDS_SETTINGS_STYLUS_NOTE_TAKING_APP_LABEL},
      {"stylusNoteTakingAppEnabledOnLockScreen",
       IDS_SETTINGS_STYLUS_NOTE_TAKING_APP_LOCK_SCREEN_CHECKBOX},
      {"stylusNoteTakingAppKeepsLastNoteOnLockScreen",
       IDS_SETTINGS_STYLUS_NOTE_TAKING_APP_KEEP_LATEST_NOTE},
      {"stylusNoteTakingAppLockScreenSettingsHeader",
       IDS_SETTINGS_STYLUS_LOCK_SCREEN_NOTES_TITLE},
      {"stylusNoteTakingAppNoneAvailable",
       IDS_SETTINGS_STYLUS_NOTE_TAKING_APP_NONE_AVAILABLE},
      {"stylusNoteTakingAppWaitingForAndroid",
       IDS_SETTINGS_STYLUS_NOTE_TAKING_APP_WAITING_FOR_ANDROID}};
  AddLocalizedStringsBulk(html_source, stylus_strings,
                          arraysize(stylus_strings));

  LocalizedString display_strings[] = {
      {"displayTitle", IDS_SETTINGS_DISPLAY_TITLE},
      {"displayArrangementText", IDS_SETTINGS_DISPLAY_ARRANGEMENT_TEXT},
      {"displayArrangementTitle", IDS_SETTINGS_DISPLAY_ARRANGEMENT_TITLE},
      {"displayMirror", IDS_SETTINGS_DISPLAY_MIRROR},
      {"displayMirrorDisplayName", IDS_SETTINGS_DISPLAY_MIRROR_DISPLAY_NAME},
      {"displayNightLightLabel", IDS_SETTINGS_DISPLAY_NIGHT_LIGHT_LABEL},
      {"displayNightLightScheduleCustom",
       IDS_SETTINGS_DISPLAY_NIGHT_LIGHT_SCHEDULE_CUSTOM},
      {"displayNightLightScheduleLabel",
       IDS_SETTINGS_DISPLAY_NIGHT_LIGHT_SCHEDULE_LABEL},
      {"displayNightLightScheduleNever",
       IDS_SETTINGS_DISPLAY_NIGHT_LIGHT_SCHEDULE_NEVER},
      {"displayNightLightScheduleSunsetToSunRise",
       IDS_SETTINGS_DISPLAY_NIGHT_LIGHT_SCHEDULE_SUNSET_TO_SUNRISE},
      {"displayNightLightStartTime",
       IDS_SETTINGS_DISPLAY_NIGHT_LIGHT_START_TIME},
      {"displayNightLightStopTime", IDS_SETTINGS_DISPLAY_NIGHT_LIGHT_STOP_TIME},
      {"displayNightLightText", IDS_SETTINGS_DISPLAY_NIGHT_LIGHT_TEXT},
      {"displayNightLightTemperatureLabel",
       IDS_SETTINGS_DISPLAY_NIGHT_LIGHT_TEMPERATURE_LABEL},
      {"displayNightLightTempSliderMaxLabel",
       IDS_SETTINGS_DISPLAY_NIGHT_LIGHT_TEMP_SLIDER_MAX_LABEL},
      {"displayNightLightTempSliderMinLabel",
       IDS_SETTINGS_DISPLAY_NIGHT_LIGHT_TEMP_SLIDER_MIN_LABEL},
      {"displayUnifiedDesktop", IDS_SETTINGS_DISPLAY_UNIFIED_DESKTOP},
      {"displayResolutionTitle", IDS_SETTINGS_DISPLAY_RESOLUTION_TITLE},
      {"displayResolutionText", IDS_SETTINGS_DISPLAY_RESOLUTION_TEXT},
      {"displayResolutionTextBest", IDS_SETTINGS_DISPLAY_RESOLUTION_TEXT_BEST},
      {"displayResolutionTextNative",
       IDS_SETTINGS_DISPLAY_RESOLUTION_TEXT_NATIVE},
      {"displayScreenTitle", IDS_SETTINGS_DISPLAY_SCREEN},
      {"displayScreenExtended", IDS_SETTINGS_DISPLAY_SCREEN_EXTENDED},
      {"displayScreenPrimary", IDS_SETTINGS_DISPLAY_SCREEN_PRIMARY},
      {"displayOrientation", IDS_SETTINGS_DISPLAY_ORIENTATION},
      {"displayOrientationStandard", IDS_SETTINGS_DISPLAY_ORIENTATION_STANDARD},
      {"displayOverscanPageText", IDS_SETTINGS_DISPLAY_OVERSCAN_TEXT},
      {"displayOverscanPageTitle", IDS_SETTINGS_DISPLAY_OVERSCAN_TITLE},
      {"displayOverscanSubtitle", IDS_SETTINGS_DISPLAY_OVERSCAN_SUBTITLE},
      {"displayOverscanInstructions",
       IDS_SETTINGS_DISPLAY_OVERSCAN_INSTRUCTIONS},
      {"displayOverscanResize", IDS_SETTINGS_DISPLAY_OVERSCAN_RESIZE},
      {"displayOverscanPosition", IDS_SETTINGS_DISPLAY_OVERSCAN_POSITION},
      {"displayOverscanReset", IDS_SETTINGS_DISPLAY_OVERSCAN_RESET},
      {"displayTouchCalibrationTitle",
       IDS_SETTINGS_DISPLAY_TOUCH_CALIBRATION_TITLE},
      {"displayTouchCalibrationText",
       IDS_SETTINGS_DISPLAY_TOUCH_CALIBRATION_TEXT}};
  AddLocalizedStringsBulk(html_source, display_strings,
                          arraysize(display_strings));
  html_source->AddBoolean("unifiedDesktopAvailable",
                          base::CommandLine::ForCurrentProcess()->HasSwitch(
                              ::switches::kEnableUnifiedDesktop));
  html_source->AddBoolean("multiMirroringAvailable",
                          base::CommandLine::ForCurrentProcess()->HasSwitch(
                              ::switches::kEnableMultiMirroring));

  html_source->AddBoolean(
      "enableTouchCalibrationSetting",
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kEnableTouchCalibrationSetting));

  html_source->AddBoolean("hasExternalTouchDevice",
                          display::HasExternalTouchscreenDevice());

  html_source->AddBoolean("nightLightFeatureEnabled",
                          ash::switches::IsNightLightEnabled());

  LocalizedString storage_strings[] = {
      {"storageTitle", IDS_SETTINGS_STORAGE_TITLE},
      {"storageItemInUse", IDS_SETTINGS_STORAGE_ITEM_IN_USE},
      {"storageItemAvailable", IDS_SETTINGS_STORAGE_ITEM_AVAILABLE},
      {"storageItemDownloads", IDS_SETTINGS_STORAGE_ITEM_DOWNLOADS},
      {"storageItemDriveCache", IDS_SETTINGS_STORAGE_ITEM_DRIVE_CACHE},
      {"storageItemBrowsingData", IDS_SETTINGS_STORAGE_ITEM_BROWSING_DATA},
      {"storageItemAndroid", IDS_SETTINGS_STORAGE_ITEM_ANDROID},
      {"storageItemOtherUsers", IDS_SETTINGS_STORAGE_ITEM_OTHER_USERS},
      {"storageSizeComputing", IDS_SETTINGS_STORAGE_SIZE_CALCULATING},
      {"storageSizeUnknown", IDS_SETTINGS_STORAGE_SIZE_UNKNOWN},
      {"storageSpaceLowMessageTitle",
       IDS_SETTINGS_STORAGE_SPACE_LOW_MESSAGE_TITLE},
      {"storageSpaceLowMessageLine1",
       IDS_SETTINGS_STORAGE_SPACE_LOW_MESSAGE_LINE_1},
      {"storageSpaceLowMessageLine2",
       IDS_SETTINGS_STORAGE_SPACE_LOW_MESSAGE_LINE_2},
      {"storageSpaceCriticallyLowMessageTitle",
       IDS_SETTINGS_STORAGE_SPACE_CRITICALLY_LOW_MESSAGE_TITLE},
      {"storageSpaceCriticallyLowMessageLine1",
       IDS_SETTINGS_STORAGE_SPACE_CRITICALLY_LOW_MESSAGE_LINE_1},
      {"storageSpaceCriticallyLowMessageLine2",
       IDS_SETTINGS_STORAGE_SPACE_CRITICALLY_LOW_MESSAGE_LINE_2},
      {"storageClearDriveCacheDialogTitle",
       IDS_SETTINGS_STORAGE_CLEAR_DRIVE_CACHE_DIALOG_TITLE},
      {"storageClearDriveCacheDialogDescription",
       IDS_SETTINGS_STORAGE_CLEAR_DRIVE_CACHE_DESCRIPTION},
      {"storageDeleteAllButtonTitle",
       IDS_SETTINGS_STORAGE_DELETE_ALL_BUTTON_TITLE}};
  AddLocalizedStringsBulk(html_source, storage_strings,
                          arraysize(storage_strings));

  LocalizedString power_strings[] = {
      {"powerTitle", IDS_SETTINGS_POWER_TITLE},
      {"powerSourceLabel", IDS_SETTINGS_POWER_SOURCE_LABEL},
      {"powerSourceBattery", IDS_SETTINGS_POWER_SOURCE_BATTERY},
      {"powerSourceAcAdapter", IDS_SETTINGS_POWER_SOURCE_AC_ADAPTER},
      {"powerSourceLowPowerCharger",
       IDS_SETTINGS_POWER_SOURCE_LOW_POWER_CHARGER},
      {"calculatingPower", IDS_SETTINGS_POWER_SOURCE_CALCULATING},
      {"powerIdleLabel", IDS_SETTINGS_POWER_IDLE_LABEL},
      {"powerIdleDisplayOffSleep", IDS_SETTINGS_POWER_IDLE_DISPLAY_OFF_SLEEP},
      {"powerIdleDisplayOff", IDS_SETTINGS_POWER_IDLE_DISPLAY_OFF},
      {"powerIdleDisplayOn", IDS_SETTINGS_POWER_IDLE_DISPLAY_ON},
      {"powerIdleOther", IDS_SETTINGS_POWER_IDLE_OTHER},
      {"powerLidSleepLabel", IDS_SETTINGS_POWER_LID_CLOSED_SLEEP_LABEL},
      {"powerLidSignOutLabel", IDS_SETTINGS_POWER_LID_CLOSED_SIGN_OUT_LABEL},
      {"powerLidShutDownLabel", IDS_SETTINGS_POWER_LID_CLOSED_SHUT_DOWN_LABEL},
  };
  AddLocalizedStringsBulk(html_source, power_strings, arraysize(power_strings));

  html_source->AddString("naturalScrollLearnMoreLink",
                         base::ASCIIToUTF16(chrome::kNaturalScrollHelpURL));
}
#endif

void AddDownloadsStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
      {"downloadsPageTitle", IDS_SETTINGS_DOWNLOADS},
      {"downloadLocation", IDS_SETTINGS_DOWNLOAD_LOCATION},
      {"changeDownloadLocation", IDS_SETTINGS_CHANGE_DOWNLOAD_LOCATION},
      {"promptForDownload", IDS_SETTINGS_PROMPT_FOR_DOWNLOAD},
      {"disconnectGoogleDriveAccount", IDS_SETTINGS_DISCONNECT_GOOGLE_DRIVE},
      {"openFileTypesAutomatically",
       IDS_SETTINGS_OPEN_FILE_TYPES_AUTOMATICALLY}};
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));
}

#if defined(OS_WIN) && defined(GOOGLE_CHROME_BUILD)
void AddChromeCleanupStrings(content::WebUIDataSource* html_source) {
  const wchar_t kUnwantedSoftwareProtectionWhitePaperUrl[] =
      L"https://www.google.ca/chrome/browser/privacy/"
      "whitepaper.html#unwantedsoftware";

  LocalizedString localized_strings[] = {
      {"chromeCleanupPageTitle",
       IDS_SETTINGS_RESET_CLEAN_UP_COMPUTER_PAGE_TITLE},
      {"chromeCleanupDetailsFilesAndPrograms",
       IDS_SETTINGS_RESET_CLEANUP_DETAILS_FILES_AND_PROGRAMS},
      {"chromeCleanupDetailsRegistryEntries",
       IDS_SETTINGS_RESET_CLEANUP_DETAILS_REGISTRY_ENTRIES},
      {"chromeCleanupDoneButtonLabel",
       IDS_SETTINGS_RESET_CLEANUP_DONE_BUTTON_LABEL},
      {"chromeCleanupExplanationCleanupError",
       IDS_SETTINGS_RESET_CLEANUP_EXPLANATION_CLEANUP_ERROR},
      {"chromeCleanupExplanationFindAndRemove",
       IDS_SETTINGS_RESET_CLEANUP_EXPLANATION_FIND_AND_REMOVE},
      {"chromeCleanupExplanationNoInternet",
       IDS_SETTINGS_RESET_CLEANUP_EXPLANATION_NO_INTERNET_CONNECTION},
      {"chromeCleanupExplanationPermissionsNeeded",
       IDS_SETTINGS_RESET_CLEANUP_EXPLANATION_PERMISSIONS_NEEDED},
      {"chromeCleanupExplanationRemove",
       IDS_SETTINGS_RESET_CLEANUP_EXPLANATION_REMOVE},
      {"chromeCleanupExplanationScanError",
       IDS_SETTINGS_RESET_CLEANUP_EXPLANATION_SCAN_ERROR},
      {"chromeCleanupFindButtonLable",
       IDS_SETTINGS_RESET_CLEANUP_FIND_BUTTON_LABEL},
      {"chromeCleanupLinkShowFiles",
       IDS_SETTINGS_RESET_CLEANUP_LINK_SHOW_FILES},
      {"chromeCleanupLogsUploadPermission", IDS_CHROME_CLEANUP_LOGS_PERMISSION},
      {"chromeCleanupRemoveButtonLabel",
       IDS_SETTINGS_RESET_CLEANUP_REMOVE_BUTTON_LABEL},
      {"chromeCleanupRestartButtonLabel",
       IDS_SETTINGS_RESET_CLEANUP_RESTART_BUTTON_LABEL},
      {"chromeCleanupTitleDone", IDS_SETTINGS_RESET_CLEANUP_TITLE_DONE},
      {"chromeCleanupTitleErrorCantRemove",
       IDS_SETTINGS_RESET_CLEANUP_TITLE_ERROR_CANT_REMOVE},
      {"chromeCleanupTitleErrorPermissions",
       IDS_SETTINGS_RESET_CLEANUP_TITLE_ERROR_PERMISSIONS_NEEDED},
      {"chromeCleanupTitleFindAndRemove",
       IDS_SETTINGS_RESET_CLEANUP_TITLE_FIND_AND_REMOVE},
      {"chromeCleanupTitleNoInternet",
       IDS_SETTINGS_RESET_CLEANUP_TITLE_NO_INTERNET_CONNECTION},
      {"chromeCleanupTitleNothingFound",
       IDS_SETTINGS_RESET_CLEANUP_TITLE_NOTHING_FOUND},
      {"chromeCleanupTitleRemove", IDS_SETTINGS_RESET_CLEANUP_TITLE_REMOVE},
      {"chromeCleanupTitleRemoved", IDS_SETTINGS_RESET_CLEANUP_TITLE_REMOVED},
      {"chromeCleanupTitleRemoving", IDS_SETTINGS_RESET_CLEANUP_TITLE_REMOVING},
      {"chromeCleanupTitleRestart", IDS_SETTINGS_RESET_CLEANUP_TITLE_RESTART},
      {"chromeCleanupTitleScanning", IDS_SETTINGS_RESET_CLEANUP_TITLE_SCANNING},
      {"chromeCleanupTitleScanningFailed",
       IDS_SETTINGS_RESET_CLEANUP_TITLE_ERROR_SCANNING_FAILED},
      {"chromeCleanupTitleTryAgainButtonLabel",
       IDS_SETTINGS_RESET_CLEANUP_TRY_AGAIN_BUTTON_LABEL},
      {"chromeCleanupTitleLogsPermissionExplanation",
       IDS_SETTINGS_RESET_CLEANUP_LOGS_PERMISSION_EXPLANATION},
  };

  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));
  const std::string cleanup_learn_more_url =
      google_util::AppendGoogleLocaleParam(
          GURL(chrome::kChromeCleanerLearnMoreURL),
          g_browser_process->GetApplicationLocale())
          .spec();
  html_source->AddString("chromeCleanupLearnMoreUrl", cleanup_learn_more_url);

  const base::string16 powered_by_html =
      l10n_util::GetStringFUTF16(IDS_SETTINGS_RESET_CLEANUP_FOOTER_POWERED_BY,
                                 L"<span id='powered-by-logo'></span>");
  html_source->AddString("chromeCleanupPoweredByHtml", powered_by_html);

  const base::string16 cleanup_details_explanation =
      l10n_util::GetStringFUTF16(IDS_SETTINGS_RESET_CLEANUP_DETAILS_EXPLANATION,
                                 kUnwantedSoftwareProtectionWhitePaperUrl);
  html_source->AddString("chromeCleanupDetailsExplanation",
                         cleanup_details_explanation);
}
#endif  // defined(OS_WIN) && defined(GOOGLE_CHROME_BUILD)

void AddResetStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
    {"resetPageTitle", IDS_SETTINGS_RESET},
    {"resetPageDescription", IDS_SETTINGS_RESET_PROFILE_SETTINGS_DESCRIPTION},
    {"resetPageExplanation", IDS_RESET_PROFILE_SETTINGS_EXPLANATION},
    {"triggeredResetPageExplanation",
     IDS_TRIGGERED_RESET_PROFILE_SETTINGS_EXPLANATION},
    {"triggeredResetPageTitle", IDS_TRIGGERED_RESET_PROFILE_SETTINGS_TITLE},
    {"resetPageCommit", IDS_RESET_PROFILE_SETTINGS_COMMIT_BUTTON},
    {"resetPageFeedback", IDS_SETTINGS_RESET_PROFILE_FEEDBACK},
#if defined(OS_CHROMEOS)
    {"powerwashTitle", IDS_SETTINGS_FACTORY_RESET},
    {"powerwashDialogTitle", IDS_SETTINGS_FACTORY_RESET_HEADING},
    {"powerwashDialogExplanation", IDS_SETTINGS_FACTORY_RESET_WARNING},
    {"powerwashDialogButton", IDS_SETTINGS_RESTART},
    {"powerwashLearnMoreUrl", IDS_FACTORY_RESET_HELP_URL},
#endif
    // Automatic reset banner (now a dialog).
    {"resetAutomatedDialogTitle", IDS_SETTINGS_RESET_AUTOMATED_DIALOG_TITLE},
    {"resetProfileBannerButton", IDS_SETTINGS_RESET_BANNER_RESET_BUTTON_TEXT},
    {"resetProfileBannerDescription", IDS_SETTINGS_RESET_BANNER_TEXT},
#if defined(OS_WIN) && defined(GOOGLE_CHROME_BUILD)
    // The page title to be used if user initiated cleanups are enabled.
    {"resetAndCleanupPageTitle", IDS_SETTINGS_RESET_AND_CLEANUP},
    {"resetCleanupComputerTrigger",
     IDS_SETTINGS_RESET_CLEAN_UP_COMPUTER_TRIGGER},
    {"resetCleanupComputerTriggerDescription",
     IDS_SETTINGS_RESET_CLEAN_UP_COMPUTER_TRIGGER_DESCRIPTION},
#endif
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
      l10n_util::GetStringFUTF16(IDS_SETTINGS_FACTORY_RESET_DESCRIPTION,
                                 l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)));
#endif
}

#if !defined(OS_CHROMEOS)
void AddImportDataStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
    {"importTitle", IDS_SETTINGS_IMPORT_SETTINGS_TITLE},
    {"importFromLabel", IDS_SETTINGS_IMPORT_FROM_LABEL},
    {"importDescription", IDS_SETTINGS_IMPORT_ITEMS_LABEL},
    {"importLoading", IDS_SETTINGS_IMPORT_LOADING_PROFILES},
    {"importHistory", IDS_SETTINGS_IMPORT_HISTORY_CHECKBOX},
    {"importFavorites", IDS_SETTINGS_IMPORT_FAVORITES_CHECKBOX},
    {"importPasswords", IDS_SETTINGS_IMPORT_PASSWORDS_CHECKBOX},
    {"importSearch", IDS_SETTINGS_IMPORT_SEARCH_ENGINES_CHECKBOX},
    {"importAutofillFormData", IDS_SETTINGS_IMPORT_AUTOFILL_FORM_DATA_CHECKBOX},
    {"importChooseFile", IDS_SETTINGS_IMPORT_CHOOSE_FILE},
    {"importCommit", IDS_SETTINGS_IMPORT_COMMIT},
    {"noProfileFound", IDS_SETTINGS_IMPORT_NO_PROFILE_FOUND},
    {"importSuccess", IDS_SETTINGS_IMPORT_SUCCESS},
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));
}
#endif

#if defined(OS_CHROMEOS)
void AddDateTimeStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
      {"dateTimePageTitle", IDS_SETTINGS_DATE_TIME},
      {"timeZoneColon", IDS_SETTINGS_TIME_ZONE},
      {"timeZoneGeolocation", IDS_SETTINGS_TIME_ZONE_GEOLOCATION},
      {"timeZoneButton", IDS_SETTINGS_TIME_ZONE_BUTTON},
      {"timeZoneSubpageTitle", IDS_SETTINGS_TIME_ZONE_SUBPAGE_TITLE},
      {"setTimeZoneAutomaticallyDisabled",
       IDS_SETTINGS_TIME_ZONE_DETECTION_MODE_DISABLED},
      {"setTimeZoneAutomaticallyIpOnlyDefault",
       IDS_SETTINGS_TIME_ZONE_DETECTION_MODE_IP_ONLY_DEFAULT},
      {"setTimeZoneAutomaticallyIpOnlyDefaultDescription",
       IDS_SETTINGS_TIME_ZONE_DETECTION_MODE_IP_ONLY_DESCRIPTION},
      {"setTimeZoneAutomaticallyWithWiFiAccessPointsData",
       IDS_SETTINGS_TIME_ZONE_DETECTION_MODE_SEND_WIFI_AP},
      {"setTimeZoneAutomaticallyWithWiFiAccessPointsDataDescription",
       IDS_SETTINGS_TIME_ZONE_DETECTION_MODE_SEND_WIFI_AP_DESCRIPTION},
      {"setTimeZoneAutomaticallyWithAllLocationInfo",
       IDS_SETTINGS_TIME_ZONE_DETECTION_MODE_SEND_ALL_INFO},
      {"setTimeZoneAutomaticallyWithAllLocationInfoDescription",
       IDS_SETTINGS_TIME_ZONE_DETECTION_MODE_SEND_ALL_INFO_DESCRIPTION},
      {"selectYourTimeZone",
       IDS_SETTINGS_TIME_ZONE_DETECTION_SELECT_YOUR_TIME_ZONE},
      {"use24HourClock", IDS_SETTINGS_USE_24_HOUR_CLOCK},
      {"setDateTime", IDS_SETTINGS_SET_DATE_TIME},
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));
}

void AddEasyUnlockStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
      {"easyUnlockSectionTitle", IDS_SETTINGS_EASY_UNLOCK_SECTION_TITLE},
      {"easyUnlockSetupButton", IDS_SETTINGS_EASY_UNLOCK_SETUP},
      // Easy Unlock turn-off dialog.
      {"easyUnlockTurnOffButton", IDS_SETTINGS_EASY_UNLOCK_TURN_OFF},
      {"easyUnlockTurnOffOfflineTitle",
       IDS_SETTINGS_EASY_UNLOCK_TURN_OFF_OFFLINE_TITLE},
      {"easyUnlockTurnOffOfflineMessage",
       IDS_SETTINGS_EASY_UNLOCK_TURN_OFF_OFFLINE_MESSAGE},
      {"easyUnlockTurnOffErrorTitle",
       IDS_SETTINGS_EASY_UNLOCK_TURN_OFF_ERROR_TITLE},
      {"easyUnlockTurnOffErrorMessage",
       IDS_SETTINGS_EASY_UNLOCK_TURN_OFF_ERROR_MESSAGE},
      {"easyUnlockTurnOffRetryButton", IDS_SETTINGS_EASY_UNLOCK_TURN_OFF_RETRY},
      {"easyUnlockAllowSignInLabel",
       IDS_SETTINGS_EASY_UNLOCK_ALLOW_SIGN_IN_LABEL},
      {"easyUnlockProximityThresholdLabel",
       IDS_SETTINGS_EASY_UNLOCK_PROXIMITY_THRESHOLD_LABEL},
      {"easyUnlockProximityThresholdVeryClose",
       IDS_SETTINGS_EASY_UNLOCK_PROXIMITY_THRESHOLD_VERY_CLOSE},
      {"easyUnlockProximityThresholdClose",
       IDS_SETTINGS_EASY_UNLOCK_PROXIMITY_THRESHOLD_CLOSE},
      {"easyUnlockProximityThresholdFar",
       IDS_SETTINGS_EASY_UNLOCK_PROXIMITY_THRESHOLD_FAR},
      {"easyUnlockProximityThresholdVeryFar",
       IDS_SETTINGS_EASY_UNLOCK_PROXIMITY_THRESHOLD_VERY_FAR},
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));

  base::string16 device_name =
      l10n_util::GetStringUTF16(ui::GetChromeOSDeviceTypeResourceId());
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
      "easyUnlockProximityThresholdLabel",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_EASY_UNLOCK_PROXIMITY_THRESHOLD_LABEL, device_name));

  html_source->AddString("easyUnlockLearnMoreURL",
                         chrome::kEasyUnlockLearnMoreUrl);
}

void AddInternetStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
      {"internetAddConnection", IDS_SETTINGS_INTERNET_ADD_CONNECTION},
      {"internetAddConnectionExpandA11yLabel",
       IDS_SETTINGS_INTERNET_ADD_CONNECTION_EXPAND_ACCESSIBILITY_LABEL},
      {"internetAddConnectionNotAllowed",
       IDS_SETTINGS_INTERNET_ADD_CONNECTION_NOT_ALLOWED},
      {"internetAddThirdPartyVPN", IDS_SETTINGS_INTERNET_ADD_THIRD_PARTY_VPN},
      {"internetAddVPN", IDS_SETTINGS_INTERNET_ADD_VPN},
      {"internetAddArcVPN", IDS_SETTINGS_INTERNET_ADD_ARC_VPN},
      {"internetAddArcVPNProvider", IDS_SETTINGS_INTERNET_ADD_ARC_VPN_PROVIDER},
      {"internetAddWiFi", IDS_SETTINGS_INTERNET_ADD_WIFI},
      {"internetConfigName", IDS_SETTINGS_INTERNET_CONFIG_NAME},
      {"internetDetailPageTitle", IDS_SETTINGS_INTERNET_DETAIL},
      {"internetDeviceEnabling", IDS_SETTINGS_INTERNET_DEVICE_ENABLING},
      {"internetDeviceInitializing", IDS_SETTINGS_INTERNET_DEVICE_INITIALIZING},
      {"internetJoinType", IDS_SETTINGS_INTERNET_JOIN_TYPE},
      {"internetKnownNetworksPageTitle", IDS_SETTINGS_INTERNET_KNOWN_NETWORKS},
      {"internetMobileSearching", IDS_SETTINGS_INTERNET_MOBILE_SEARCH},
      {"internetNoNetworks", IDS_SETTINGS_INTERNET_NO_NETWORKS},
      {"internetPageTitle", IDS_SETTINGS_INTERNET},
      {"internetToggleMobileA11yLabel",
       IDS_SETTINGS_INTERNET_TOGGLE_MOBILE_ACCESSIBILITY_LABEL},
      {"internetToggleTetherLabel", IDS_SETTINGS_INTERNET_TOGGLE_TETHER_LABEL},
      {"internetToggleTetherSubtext",
       IDS_SETTINGS_INTERNET_TOGGLE_TETHER_SUBTEXT},
      {"internetToggleWiFiA11yLabel",
       IDS_SETTINGS_INTERNET_TOGGLE_WIFI_ACCESSIBILITY_LABEL},
      {"internetToggleWiMAXA11yLabel",
       IDS_SETTINGS_INTERNET_TOGGLE_WIMAX_ACCESSIBILITY_LABEL},
      {"knownNetworksAll", IDS_SETTINGS_INTERNET_KNOWN_NETWORKS_ALL},
      {"knownNetworksButton", IDS_SETTINGS_INTERNET_KNOWN_NETWORKS_BUTTON},
      {"knownNetworksMessage", IDS_SETTINGS_INTERNET_KNOWN_NETWORKS_MESSAGE},
      {"knownNetworksPreferred",
       IDS_SETTINGS_INTERNET_KNOWN_NETWORKS_PREFFERED},
      {"knownNetworksMenuAddPreferred",
       IDS_SETTINGS_INTERNET_KNOWN_NETWORKS_MENU_ADD_PREFERRED},
      {"knownNetworksMenuRemovePreferred",
       IDS_SETTINGS_INTERNET_KNOWN_NETWORKS_MENU_REMOVE_PREFERRED},
      {"knownNetworksMenuForget",
       IDS_SETTINGS_INTERNET_KNOWN_NETWORKS_MENU_FORGET},
      {"networkAllowDataRoaming",
       IDS_SETTINGS_SETTINGS_NETWORK_ALLOW_DATA_ROAMING},
      {"networkAutoConnect", IDS_SETTINGS_INTERNET_NETWORK_AUTO_CONNECT},
      {"networkButtonActivate", IDS_SETTINGS_INTERNET_BUTTON_ACTIVATE},
      {"networkButtonConfigure", IDS_SETTINGS_INTERNET_BUTTON_CONFIGURE},
      {"networkButtonConnect", IDS_SETTINGS_INTERNET_BUTTON_CONNECT},
      {"networkButtonDisconnect", IDS_SETTINGS_INTERNET_BUTTON_DISCONNECT},
      {"networkButtonForget", IDS_SETTINGS_INTERNET_BUTTON_FORGET},
      {"networkButtonViewAccount", IDS_SETTINGS_INTERNET_BUTTON_VIEW_ACCOUNT},
      {"networkConnectNotAllowed", IDS_SETTINGS_INTERNET_CONNECT_NOT_ALLOWED},
      {"networkIPAddress", IDS_SETTINGS_INTERNET_NETWORK_IP_ADDRESS},
      {"networkIPConfigAuto", IDS_SETTINGS_INTERNET_NETWORK_IP_CONFIG_AUTO},
      {"networkNameserversLearnMore",
       IDS_SETTINGS_INTERNET_NETWORK_NAMESERVERS_LEARN_MORE},
      {"networkPrefer", IDS_SETTINGS_INTERNET_NETWORK_PREFER},
      {"networkPrimaryUserControlled",
       IDS_SETTINGS_INTERNET_NETWORK_PRIMARY_USER_CONTROLLED},
      {"networkSectionAdvanced",
       IDS_SETTINGS_INTERNET_NETWORK_SECTION_ADVANCED},
      {"networkSectionAdvancedA11yLabel",
       IDS_SETTINGS_INTERNET_NETWORK_SECTION_ADVANCED_ACCESSIBILITY_LABEL},
      {"networkSectionNetwork", IDS_SETTINGS_INTERNET_NETWORK_SECTION_NETWORK},
      {"networkSectionNetworkExpandA11yLabel",
       IDS_SETTINGS_INTERNET_NETWORK_SECTION_NETWORK_ACCESSIBILITY_LABEL},
      {"networkSectionProxy", IDS_SETTINGS_INTERNET_NETWORK_SECTION_PROXY},
      {"networkSectionProxyExpandA11yLabel",
       IDS_SETTINGS_INTERNET_NETWORK_SECTION_PROXY_ACCESSIBILITY_LABEL},
      {"networkShared", IDS_SETTINGS_INTERNET_NETWORK_SHARED},
      {"networkVpnBuiltin", IDS_NETWORK_TYPE_VPN_BUILTIN},
      {"tetherConnectionDialogTitle",
       IDS_SETTINGS_INTERNET_TETHER_CONNECTION_DIALOG_TITLE},
      {"tetherConnectionAvailableDeviceTitle",
       IDS_SETTINGS_INTERNET_TETHER_CONNECTION_AVAILABLE_DEVICE_TITLE},
      {"tetherConnectionBatteryPercentage",
       IDS_SETTINGS_INTERNET_TETHER_CONNECTION_BATTERY_PERCENTAGE},
      {"tetherConnectionExplanation",
       IDS_SETTINGS_INTERNET_TETHER_CONNECTION_EXPLANATION},
      {"tetherConnectionCarrierWarning",
       IDS_SETTINGS_INTERNET_TETHER_CONNECTION_CARRIER_WARNING},
      {"tetherConnectionDescriptionTitle",
       IDS_SETTINGS_INTERNET_TETHER_CONNECTION_DESCRIPTION_TITLE},
      {"tetherConnectionDescriptionMobileData",
       IDS_SETTINGS_INTERNET_TETHER_CONNECTION_DESCRIPTION_MOBILE_DATA},
      {"tetherConnectionDescriptionBattery",
       IDS_SETTINGS_INTERNET_TETHER_CONNECTION_DESCRIPTION_BATTERY},
      {"tetherConnectionDescriptionWiFi",
       IDS_SETTINGS_INTERNET_TETHER_CONNECTION_DESCRIPTION_WIFI},
      {"tetherConnectionNotNowButton",
       IDS_SETTINGS_INTERNET_TETHER_CONNECTION_NOT_NOW_BUTTON},
      {"tetherConnectionConnectButton",
       IDS_SETTINGS_INTERNET_TETHER_CONNECTION_CONNECT_BUTTON},
      {"tetherEnableBluetooth", IDS_ASH_STATUS_TRAY_ENABLE_BLUETOOTH},
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));

  html_source->AddBoolean("networkSettingsConfig",
                          base::CommandLine::ForCurrentProcess()->HasSwitch(
                              chromeos::switches::kNetworkSettingsConfig));
  html_source->AddString("networkGoogleNameserversLearnMoreUrl",
                         chrome::kGoogleNameserversLearnMoreURL);

  std::string tether_learn_more_url(chrome::kInstantTetheringLearnMoreURL);
  tether_learn_more_url += "&b=" + base::SysInfo::GetLsbReleaseBoard();
  html_source->AddString(
      "internetNoNetworksMobileData",
      l10n_util::GetStringFUTF16(IDS_SETTINGS_INTERNET_NO_NETWORKS_MOBILE_DATA,
                                 base::ASCIIToUTF16(tether_learn_more_url)));
}
#endif

void AddLanguagesStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
    {"languagesPageTitle", IDS_SETTINGS_LANGUAGES_PAGE_TITLE},
    {"languagesListTitle", IDS_SETTINGS_LANGUAGES_LANGUAGES_LIST_TITLE},
    {"languagesExpandA11yLabel",
     IDS_SETTINGS_LANGUAGES_EXPAND_ACCESSIBILITY_LABEL},
    {"orderLanguagesInstructions",
     IDS_SETTINGS_LANGUAGES_LANGUAGES_LIST_ORDERING_INSTRUCTIONS},
    {"moveToTop", IDS_SETTINGS_LANGUAGES_LANGUAGES_LIST_MOVE_TO_TOP},
    {"moveUp", IDS_SETTINGS_LANGUAGES_LANGUAGES_LIST_MOVE_UP},
    {"moveDown", IDS_SETTINGS_LANGUAGES_LANGUAGES_LIST_MOVE_DOWN},
    {"removeLanguage", IDS_SETTINGS_LANGUAGES_LANGUAGES_LIST_REMOVE},
    {"addLanguages", IDS_SETTINGS_LANGUAGES_LANGUAGES_ADD},
#if defined(OS_CHROMEOS)
    {"inputMethodsListTitle", IDS_SETTINGS_LANGUAGES_INPUT_METHODS_LIST_TITLE},
    {"inputMethodEnabled", IDS_SETTINGS_LANGUAGES_INPUT_METHOD_ENABLED},
    {"inputMethodsExpandA11yLabel",
     IDS_SETTINGS_LANGUAGES_INPUT_METHODS_EXPAND_ACCESSIBILITY_LABEL},
    {"manageInputMethods", IDS_SETTINGS_LANGUAGES_INPUT_METHODS_MANAGE},
    {"manageInputMethodsPageTitle",
     IDS_SETTINGS_LANGUAGES_MANAGE_INPUT_METHODS_TITLE},
    {"showImeMenu", IDS_SETTINGS_LANGUAGES_SHOW_IME_MENU},
#endif
    {"addLanguagesDialogTitle", IDS_SETTINGS_LANGUAGES_MANAGE_LANGUAGES_TITLE},
    {"allLanguages", IDS_SETTINGS_LANGUAGES_ALL_LANGUAGES},
    {"enabledLanguages", IDS_SETTINGS_LANGUAGES_ENABLED_LANGUAGES},
    {"isDisplayedInThisLanguage",
     IDS_SETTINGS_LANGUAGES_IS_DISPLAYED_IN_THIS_LANGUAGE},
    {"displayInThisLanguage", IDS_SETTINGS_LANGUAGES_DISPLAY_IN_THIS_LANGUAGE},
    {"offerToTranslateInThisLanguage",
     IDS_SETTINGS_LANGUAGES_OFFER_TO_TRANSLATE_IN_THIS_LANGUAGE},
    {"offerToEnableTranslate",
     IDS_SETTINGS_LANGUAGES_OFFER_TO_ENABLE_TRANSLATE},
#if !defined(OS_MACOSX)
    {"spellCheckListTitle", IDS_SETTINGS_LANGUAGES_SPELL_CHECK_LIST_TITLE},
    {"spellCheckExpandA11yLabel",
     IDS_SETTINGS_LANGUAGES_SPELL_CHECK_EXPAND_ACCESSIBILITY_LABEL},
    {"spellCheckSummaryTwoLanguages",
     IDS_SETTINGS_LANGUAGES_SPELL_CHECK_SUMMARY_TWO_LANGUAGES},
    // TODO(michaelpg): Use ICU plural format when available to properly
    // translate "and [n] other(s)".
    {"spellCheckSummaryThreeLanguages",
     IDS_SETTINGS_LANGUAGES_SPELL_CHECK_SUMMARY_THREE_LANGUAGES},
    {"spellCheckSummaryMultipleLanguages",
     IDS_SETTINGS_LANGUAGES_SPELL_CHECK_SUMMARY_MULTIPLE_LANGUAGES},
    {"manageSpellCheck", IDS_SETTINGS_LANGUAGES_SPELL_CHECK_MANAGE},
    {"editDictionaryPageTitle", IDS_SETTINGS_LANGUAGES_EDIT_DICTIONARY_TITLE},
    {"addDictionaryWordLabel", IDS_SETTINGS_LANGUAGES_ADD_DICTIONARY_WORD},
    {"addDictionaryWordButton",
     IDS_SETTINGS_LANGUAGES_ADD_DICTIONARY_WORD_BUTTON},
    {"customDictionaryWords", IDS_SETTINGS_LANGUAGES_DICTIONARY_WORDS},
    {"noCustomDictionaryWordsFound",
     IDS_SETTINGS_LANGUAGES_DICTIONARY_WORDS_NONE},
#endif
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));

#if defined(OS_CHROMEOS)
  // Only the Chrome OS help article explains how language order affects website
  // language.
  html_source->AddString(
      "languagesLearnMoreURL",
      base::ASCIIToUTF16(chrome::kLanguageSettingsLearnMoreUrl));
#endif
}

#if defined(OS_CHROMEOS)
void AddChromeOSUserStrings(content::WebUIDataSource* html_source,
                            Profile* profile) {
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();

  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  const user_manager::User* primary_user = user_manager->GetPrimaryUser();
  std::string primary_user_email = primary_user->GetAccountId().GetUserEmail();
  html_source->AddString("primaryUserEmail", primary_user_email);
  html_source->AddBoolean(
      "isSecondaryUser",
      user && user->GetAccountId() != primary_user->GetAccountId());
  html_source->AddString(
      "secondaryUserBannerText",
      l10n_util::GetStringFUTF16(IDS_SETTINGS_SECONDARY_USER_BANNER,
                                 base::ASCIIToUTF16(primary_user_email)));

  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  if (!connector->IsEnterpriseManaged() &&
      !user_manager->IsCurrentUserOwner()) {
    html_source->AddString("ownerEmail",
                           user_manager->GetOwnerAccountId().GetUserEmail());
  }
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
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));
}

void AddPasswordsAndFormsStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
      {"passwordsAndAutofillPageTitle",
       IDS_SETTINGS_PASSWORDS_AND_AUTOFILL_PAGE_TITLE},
      {"autofill", IDS_SETTINGS_AUTOFILL},
      {"googlePayments", IDS_SETTINGS_GOOGLE_PAYMENTS},
      {"googlePaymentsCached", IDS_SETTINGS_GOOGLE_PAYMENTS_CACHED},
      {"addresses", IDS_SETTINGS_AUTOFILL_ADDRESSES_HEADING},
      {"addAddressTitle", IDS_SETTINGS_AUTOFILL_ADDRESSES_ADD_TITLE},
      {"editAddressTitle", IDS_SETTINGS_AUTOFILL_ADDRESSES_EDIT_TITLE},
      {"addressCountry", IDS_SETTINGS_AUTOFILL_ADDRESSES_COUNTRY},
      {"addressPhone", IDS_SETTINGS_AUTOFILL_ADDRESSES_PHONE},
      {"addressEmail", IDS_SETTINGS_AUTOFILL_ADDRESSES_EMAIL},
      {"removeAddress", IDS_SETTINGS_ADDRESS_REMOVE},
      {"creditCards", IDS_SETTINGS_AUTOFILL_CREDIT_CARD_HEADING},
      {"removeCreditCard", IDS_SETTINGS_CREDIT_CARD_REMOVE},
      {"clearCreditCard", IDS_SETTINGS_CREDIT_CARD_CLEAR},
      {"creditCardType", IDS_SETTINGS_AUTOFILL_CREDIT_CARD_TYPE_COLUMN_LABEL},
      {"creditCardExpiration", IDS_SETTINGS_CREDIT_CARD_EXPIRATION_DATE},
      {"creditCardName", IDS_SETTINGS_NAME_ON_CREDIT_CARD},
      {"creditCardNumber", IDS_SETTINGS_CREDIT_CARD_NUMBER},
      {"creditCardExpirationMonth", IDS_SETTINGS_CREDIT_CARD_EXPIRATION_MONTH},
      {"creditCardExpirationYear", IDS_SETTINGS_CREDIT_CARD_EXPIRATION_YEAR},
      {"creditCardExpired", IDS_SETTINGS_CREDIT_CARD_EXPIRED},
      {"editCreditCardTitle", IDS_SETTINGS_EDIT_CREDIT_CARD_TITLE},
      {"addCreditCardTitle", IDS_SETTINGS_ADD_CREDIT_CARD_TITLE},
      {"autofillDetail", IDS_SETTINGS_AUTOFILL_DETAIL},
      {"passwords", IDS_SETTINGS_PASSWORDS},
      {"passwordsAutosigninLabel",
       IDS_SETTINGS_PASSWORDS_AUTOSIGNIN_CHECKBOX_LABEL},
      {"passwordsAutosigninDescription",
       IDS_SETTINGS_PASSWORDS_AUTOSIGNIN_CHECKBOX_DESC},
      {"passwordsDetail", IDS_SETTINGS_PASSWORDS_DETAIL},
      {"savedPasswordsHeading", IDS_SETTINGS_PASSWORDS_SAVED_HEADING},
      {"passwordExceptionsHeading", IDS_SETTINGS_PASSWORDS_EXCEPTIONS_HEADING},
      {"deletePasswordException", IDS_SETTINGS_PASSWORDS_DELETE_EXCEPTION},
      {"passwordsDone", IDS_SETTINGS_PASSWORD_DONE},
      {"removePassword", IDS_SETTINGS_PASSWORD_REMOVE},
      {"searchPasswords", IDS_SETTINGS_PASSWORD_SEARCH},
      {"showPassword", IDS_SETTINGS_PASSWORD_SHOW},
      {"hidePassword", IDS_SETTINGS_PASSWORD_HIDE},
      {"passwordDetailsTitle", IDS_SETTINGS_PASSWORDS_VIEW_DETAILS_TITLE},
      {"passwordViewDetails", IDS_SETTINGS_PASSWORD_DETAILS},
      {"editPasswordWebsiteLabel", IDS_SETTINGS_PASSWORDS_WEBSITE},
      {"editPasswordUsernameLabel", IDS_SETTINGS_PASSWORDS_USERNAME},
      {"editPasswordPasswordLabel", IDS_SETTINGS_PASSWORDS_PASSWORD},
      {"noAddressesFound", IDS_SETTINGS_ADDRESS_NONE},
      {"noCreditCardsFound", IDS_SETTINGS_CREDIT_CARD_NONE},
      {"noCreditCardsPolicy", IDS_SETTINGS_CREDIT_CARD_DISABLED},
      {"noPasswordsFound", IDS_SETTINGS_PASSWORDS_NONE},
      {"noExceptionsFound", IDS_SETTINGS_PASSWORDS_EXCEPTIONS_NONE},
      {"import", IDS_PASSWORD_MANAGER_IMPORT_BUTTON},
      {"export", IDS_PASSWORD_MANAGER_EXPORT_BUTTON},
      {"undoRemovePassword", IDS_SETTINGS_PASSWORD_UNDO},
      {"passwordDeleted", IDS_SETTINGS_PASSWORD_DELETED_PASSWORD},
      {"exportPasswordsTitle", IDS_SETTINGS_PASSWORDS_EXPORT_TITLE},
      {"exportPasswordsDescription", IDS_SETTINGS_PASSWORDS_EXPORT_DESCRIPTION},
      {"exportPasswords", IDS_SETTINGS_PASSWORDS_EXPORT}};

  html_source->AddString(
      "managePasswordsLabel",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_PASSWORDS_MANAGE_PASSWORDS,
          base::ASCIIToUTF16(
              password_manager::kPasswordManagerAccountDashboardURL)));
  html_source->AddString("passwordManagerLearnMoreURL",
                         chrome::kPasswordManagerLearnMoreURL);
  html_source->AddString("manageAddressesUrl",
                         autofill::payments::GetManageAddressesUrl(0).spec());
  html_source->AddString("manageCreditCardsUrl",
                         autofill::payments::GetManageInstrumentsUrl(0).spec());

  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));
}

void AddPeopleStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
    {"peoplePageTitle", IDS_SETTINGS_PEOPLE},
    {"manageOtherPeople", IDS_SETTINGS_PEOPLE_MANAGE_OTHER_PEOPLE},
    {"manageSupervisedUsers", IDS_SETTINGS_PEOPLE_MANAGE_SUPERVISED_USERS},
#if defined(OS_CHROMEOS)
    {"configureFingerprintTitle", IDS_SETTINGS_ADD_FINGERPRINT_DIALOG_TITLE},
    {"configureFingerprintInstructionLocateScannerStep",
     IDS_SETTINGS_ADD_FINGERPRINT_DIALOG_INSTRUCTION_LOCATE_SCANNER},
    {"configureFingerprintInstructionMoveFingerStep",
     IDS_SETTINGS_ADD_FINGERPRINT_DIALOG_INSTRUCTION_MOVE_FINGER},
    {"configureFingerprintInstructionReadyStep",
     IDS_SETTINGS_ADD_FINGERPRINT_DIALOG_INSTRUCTION_READY},
    {"configureFingerprintLiftFinger",
     IDS_SETTINGS_ADD_FINGERPRINT_DIALOG_LIFT_FINGER},
    {"configureFingerprintPartialData",
     IDS_SETTINGS_ADD_FINGERPRINT_DIALOG_PARTIAL_DATA},
    {"configureFingerprintInsufficientData",
     IDS_SETTINGS_ADD_FINGERPRINT_DIALOG_INSUFFICIENT_DATA},
    {"configureFingerprintSensorDirty",
     IDS_SETTINGS_ADD_FINGERPRINT_DIALOG_SENSOR_DIRTY},
    {"configureFingerprintTooSlow",
     IDS_SETTINGS_ADD_FINGERPRINT_DIALOG_FINGER_TOO_SLOW},
    {"configureFingerprintTooFast",
     IDS_SETTINGS_ADD_FINGERPRINT_DIALOG_FINGER_TOO_FAST},
    {"configureFingerprintImmobile",
     IDS_SETTINGS_ADD_FINGERPRINT_DIALOG_FINGER_IMMOBILE},
    {"configureFingerprintCancelButton",
     IDS_SETTINGS_ADD_FINGERPRINT_DIALOG_CANCEL_BUTTON},
    {"configureFingerprintDoneButton",
     IDS_SETTINGS_ADD_FINGERPRINT_DIALOG_DONE_BUTTON},
    {"configureFingerprintAddAnotherButton",
     IDS_SETTINGS_ADD_FINGERPRINT_DIALOG_ADD_ANOTHER_BUTTON},
    {"configurePinChoosePinTitle",
     IDS_SETTINGS_PEOPLE_CONFIGURE_PIN_CHOOSE_PIN_TITLE},
    {"configurePinConfirmPinTitle",
     IDS_SETTINGS_PEOPLE_CONFIGURE_PIN_CONFIRM_PIN_TITLE},
    {"configurePinContinueButton",
     IDS_SETTINGS_PEOPLE_CONFIGURE_PIN_CONTINUE_BUTTON},
    {"configurePinMismatched", IDS_SETTINGS_PEOPLE_CONFIGURE_PIN_MISMATCHED},
    {"configurePinTooShort", IDS_SETTINGS_PEOPLE_CONFIGURE_PIN_TOO_SHORT},
    {"configurePinTooLong", IDS_SETTINGS_PEOPLE_CONFIGURE_PIN_TOO_LONG},
    {"configurePinWeakPin", IDS_SETTINGS_PEOPLE_CONFIGURE_PIN_WEAK_PIN},
    {"enableScreenlock", IDS_SETTINGS_PEOPLE_ENABLE_SCREENLOCK},
    {"lockScreenAddFingerprint",
     IDS_SETTINGS_PEOPLE_LOCK_SCREEN_ADD_FINGERPRINT_BUTTON},
    {"lockScreenChangePinButton",
     IDS_SETTINGS_PEOPLE_LOCK_SCREEN_CHANGE_PIN_BUTTON},
    {"lockScreenEditFingerprints",
     IDS_SETTINGS_PEOPLE_LOCK_SCREEN_EDIT_FINGERPRINTS},
    {"lockScreenEditFingerprintsDescription",
     IDS_SETTINGS_PEOPLE_LOCK_SCREEN_EDIT_FINGERPRINTS_DESCRIPTION},
    {"lockScreenSetupFingerprintButton",
     IDS_SETTINGS_PEOPLE_LOCK_SCREEN_FINGERPRINT_SETUP_BUTTON},
    {"lockScreenNumberFingerprints",
     IDS_SETTINGS_PEOPLE_LOCK_SCREEN_NUM_FINGERPRINTS},
    {"lockScreenNone", IDS_SETTINGS_PEOPLE_LOCK_SCREEN_NONE},
    {"lockScreenFingerprintEnable",
     IDS_SETTINGS_PEOPLE_LOCK_SCREEN_ENABLE_FINGERPRINT_CHECKBOX_LABEL},
    {"lockScreenFingerprintNewName",
     IDS_SETTINGS_PEOPLE_LOCK_SCREEN_NEW_FINGERPRINT_DEFAULT_NAME},
    {"lockScreenFingerprintTitle",
     IDS_SETTINGS_PEOPLE_LOCK_SCREEN_FINGERPRINT_SUBPAGE_TITLE},
    {"lockScreenFingerprintWarning",
     IDS_SETTINGS_PEOPLE_LOCK_SCREEN_FINGERPRINT_LESS_SECURE},
    {"lockScreenOptions", IDS_SETTINGS_PEOPLE_LOCK_SCREEN_OPTIONS},
    {"lockScreenPasswordOnly", IDS_SETTINGS_PEOPLE_LOCK_SCREEN_PASSWORD_ONLY},
    {"lockScreenPinOrPassword",
     IDS_SETTINGS_PEOPLE_LOCK_SCREEN_PIN_OR_PASSWORD},
    {"lockScreenRegisteredFingerprints",
     IDS_SETTINGS_PEOPLE_LOCK_SCREEN_REGISTERED_FINGERPRINTS_LABEL},
    {"lockScreenSetupPinButton",
     IDS_SETTINGS_PEOPLE_LOCK_SCREEN_SETUP_PIN_BUTTON},
    {"lockScreenTitle", IDS_SETTINGS_PEOPLE_LOCK_SCREEN_TITLE},
    {"passwordPromptEnterPassword",
     IDS_SETTINGS_PEOPLE_PASSWORD_PROMPT_ENTER_PASSWORD},
    {"passwordPromptInvalidPassword",
     IDS_SETTINGS_PEOPLE_PASSWORD_PROMPT_INVALID_PASSWORD},
    {"passwordPromptPasswordLabel",
     IDS_SETTINGS_PEOPLE_PASSWORD_PROMPT_PASSWORD_LABEL},
    {"passwordPromptTitle", IDS_SETTINGS_PEOPLE_PASSWORD_PROMPT_TITLE},
    {"pinKeyboardPlaceholderPin", IDS_PIN_KEYBOARD_HINT_TEXT_PIN},
    {"pinKeyboardPlaceholderPinPassword",
     IDS_PIN_KEYBOARD_HINT_TEXT_PIN_PASSWORD},
    {"pinKeyboardDeleteAccessibleName",
     IDS_PIN_KEYBOARD_DELETE_ACCESSIBLE_NAME},
    {"changePictureTitle", IDS_SETTINGS_CHANGE_PICTURE_DIALOG_TITLE},
    {"changePicturePageDescription", IDS_SETTINGS_CHANGE_PICTURE_DIALOG_TEXT},
    {"takePhoto", IDS_SETTINGS_CHANGE_PICTURE_TAKE_PHOTO},
    {"captureVideo", IDS_SETTINGS_CHANGE_PICTURE_CAPTURE_VIDEO},
    {"discardPhoto", IDS_SETTINGS_CHANGE_PICTURE_DISCARD_PHOTO},
    {"switchModeToCamera", IDS_SETTINGS_CHANGE_PICTURE_SWITCH_MODE_TO_CAMERA},
    {"switchModeToVideo", IDS_SETTINGS_CHANGE_PICTURE_SWITCH_MODE_TO_VIDEO},
    {"chooseFile", IDS_SETTINGS_CHANGE_PICTURE_CHOOSE_FILE},
    {"profilePhoto", IDS_SETTINGS_CHANGE_PICTURE_PROFILE_PHOTO},
    {"oldPhoto", IDS_SETTINGS_CHANGE_PICTURE_OLD_PHOTO},
    {"previewAltText", IDS_SETTINGS_CHANGE_PICTURE_PREVIEW_ALT},
    {"authorCreditText", IDS_SETTINGS_CHANGE_PICTURE_AUTHOR_CREDIT_TEXT},
    {"photoCaptureAccessibleText", IDS_SETTINGS_PHOTO_CAPTURE_ACCESSIBLE_TEXT},
    {"photoDiscardAccessibleText", IDS_SETTINGS_PHOTO_DISCARD_ACCESSIBLE_TEXT},
    {"photoModeAccessibleText", IDS_SETTINGS_PHOTO_MODE_ACCESSIBLE_TEXT},
    {"videoModeAccessibleText", IDS_SETTINGS_VIDEO_MODE_ACCESSIBLE_TEXT},
#else   // !defined(OS_CHROMEOS)
    {"domainManagedProfile", IDS_SETTINGS_PEOPLE_DOMAIN_MANAGED_PROFILE},
    {"editPerson", IDS_SETTINGS_EDIT_PERSON},
    {"showShortcutLabel", IDS_SETTINGS_PROFILE_SHORTCUT_TOGGLE_LABEL},
#endif  // defined(OS_CHROMEOS)
    {"syncOverview", IDS_SETTINGS_SYNC_OVERVIEW},
    {"syncDisabledByAdministrator",
     IDS_SETTINGS_SYNC_DISABLED_BY_ADMINISTRATOR},
    {"syncSignin", IDS_SETTINGS_SYNC_SIGNIN},
    {"syncDisconnect", IDS_SETTINGS_SYNC_DISCONNECT},
    {"syncDisconnectTitle", IDS_SETTINGS_SYNC_DISCONNECT_TITLE},
    {"syncDisconnectDeleteProfile",
     IDS_SETTINGS_SYNC_DISCONNECT_DELETE_PROFILE},
    {"deleteProfileWarningExpandA11yLabel",
     IDS_SETTINGS_SYNC_DISCONNECT_EXPAND_ACCESSIBILITY_LABEL},
    {"deleteProfileWarningWithCountsSingular",
     IDS_SETTINGS_SYNC_DISCONNECT_DELETE_PROFILE_WARNING_WITH_COUNTS_SINGULAR},
    {"deleteProfileWarningWithCountsPlural",
     IDS_SETTINGS_SYNC_DISCONNECT_DELETE_PROFILE_WARNING_WITH_COUNTS_PLURAL},
    {"deleteProfileWarningWithoutCounts",
     IDS_SETTINGS_SYNC_DISCONNECT_DELETE_PROFILE_WARNING_WITHOUT_COUNTS},
    {"syncDisconnectExplanation", IDS_SETTINGS_SYNC_DISCONNECT_EXPLANATION},
    {"syncDisconnectConfirm", IDS_SETTINGS_SYNC_DISCONNECT_CONFIRM},
    {"sync", IDS_SETTINGS_SYNC},
    {"syncPageTitle", IDS_SETTINGS_SYNC_PAGE_TITLE},
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
    {"enablePaymentsIntegrationCheckboxLabel",
     IDS_SETTINGS_ENABLE_PAYMENTS_INTEGRATION_CHECKBOX_LABEL},
    {"manageSyncedDataTitle", IDS_SETTINGS_MANAGE_SYNCED_DATA_TITLE},
    {"encryptionOptionsTitle", IDS_SETTINGS_ENCRYPTION_OPTIONS},
    {"syncDataEncryptedText", IDS_SETTINGS_SYNC_DATA_ENCRYPTED_TEXT},
    {"encryptWithGoogleCredentialsLabel",
     IDS_SETTINGS_ENCRYPT_WITH_GOOGLE_CREDENTIALS_LABEL},
    {"useDefaultSettingsButton", IDS_SETTINGS_USE_DEFAULT_SETTINGS},
    {"emptyPassphraseError", IDS_SETTINGS_EMPTY_PASSPHRASE_ERROR},
    {"mismatchedPassphraseError", IDS_SETTINGS_MISMATCHED_PASSPHRASE_ERROR},
    {"incorrectPassphraseError", IDS_SETTINGS_INCORRECT_PASSPHRASE_ERROR},
    {"passphrasePlaceholder", IDS_SETTINGS_PASSPHRASE_PLACEHOLDER},
    {"passphraseConfirmationPlaceholder",
     IDS_SETTINGS_PASSPHRASE_CONFIRMATION_PLACEHOLDER},
    {"submitPassphraseButton", IDS_SETTINGS_SUBMIT_PASSPHRASE},
    {"personalizeGoogleServicesTitle",
     IDS_SETTINGS_PERSONALIZE_GOOGLE_SERVICES_TITLE},
    {"personalizeGoogleServicesText",
     IDS_SETTINGS_PERSONALIZE_GOOGLE_SERVICES_TEXT},
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));

  // Format numbers to be used on the pin keyboard.
  for (int j = 0; j <= 9; j++) {
    html_source->AddString("pinKeyboard" + base::IntToString(j),
                           base::FormatNumber(int64_t{j}));
  }

  html_source->AddString("syncLearnMoreUrl", chrome::kSyncLearnMoreURL);
  html_source->AddString("autofillHelpURL", autofill::kHelpURL);
  html_source->AddString("supervisedUsersUrl",
                         chrome::kLegacySupervisedUserManagementURL);

  html_source->AddString(
      "encryptWithSyncPassphraseLabel",
      l10n_util::GetStringFUTF8(
          IDS_SETTINGS_ENCRYPT_WITH_SYNC_PASSPHRASE_LABEL,
          base::ASCIIToUTF16(chrome::kSyncEncryptionHelpURL)));

  std::string sync_dashboard_url =
      google_util::AppendGoogleLocaleParam(
          GURL(chrome::kSyncGoogleDashboardURL),
          g_browser_process->GetApplicationLocale())
          .spec();
  html_source->AddString("syncDashboardUrl", sync_dashboard_url);

  html_source->AddString(
      "passphraseExplanationText",
      l10n_util::GetStringFUTF8(IDS_SETTINGS_PASSPHRASE_EXPLANATION_TEXT,
                                base::ASCIIToUTF16(sync_dashboard_url)));
  html_source->AddString(
      "passphraseResetHint",
      l10n_util::GetStringFUTF8(IDS_SETTINGS_PASSPHRASE_RESET_HINT,
                                base::ASCIIToUTF16(sync_dashboard_url)));
  html_source->AddString(
      "passphraseRecover",
      l10n_util::GetStringFUTF8(IDS_SETTINGS_PASSPHRASE_RECOVER,
                                base::ASCIIToUTF16(sync_dashboard_url)));
  html_source->AddString(
      "syncDisconnectExplanation",
      l10n_util::GetStringFUTF8(IDS_SETTINGS_SYNC_DISCONNECT_EXPLANATION,
                                base::ASCIIToUTF16(sync_dashboard_url)));
#if !defined(OS_CHROMEOS)
  html_source->AddString(
      "syncDisconnectManagedProfileExplanation",
      l10n_util::GetStringFUTF8(
          IDS_SETTINGS_SYNC_DISCONNECT_MANAGED_PROFILE_EXPLANATION,
          base::ASCIIToUTF16("$1"),
          base::ASCIIToUTF16(sync_dashboard_url)));
#endif

  html_source->AddString("syncErrorHelpUrl", chrome::kSyncErrorsHelpURL);

  html_source->AddString("activityControlsUrl",
                         chrome::kGoogleAccountActivityControlsURL);

  html_source->AddBoolean("profileShortcutsEnabled",
                          ProfileShortcutManager::IsFeatureEnabled());
}

void AddPrintingStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
    {"printingPageTitle", IDS_SETTINGS_PRINTING},
    {"printingCloudPrintLearnMoreLabel",
     IDS_SETTINGS_PRINTING_CLOUD_PRINT_LEARN_MORE_LABEL},
    {"printingNotificationsLabel", IDS_SETTINGS_PRINTING_NOTIFICATIONS_LABEL},
    {"printingManageCloudPrintDevices",
     IDS_SETTINGS_PRINTING_MANAGE_CLOUD_PRINT_DEVICES},
    {"cloudPrintersTitle", IDS_SETTINGS_PRINTING_CLOUD_PRINTERS},
#if defined(OS_CHROMEOS)
    {"cupsPrintersTitle", IDS_SETTINGS_PRINTING_CUPS_PRINTERS},
    {"cupsPrintersLearnMoreLabel",
     IDS_SETTINGS_PRINTING_CUPS_PRINTERS_LEARN_MORE_LABEL},
    {"addCupsPrinter", IDS_SETTINGS_PRINTING_CUPS_PRINTERS_ADD_PRINTER},
    {"editPrinter", IDS_SETTINGS_PRINTING_CUPS_PRINTERS_EDIT},
    {"removePrinter", IDS_SETTINGS_PRINTING_CUPS_PRINTERS_REMOVE},
    {"searchLabel", IDS_SETTINGS_PRINTING_CUPS_SEARCH_LABEL},
    {"printerDetailsTitle", IDS_SETTINGS_PRINTING_CUPS_PRINTER_DETAILS_TITLE},
    {"printerName", IDS_SETTINGS_PRINTING_CUPS_PRINTER_DETAILS_NAME},
    {"printerModel", IDS_SETTINGS_PRINTING_CUPS_PRINTER_DETAILS_MODEL},
    {"printerQueue", IDS_SETTINGS_PRINTING_CUPS_PRINTER_DETAILS_QUEUE},
    {"addPrintersNearbyTitle",
     IDS_SETTINGS_PRINTING_CUPS_ADD_PRINTERS_NEARBY_TITLE},
    {"addPrintersManuallyTitle",
     IDS_SETTINGS_PRINTING_CUPS_ADD_PRINTERS_MANUALLY_TITLE},
    {"selectManufacturerAndModelTitle",
     IDS_SETTINGS_PRINTING_CUPS_SELECT_MANUFACTURER_AND_MODEL_TITLE},
    {"cancelButtonText", IDS_SETTINGS_PRINTING_CUPS_ADD_PRINTER_BUTTON_CANCEL},
    {"addPrinterButtonText", IDS_SETTINGS_PRINTING_CUPS_ADD_PRINTER_BUTTON_ADD},
    {"printerDetailsAdvanced", IDS_SETTINGS_PRINTING_CUPS_PRINTER_ADVANCED},
    {"printerDetailsA11yLabel",
     IDS_SETTINGS_PRINTING_CUPS_PRINTER_ADVANCED_ACCESSIBILITY_LABEL},
    {"printerAddress", IDS_SETTINGS_PRINTING_CUPS_PRINTER_ADVANCED_ADDRESS},
    {"printerProtocol", IDS_SETTINGS_PRINTING_CUPS_PRINTER_ADVANCED_PROTOCOL},
    {"printerURI", IDS_SETTINGS_PRINTING_CUPS_PRINTER_ADVANCED_URI},
    {"manuallyAddPrinterButtonText",
     IDS_SETTINGS_PRINTING_CUPS_ADD_PRINTER_BUTTON_MANUAL_ADD},
    {"discoverPrintersButtonText",
     IDS_SETTINGS_PRINTING_CUPS_ADD_PRINTER_BUTTON_DISCOVER_PRINTERS},
    {"printerProtocolIpp", IDS_SETTINGS_PRINTING_CUPS_PRINTER_PROTOCOL_IPP},
    {"printerProtocolIpps", IDS_SETTINGS_PRINTING_CUPS_PRINTER_PROTOCOL_IPPS},
    {"printerProtocolHttp", IDS_SETTINGS_PRINTING_CUPS_PRINTER_PROTOCOL_HTTP},
    {"printerProtocolHttps", IDS_SETTINGS_PRINTING_CUPS_PRINTER_PROTOCOL_HTTPS},
    {"printerProtocolAppSocket",
     IDS_SETTINGS_PRINTING_CUPS_PRINTER_PROTOCOL_APP_SOCKET},
    {"printerProtocolLpd", IDS_SETTINGS_PRINTING_CUPS_PRINTER_PROTOCOL_LPD},
    {"printerProtocolUsb", IDS_SETTINGS_PRINTING_CUPS_PRINTER_PROTOCOL_USB},
    {"printerConfiguringMessage",
     IDS_SETTINGS_PRINTING_CUPS_PRINTER_CONFIGURING_MESSAGE},
    {"searchingPrinter", IDS_SETTINGS_PRINTING_CUPS_PRINTER_SEARCHING_PRINTER},
    {"printerNotFound", IDS_SETTINGS_PRINTING_CUPS_PRINTER_NOT_FOUND_PRINTER},
    {"printerFound", IDS_SETTINGS_PRINTING_CUPS_PRINTER_FOUND_PRINTER},
    {"printerManufacturer", IDS_SETTINGS_PRINTING_CUPS_PRINTER_MANUFACTURER},
    {"selectDriver", IDS_SETTINGS_PRINTING_CUPS_PRINTER_SELECT_DRIVER},
    {"selectDriverButtonText",
     IDS_SETTINGS_PRINTING_CUPS_PRINTER_BUTTON_SELECT_DRIVER},
    {"printerAddedSuccessfulMessage",
     IDS_SETTINGS_PRINTING_CUPS_PRINTER_ADDED_PRINTER_DONE_MESSAGE},
    {"noPrinterNearbyMessage",
     IDS_SETTINGS_PRINTING_CUPS_PRINTER_NO_PRINTER_NEARBY},
    {"searchingNearbyPrinters",
     IDS_SETTINGS_PRINTING_CUPS_PRINTER_SEARCHING_NEARBY_PRINTER},
    {"printerAddedFailedMessage",
     IDS_SETTINGS_PRINTING_CUPS_PRINTER_ADDED_PRINTER_ERROR_MESSAGE},
    {"printerAddedFatalErrorMessage",
     IDS_SETTINGS_PRINTING_CUPS_PRINTER_ADDED_PRINTER_FATAL_ERROR_MESSAGE},
    {"printerAddedUnreachableMessage",
     IDS_SETTINGS_PRINTING_CUPS_PRINTER_ADDED_PRINTER_PRINTER_UNREACHABLE_MESSAGE},
    {"printerAddedPpdTooLargeMessage",
     IDS_SETTINGS_PRINTING_CUPS_PRINTER_ADDED_PRINTER_PPD_TOO_LARGE_MESSAGE},
    {"printerAddedInvalidPpdMessage",
     IDS_SETTINGS_PRINTING_CUPS_PRINTER_ADDED_PRINTER_INVALID_PPD_MESSAGE},
    {"printerAddedPpdNotFoundMessage",
     IDS_SETTINGS_PRINTING_CUPS_PRINTER_ADDED_PRINTER_PPD_NOT_FOUND},
    {"printerAddedPpdUnretrievableMessage",
     IDS_SETTINGS_PRINTING_CUPS_PRINTER_ADDED_PRINTER_PPD_UNRETRIEVABLE},
    {"requireNetworkMessage",
     IDS_SETTINGS_PRINTING_CUPS_PRINTER_REQUIRE_INTERNET_MESSAGE},
    {"editPrinterDialogTitle",
     IDS_SETTINGS_PRINTING_CUPS_EDIT_PRINTER_DIALOG_TITLE},
    {"editPrinterButtonText", IDS_SETTINGS_PRINTING_CUPS_EDIT_PRINTER_BUTTON},
#else
    {"localPrintersTitle", IDS_SETTINGS_PRINTING_LOCAL_PRINTERS_TITLE},
#endif
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));

  html_source->AddString("devicesUrl", chrome::kChromeUIDevicesURL);
  html_source->AddString("printingCloudPrintLearnMoreUrl",
                         chrome::kCloudPrintLearnMoreURL);

#if defined(OS_CHROMEOS)
  html_source->AddString("printingCUPSPrintLearnMoreUrl",
                         chrome::kCupsPrintLearnMoreURL);
#endif
}

void AddPrivacyStrings(content::WebUIDataSource* html_source,
                       Profile* profile) {
  LocalizedString localized_strings[] = {
    {"privacyPageTitle", IDS_SETTINGS_PRIVACY},
    {"linkDoctorPref", IDS_SETTINGS_LINKDOCTOR_PREF},
    {"searchSuggestPref", IDS_SETTINGS_SUGGEST_PREF},
    {"networkPredictionEnabled",
     IDS_SETTINGS_NETWORK_PREDICTION_ENABLED_DESCRIPTION},
    {"safeBrowsingEnableProtection",
     IDS_SETTINGS_SAFEBROWSING_ENABLEPROTECTION},
    {"spellingPref", IDS_SETTINGS_SPELLING_PREF},
    {"spellingDescription", IDS_SETTINGS_SPELLING_DESCRIPTION},
#if defined(OS_CHROMEOS)
    {"enableLogging", IDS_SETTINGS_ENABLE_LOGGING_DIAGNOSTIC_AND_USAGE_DATA},
#else
    {"enableLogging", IDS_SETTINGS_ENABLE_LOGGING},
#endif
    {"doNotTrack", IDS_SETTINGS_ENABLE_DO_NOT_TRACK},
    {"doNotTrackDialogTitle", IDS_SETTINGS_ENABLE_DO_NOT_TRACK_DIALOG_TITLE},
    {"enableContentProtectionAttestation",
     IDS_SETTINGS_ENABLE_CONTENT_PROTECTION_ATTESTATION},
    {"wakeOnWifi", IDS_SETTINGS_WAKE_ON_WIFI_DESCRIPTION},
    {"manageCertificates", IDS_SETTINGS_MANAGE_CERTIFICATES},
    {"manageCertificatesDescription",
     IDS_SETTINGS_MANAGE_CERTIFICATES_DESCRIPTION},
    {"contentSettings", IDS_SETTINGS_CONTENT_SETTINGS},
    {"siteSettings", IDS_SETTINGS_SITE_SETTINGS},
    {"siteSettingsDescription", IDS_SETTINGS_SITE_SETTINGS_DESCRIPTION},
    {"clearData", IDS_SETTINGS_CLEAR_DATA},
    {"clearBrowsingData", IDS_SETTINGS_CLEAR_BROWSING_DATA},
    {"clearBrowsingDataDescription", IDS_SETTINGS_CLEAR_DATA_DESCRIPTION},
    {"titleAndCount", IDS_SETTINGS_TITLE_AND_COUNT},
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));

  html_source->AddBoolean("tabsInCbd",
                          base::FeatureList::IsEnabled(features::kTabsInCbd));

  html_source->AddBoolean(
      "importantSitesInCbd",
      base::FeatureList::IsEnabled(features::kImportantSitesInCbd));

  html_source->AddLocalizedString(
      "safeBrowsingEnableExtendedReporting",
      safe_browsing::ChooseOptInTextResource(
          *profile->GetPrefs(),
          IDS_SETTINGS_SAFEBROWSING_ENABLE_EXTENDED_REPORTING,
          IDS_SETTINGS_SAFEBROWSING_ENABLE_SCOUT_REPORTING));
  html_source->AddString("improveBrowsingExperience",
                         l10n_util::GetStringFUTF16(
                             IDS_SETTINGS_IMPROVE_BROWSING_EXPERIENCE,
                             base::ASCIIToUTF16(chrome::kPrivacyLearnMoreURL)));
  html_source->AddString(
      "doNotTrackDialogMessage",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_ENABLE_DO_NOT_TRACK_DIALOG_TEXT,
          base::ASCIIToUTF16(chrome::kDoNotTrackLearnMoreURL)));
  html_source->AddString(
      "exceptionsLearnMoreURL",
      base::ASCIIToUTF16(chrome::kContentSettingsExceptionsLearnMoreURL));
}

void AddSearchInSettingsStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
      {"searchPrompt", IDS_SETTINGS_SEARCH_PROMPT},
      {"searchNoResults", IDS_SEARCH_NO_RESULTS},
      {"searchResults", IDS_SEARCH_RESULTS},
      // TODO(dpapad): IDS_DOWNLOAD_CLEAR_SEARCH and IDS_MD_HISTORY_CLEAR_SEARCH
      // are identical, merge them to one and re-use here.
      {"clearSearch", IDS_DOWNLOAD_CLEAR_SEARCH},
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));

  base::string16 help_text = l10n_util::GetStringFUTF16(
      IDS_SETTINGS_SEARCH_NO_RESULTS_HELP,
      base::ASCIIToUTF16(chrome::kSettingsSearchHelpURL));
  html_source->AddString("searchNoResultsHelp", help_text);
}

void AddSearchStrings(content::WebUIDataSource* html_source, Profile* profile) {
#if defined(OS_CHROMEOS)
  const bool is_assistant_allowed =
      arc::IsAssistantAllowedForProfile(profile) ==
      ash::mojom::AssistantAllowedState::ALLOWED;
#endif

  LocalizedString localized_strings[] = {
#if defined(OS_CHROMEOS)
    {"searchPageTitle", is_assistant_allowed ? IDS_SETTINGS_SEARCH_AND_ASSISTANT
                                             : IDS_SETTINGS_SEARCH},
#else
    {"searchPageTitle", IDS_SETTINGS_SEARCH},
#endif
    {"searchEnginesManage", IDS_SETTINGS_SEARCH_MANAGE_SEARCH_ENGINES},
#if defined(OS_CHROMEOS)
    {"searchGoogleAssistant", IDS_SETTINGS_SEARCH_GOOGLE_ASSISTANT},
    {"searchGoogleAssistantEnabled",
     IDS_SETTINGS_SEARCH_GOOGLE_ASSISTANT_ENABLED},
    {"searchGoogleAssistantDisabled",
     IDS_SETTINGS_SEARCH_GOOGLE_ASSISTANT_DISABLED},
    {"assistantTurnOn", IDS_SETTINGS_SEARCH_GOOGLE_ASSISTANT_TURN_ON},
#endif
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));
  base::string16 search_explanation_text = l10n_util::GetStringFUTF16(
      IDS_SETTINGS_SEARCH_EXPLANATION,
      base::ASCIIToUTF16(chrome::kOmniboxLearnMoreURL));
  html_source->AddString("searchExplanation", search_explanation_text);
#if defined(OS_CHROMEOS)
  html_source->AddBoolean("enableVoiceInteraction", is_assistant_allowed);
#endif
}

void AddSearchEnginesStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
      {"searchEnginesPageTitle", IDS_SETTINGS_SEARCH_ENGINES},
      {"searchEnginesAddSearchEngine",
       IDS_SETTINGS_SEARCH_ENGINES_ADD_SEARCH_ENGINE},
      {"searchEnginesEditSearchEngine",
       IDS_SETTINGS_SEARCH_ENGINES_EDIT_SEARCH_ENGINE},
      {"searchEngines", IDS_SETTINGS_SEARCH_ENGINES},
      {"searchEnginesDefault", IDS_SETTINGS_SEARCH_ENGINES_DEFAULT_ENGINES},
      {"searchEnginesOther", IDS_SETTINGS_SEARCH_ENGINES_OTHER_ENGINES},
      {"searchEnginesNoOtherEngines",
       IDS_SETTINGS_SEARCH_ENGINES_NO_OTHER_ENGINES},
      {"searchEnginesExtension", IDS_SETTINGS_SEARCH_ENGINES_EXTENSION_ENGINES},
      {"searchEnginesSearch", IDS_SETTINGS_SEARCH_ENGINES_SEARCH},
      {"searchEnginesSearchEngine", IDS_SETTINGS_SEARCH_ENGINES_SEARCH_ENGINE},
      {"searchEnginesKeyword", IDS_SETTINGS_SEARCH_ENGINES_KEYWORD},
      {"searchEnginesQueryURL", IDS_SETTINGS_SEARCH_ENGINES_QUERY_URL},
      {"searchEnginesQueryURLExplanation",
       IDS_SETTINGS_SEARCH_ENGINES_QUERY_URL_EXPLANATION},
      {"searchEnginesMakeDefault", IDS_SETTINGS_SEARCH_ENGINES_MAKE_DEFAULT},
      {"searchEnginesRemoveFromList",
       IDS_SETTINGS_SEARCH_ENGINES_REMOVE_FROM_LIST},
      {"searchEnginesManageExtension",
       IDS_SETTINGS_SEARCH_ENGINES_MANAGE_EXTENSION},
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));
}

#if defined(OS_CHROMEOS)
void AddGoogleAssistantStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
      {"googleAssistantPageTitle", IDS_SETTINGS_GOOGLE_ASSISTANT},
      {"googleAssistantEnableContext",
       IDS_SETTINGS_GOOGLE_ASSISTANT_ENABLE_CONTEXT},
      {"googleAssistantEnableContextDescription",
       IDS_SETTINGS_GOOGLE_ASSISTANT_ENABLE_CONTEXT_DESCRIPTION},
      {"googleAssistantSettings", IDS_SETTINGS_GOOGLE_ASSISTANT_SETTINGS},
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));
}
#endif

void AddSiteSettingsStrings(content::WebUIDataSource* html_source,
                            Profile* profile) {
  LocalizedString localized_strings[] = {
    {"addSite", IDS_SETTINGS_ADD_SITE},
    {"addSiteExceptionPlaceholder",
     IDS_SETTINGS_ADD_SITE_EXCEPTION_PLACEHOLDER},
    {"addSiteTitle", IDS_SETTINGS_ADD_SITE_TITLE},
    {"cookieAppCache", IDS_SETTINGS_COOKIES_APPLICATION_CACHE},
    {"cookieCacheStorage", IDS_SETTINGS_COOKIES_CACHE_STORAGE},
    {"cookieChannelId", IDS_SETTINGS_COOKIES_CHANNEL_ID},
    {"cookieDatabaseStorage", IDS_SETTINGS_COOKIES_DATABASE_STORAGE},
    {"cookieFileSystem", IDS_SETTINGS_COOKIES_FILE_SYSTEM},
    {"cookieFlashLso", IDS_SETTINGS_COOKIES_FLASH_LSO},
    {"cookieLocalStorage", IDS_SETTINGS_COOKIES_LOCAL_STORAGE},
    {"cookieMediaLicense", IDS_SETTINGS_COOKIES_MEDIA_LICENSE},
    {"cookiePlural", IDS_SETTINGS_COOKIES_PLURAL_COOKIES},
    {"cookieServiceWorker", IDS_SETTINGS_COOKIES_SERVICE_WORKER},
    {"cookieSharedWorker", IDS_SETTINGS_COOKIES_SHARED_WORKER},
    {"embeddedOnAnyHost", IDS_SETTINGS_EXCEPTIONS_EMBEDDED_ON_ANY_HOST},
    {"embeddedOnHost", IDS_SETTINGS_EXCEPTIONS_EMBEDDED_ON_HOST},
    {"editSiteTitle", IDS_SETTINGS_EDIT_SITE_TITLE},
    {"appCacheManifest", IDS_SETTINGS_COOKIES_APPLICATION_CACHE_MANIFEST_LABEL},
    {"cacheStorageLastModified",
     IDS_SETTINGS_COOKIES_LOCAL_STORAGE_LAST_MODIFIED_LABEL},
    {"cacheStorageOrigin", IDS_SETTINGS_COOKIES_LOCAL_STORAGE_ORIGIN_LABEL},
    {"cacheStorageSize", IDS_SETTINGS_COOKIES_LOCAL_STORAGE_SIZE_ON_DISK_LABEL},
    {"channelIdServerId", IDS_SETTINGS_COOKIES_CHANNEL_ID_ORIGIN_LABEL},
    {"channelIdType", IDS_SETTINGS_COOKIES_CHANNEL_ID_TYPE_LABEL},
    {"channelIdCreated", IDS_SETTINGS_COOKIES_CHANNEL_ID_CREATED_LABEL},
    {"channelIdExpires", IDS_SETTINGS_COOKIES_CHANNEL_ID_EXPIRES_LABEL},
    {"cookieAccessibleToScript",
     IDS_SETTINGS_COOKIES_COOKIE_ACCESSIBLE_TO_SCRIPT_LABEL},
    {"cookieLastAccessed", IDS_SETTINGS_COOKIES_LAST_ACCESSED_LABEL},
    {"cookieContent", IDS_SETTINGS_COOKIES_COOKIE_CONTENT_LABEL},
    {"cookieCreated", IDS_SETTINGS_COOKIES_COOKIE_CREATED_LABEL},
    {"cookieDomain", IDS_SETTINGS_COOKIES_COOKIE_DOMAIN_LABEL},
    {"cookieExpires", IDS_SETTINGS_COOKIES_COOKIE_EXPIRES_LABEL},
    {"cookieName", IDS_SETTINGS_COOKIES_COOKIE_NAME_LABEL},
    {"cookiePath", IDS_SETTINGS_COOKIES_COOKIE_PATH_LABEL},
    {"cookieSendFor", IDS_SETTINGS_COOKIES_COOKIE_SENDFOR_LABEL},
    {"fileSystemOrigin", IDS_SETTINGS_COOKIES_LOCAL_STORAGE_ORIGIN_LABEL},
    {"fileSystemPersistentUsage",
     IDS_SETTINGS_COOKIES_FILE_SYSTEM_PERSISTENT_USAGE_LABEL},
    {"fileSystemTemporaryUsage",
     IDS_SETTINGS_COOKIES_FILE_SYSTEM_TEMPORARY_USAGE_LABEL},
    {"indexedDbSize", IDS_SETTINGS_COOKIES_LOCAL_STORAGE_SIZE_ON_DISK_LABEL},
    {"indexedDbLastModified",
     IDS_SETTINGS_COOKIES_LOCAL_STORAGE_LAST_MODIFIED_LABEL},
    {"indexedDbOrigin", IDS_SETTINGS_COOKIES_LOCAL_STORAGE_ORIGIN_LABEL},
    {"localStorageLastModified",
     IDS_SETTINGS_COOKIES_LOCAL_STORAGE_LAST_MODIFIED_LABEL},
    {"localStorageOrigin", IDS_SETTINGS_COOKIES_LOCAL_STORAGE_ORIGIN_LABEL},
    {"localStorageSize", IDS_SETTINGS_COOKIES_LOCAL_STORAGE_SIZE_ON_DISK_LABEL},
    {"mediaLicenseOrigin", IDS_SETTINGS_COOKIES_LOCAL_STORAGE_ORIGIN_LABEL},
    {"mediaLicenseSize", IDS_SETTINGS_COOKIES_LOCAL_STORAGE_SIZE_ON_DISK_LABEL},
    {"mediaLicenseLastModified",
     IDS_SETTINGS_COOKIES_LOCAL_STORAGE_LAST_MODIFIED_LABEL},
    {"noUsbDevicesFound", IDS_SETTINGS_NO_USB_DEVICES_FOUND},
    {"serviceWorkerOrigin", IDS_SETTINGS_COOKIES_LOCAL_STORAGE_ORIGIN_LABEL},
    {"serviceWorkerScopes", IDS_SETTINGS_COOKIES_SERVICE_WORKER_SCOPES_LABEL},
    {"serviceWorkerSize",
     IDS_SETTINGS_COOKIES_LOCAL_STORAGE_SIZE_ON_DISK_LABEL},
    {"sharedWorkerWorker", IDS_SETTINGS_COOKIES_SHARED_WORKER_WORKER_LABEL},
    {"sharedWorkerName", IDS_SETTINGS_COOKIES_COOKIE_NAME_LABEL},
    {"webdbDesc", IDS_SETTINGS_COOKIES_WEB_DATABASE_DESCRIPTION_LABEL},
    {"siteSettingsCategoryPageTitle", IDS_SETTINGS_SITE_SETTINGS_CATEGORY},
    {"siteSettingsCategoryAllSites", IDS_SETTINGS_SITE_SETTINGS_ALL_SITES},
    {"siteSettingsCategoryCamera", IDS_SETTINGS_SITE_SETTINGS_CAMERA},
    {"siteSettingsCameraLabel", IDS_SETTINGS_SITE_SETTINGS_CAMERA_LABEL},
    {"siteSettingsCategoryCookies", IDS_SETTINGS_SITE_SETTINGS_COOKIES},
    {"siteSettingsCategoryHandlers", IDS_SETTINGS_SITE_SETTINGS_HANDLERS},
    {"siteSettingsCategoryImages", IDS_SETTINGS_SITE_SETTINGS_IMAGES},
    {"siteSettingsCategoryLocation", IDS_SETTINGS_SITE_SETTINGS_LOCATION},
    {"siteSettingsCategoryJavascript", IDS_SETTINGS_SITE_SETTINGS_JAVASCRIPT},
    {"siteSettingsCategoryMicrophone", IDS_SETTINGS_SITE_SETTINGS_MIC},
    {"siteSettingsMicrophoneLabel", IDS_SETTINGS_SITE_SETTINGS_MIC_LABEL},
    {"siteSettingsCategoryNotifications",
     IDS_SETTINGS_SITE_SETTINGS_NOTIFICATIONS},
    {"siteSettingsCategoryPopups", IDS_SETTINGS_SITE_SETTINGS_POPUPS},
    {"siteSettingsCategoryZoomLevels", IDS_SETTINGS_SITE_SETTINGS_ZOOM_LEVELS},
    {"siteSettingsAllSites", IDS_SETTINGS_SITE_SETTINGS_ALL_SITES},
    {"siteSettingsAutomaticDownloads",
     IDS_SETTINGS_SITE_SETTINGS_AUTOMATIC_DOWNLOADS},
    {"siteSettingsBackgroundSync", IDS_SETTINGS_SITE_SETTINGS_BACKGROUND_SYNC},
    {"siteSettingsCamera", IDS_SETTINGS_SITE_SETTINGS_CAMERA},
    {"siteSettingsClipboard", IDS_SETTINGS_SITE_SETTINGS_CLIPBOARD},
    {"siteSettingsClipboardAsk", IDS_SETTINGS_SITE_SETTINGS_CLIPBOARD_ASK},
    {"siteSettingsClipboardAskRecommended",
     IDS_SETTINGS_SITE_SETTINGS_CLIPBOARD_ASK_RECOMMENDED},
    {"siteSettingsClipboardBlock", IDS_SETTINGS_SITE_SETTINGS_CLIPBOARD_BLOCK},
    {"siteSettingsCookies", IDS_SETTINGS_SITE_SETTINGS_COOKIES},
    {"siteSettingsHandlers", IDS_SETTINGS_SITE_SETTINGS_HANDLERS},
    {"siteSettingsLocation", IDS_SETTINGS_SITE_SETTINGS_LOCATION},
    {"siteSettingsMic", IDS_SETTINGS_SITE_SETTINGS_MIC},
    {"siteSettingsNotifications", IDS_SETTINGS_SITE_SETTINGS_NOTIFICATIONS},
    {"siteSettingsImages", IDS_SETTINGS_SITE_SETTINGS_IMAGES},
    {"siteSettingsJavascript", IDS_SETTINGS_SITE_SETTINGS_JAVASCRIPT},
    {"siteSettingsSound", IDS_SETTINGS_SITE_SETTINGS_SOUND},
    {"siteSettingsSoundAllow", IDS_SETTINGS_SITE_SETTINGS_SOUND_ALLOW},
    {"siteSettingsSoundAllowRecommended",
     IDS_SETTINGS_SITE_SETTINGS_SOUND_ALLOW_RECOMMENDED},
    {"siteSettingsSoundBlock", IDS_SETTINGS_SITE_SETTINGS_SOUND_BLOCK},
    {"siteSettingsFlash", IDS_SETTINGS_SITE_SETTINGS_FLASH},
    {"siteSettingsPdfDocuments", IDS_SETTINGS_SITE_SETTINGS_PDF_DOCUMENTS},
    {"siteSettingsPdfDownloadPdfs",
     IDS_SETTINGS_SITE_SETTINGS_PDF_DOWNLOAD_PDFS},
    {"siteSettingsProtectedContent",
     IDS_SETTINGS_SITE_SETTINGS_PROTECTED_CONTENT},
    {"siteSettingsProtectedContentIdentifiers",
     IDS_SETTINGS_SITE_SETTINGS_PROTECTED_CONTENT_IDENTIFIERS},
    {"siteSettingsProtectedContentEnable",
     IDS_SETTINGS_SITE_SETTINGS_PROTECTED_CONTENT_ENABLE},
#if defined(OS_CHROMEOS) || defined(OS_WIN)
    {"siteSettingsProtectedContentIdentifiersExplanation",
     IDS_SETTINGS_SITE_SETTINGS_PROTECTED_CONTENT_IDENTIFIERS_EXPLANATION},
    {"siteSettingsProtectedContentEnableIdentifiers",
     IDS_SETTINGS_SITE_SETTINGS_PROTECTED_CONTENT_ENABLE_IDENTIFIERS},
#endif
    {"siteSettingsPopups", IDS_SETTINGS_SITE_SETTINGS_POPUPS},
    {"siteSettingsUnsandboxedPlugins",
     IDS_SETTINGS_SITE_SETTINGS_UNSANDBOXED_PLUGINS},
    {"siteSettingsMidiDevices", IDS_SETTINGS_SITE_SETTINGS_MIDI_DEVICES},
    {"siteSettingsMidiDevicesAsk", IDS_SETTINGS_SITE_SETTINGS_MIDI_DEVICES_ASK},
    {"siteSettingsMidiDevicesAskRecommended",
     IDS_SETTINGS_SITE_SETTINGS_MIDI_DEVICES_ASK_RECOMMENDED},
    {"siteSettingsMidiDevicesBlock",
     IDS_SETTINGS_SITE_SETTINGS_MIDI_DEVICES_BLOCK},
    {"siteSettingsUsbDevices", IDS_SETTINGS_SITE_SETTINGS_USB_DEVICES},
    {"siteSettingsRemoveZoomLevel",
     IDS_SETTINGS_SITE_SETTINGS_REMOVE_ZOOM_LEVEL},
    {"siteSettingsZoomLevels", IDS_SETTINGS_SITE_SETTINGS_ZOOM_LEVELS},
    {"siteSettingsNoZoomedSites", IDS_SETTINGS_SITE_SETTINGS_NO_ZOOMED_SITES},
    {"siteSettingsMaySaveCookies", IDS_SETTINGS_SITE_SETTINGS_MAY_SAVE_COOKIES},
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
    {"siteSettingsFlashBlock", IDS_SETTINGS_SITE_SETTINGS_FLASH_BLOCK},
    {"siteSettingsAllowRecentlyClosedSites",
     IDS_SETTINGS_SITE_SETTINGS_ALLOW_RECENTLY_CLOSED_SITES},
    {"siteSettingsAllowRecentlyClosedSitesRecommended",
     IDS_SETTINGS_SITE_SETTINGS_ALLOW_RECENTLY_CLOSED_SITES_RECOMMENDED},
    {"siteSettingsBackgroundSyncBlocked",
     IDS_SETTINGS_SITE_SETTINGS_BACKGROUND_SYNC_BLOCKED},
    {"siteSettingsHandlersAsk", IDS_SETTINGS_SITE_SETTINGS_HANDLERS_ASK},
    {"siteSettingsHandlersAskRecommended",
     IDS_SETTINGS_SITE_SETTINGS_HANDLERS_ASK_RECOMMENDED},
    {"siteSettingsHandlersBlocked",
     IDS_SETTINGS_SITE_SETTINGS_HANDLERS_BLOCKED},
    {"siteSettingsAutoDownloadAsk",
     IDS_SETTINGS_SITE_SETTINGS_AUTOMATIC_DOWNLOAD_ASK},
    {"siteSettingsAutoDownloadAskRecommended",
     IDS_SETTINGS_SITE_SETTINGS_AUTOMATIC_DOWNLOAD_ASK_RECOMMENDED},
    {"siteSettingsAutoDownloadBlock",
     IDS_SETTINGS_SITE_SETTINGS_AUTOMATIC_DOWNLOAD_BLOCK},
    {"siteSettingsUnsandboxedPluginsAsk",
     IDS_SETTINGS_SITE_SETTINGS_UNSANDBOXED_PLUGINS_ASK},
    {"siteSettingsUnsandboxedPluginsAskRecommended",
     IDS_SETTINGS_SITE_SETTINGS_UNSANDBOXED_PLUGINS_ASK_RECOMMENDED},
    {"siteSettingsUnsandboxedPluginsBlock",
     IDS_SETTINGS_SITE_SETTINGS_UNSANDBOXED_PLUGINS_BLOCK},
    {"siteSettingsDontShowImages", IDS_SETTINGS_SITE_SETTINGS_DONT_SHOW_IMAGES},
    {"siteSettingsShowAll", IDS_SETTINGS_SITE_SETTINGS_SHOW_ALL},
    {"siteSettingsShowAllRecommended",
     IDS_SETTINGS_SITE_SETTINGS_SHOW_ALL_RECOMMENDED},
    {"siteSettingsCookiesAllowed",
     IDS_SETTINGS_SITE_SETTINGS_COOKIES_ALLOW_SITES},
    {"siteSettingsCookiesAllowedRecommended",
     IDS_SETTINGS_SITE_SETTINGS_COOKIES_ALLOW_SITES_RECOMMENDED},
    {"siteSettingsAllow", IDS_SETTINGS_SITE_SETTINGS_ALLOW},
    {"siteSettingsBlock", IDS_SETTINGS_SITE_SETTINGS_BLOCK},
    {"siteSettingsBlockSound", IDS_SETTINGS_SITE_SETTINGS_BLOCK_SOUND},
    {"siteSettingsSessionOnly", IDS_SETTINGS_SITE_SETTINGS_SESSION_ONLY},
    {"siteSettingsAllowed", IDS_SETTINGS_SITE_SETTINGS_ALLOWED},
    {"siteSettingsAllowedRecommended",
     IDS_SETTINGS_SITE_SETTINGS_ALLOWED_RECOMMENDED},
    {"siteSettingsBlocked", IDS_SETTINGS_SITE_SETTINGS_BLOCKED},
    {"siteSettingsBlockedRecommended",
     IDS_SETTINGS_SITE_SETTINGS_BLOCKED_RECOMMENDED},
    {"siteSettingsSiteUrl", IDS_SETTINGS_SITE_SETTINGS_SITE_URL},
    {"siteSettingsActionAskDefault",
     IDS_SETTINGS_SITE_SETTINGS_ASK_DEFAULT_MENU},
    {"siteSettingsActionAllowDefault",
     IDS_SETTINGS_SITE_SETTINGS_ALLOW_DEFAULT_MENU},
    {"siteSettingsActionBlockDefault",
     IDS_SETTINGS_SITE_SETTINGS_BLOCK_DEFAULT_MENU},
    {"siteSettingsActionAllow", IDS_SETTINGS_SITE_SETTINGS_ALLOW_MENU},
    {"siteSettingsActionBlock", IDS_SETTINGS_SITE_SETTINGS_BLOCK_MENU},
    {"siteSettingsActionAsk", IDS_SETTINGS_SITE_SETTINGS_ASK_MENU},
    {"siteSettingsActionReset", IDS_SETTINGS_SITE_SETTINGS_RESET_MENU},
    {"siteSettingsActionSessionOnly",
     IDS_SETTINGS_SITE_SETTINGS_SESSION_ONLY_MENU},
    {"siteSettingsUsage", IDS_SETTINGS_SITE_SETTINGS_USAGE},
    {"siteSettingsUsageNone", IDS_SETTINGS_SITE_SETTINGS_USAGE_NONE},
    {"siteSettingsPermissions", IDS_SETTINGS_SITE_SETTINGS_PERMISSIONS},
    {"siteSettingsSourceExtensionAllow",
     IDS_PAGE_INFO_PERMISSION_ALLOWED_BY_EXTENSION},
    {"siteSettingsSourceExtensionBlock",
     IDS_PAGE_INFO_PERMISSION_BLOCKED_BY_EXTENSION},
    {"siteSettingsSourceExtensionAsk",
     IDS_PAGE_INFO_PERMISSION_ASK_BY_EXTENSION},
    {"siteSettingsSourcePolicyAllow",
     IDS_PAGE_INFO_PERMISSION_ALLOWED_BY_POLICY},
    {"siteSettingsSourcePolicyBlock",
     IDS_PAGE_INFO_PERMISSION_BLOCKED_BY_POLICY},
    {"siteSettingsSourcePolicyAsk", IDS_PAGE_INFO_PERMISSION_ASK_BY_POLICY},
    {"siteSettingsAdsBlockSingular",
     IDS_SETTINGS_SITE_SETTINGS_ADS_BLOCK_SINGULAR},
    {"siteSettingsSourceAdsBlacklist", IDS_PAGE_INFO_PERMISSION_ADS_SUBTITLE},
    {"siteSettingsSourceDrmDisabled",
     IDS_SETTINGS_SITE_SETTINGS_SOURCE_DRM_DISABLED},
    {"siteSettingsSourceEmbargo",
     IDS_PAGE_INFO_PERMISSION_AUTOMATICALLY_BLOCKED},
    {"siteSettingsSourceInsecureOrigin",
     IDS_SETTINGS_SITE_SETTINGS_SOURCE_INSECURE_ORIGIN},
    {"siteSettingsSourceKillSwitch",
     IDS_SETTINGS_SITE_SETTINGS_SOURCE_KILL_SWITCH},
    {"siteSettingsReset", IDS_SETTINGS_SITE_SETTINGS_RESET_BUTTON},
    {"siteSettingsCookieHeader", IDS_SETTINGS_SITE_SETTINGS_COOKIE_HEADER},
    {"siteSettingsCookieLink", IDS_SETTINGS_SITE_SETTINGS_COOKIE_LINK},
    {"siteSettingsCookieRemove", IDS_SETTINGS_SITE_SETTINGS_COOKIE_REMOVE},
    {"siteSettingsCookieRemoveAll",
     IDS_SETTINGS_SITE_SETTINGS_COOKIE_REMOVE_ALL},
    {"siteSettingsCookieRemoveAllShown",
     IDS_SETTINGS_SITE_SETTINGS_COOKIE_REMOVE_ALL_SHOWN},
    {"siteSettingsCookieRemoveDialogTitle",
     IDS_SETTINGS_SITE_SETTINGS_COOKIE_REMOVE_DIALOG_TITLE},
    {"siteSettingsCookieRemoveMultipleConfirmation",
     IDS_SETTINGS_SITE_SETTINGS_COOKIE_REMOVE_MULTIPLE},
    {"siteSettingsCookieRemoveSite",
     IDS_SETTINGS_SITE_SETTINGS_COOKIE_REMOVE_SITE},
    {"siteSettingsCookiesClearAll",
     IDS_SETTINGS_SITE_SETTINGS_COOKIES_CLEAR_ALL},
    {"siteSettingsCookieSearch", IDS_SETTINGS_SITE_SETTINGS_COOKIE_SEARCH},
    {"siteSettingsCookieSubpage", IDS_SETTINGS_SITE_SETTINGS_COOKIE_SUBPAGE},
    {"siteSettingsDelete", IDS_SETTINGS_SITE_SETTINGS_DELETE},
    {"siteSettingsSiteResetAll", IDS_SETTINGS_SITE_SETTINGS_SITE_RESET_ALL},
    {"siteSettingsSiteResetConfirmation",
     IDS_SETTINGS_SITE_SETTINGS_SITE_RESET_CONFIRMATION},
    {"siteSettingsSiteResetDialogTitle",
     IDS_SETTINGS_SITE_SETTINGS_SITE_RESET_DIALOG_TITLE},
    {"thirdPartyCookie", IDS_SETTINGS_SITE_SETTINGS_THIRD_PARTY_COOKIE},
    {"thirdPartyCookieSublabel",
     IDS_SETTINGS_SITE_SETTINGS_THIRD_PARTY_COOKIE_SUBLABEL},
    {"deleteDataPostSession",
     IDS_SETTINGS_SITE_SETTINGS_DELETE_DATA_POST_SESSION},
    {"handlerIsDefault", IDS_SETTINGS_SITE_SETTINGS_HANDLER_IS_DEFAULT},
    {"handlerSetDefault", IDS_SETTINGS_SITE_SETTINGS_HANDLER_SET_DEFAULT},
    {"handlerRemove", IDS_SETTINGS_SITE_SETTINGS_REMOVE},
    {"adobeFlashStorage", IDS_SETTINGS_SITE_SETTINGS_ADOBE_FLASH_SETTINGS},
    {"learnMore", IDS_SETTINGS_SITE_SETTINGS_LEARN_MORE},
    {"incognitoSite", IDS_SETTINGS_SITE_SETTINGS_INCOGNITO},
    {"incognitoSiteOnly", IDS_SETTINGS_SITE_SETTINGS_INCOGNITO_ONLY},
    {"embeddedIncognitoSite", IDS_SETTINGS_SITE_SETTINGS_INCOGNITO_EMBEDDED},
    {"noSitesAdded", IDS_SETTINGS_SITE_NO_SITES_ADDED},
    {"siteSettingsAds", IDS_SETTINGS_SITE_SETTINGS_ADS},
    {"siteSettingsAdsBlock", IDS_SETTINGS_SITE_SETTINGS_ADS_BLOCK},
    {"siteSettingsAdsBlockRecommended",
     IDS_SETTINGS_SITE_SETTINGS_ADS_BLOCK_RECOMMENDED},
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));

  html_source->AddBoolean("enableSiteSettings",
                          base::CommandLine::ForCurrentProcess()->HasSwitch(
                              switches::kEnableSiteSettings));
  html_source->AddBoolean(
      "enableSafeBrowsingSubresourceFilter",
      base::FeatureList::IsEnabled(
          subresource_filter::kSafeBrowsingSubresourceFilterExperimentalUI));

  html_source->AddBoolean(
      "enableSoundContentSetting",
      base::FeatureList::IsEnabled(features::kSoundContentSetting));

  html_source->AddBoolean(
      "enableClipboardContentSetting",
      base::FeatureList::IsEnabled(features::kClipboardContentSetting));

  if (PluginUtils::ShouldPreferHtmlOverPlugins(
          HostContentSettingsMapFactory::GetForProfile(profile))) {
    LocalizedString flash_strings[] = {
        {"siteSettingsFlashAskFirst", IDS_SETTINGS_SITE_SETTINGS_ASK_FIRST},
        {"siteSettingsFlashAskFirstRecommended",
         IDS_SETTINGS_SITE_SETTINGS_ASK_FIRST_RECOMMENDED},
    };
    AddLocalizedStringsBulk(html_source, flash_strings,
                            arraysize(flash_strings));
  } else {
    LocalizedString flash_strings[] = {
        {"siteSettingsFlashAskFirst",
         IDS_SETTINGS_SITE_SETTINGS_FLASH_DETECT_IMPORTANT},
        {"siteSettingsFlashAskFirstRecommended",
         IDS_SETTINGS_SITE_SETTINGS_FLASH_DETECT_IMPORTANT_RECOMMENDED},
    };
    AddLocalizedStringsBulk(html_source, flash_strings,
                            arraysize(flash_strings));
  }
}

#if defined(OS_CHROMEOS)
void AddUsersStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
    {"usersModifiedByOwnerLabel", IDS_SETTINGS_USERS_MODIFIED_BY_OWNER_LABEL},
    {"guestBrowsingLabel", IDS_SETTINGS_USERS_GUEST_BROWSING_LABEL},
    {"settingsManagedLabel", IDS_SETTINGS_USERS_MANAGED_LABEL},
    {"supervisedUsersLabel", IDS_SETTINGS_USERS_SUPERVISED_USERS_LABEL},
    {"showOnSigninLabel", IDS_SETTINGS_USERS_SHOW_ON_SIGNIN_LABEL},
    {"restrictSigninLabel", IDS_SETTINGS_USERS_RESTRICT_SIGNIN_LABEL},
    {"deviceOwnerLabel", IDS_SETTINGS_USERS_DEVICE_OWNER_LABEL},
    {"addUsers", IDS_SETTINGS_USERS_ADD_USERS},
    {"addUsersEmail", IDS_SETTINGS_USERS_ADD_USERS_EMAIL},
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));
}
#endif

#if !defined(OS_CHROMEOS)
void AddSystemStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
    {"systemPageTitle", IDS_SETTINGS_SYSTEM},
#if !defined(OS_MACOSX)
    {"backgroundAppsLabel", IDS_SETTINGS_SYSTEM_BACKGROUND_APPS_LABEL},
#endif
    {"hardwareAccelerationLabel",
     IDS_SETTINGS_SYSTEM_HARDWARE_ACCELERATION_LABEL},
    {"proxySettingsLabel", IDS_SETTINGS_SYSTEM_PROXY_SETTINGS_LABEL},
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
      {"fonts", IDS_SETTINGS_FONTS},
      {"standardFont", IDS_SETTINGS_STANDARD_FONT_LABEL},
      {"serifFont", IDS_SETTINGS_SERIF_FONT_LABEL},
      {"sansSerifFont", IDS_SETTINGS_SANS_SERIF_FONT_LABEL},
      {"fixedWidthFont", IDS_SETTINGS_FIXED_WIDTH_FONT_LABEL},
      {"minimumFont", IDS_SETTINGS_MINIMUM_FONT_SIZE_LABEL},
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

#if defined(OS_CHROMEOS)
void AddMultideviceStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
      {"multidevicePageTitle", IDS_SETTINGS_MULTIDEVICE},
      {"smsConnect", IDS_SETTINGS_MULTIDEVICE_SMS_CONNECT},
      {"smsConnectSummary", IDS_SETTINGS_MULTIDEVICE_SMS_CONNECT_SUMMARY},
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));

  html_source->AddBoolean("enableMultideviceSettings",
                          base::FeatureList::IsEnabled(features::kMultidevice));
}
#endif

}  // namespace

void AddLocalizedStrings(content::WebUIDataSource* html_source,
                         Profile* profile) {
  AddA11yStrings(html_source);
  AddAboutStrings(html_source);
  AddAppearanceStrings(html_source, profile);

#if defined(OS_WIN) && defined(GOOGLE_CHROME_BUILD)
  AddChromeCleanupStrings(html_source);
#endif  // defined(OS_WIN) && defined(GOOGLE_CHROME_BUILD)

  AddChangePasswordStrings(html_source);
  AddClearBrowsingDataStrings(html_source);
  AddCommonStrings(html_source, profile);
  AddDownloadsStrings(html_source);
  AddLanguagesStrings(html_source);
  AddOnStartupStrings(html_source);
  AddPasswordsAndFormsStrings(html_source);
  AddPeopleStrings(html_source);
  AddPrintingStrings(html_source);
  AddPrivacyStrings(html_source, profile);
  AddResetStrings(html_source);
  AddSearchEnginesStrings(html_source);
#if defined(OS_CHROMEOS)
  AddGoogleAssistantStrings(html_source);
#endif
  AddSearchInSettingsStrings(html_source);
  AddSearchStrings(html_source, profile);
  AddSiteSettingsStrings(html_source, profile);
  AddWebContentStrings(html_source);

#if defined(OS_CHROMEOS)
  AddAndroidAppStrings(html_source);
  AddBluetoothStrings(html_source);
  AddChromeOSUserStrings(html_source, profile);
  AddDateTimeStrings(html_source);
  AddDeviceStrings(html_source);
  AddEasyUnlockStrings(html_source);
  AddInternetStrings(html_source);
  AddMultideviceStrings(html_source);
  AddUsersStrings(html_source);
#else
  AddDefaultBrowserStrings(html_source);
  AddImportDataStrings(html_source);
  AddSystemStrings(html_source);
#endif

#if defined(USE_NSS_CERTS)
  certificate_manager::AddLocalizedStrings(html_source);
#endif

#if defined(OS_CHROMEOS)
  chromeos::network_element::AddLocalizedStrings(html_source);
  chromeos::network_element::AddOncLocalizedStrings(html_source);
  chromeos::network_element::AddDetailsLocalizedStrings(html_source);
  chromeos::network_element::AddConfigLocalizedStrings(html_source);
  chromeos::network_element::AddErrorLocalizedStrings(html_source);
#endif
  policy_indicator::AddLocalizedStrings(html_source);

  html_source->SetJsonPath(kLocalizedStringsFile);
}

}  // namespace settings
