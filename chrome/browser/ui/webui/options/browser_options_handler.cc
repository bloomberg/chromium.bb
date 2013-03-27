// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/browser_options_handler.h"

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/memory/singleton.h"
#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/value_conversions.h"
#include "base/values.h"
#include "chrome/browser/auto_launch_trial.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_page_zoom.h"
#include "chrome/browser/custom_home_pages_table_model.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/gpu/gpu_mode_manager.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/printing/cloud_print/cloud_print_proxy_service.h"
#include "chrome/browser/printing/cloud_print/cloud_print_proxy_service_factory.h"
#include "chrome/browser/printing/cloud_print/cloud_print_setup_flow.h"
#include "chrome/browser/printing/cloud_print/cloud_print_url.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_info_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_shortcut_manager.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/service/service_process_control.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/options/options_util.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/common/page_zoom.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/webui/web_ui_util.h"

#if defined(ENABLE_MANAGED_USERS)
#include "chrome/browser/managed_mode/managed_user_service.h"
#include "chrome/browser/managed_mode/managed_user_service_factory.h"
#endif

#if !defined(OS_CHROMEOS)
#include "chrome/browser/printing/cloud_print/cloud_print_setup_handler.h"
#include "chrome/browser/ui/webui/options/advanced_options_utils.h"
#endif

#if defined(OS_CHROMEOS)
#include "ash/magnifier/magnifier_constants.h"
#include "base/chromeos/chromeos_version.h"
#include "chrome/browser/chromeos/accessibility/accessibility_util.h"
#include "chrome/browser/chromeos/extensions/wallpaper_manager_util.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/options/chromeos/timezone_options_util.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"
#include "ui/gfx/image/image_skia.h"
#endif  // defined(OS_CHROMEOS)

#if defined(OS_WIN)
#include "chrome/installer/util/auto_launch_util.h"
#endif  // defined(OS_WIN)

#if defined(TOOLKIT_GTK)
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#endif  // defined(TOOLKIT_GTK)

using content::BrowserContext;
using content::BrowserThread;
using content::DownloadManager;
using content::OpenURLParams;
using content::Referrer;
using content::UserMetricsAction;

namespace options {

namespace {

bool ShouldShowMultiProfilesUserList() {
#if defined(OS_CHROMEOS)
  // On Chrome OS we use different UI for multi-profiles.
  return false;
#else
  return ProfileManager::IsMultipleProfilesEnabled();
#endif
}

}  // namespace

BrowserOptionsHandler::BrowserOptionsHandler()
    : page_initialized_(false),
      template_url_service_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
#if !defined(OS_MACOSX)
  default_browser_worker_ = new ShellIntegration::DefaultBrowserWorker(this);
#endif
#if(!defined(GOOGLE_CHROME_BUILD) && defined(OS_WIN))
  // On Windows, we need the PDF plugin which is only guaranteed to exist on
  // Google Chrome builds. Use a command-line switch for Windows non-Google
  //  Chrome builds.
  cloud_print_connector_ui_enabled_ =
      CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableCloudPrintProxy);
#elif(!defined(OS_CHROMEOS))
  // Always enabled for Mac, Linux and Google Chrome Windows builds.
  // Never enabled for Chrome OS, we don't even need to indicate it.
  cloud_print_connector_ui_enabled_ = true;
#endif
}

BrowserOptionsHandler::~BrowserOptionsHandler() {
  ProfileSyncService* sync_service(ProfileSyncServiceFactory::
      GetInstance()->GetForProfile(Profile::FromWebUI(web_ui())));
  if (sync_service)
    sync_service->RemoveObserver(this);

  if (default_browser_worker_.get())
    default_browser_worker_->ObserverDestroyed();
  if (template_url_service_)
    template_url_service_->RemoveObserver(this);
  // There may be pending file dialogs, we need to tell them that we've gone
  // away so they don't try and call back to us.
  if (select_folder_dialog_.get())
    select_folder_dialog_->ListenerDestroyed();
}

void BrowserOptionsHandler::GetLocalizedValues(DictionaryValue* values) {
  DCHECK(values);

  static OptionsStringResource resources[] = {
    { "advancedSectionTitleCloudPrint", IDS_GOOGLE_CLOUD_PRINT },
    { "currentUserOnly", IDS_OPTIONS_CURRENT_USER_ONLY },
    { "advancedSectionTitleContent",
      IDS_OPTIONS_ADVANCED_SECTION_TITLE_CONTENT },
    { "advancedSectionTitleLanguages",
      IDS_OPTIONS_ADVANCED_SECTION_TITLE_LANGUAGES },
    { "advancedSectionTitleNetwork",
      IDS_OPTIONS_ADVANCED_SECTION_TITLE_NETWORK },
    { "advancedSectionTitlePrivacy",
      IDS_OPTIONS_ADVANCED_SECTION_TITLE_PRIVACY },
    { "advancedSectionTitleSecurity",
      IDS_OPTIONS_ADVANCED_SECTION_TITLE_SECURITY },
    { "autofillEnabled", IDS_OPTIONS_AUTOFILL_ENABLE },
    { "autologinEnabled", IDS_OPTIONS_PASSWORDS_AUTOLOGIN },
    { "autoOpenFileTypesInfo", IDS_OPTIONS_OPEN_FILE_TYPES_AUTOMATICALLY },
    { "autoOpenFileTypesResetToDefault",
      IDS_OPTIONS_AUTOOPENFILETYPES_RESETTODEFAULT },
    { "changeHomePage", IDS_OPTIONS_CHANGE_HOME_PAGE },
    { "certificatesManageButton", IDS_OPTIONS_CERTIFICATES_MANAGE_BUTTON },
    { "customizeSync", IDS_OPTIONS_CUSTOMIZE_SYNC_BUTTON_LABEL },
    { "defaultFontSizeLabel", IDS_OPTIONS_DEFAULT_FONT_SIZE_LABEL },
    { "defaultSearchManageEngines", IDS_OPTIONS_DEFAULTSEARCH_MANAGE_ENGINES },
    { "defaultZoomFactorLabel", IDS_OPTIONS_DEFAULT_ZOOM_LEVEL_LABEL },
#if defined(OS_CHROMEOS)
    { "disableGData", IDS_OPTIONS_DISABLE_GDATA },
#endif
    { "disableWebServices", IDS_OPTIONS_DISABLE_WEB_SERVICES },
#if defined(OS_CHROMEOS)
    { "displayOptions",
      IDS_OPTIONS_SETTINGS_DISPLAY_OPTIONS_BUTTON_LABEL },
#endif
    { "doNotTrack", IDS_OPTIONS_ENABLE_DO_NOT_TRACK },
    { "doNotTrackConfirmMessage", IDS_OPTIONS_ENABLE_DO_NOT_TRACK_BUBBLE_TEXT },
    { "doNotTrackConfirmEnable",
       IDS_OPTIONS_ENABLE_DO_NOT_TRACK_BUBBLE_ENABLE },
    { "doNotTrackConfirmDisable",
       IDS_OPTIONS_ENABLE_DO_NOT_TRACK_BUBBLE_DISABLE },
    { "downloadLocationAskForSaveLocation",
      IDS_OPTIONS_DOWNLOADLOCATION_ASKFORSAVELOCATION },
    { "downloadLocationBrowseTitle",
      IDS_OPTIONS_DOWNLOADLOCATION_BROWSE_TITLE },
    { "downloadLocationChangeButton",
      IDS_OPTIONS_DOWNLOADLOCATION_CHANGE_BUTTON },
    { "downloadLocationGroupName", IDS_OPTIONS_DOWNLOADLOCATION_GROUP_NAME },
    { "enableLogging", IDS_OPTIONS_ENABLE_LOGGING },
    { "fontSettingsCustomizeFontsButton",
      IDS_OPTIONS_FONTSETTINGS_CUSTOMIZE_FONTS_BUTTON },
    { "fontSizeLabelCustom", IDS_OPTIONS_FONT_SIZE_LABEL_CUSTOM },
    { "fontSizeLabelLarge", IDS_OPTIONS_FONT_SIZE_LABEL_LARGE },
    { "fontSizeLabelMedium", IDS_OPTIONS_FONT_SIZE_LABEL_MEDIUM },
    { "fontSizeLabelSmall", IDS_OPTIONS_FONT_SIZE_LABEL_SMALL },
    { "fontSizeLabelVeryLarge", IDS_OPTIONS_FONT_SIZE_LABEL_VERY_LARGE },
    { "fontSizeLabelVerySmall", IDS_OPTIONS_FONT_SIZE_LABEL_VERY_SMALL },
    { "hideAdvancedSettings", IDS_SETTINGS_HIDE_ADVANCED_SETTINGS },
    { "homePageNtp", IDS_OPTIONS_HOMEPAGE_NTP },
    { "homePageShowHomeButton", IDS_OPTIONS_TOOLBAR_SHOW_HOME_BUTTON },
    { "homePageUseNewTab", IDS_OPTIONS_HOMEPAGE_USE_NEWTAB },
    { "homePageUseURL", IDS_OPTIONS_HOMEPAGE_USE_URL },
    { "instantConfirmMessage", IDS_INSTANT_OPT_IN_MESSAGE },
    { "importData", IDS_OPTIONS_IMPORT_DATA_BUTTON },
    { "improveBrowsingExperience", IDS_OPTIONS_IMPROVE_BROWSING_EXPERIENCE },
    { "languageAndSpellCheckSettingsButton",
      IDS_OPTIONS_SETTINGS_LANGUAGE_AND_INPUT_SETTINGS },
    { "linkDoctorPref", IDS_OPTIONS_LINKDOCTOR_PREF },
    { "manageAutofillSettings", IDS_OPTIONS_MANAGE_AUTOFILL_SETTINGS_LINK },
    { "managePasswords", IDS_OPTIONS_PASSWORDS_MANAGE_PASSWORDS_LINK },
    { "managedUsersSectionTitle", IDS_OPTIONS_MANAGED_USERS_SECTION_TITLE },
    { "networkPredictionEnabledDescription",
      IDS_NETWORK_PREDICTION_ENABLED_DESCRIPTION },
    { "passwordsAndAutofillGroupName",
      IDS_OPTIONS_PASSWORDS_AND_FORMS_GROUP_NAME },
    { "passwordManagerEnabled", IDS_OPTIONS_PASSWORD_MANAGER_ENABLE },
    { "passwordGenerationEnabledDescription",
      IDS_OPTIONS_PASSWORD_GENERATION_ENABLED_LABEL },
    { "privacyClearDataButton", IDS_OPTIONS_PRIVACY_CLEAR_DATA_BUTTON },
    { "privacyContentSettingsButton",
      IDS_OPTIONS_PRIVACY_CONTENT_SETTINGS_BUTTON },
    { "profilesCreate", IDS_PROFILES_CREATE_BUTTON_LABEL },
    { "profilesDelete", IDS_PROFILES_DELETE_BUTTON_LABEL },
    { "profilesDeleteSingle", IDS_PROFILES_DELETE_SINGLE_BUTTON_LABEL },
    { "profilesListItemCurrent", IDS_PROFILES_LIST_ITEM_CURRENT },
    { "profilesManage", IDS_PROFILES_MANAGE_BUTTON_LABEL },
#if defined(ENABLE_SETTINGS_APP)
    { "profilesAppListSwitch", IDS_SETTINGS_APP_PROFILES_SWITCH_BUTTON_LABEL },
#endif
    { "proxiesLabelExtension", IDS_OPTIONS_EXTENSION_PROXIES_LABEL },
    { "proxiesLabelSystem", IDS_OPTIONS_SYSTEM_PROXIES_LABEL,
      IDS_PRODUCT_NAME },
    { "safeBrowsingEnableProtection",
      IDS_OPTIONS_SAFEBROWSING_ENABLEPROTECTION },
    { "sectionTitleAppearance", IDS_APPEARANCE_GROUP_NAME },
    { "sectionTitleDefaultBrowser", IDS_OPTIONS_DEFAULTBROWSER_GROUP_NAME },
    { "sectionTitleUsers", IDS_PROFILES_OPTIONS_GROUP_NAME },
    { "sectionTitleSearch", IDS_OPTIONS_DEFAULTSEARCH_GROUP_NAME },
    { "sectionTitleStartup", IDS_OPTIONS_STARTUP_GROUP_NAME },
    { "sectionTitleSync", IDS_SYNC_OPTIONS_GROUP_NAME },
    { "spellingConfirmMessage", IDS_CONTENT_CONTEXT_SPELLING_BUBBLE_TEXT },
    { "spellingConfirmEnable", IDS_CONTENT_CONTEXT_SPELLING_BUBBLE_ENABLE },
    { "spellingConfirmDisable", IDS_CONTENT_CONTEXT_SPELLING_BUBBLE_DISABLE },
    { "spellingPref", IDS_OPTIONS_SPELLING_PREF },
    { "startupRestoreLastSession", IDS_OPTIONS_STARTUP_RESTORE_LAST_SESSION },
    { "settingsTitle", IDS_SETTINGS_TITLE },
    { "showAdvancedSettings", IDS_SETTINGS_SHOW_ADVANCED_SETTINGS },
    { "sslCheckRevocation", IDS_OPTIONS_SSL_CHECKREVOCATION },
    { "startupSetPages", IDS_OPTIONS_STARTUP_SET_PAGES },
    { "startupShowNewTab", IDS_OPTIONS_STARTUP_SHOW_NEWTAB },
    { "startupShowPages", IDS_OPTIONS_STARTUP_SHOW_PAGES },
    { "suggestPref", IDS_OPTIONS_SUGGEST_PREF },
    { "syncButtonTextInProgress", IDS_SYNC_NTP_SETUP_IN_PROGRESS },
    { "syncButtonTextStop", IDS_SYNC_STOP_SYNCING_BUTTON_LABEL },
    { "themesGallery", IDS_THEMES_GALLERY_BUTTON },
    { "themesGalleryURL", IDS_THEMES_GALLERY_URL },
    { "tabsToLinksPref", IDS_OPTIONS_TABS_TO_LINKS_PREF },
    { "toolbarShowBookmarksBar", IDS_OPTIONS_TOOLBAR_SHOW_BOOKMARKS_BAR },
    { "toolbarShowHomeButton", IDS_OPTIONS_TOOLBAR_SHOW_HOME_BUTTON },
    { "translateEnableTranslate",
      IDS_OPTIONS_TRANSLATE_ENABLE_TRANSLATE },
#if defined(TOOLKIT_GTK)
    { "showWindowDecorations", IDS_SHOW_WINDOW_DECORATIONS },
    { "themesGTKButton", IDS_THEMES_GTK_BUTTON },
    { "themesSetClassic", IDS_THEMES_SET_CLASSIC },
#else
    { "themes", IDS_THEMES_GROUP_NAME },
    { "themesReset", IDS_THEMES_RESET_BUTTON },
#endif
#if defined(OS_CHROMEOS)
    { "accessibilityExplanation",
      IDS_OPTIONS_SETTINGS_ACCESSIBILITY_EXPLANATION },
    { "accessibilityHighContrast",
      IDS_OPTIONS_SETTINGS_ACCESSIBILITY_HIGH_CONTRAST_DESCRIPTION },
    { "accessibilityScreenMagnifier",
      IDS_OPTIONS_SETTINGS_ACCESSIBILITY_SCREEN_MAGNIFIER_DESCRIPTION },
    { "accessibilityTapDragging",
      IDS_OPTIONS_SETTINGS_ACCESSIBILITY_TOUCHPAD_TAP_DRAGGING_DESCRIPTION },
    { "accessibilityScreenMagnifierOff",
      IDS_OPTIONS_SETTINGS_ACCESSIBILITY_SCREEN_MAGNIFIER_OFF },
    { "accessibilityScreenMagnifierFull",
      IDS_OPTIONS_SETTINGS_ACCESSIBILITY_SCREEN_MAGNIFIER_FULL },
    { "accessibilityScreenMagnifierPartial",
      IDS_OPTIONS_SETTINGS_ACCESSIBILITY_SCREEN_MAGNIFIER_PARTIAL },
    { "accessibilitySpokenFeedback",
      IDS_OPTIONS_SETTINGS_ACCESSIBILITY_SPOKEN_FEEDBACK_DESCRIPTION },
    { "accessibilityTitle",
      IDS_OPTIONS_SETTINGS_SECTION_TITLE_ACCESSIBILITY },
    { "accessibilityVirtualKeyboard",
      IDS_OPTIONS_SETTINGS_ACCESSIBILITY_VIRTUAL_KEYBOARD_DESCRIPTION },
    { "accessibilityAlwaysShowMenu",
      IDS_OPTIONS_SETTINGS_ACCESSIBILITY_SHOULD_ALWAYS_SHOW_MENU },
    { "advancedSectionTitleKiosk", IDS_OPTIONS_KIOSK },
    { "factoryResetHeading", IDS_OPTIONS_FACTORY_RESET_HEADING },
    { "factoryResetTitle", IDS_OPTIONS_FACTORY_RESET },
    { "factoryResetRestart", IDS_OPTIONS_FACTORY_RESET_BUTTON },
    { "factoryResetDataRestart", IDS_RELAUNCH_BUTTON },
    { "factoryResetWarning", IDS_OPTIONS_FACTORY_RESET_WARNING },
    { "factoryResetHelpUrl", IDS_FACTORY_RESET_HELP_URL },
    { "changePicture", IDS_OPTIONS_CHANGE_PICTURE_CAPTION },
    { "datetimeTitle", IDS_OPTIONS_SETTINGS_SECTION_TITLE_DATETIME },
    { "deviceGroupDescription", IDS_OPTIONS_DEVICE_GROUP_DESCRIPTION },
    { "deviceGroupPointer", IDS_OPTIONS_DEVICE_GROUP_POINTER_SECTION },
    { "mouseSpeed", IDS_OPTIONS_SETTINGS_MOUSE_SPEED_DESCRIPTION },
    { "touchpadSpeed", IDS_OPTIONS_SETTINGS_TOUCHPAD_SPEED_DESCRIPTION },
    { "enableScreenlock", IDS_OPTIONS_ENABLE_SCREENLOCKER_CHECKBOX },
    { "internetOptionsButtonTitle", IDS_OPTIONS_INTERNET_OPTIONS_BUTTON_TITLE },
    { "keyboardSettingsButtonTitle",
      IDS_OPTIONS_DEVICE_GROUP_KEYBOARD_SETTINGS_BUTTON_TITLE },
    { "manageAccountsButtonTitle", IDS_OPTIONS_ACCOUNTS_BUTTON_TITLE },
    { "manageKioskAppsButton", IDS_OPTIONS_KIOSK_MANAGE_BUTTON },
    { "noPointingDevices", IDS_OPTIONS_NO_POINTING_DEVICES },
    { "sectionTitleDevice", IDS_OPTIONS_DEVICE_GROUP_NAME },
    { "sectionTitleInternet", IDS_OPTIONS_INTERNET_OPTIONS_GROUP_LABEL },
    { "syncOverview", IDS_SYNC_OVERVIEW },
    { "syncButtonTextStart", IDS_SYNC_SETUP_BUTTON_LABEL },
    { "timezone", IDS_OPTIONS_SETTINGS_TIMEZONE_DESCRIPTION },
    { "use24HourClock", IDS_OPTIONS_SETTINGS_USE_24HOUR_CLOCK_DESCRIPTION },
#else
    { "cloudPrintConnectorEnabledManageButton",
      IDS_OPTIONS_CLOUD_PRINT_CONNECTOR_ENABLED_MANAGE_BUTTON},
    { "cloudPrintConnectorEnablingButton",
      IDS_OPTIONS_CLOUD_PRINT_CONNECTOR_ENABLING_BUTTON },
    { "proxiesConfigureButton", IDS_OPTIONS_PROXIES_CONFIGURE_BUTTON },
#endif
#if defined(OS_CHROMEOS) && defined(USE_ASH)
    { "setWallpaper", IDS_SET_WALLPAPER_BUTTON },
#endif
    { "advancedSectionTitleSystem",
      IDS_OPTIONS_ADVANCED_SECTION_TITLE_SYSTEM },
#if !defined(OS_MACOSX) && !defined(OS_CHROMEOS)
    { "backgroundModeCheckbox", IDS_OPTIONS_SYSTEM_ENABLE_BACKGROUND_MODE },
#endif
#if !defined(OS_CHROMEOS)
    { "gpuModeCheckbox",
      IDS_OPTIONS_SYSTEM_ENABLE_HARDWARE_ACCELERATION_MODE },
    { "gpuModeResetRestart",
      IDS_OPTIONS_SYSTEM_ENABLE_HARDWARE_ACCELERATION_MODE_RESTART },
    // Strings with product-name substitutions.
    { "syncOverview", IDS_SYNC_OVERVIEW, IDS_PRODUCT_NAME },
    { "syncButtonTextStart", IDS_SYNC_START_SYNC_BUTTON_LABEL,
      IDS_SHORT_PRODUCT_NAME },
#endif
    { "profilesSingleUser", IDS_PROFILES_SINGLE_USER_MESSAGE,
      IDS_PRODUCT_NAME },
    { "defaultBrowserUnknown", IDS_OPTIONS_DEFAULTBROWSER_UNKNOWN,
      IDS_PRODUCT_NAME },
    { "defaultBrowserUseAsDefault", IDS_OPTIONS_DEFAULTBROWSER_USEASDEFAULT,
      IDS_PRODUCT_NAME },
    { "autoLaunchText", IDS_AUTOLAUNCH_TEXT, IDS_PRODUCT_NAME },
#if defined(OS_CHROMEOS)
    { "factoryResetDescription", IDS_OPTIONS_FACTORY_RESET_DESCRIPTION,
      IDS_SHORT_PRODUCT_NAME },
#endif
    { "languageSectionLabel", IDS_OPTIONS_ADVANCED_LANGUAGE_LABEL,
      IDS_SHORT_PRODUCT_NAME },
  };

#if defined(ENABLE_SETTINGS_APP)
  static OptionsStringResource app_resources[] = {
    { "syncOverview", IDS_SETTINGS_APP_SYNC_OVERVIEW },
    { "syncButtonTextStart", IDS_SYNC_START_SYNC_BUTTON_LABEL,
      IDS_SETTINGS_APP_LAUNCHER_PRODUCT_NAME },
    { "profilesSingleUser", IDS_PROFILES_SINGLE_USER_MESSAGE,
      IDS_SETTINGS_APP_LAUNCHER_PRODUCT_NAME },
    { "languageSectionLabel", IDS_OPTIONS_ADVANCED_LANGUAGE_LABEL,
      IDS_SETTINGS_APP_LAUNCHER_PRODUCT_NAME },
    { "proxiesLabelSystem", IDS_OPTIONS_SYSTEM_PROXIES_LABEL,
      IDS_SETTINGS_APP_LAUNCHER_PRODUCT_NAME },
  };
  DictionaryValue* app_values = NULL;
  CHECK(values->GetDictionary(kSettingsAppKey, &app_values));
  RegisterStrings(app_values, app_resources, arraysize(app_resources));
#endif

  RegisterStrings(values, resources, arraysize(resources));
  RegisterTitle(values, "instantConfirmOverlay", IDS_INSTANT_OPT_IN_TITLE);
  RegisterTitle(values, "doNotTrackConfirmOverlay",
                IDS_OPTIONS_ENABLE_DO_NOT_TRACK_BUBBLE_TITLE);
  RegisterTitle(values, "spellingConfirmOverlay",
                IDS_CONTENT_CONTEXT_SPELLING_ASK_GOOGLE);
  RegisterCloudPrintValues(values);

  values->SetString("syncLearnMoreURL", chrome::kSyncLearnMoreURL);
  string16 omnibox_url = ASCIIToUTF16(chrome::kOmniboxLearnMoreURL);
  values->SetString(
      "defaultSearchGroupLabel",
      l10n_util::GetStringFUTF16(IDS_SEARCH_PREF_EXPLANATION, omnibox_url));

  std::string instant_pref_name = chrome::GetInstantPrefName();
  int instant_message_id = instant_pref_name == prefs::kInstantEnabled ?
      IDS_INSTANT_PREF_WITH_WARNING : IDS_INSTANT_EXTENDED_PREF_WITH_WARNING;
  string16 instant_learn_more_url = ASCIIToUTF16(chrome::kInstantLearnMoreURL);
  values->SetString("instant_enabled", instant_pref_name);
  values->SetString(
      "instantPrefAndWarning",
      l10n_util::GetStringFUTF16(instant_message_id, instant_learn_more_url));
  values->SetString("instantLearnMoreLink", instant_learn_more_url);

#if defined(OS_CHROMEOS)
  const chromeos::User* user = chromeos::UserManager::Get()->GetLoggedInUser();
  values->SetString("username", user ? user->email() : std::string());
#endif

  // Pass along sync status early so it will be available during page init.
  values->Set("syncData", GetSyncStateDictionary().release());

  values->SetString("privacyLearnMoreURL", chrome::kPrivacyLearnMoreURL);
  values->SetString("doNotTrackLearnMoreURL", chrome::kDoNotTrackLearnMoreURL);

#if defined(OS_CHROMEOS)
  values->SetString("cloudPrintLearnMoreURL", chrome::kCloudPrintLearnMoreURL);

  // TODO(pastarmovj): replace this with a call to the CrosSettings list
  // handling functionality to come.
  values->Set("timezoneList", GetTimezoneList().release());

  values->SetString("accessibilityLearnMoreURL",
                    chrome::kChromeAccessibilityHelpURL);

  // Creates magnifierList.
  scoped_ptr<base::ListValue> magnifier_list(new base::ListValue);

  scoped_ptr<base::ListValue> option_full(new base::ListValue);
  option_full->AppendInteger(ash::MAGNIFIER_FULL);
  option_full->AppendString(l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_ACCESSIBILITY_SCREEN_MAGNIFIER_FULL));
  magnifier_list->Append(option_full.release());

  scoped_ptr<base::ListValue> option_partial(new base::ListValue);
  option_partial->AppendInteger(ash::MAGNIFIER_PARTIAL);
  option_partial->Append(new base::StringValue(l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_ACCESSIBILITY_SCREEN_MAGNIFIER_PARTIAL)));
  magnifier_list->Append(option_partial.release());

  values->Set("magnifierList", magnifier_list.release());

  // Sets flag of whether kiosk section should be enabled.
  values->SetBoolean(
      "enableKioskSection",
      !CommandLine::ForCurrentProcess()->HasSwitch(switches::kDisableAppMode) &&
      (chromeos::UserManager::Get()->IsCurrentUserOwner() ||
       !base::chromeos::IsRunningOnChromeOS()));
#endif

#if defined(OS_MACOSX)
  values->SetString("macPasswordsWarning",
      l10n_util::GetStringUTF16(IDS_OPTIONS_PASSWORDS_MAC_WARNING));
  values->SetBoolean("multiple_profiles",
      g_browser_process->profile_manager()->GetNumberOfProfiles() > 1);
#endif

  if (ShouldShowMultiProfilesUserList())
    values->Set("profilesInfo", GetProfilesInfoList().release());

#if defined(ENABLE_MANAGED_USERS)
  ManagedUserService* service =
      ManagedUserServiceFactory::GetForProfile(Profile::FromWebUI(web_ui()));
  values->SetBoolean("profileIsManaged", service->ProfileIsManaged());
#endif

#if !defined(OS_CHROMEOS)
  values->SetBoolean(
      "gpuEnabledAtStart",
      g_browser_process->gpu_mode_manager()->initial_gpu_mode_pref());
#endif
}

void BrowserOptionsHandler::RegisterCloudPrintValues(DictionaryValue* values) {
#if defined(OS_CHROMEOS)
  values->SetString("cloudPrintChromeosOptionLabel",
      l10n_util::GetStringFUTF16(
      IDS_CLOUD_PRINT_CHROMEOS_OPTION_LABEL,
      l10n_util::GetStringUTF16(IDS_GOOGLE_CLOUD_PRINT)));
  values->SetString("cloudPrintChromeosOptionButton",
      l10n_util::GetStringFUTF16(
      IDS_CLOUD_PRINT_CHROMEOS_OPTION_BUTTON,
      l10n_util::GetStringUTF16(IDS_GOOGLE_CLOUD_PRINT)));
#else
  values->SetString("cloudPrintConnectorDisabledLabel",
      l10n_util::GetStringFUTF16(
      IDS_OPTIONS_CLOUD_PRINT_CONNECTOR_DISABLED_LABEL,
      l10n_util::GetStringUTF16(IDS_GOOGLE_CLOUD_PRINT)));
  values->SetString("cloudPrintConnectorDisabledButton",
      l10n_util::GetStringUTF16(
      IDS_OPTIONS_CLOUD_PRINT_CONNECTOR_DISABLED_BUTTON));
  values->SetString("cloudPrintConnectorEnabledButton",
      l10n_util::GetStringUTF16(
      IDS_OPTIONS_CLOUD_PRINT_CONNECTOR_ENABLED_BUTTON));
#endif
}

void BrowserOptionsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "becomeDefaultBrowser",
      base::Bind(&BrowserOptionsHandler::BecomeDefaultBrowser,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setDefaultSearchEngine",
      base::Bind(&BrowserOptionsHandler::SetDefaultSearchEngine,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "createProfile",
      base::Bind(&BrowserOptionsHandler::CreateProfile,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "themesReset",
      base::Bind(&BrowserOptionsHandler::ThemesReset,
                 base::Unretained(this)));
#if defined(TOOLKIT_GTK)
  web_ui()->RegisterMessageCallback(
      "themesSetGTK",
      base::Bind(&BrowserOptionsHandler::ThemesSetGTK,
                 base::Unretained(this)));
#endif
  web_ui()->RegisterMessageCallback(
      "selectDownloadLocation",
      base::Bind(&BrowserOptionsHandler::HandleSelectDownloadLocation,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "autoOpenFileTypesAction",
      base::Bind(&BrowserOptionsHandler::HandleAutoOpenButton,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "defaultFontSizeAction",
      base::Bind(&BrowserOptionsHandler::HandleDefaultFontSize,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "defaultZoomFactorAction",
      base::Bind(&BrowserOptionsHandler::HandleDefaultZoomFactor,
                 base::Unretained(this)));
#if !defined(USE_NSS) && !defined(USE_OPENSSL)
  web_ui()->RegisterMessageCallback(
      "showManageSSLCertificates",
      base::Bind(&BrowserOptionsHandler::ShowManageSSLCertificates,
                 base::Unretained(this)));
#endif
  web_ui()->RegisterMessageCallback(
      "showCloudPrintManagePage",
      base::Bind(&BrowserOptionsHandler::ShowCloudPrintManagePage,
                 base::Unretained(this)));
#if !defined(OS_CHROMEOS)
  if (cloud_print_connector_ui_enabled_) {
    web_ui()->RegisterMessageCallback(
        "showCloudPrintSetupDialog",
        base::Bind(&BrowserOptionsHandler::ShowCloudPrintSetupDialog,
                   base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        "disableCloudPrintConnector",
        base::Bind(&BrowserOptionsHandler::HandleDisableCloudPrintConnector,
                   base::Unretained(this)));
  }
  web_ui()->RegisterMessageCallback(
      "showNetworkProxySettings",
      base::Bind(&BrowserOptionsHandler::ShowNetworkProxySettings,
                 base::Unretained(this)));
#endif
#if defined(OS_CHROMEOS)
  web_ui()->RegisterMessageCallback(
      "openWallpaperManager",
      base::Bind(&BrowserOptionsHandler::HandleOpenWallpaperManager,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "spokenFeedbackChange",
      base::Bind(&BrowserOptionsHandler::SpokenFeedbackChangeCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "highContrastChange",
      base::Bind(&BrowserOptionsHandler::HighContrastChangeCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "virtualKeyboardChange",
      base::Bind(&BrowserOptionsHandler::VirtualKeyboardChangeCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "performFactoryResetRestart",
      base::Bind(&BrowserOptionsHandler::PerformFactoryResetRestart,
                 base::Unretained(this)));
#else
  web_ui()->RegisterMessageCallback(
      "restartBrowser",
      base::Bind(&BrowserOptionsHandler::HandleRestartBrowser,
                 base::Unretained(this)));
#endif
}

void BrowserOptionsHandler::OnStateChanged() {
  web_ui()->CallJavascriptFunction("BrowserOptions.updateSyncState",
                                   *GetSyncStateDictionary());

  SendProfilesInfo();
}

void BrowserOptionsHandler::OnSigninAllowedPrefChange() {
  web_ui()->CallJavascriptFunction("BrowserOptions.updateSyncState",
                                   *GetSyncStateDictionary());

  SendProfilesInfo();
}

void BrowserOptionsHandler::PageLoadStarted() {
  page_initialized_ = false;
}

void BrowserOptionsHandler::InitializeHandler() {
  Profile* profile = Profile::FromWebUI(web_ui());
  PrefService* prefs = profile->GetPrefs();

  ProfileSyncService* sync_service(
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile));
  if (sync_service)
    sync_service->AddObserver(this);

  // Create our favicon data source.
  content::URLDataSource::Add(
      profile, new FaviconSource(profile, FaviconSource::FAVICON));

  default_browser_policy_.Init(
      prefs::kDefaultBrowserSettingEnabled,
      g_browser_process->local_state(),
      base::Bind(&BrowserOptionsHandler::UpdateDefaultBrowserState,
                 base::Unretained(this)));

  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_CACHED_INFO_CHANGED,
                 content::NotificationService::AllSources());
#if defined(OS_CHROMEOS)
  registrar_.Add(this, chrome::NOTIFICATION_LOGIN_USER_IMAGE_CHANGED,
                 content::NotificationService::AllSources());
#endif
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
                 content::Source<ThemeService>(
                     ThemeServiceFactory::GetForProfile(profile)));
  registrar_.Add(this, chrome::NOTIFICATION_GOOGLE_SIGNIN_SUCCESSFUL,
                 content::Source<Profile>(profile));
  registrar_.Add(this, chrome::NOTIFICATION_GOOGLE_SIGNED_OUT,
                 content::Source<Profile>(profile));
  AddTemplateUrlServiceObserver();

#if defined(OS_WIN)
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (!command_line.HasSwitch(switches::kChromeFrame) &&
      !command_line.HasSwitch(switches::kUserDataDir)) {
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
        base::Bind(&BrowserOptionsHandler::CheckAutoLaunch,
                   weak_ptr_factory_.GetWeakPtr(),
                   profile->GetPath()));
  }
#endif

#if !defined(OS_CHROMEOS)
  base::Closure cloud_print_callback = base::Bind(
      &BrowserOptionsHandler::OnCloudPrintPrefsChanged, base::Unretained(this));
  cloud_print_connector_email_.Init(
      prefs::kCloudPrintEmail, prefs, cloud_print_callback);
  cloud_print_connector_enabled_.Init(
      prefs::kCloudPrintProxyEnabled, prefs, cloud_print_callback);
#endif

  auto_open_files_.Init(
      prefs::kDownloadExtensionsToOpen, prefs,
      base::Bind(&BrowserOptionsHandler::SetupAutoOpenFileTypes,
                 base::Unretained(this)));
  default_font_size_.Init(
      prefs::kWebKitDefaultFontSize, prefs,
      base::Bind(&BrowserOptionsHandler::SetupFontSizeSelector,
                 base::Unretained(this)));
  default_zoom_level_.Init(
      prefs::kDefaultZoomLevel, prefs,
      base::Bind(&BrowserOptionsHandler::SetupPageZoomSelector,
                 base::Unretained(this)));
  profile_pref_registrar_.Init(prefs);
  profile_pref_registrar_.Add(
      prefs::kSigninAllowed,
      base::Bind(&BrowserOptionsHandler::OnSigninAllowedPrefChange,
                 base::Unretained(this)));
#if !defined(OS_CHROMEOS)
  profile_pref_registrar_.Add(
      prefs::kProxy,
      base::Bind(&BrowserOptionsHandler::SetupProxySettingsSection,
                 base::Unretained(this)));
#endif  // !defined(OS_CHROMEOS)
}

void BrowserOptionsHandler::InitializePage() {
  page_initialized_ = true;
  OnTemplateURLServiceChanged();
  ObserveThemeChanged();
  OnStateChanged();
  UpdateDefaultBrowserState();

  SetupMetricsReportingSettingVisibility();
  SetupPasswordGenerationSettingVisibility();
  SetupFontSizeSelector();
  SetupPageZoomSelector();
  SetupAutoOpenFileTypes();
  SetupProxySettingsSection();
#if !defined(OS_CHROMEOS)
  if (cloud_print_connector_ui_enabled_) {
    SetupCloudPrintConnectorSection();
    RefreshCloudPrintStatusFromService();
  } else {
    RemoveCloudPrintConnectorSection();
  }
#endif
#if defined(OS_CHROMEOS)
  SetupAccessibilityFeatures();
  if (!g_browser_process->browser_policy_connector()->IsEnterpriseManaged() &&
      !chromeos::UserManager::Get()->IsLoggedInAsGuest()) {
    web_ui()->CallJavascriptFunction(
        "BrowserOptions.enableFactoryResetSection");
  }
#endif
}

// static
void BrowserOptionsHandler::CheckAutoLaunch(
    base::WeakPtr<BrowserOptionsHandler> weak_this,
    const base::FilePath& profile_path) {
#if defined(OS_WIN)
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  // Auto-launch is not supported for secondary profiles yet.
  if (profile_path.BaseName().value() != ASCIIToUTF16(chrome::kInitialProfile))
    return;

  // Pass in weak pointer to this to avoid race if BrowserOptionsHandler is
  // deleted.
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&BrowserOptionsHandler::CheckAutoLaunchCallback,
                 weak_this,
                 auto_launch_trial::IsInAutoLaunchGroup(),
                 auto_launch_util::AutoStartRequested(
                     profile_path.BaseName().value(),
                     true,  // Window requested.
                     base::FilePath())));
#endif
}

void BrowserOptionsHandler::CheckAutoLaunchCallback(
    bool is_in_auto_launch_group,
    bool will_launch_at_login) {
#if defined(OS_WIN)
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (is_in_auto_launch_group) {
    web_ui()->RegisterMessageCallback("toggleAutoLaunch",
        base::Bind(&BrowserOptionsHandler::ToggleAutoLaunch,
        base::Unretained(this)));

    base::FundamentalValue enabled(will_launch_at_login);
    web_ui()->CallJavascriptFunction("BrowserOptions.updateAutoLaunchState",
                                     enabled);
  }
#endif
}

void BrowserOptionsHandler::UpdateDefaultBrowserState() {
  // Check for side-by-side first.
  if (ShellIntegration::CanSetAsDefaultBrowser() ==
          ShellIntegration::SET_DEFAULT_NOT_ALLOWED) {
    SetDefaultBrowserUIString(IDS_OPTIONS_DEFAULTBROWSER_SXS);
    return;
  }

#if defined(OS_MACOSX)
  ShellIntegration::DefaultWebClientState state =
      ShellIntegration::GetDefaultBrowser();
  int status_string_id;
  if (state == ShellIntegration::IS_DEFAULT)
    status_string_id = IDS_OPTIONS_DEFAULTBROWSER_DEFAULT;
  else if (state == ShellIntegration::NOT_DEFAULT)
    status_string_id = IDS_OPTIONS_DEFAULTBROWSER_NOTDEFAULT;
  else
    status_string_id = IDS_OPTIONS_DEFAULTBROWSER_UNKNOWN;

  SetDefaultBrowserUIString(status_string_id);
#else
  default_browser_worker_->StartCheckIsDefault();
#endif
}

void BrowserOptionsHandler::BecomeDefaultBrowser(const ListValue* args) {
  // If the default browser setting is managed then we should not be able to
  // call this function.
  if (default_browser_policy_.IsManaged())
    return;

  content::RecordAction(UserMetricsAction("Options_SetAsDefaultBrowser"));
#if defined(OS_MACOSX)
  if (ShellIntegration::SetAsDefaultBrowser())
    UpdateDefaultBrowserState();
#else
  default_browser_worker_->StartSetAsDefault();
  // Callback takes care of updating UI.
#endif

  // If the user attempted to make Chrome the default browser, then he/she
  // arguably wants to be notified when that changes.
  PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();
  prefs->SetBoolean(prefs::kCheckDefaultBrowser, true);
}

int BrowserOptionsHandler::StatusStringIdForState(
    ShellIntegration::DefaultWebClientState state) {
  if (state == ShellIntegration::IS_DEFAULT)
    return IDS_OPTIONS_DEFAULTBROWSER_DEFAULT;
  if (state == ShellIntegration::NOT_DEFAULT)
    return IDS_OPTIONS_DEFAULTBROWSER_NOTDEFAULT;
  return IDS_OPTIONS_DEFAULTBROWSER_UNKNOWN;
}

void BrowserOptionsHandler::SetDefaultWebClientUIState(
    ShellIntegration::DefaultWebClientUIState state) {
  int status_string_id;
  if (state == ShellIntegration::STATE_IS_DEFAULT)
    status_string_id = IDS_OPTIONS_DEFAULTBROWSER_DEFAULT;
  else if (state == ShellIntegration::STATE_NOT_DEFAULT)
    status_string_id = IDS_OPTIONS_DEFAULTBROWSER_NOTDEFAULT;
  else if (state == ShellIntegration::STATE_UNKNOWN)
    status_string_id = IDS_OPTIONS_DEFAULTBROWSER_UNKNOWN;
  else
    return;  // Still processing.

  SetDefaultBrowserUIString(status_string_id);
}

bool BrowserOptionsHandler::IsInteractiveSetDefaultPermitted() {
  return true;  // This is UI so we can allow it.
}

void BrowserOptionsHandler::SetDefaultBrowserUIString(int status_string_id) {
  base::StringValue status_string(
      l10n_util::GetStringFUTF16(status_string_id,
                                 l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)));

  base::FundamentalValue is_default(
      status_string_id == IDS_OPTIONS_DEFAULTBROWSER_DEFAULT);

  base::FundamentalValue can_be_default(
      !default_browser_policy_.IsManaged() &&
      (status_string_id == IDS_OPTIONS_DEFAULTBROWSER_DEFAULT ||
       status_string_id == IDS_OPTIONS_DEFAULTBROWSER_NOTDEFAULT));

  web_ui()->CallJavascriptFunction(
      "BrowserOptions.updateDefaultBrowserState",
      status_string, is_default, can_be_default);
}

void BrowserOptionsHandler::OnTemplateURLServiceChanged() {
  if (!template_url_service_ || !template_url_service_->loaded())
    return;

  const TemplateURL* default_url =
      template_url_service_->GetDefaultSearchProvider();

  int default_index = -1;
  ListValue search_engines;
  TemplateURLService::TemplateURLVector model_urls(
      template_url_service_->GetTemplateURLs());
  for (size_t i = 0; i < model_urls.size(); ++i) {
    if (!model_urls[i]->ShowInDefaultList())
      continue;

    DictionaryValue* entry = new DictionaryValue();
    entry->SetString("name", model_urls[i]->short_name());
    entry->SetInteger("index", i);
    search_engines.Append(entry);
    if (model_urls[i] == default_url)
      default_index = i;
  }

  web_ui()->CallJavascriptFunction(
      "BrowserOptions.updateSearchEngines",
      search_engines,
      base::FundamentalValue(default_index),
      base::FundamentalValue(
          template_url_service_->is_default_search_managed()));
}

// static
void BrowserOptionsHandler::CreateDesktopShortcutForProfile(
     Profile* profile, Profile::CreateStatus status) {
  ProfileShortcutManager* shortcut_manager =
      g_browser_process->profile_manager()->profile_shortcut_manager();
  if (shortcut_manager)
    shortcut_manager->CreateProfileShortcut(profile->GetPath());
}

void BrowserOptionsHandler::SetDefaultSearchEngine(const ListValue* args) {
  int selected_index = -1;
  if (!ExtractIntegerValue(args, &selected_index)) {
    NOTREACHED();
    return;
  }

  TemplateURLService::TemplateURLVector model_urls(
      template_url_service_->GetTemplateURLs());
  if (selected_index >= 0 &&
      selected_index < static_cast<int>(model_urls.size()))
    template_url_service_->SetDefaultSearchProvider(model_urls[selected_index]);

  content::RecordAction(UserMetricsAction("Options_SearchEngineChanged"));
}

void BrowserOptionsHandler::AddTemplateUrlServiceObserver() {
  template_url_service_ =
      TemplateURLServiceFactory::GetForProfile(Profile::FromWebUI(web_ui()));
  if (template_url_service_) {
    template_url_service_->Load();
    template_url_service_->AddObserver(this);
  }
}

void BrowserOptionsHandler::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  // Notifications are used to update the UI dynamically when settings change in
  // the background. If the UI is currently being loaded, no dynamic updates are
  // possible (as the DOM and JS are not fully loaded) or necessary (as
  // InitializePage() will update the UI at the end of the load).
  if (!page_initialized_)
    return;

  switch (type) {
    case chrome::NOTIFICATION_BROWSER_THEME_CHANGED:
      ObserveThemeChanged();
      break;
#if defined(OS_CHROMEOS)
    case chrome::NOTIFICATION_LOGIN_USER_IMAGE_CHANGED:
      UpdateAccountPicture();
      break;
#endif
    case chrome::NOTIFICATION_PROFILE_CACHED_INFO_CHANGED:
      SendProfilesInfo();
      break;
    case chrome::NOTIFICATION_GOOGLE_SIGNIN_SUCCESSFUL:
    case chrome::NOTIFICATION_GOOGLE_SIGNED_OUT:
      // Update our sync/signin status display.
      OnStateChanged();
      break;
    default:
      NOTREACHED();
  }
}

void BrowserOptionsHandler::OnCloudPrintPrefsChanged() {
#if !defined(OS_CHROMEOS)
  if (cloud_print_connector_ui_enabled_)
    SetupCloudPrintConnectorSection();
#endif
}

void BrowserOptionsHandler::ToggleAutoLaunch(const ListValue* args) {
#if defined(OS_WIN)
  if (!auto_launch_trial::IsInAutoLaunchGroup())
    return;

  bool enable;
  CHECK_EQ(args->GetSize(), 1U);
  CHECK(args->GetBoolean(0, &enable));

  // Make sure we keep track of how many disable and how many enable.
  auto_launch_trial::UpdateToggleAutoLaunchMetric(enable);
  Profile* profile = Profile::FromWebUI(web_ui());
  content::BrowserThread::PostTask(
      content::BrowserThread::FILE, FROM_HERE,
      enable ?
          base::Bind(&auto_launch_util::EnableForegroundStartAtLogin,
                     profile->GetPath().BaseName().value(), base::FilePath()) :
          base::Bind(&auto_launch_util::DisableForegroundStartAtLogin,
                      profile->GetPath().BaseName().value()));
#endif  // OS_WIN
}

scoped_ptr<ListValue> BrowserOptionsHandler::GetProfilesInfoList() {
  ProfileInfoCache& cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  scoped_ptr<ListValue> profile_info_list(new ListValue);
  base::FilePath current_profile_path =
      web_ui()->GetWebContents()->GetBrowserContext()->GetPath();

  for (size_t i = 0, e = cache.GetNumberOfProfiles(); i < e; ++i) {
    DictionaryValue* profile_value = new DictionaryValue();
    profile_value->SetString("name", cache.GetNameOfProfileAtIndex(i));
    base::FilePath profile_path = cache.GetPathOfProfileAtIndex(i);
    profile_value->Set("filePath", base::CreateFilePathValue(profile_path));
    profile_value->SetBoolean("isCurrentProfile",
                              profile_path == current_profile_path);
    profile_value->SetBoolean("isManaged", cache.ProfileIsManagedAtIndex(i));

    bool is_gaia_picture =
        cache.IsUsingGAIAPictureOfProfileAtIndex(i) &&
        cache.GetGAIAPictureOfProfileAtIndex(i);
    if (is_gaia_picture) {
      gfx::Image icon = profiles::GetAvatarIconForWebUI(
          cache.GetAvatarIconOfProfileAtIndex(i), true);
      profile_value->SetString("iconURL",
          webui::GetBitmapDataUrl(icon.AsBitmap()));
    } else {
      size_t icon_index = cache.GetAvatarIconIndexOfProfileAtIndex(i);
      profile_value->SetString("iconURL",
                               cache.GetDefaultAvatarIconUrl(icon_index));
    }

    profile_info_list->Append(profile_value);
  }

  return profile_info_list.Pass();
}

void BrowserOptionsHandler::SendProfilesInfo() {
  if (!ShouldShowMultiProfilesUserList())
    return;
  web_ui()->CallJavascriptFunction("BrowserOptions.setProfilesInfo",
                                   *GetProfilesInfoList());
}

void BrowserOptionsHandler::CreateProfile(const ListValue* args) {
#if defined(ENABLE_MANAGED_USERS)
  // This handler could have been called in managed mode, for example because
  // the user fiddled with the web inspector. Silently return in this case.
  ManagedUserService* service =
      ManagedUserServiceFactory::GetForProfile(Profile::FromWebUI(web_ui()));
  if (service->ProfileIsManaged())
    return;
#endif

  if (!ProfileManager::IsMultipleProfilesEnabled())
    return;

  Browser* browser =
      chrome::FindBrowserWithWebContents(web_ui()->GetWebContents());
  chrome::HostDesktopType desktop_type = chrome::HOST_DESKTOP_TYPE_NATIVE;
  if (browser)
    desktop_type = browser->host_desktop_type();

  string16 name;
  string16 icon;
  ProfileManager::CreateCallback callback;
  bool managed_user = false;
  if (args->GetString(0, &name) && args->GetString(1, &icon)) {
    bool create_box_checked = false;
    if (args->GetBoolean(2, &create_box_checked)) {
      if (create_box_checked)
        callback = base::Bind(&CreateDesktopShortcutForProfile);

      bool success = args->GetBoolean(3, &managed_user);
      DCHECK(success);
    }
  }

  ProfileManager::CreateMultiProfileAsync(name, icon, callback, desktop_type,
                                          managed_user);
}

void BrowserOptionsHandler::ObserveThemeChanged() {
  Profile* profile = Profile::FromWebUI(web_ui());
#if defined(TOOLKIT_GTK)
  GtkThemeService* theme_service = GtkThemeService::GetFrom(profile);
  bool is_gtk_theme = theme_service->UsingNativeTheme();
  base::FundamentalValue gtk_enabled(!is_gtk_theme);
  web_ui()->CallJavascriptFunction("BrowserOptions.setGtkThemeButtonEnabled",
                                   gtk_enabled);
#else
  ThemeService* theme_service = ThemeServiceFactory::GetForProfile(profile);
  bool is_gtk_theme = false;
#endif

  bool is_classic_theme = !is_gtk_theme && theme_service->UsingDefaultTheme();
  base::FundamentalValue enabled(!is_classic_theme);
  web_ui()->CallJavascriptFunction("BrowserOptions.setThemesResetButtonEnabled",
                                   enabled);
}

void BrowserOptionsHandler::ThemesReset(const ListValue* args) {
  content::RecordAction(UserMetricsAction("Options_ThemesReset"));
  Profile* profile = Profile::FromWebUI(web_ui());
  ThemeServiceFactory::GetForProfile(profile)->UseDefaultTheme();
}

#if defined(TOOLKIT_GTK)
void BrowserOptionsHandler::ThemesSetGTK(const ListValue* args) {
  content::RecordAction(UserMetricsAction("Options_GtkThemeSet"));
  Profile* profile = Profile::FromWebUI(web_ui());
  ThemeServiceFactory::GetForProfile(profile)->SetNativeTheme();
}
#endif

#if defined(OS_CHROMEOS)
void BrowserOptionsHandler::UpdateAccountPicture() {
  std::string email = chromeos::UserManager::Get()->GetLoggedInUser()->email();
  if (!email.empty()) {
    web_ui()->CallJavascriptFunction("BrowserOptions.updateAccountPicture");
    base::StringValue email_value(email);
    web_ui()->CallJavascriptFunction("BrowserOptions.updateAccountPicture",
                                     email_value);
  }
}
#endif

scoped_ptr<DictionaryValue> BrowserOptionsHandler::GetSyncStateDictionary() {
  scoped_ptr<DictionaryValue> sync_status(new DictionaryValue);
  Profile* profile = Profile::FromWebUI(web_ui());
  if (profile->IsGuestSession()) {
    // Cannot display signin status when running in guest mode on chromeos
    // because there is no SigninManager.
    sync_status->SetBoolean("signinAllowed", false);
    return sync_status.Pass();
  }

  // Signout is not allowed if the user has policy (crbug.com/172204).
  SigninManager* signin = SigninManagerFactory::GetForProfile(profile);
  DCHECK(signin);
  sync_status->SetBoolean("signoutAllowed", !signin->IsSignoutProhibited());
  ProfileSyncService* service(
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile));
  sync_status->SetBoolean("signinAllowed", signin->IsSigninAllowed());
  sync_status->SetBoolean("syncSystemEnabled", !!service);
  sync_status->SetBoolean("setupCompleted",
                          service && service->HasSyncSetupCompleted());
  sync_status->SetBoolean("setupInProgress",
      service && !service->IsManaged() && service->FirstSetupInProgress());

  string16 status_label;
  string16 link_label;
  DCHECK(signin);
  bool status_has_error = sync_ui_util::GetStatusLabels(
      service, *signin, sync_ui_util::WITH_HTML, &status_label, &link_label) ==
          sync_ui_util::SYNC_ERROR;
  sync_status->SetString("statusText", status_label);
  sync_status->SetString("actionLinkText", link_label);
  sync_status->SetBoolean("hasError", status_has_error);

  sync_status->SetBoolean("managed", service && service->IsManaged());
  sync_status->SetBoolean("signedIn",
                          !signin->GetAuthenticatedUsername().empty());
  sync_status->SetBoolean("hasUnrecoverableError",
                          service && service->HasUnrecoverableError());
  sync_status->SetBoolean(
      "autoLoginVisible",
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kEnableAutologin) &&
      service && service->IsSyncEnabledAndLoggedIn() &&
      service->IsSyncTokenAvailable());

  return sync_status.Pass();
}

void BrowserOptionsHandler::HandleSelectDownloadLocation(
    const ListValue* args) {
  PrefService* pref_service = Profile::FromWebUI(web_ui())->GetPrefs();
  select_folder_dialog_ = ui::SelectFileDialog::Create(
      this, new ChromeSelectFilePolicy(web_ui()->GetWebContents()));
  ui::SelectFileDialog::FileTypeInfo info;
  info.support_drive = true;
  select_folder_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_FOLDER,
      l10n_util::GetStringUTF16(IDS_OPTIONS_DOWNLOADLOCATION_BROWSE_TITLE),
      pref_service->GetFilePath(prefs::kDownloadDefaultDirectory),
      &info, 0, FILE_PATH_LITERAL(""),
      web_ui()->GetWebContents()->GetView()->GetTopLevelNativeWindow(), NULL);
}

void BrowserOptionsHandler::FileSelected(const base::FilePath& path, int index,
                                         void* params) {
  content::RecordAction(UserMetricsAction("Options_SetDownloadDirectory"));
  PrefService* pref_service = Profile::FromWebUI(web_ui())->GetPrefs();
  pref_service->SetFilePath(prefs::kDownloadDefaultDirectory, path);
}

void BrowserOptionsHandler::OnCloudPrintSetupClosed() {
#if !defined(OS_CHROMEOS)
  if (cloud_print_connector_ui_enabled_)
    SetupCloudPrintConnectorSection();
#endif
}

#if defined(OS_CHROMEOS)
void BrowserOptionsHandler::TouchpadExists(bool exists) {
  base::FundamentalValue val(exists);
  web_ui()->CallJavascriptFunction("BrowserOptions.showTouchpadControls", val);
}

void BrowserOptionsHandler::MouseExists(bool exists) {
  base::FundamentalValue val(exists);
  web_ui()->CallJavascriptFunction("BrowserOptions.showMouseControls", val);
}
#endif

void BrowserOptionsHandler::HandleAutoOpenButton(const ListValue* args) {
  content::RecordAction(UserMetricsAction("Options_ResetAutoOpenFiles"));
  DownloadManager* manager = BrowserContext::GetDownloadManager(
      web_ui()->GetWebContents()->GetBrowserContext());
  if (manager)
    DownloadPrefs::FromDownloadManager(manager)->ResetAutoOpen();
}

void BrowserOptionsHandler::HandleDefaultFontSize(const ListValue* args) {
  int font_size;
  if (ExtractIntegerValue(args, &font_size)) {
    if (font_size > 0) {
      default_font_size_.SetValue(font_size);
      SetupFontSizeSelector();
    }
  }
}

void BrowserOptionsHandler::HandleDefaultZoomFactor(const ListValue* args) {
  double zoom_factor;
  if (ExtractDoubleValue(args, &zoom_factor)) {
    default_zoom_level_.SetValue(
        WebKit::WebView::zoomFactorToZoomLevel(zoom_factor));
  }
}

void BrowserOptionsHandler::HandleRestartBrowser(const ListValue* args) {
  chrome::AttemptRestart();
}

#if !defined(OS_CHROMEOS)
void BrowserOptionsHandler::ShowNetworkProxySettings(const ListValue* args) {
  content::RecordAction(UserMetricsAction("Options_ShowProxySettings"));
  AdvancedOptionsUtilities::ShowNetworkProxySettings(
      web_ui()->GetWebContents());
}
#endif

#if !defined(USE_NSS) && !defined(USE_OPENSSL)
void BrowserOptionsHandler::ShowManageSSLCertificates(const ListValue* args) {
  content::RecordAction(UserMetricsAction("Options_ManageSSLCertificates"));
  AdvancedOptionsUtilities::ShowManageSSLCertificates(
      web_ui()->GetWebContents());
}
#endif

void BrowserOptionsHandler::ShowCloudPrintManagePage(const ListValue* args) {
  content::RecordAction(UserMetricsAction("Options_ManageCloudPrinters"));
  // Open a new tab in the current window for the management page.
  Profile* profile = Profile::FromWebUI(web_ui());
  OpenURLParams params(
      CloudPrintURL(profile).GetCloudPrintServiceManageURL(), Referrer(),
      NEW_FOREGROUND_TAB, content::PAGE_TRANSITION_LINK, false);
  web_ui()->GetWebContents()->OpenURL(params);
}

#if !defined(OS_CHROMEOS)
void BrowserOptionsHandler::ShowCloudPrintSetupDialog(const ListValue* args) {
  content::RecordAction(UserMetricsAction("Options_EnableCloudPrintProxy"));
  // Open the connector enable page in the current tab.
  Profile* profile = Profile::FromWebUI(web_ui());
  OpenURLParams params(
      CloudPrintURL(profile).GetCloudPrintServiceEnableURL(
          CloudPrintProxyServiceFactory::GetForProfile(profile)->proxy_id()),
      Referrer(), CURRENT_TAB, content::PAGE_TRANSITION_LINK, false);
  web_ui()->GetWebContents()->OpenURL(params);
}

void BrowserOptionsHandler::HandleDisableCloudPrintConnector(
    const ListValue* args) {
  content::RecordAction(
      UserMetricsAction("Options_DisableCloudPrintProxy"));
  CloudPrintProxyServiceFactory::GetForProfile(Profile::FromWebUI(web_ui()))->
      DisableForUser();
}

void BrowserOptionsHandler::RefreshCloudPrintStatusFromService() {
  if (cloud_print_connector_ui_enabled_)
    CloudPrintProxyServiceFactory::GetForProfile(Profile::FromWebUI(web_ui()))->
        RefreshStatusFromService();
}

void BrowserOptionsHandler::SetupCloudPrintConnectorSection() {
  Profile* profile = Profile::FromWebUI(web_ui());
  if (!CloudPrintProxyServiceFactory::GetForProfile(profile)) {
    cloud_print_connector_ui_enabled_ = false;
    RemoveCloudPrintConnectorSection();
    return;
  }

  bool cloud_print_connector_allowed =
      !cloud_print_connector_enabled_.IsManaged() ||
      cloud_print_connector_enabled_.GetValue();
  base::FundamentalValue allowed(cloud_print_connector_allowed);

  std::string email;
  if (profile->GetPrefs()->HasPrefPath(prefs::kCloudPrintEmail) &&
      cloud_print_connector_allowed) {
    email = profile->GetPrefs()->GetString(prefs::kCloudPrintEmail);
  }
  base::FundamentalValue disabled(email.empty());

  string16 label_str;
  if (email.empty()) {
    label_str = l10n_util::GetStringFUTF16(
        IDS_OPTIONS_CLOUD_PRINT_CONNECTOR_DISABLED_LABEL,
        l10n_util::GetStringUTF16(IDS_GOOGLE_CLOUD_PRINT));
  } else {
    label_str = l10n_util::GetStringFUTF16(
        IDS_OPTIONS_CLOUD_PRINT_CONNECTOR_ENABLED_LABEL,
        l10n_util::GetStringUTF16(IDS_GOOGLE_CLOUD_PRINT),
        UTF8ToUTF16(email));
  }
  StringValue label(label_str);

  web_ui()->CallJavascriptFunction(
      "BrowserOptions.setupCloudPrintConnectorSection", disabled, label,
      allowed);
}

void BrowserOptionsHandler::RemoveCloudPrintConnectorSection() {
  web_ui()->CallJavascriptFunction(
      "BrowserOptions.removeCloudPrintConnectorSection");
}
#endif

#if defined(OS_CHROMEOS)
void BrowserOptionsHandler::HandleOpenWallpaperManager(
    const ListValue* args) {
  wallpaper_manager_util::OpenWallpaperManager();
}

void BrowserOptionsHandler::SpokenFeedbackChangeCallback(
    const ListValue* args) {
  bool enabled = false;
  args->GetBoolean(0, &enabled);

  chromeos::accessibility::EnableSpokenFeedback(
      enabled, NULL, ash::A11Y_NOTIFICATION_NONE);
}

void BrowserOptionsHandler::HighContrastChangeCallback(const ListValue* args) {
  bool enabled = false;
  args->GetBoolean(0, &enabled);

  chromeos::accessibility::EnableHighContrast(enabled);
}

void BrowserOptionsHandler::VirtualKeyboardChangeCallback(
    const ListValue* args) {
  bool enabled = false;
  args->GetBoolean(0, &enabled);

  chromeos::accessibility::EnableVirtualKeyboard(enabled);
}

#if defined(OS_CHROMEOS)

void BrowserOptionsHandler::PerformFactoryResetRestart(const ListValue* args) {
  if (g_browser_process->browser_policy_connector()->IsEnterpriseManaged())
    return;

  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
  prefs->CommitPendingWrite();

  // Perform sign out. Current chrome process will then terminate, new one will
  // be launched (as if it was a restart).
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->RequestRestart();
}

#endif

void BrowserOptionsHandler::SetupAccessibilityFeatures() {
  PrefService* pref_service = g_browser_process->local_state();
  base::FundamentalValue spoken_feedback_enabled(
      pref_service->GetBoolean(prefs::kSpokenFeedbackEnabled));
  web_ui()->CallJavascriptFunction(
      "BrowserOptions.setSpokenFeedbackCheckboxState",
      spoken_feedback_enabled);
  base::FundamentalValue high_contrast_enabled(
      pref_service->GetBoolean(prefs::kHighContrastEnabled));
  web_ui()->CallJavascriptFunction(
      "BrowserOptions.setHighContrastCheckboxState",
      high_contrast_enabled);
  base::FundamentalValue virtual_keyboard_enabled(
      pref_service->GetBoolean(prefs::kVirtualKeyboardEnabled));
  web_ui()->CallJavascriptFunction(
      "BrowserOptions.setVirtualKeyboardCheckboxState",
      virtual_keyboard_enabled);
}
#endif

void BrowserOptionsHandler::SetupMetricsReportingSettingVisibility() {
#if defined(GOOGLE_CHROME_BUILD) && defined(OS_CHROMEOS)
  // Don't show the reporting setting if we are in the guest mode.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kGuestSession)) {
    base::FundamentalValue visible(false);
    web_ui()->CallJavascriptFunction(
        "BrowserOptions.setMetricsReportingSettingVisibility", visible);
  }
#endif
}

void BrowserOptionsHandler::SetupPasswordGenerationSettingVisibility() {
  base::FundamentalValue visible(
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnablePasswordGeneration));
  web_ui()->CallJavascriptFunction(
      "BrowserOptions.setPasswordGenerationSettingVisibility", visible);
}

void BrowserOptionsHandler::SetupFontSizeSelector() {
  // We're only interested in integer values, so convert to int.
  base::FundamentalValue font_size(default_font_size_.GetValue());
  web_ui()->CallJavascriptFunction("BrowserOptions.setFontSize", font_size);
}

void BrowserOptionsHandler::SetupPageZoomSelector() {
  PrefService* pref_service = Profile::FromWebUI(web_ui())->GetPrefs();
  double default_zoom_level = pref_service->GetDouble(prefs::kDefaultZoomLevel);
  double default_zoom_factor =
      WebKit::WebView::zoomLevelToZoomFactor(default_zoom_level);

  // Generate a vector of zoom factors from an array of known presets along with
  // the default factor added if necessary.
  std::vector<double> zoom_factors =
      chrome_page_zoom::PresetZoomFactors(default_zoom_factor);

  // Iterate through the zoom factors and and build the contents of the
  // selector that will be sent to the javascript handler.
  // Each item in the list has the following parameters:
  // 1. Title (string).
  // 2. Value (double).
  // 3. Is selected? (bool).
  ListValue zoom_factors_value;
  for (std::vector<double>::const_iterator i = zoom_factors.begin();
       i != zoom_factors.end(); ++i) {
    ListValue* option = new ListValue();
    double factor = *i;
    int percent = static_cast<int>(factor * 100 + 0.5);
    option->Append(new base::StringValue(
        l10n_util::GetStringFUTF16Int(IDS_ZOOM_PERCENT, percent)));
    option->Append(new base::FundamentalValue(factor));
    bool selected = content::ZoomValuesEqual(factor, default_zoom_factor);
    option->Append(new base::FundamentalValue(selected));
    zoom_factors_value.Append(option);
  }

  web_ui()->CallJavascriptFunction(
      "BrowserOptions.setupPageZoomSelector", zoom_factors_value);
}

void BrowserOptionsHandler::SetupAutoOpenFileTypes() {
  // Set the hidden state for the AutoOpenFileTypesResetToDefault button.
  // We show the button if the user has any auto-open file types registered.
  DownloadManager* manager = BrowserContext::GetDownloadManager(
      web_ui()->GetWebContents()->GetBrowserContext());
  bool display = manager &&
      DownloadPrefs::FromDownloadManager(manager)->IsAutoOpenUsed();
  base::FundamentalValue value(display);
  web_ui()->CallJavascriptFunction(
      "BrowserOptions.setAutoOpenFileTypesDisplayed", value);
}

void BrowserOptionsHandler::SetupProxySettingsSection() {
#if !defined(OS_CHROMEOS)
  // Disable the button if proxy settings are managed by a sysadmin or
  // overridden by an extension.
  PrefService* pref_service = Profile::FromWebUI(web_ui())->GetPrefs();
  const PrefService::Preference* proxy_config =
      pref_service->FindPreference(prefs::kProxy);
  bool is_extension_controlled = (proxy_config &&
                                  proxy_config->IsExtensionControlled());

  base::FundamentalValue disabled(profile_pref_registrar_.IsManaged() ||
                                  is_extension_controlled);
  base::FundamentalValue extension_controlled(is_extension_controlled);
  web_ui()->CallJavascriptFunction("BrowserOptions.setupProxySettingsSection",
                                   disabled, extension_controlled);

#endif  // !defined(OS_CHROMEOS)
}

}  // namespace options
