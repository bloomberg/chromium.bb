// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/browser_options_handler.h"

#include <stddef.h>

#include <set>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/value_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/custom_home_pages_table_model.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/gpu/gpu_mode_manager.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/metrics/metrics_reporting_state.h"
#include "chrome/browser/net/prediction_options.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/printing/cloud_print/cloud_print_proxy_service.h"
#include "chrome/browser/printing/cloud_print/cloud_print_proxy_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profile_shortcut_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/search/hotword_audio_history_handler.h"
#include "chrome/browser/search/hotword_service.h"
#include "chrome/browser/search/hotword_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/easy_unlock_service.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils_desktop.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/options/options_handlers_helper.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/locale_settings.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/proximity_auth/switches.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "components/signin/core/common/signin_pref_names.h"
#include "components/strings/grit/components_strings.h"
#include "components/ui/zoom/page_zoom.h"
#include "components/user_manager/user_type.h"
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
#include "content/public/common/page_zoom.h"
#include "extensions/browser/extension_registry.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "policy/policy_constants.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

#if !defined(OS_CHROMEOS)
#include "chrome/browser/ui/webui/options/advanced_options_utils.h"
#endif

#if defined(OS_CHROMEOS)
#include "ash/ash_switches.h"
#include "ash/desktop_background/user_wallpaper_delegate.h"
#include "ash/shell.h"
#include "ash/system/chromeos/devicetype_utils.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/accessibility/accessibility_util.h"
#include "chrome/browser/chromeos/arc/arc_auth_service.h"
#include "chrome/browser/chromeos/login/users/wallpaper/wallpaper_manager.h"
#include "chrome/browser/chromeos/net/wake_on_wifi_manager.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/reset/metrics.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/system/timezone_util.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector_factory.h"
#include "chrome/browser/ui/browser_window.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"
#include "components/arc/arc_bridge_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "ui/chromeos/accessibility_types.h"
#include "ui/gfx/image/image_skia.h"
#endif  // defined(OS_CHROMEOS)

#if defined(OS_WIN)
#include "chrome/browser/extensions/settings_api_helpers.h"
#include "content/public/browser/browser_url_handler.h"
#endif  // defined(OS_WIN)

#if defined(ENABLE_SERVICE_DISCOVERY)
#include "chrome/browser/printing/cloud_print/privet_notifications.h"
#endif

#if defined(USE_ASH)
#include "ash/shell.h"
#endif

using base::UserMetricsAction;
using content::BrowserContext;
using content::BrowserThread;
using content::DownloadManager;
using content::OpenURLParams;
using content::Referrer;
using extensions::Extension;
using extensions::ExtensionRegistry;

namespace {

#if defined(OS_WIN)
void AppendExtensionData(const std::string& key,
                         const Extension* extension,
                         base::DictionaryValue* dict) {
  scoped_ptr<base::DictionaryValue> details(new base::DictionaryValue);
  details->SetString("id", extension ? extension->id() : std::string());
  details->SetString("name", extension ? extension->name() : std::string());
  dict->Set(key, details.release());
}
#endif  // defined(OS_WIN)

}  // namespace

namespace options {

BrowserOptionsHandler::BrowserOptionsHandler()
    : page_initialized_(false),
      template_url_service_(NULL),
      cloud_print_mdns_ui_enabled_(false),
#if defined(OS_CHROMEOS)
      enable_factory_reset_(false),
#endif  // defined(OS_CHROMEOS)
      signin_observer_(this),
      weak_ptr_factory_(this) {
  default_browser_worker_ = new ShellIntegration::DefaultBrowserWorker(this);

#if defined(ENABLE_SERVICE_DISCOVERY)
  cloud_print_mdns_ui_enabled_ = true;
#endif  // defined(ENABLE_SERVICE_DISCOVERY)
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

  g_browser_process->policy_service()->RemoveObserver(
      policy::POLICY_DOMAIN_CHROME, this);
}

void BrowserOptionsHandler::GetLocalizedValues(base::DictionaryValue* values) {
  DCHECK(values);

#if defined(OS_CHROMEOS)
  const int device_type_resource_id = ash::GetChromeOSDeviceTypeResourceId();
#else
  const int device_type_resource_id = IDS_EASY_UNLOCK_GENERIC_DEVICE_TYPE;
#endif  // defined(OS_CHROMEOS)

  static OptionsStringResource resources[] = {
    // Please keep these in alphabetical order.
    { "accessibilityFeaturesLink", IDS_OPTIONS_ACCESSIBILITY_FEATURES_LINK },
    { "accessibilityTitle", IDS_OPTIONS_SETTINGS_SECTION_TITLE_ACCESSIBILITY },
    { "advancedSectionTitleCertificates",
      IDS_OPTIONS_ADVANCED_SECTION_TITLE_CERTIFICATES },
    { "advancedSectionTitleCloudPrint", IDS_GOOGLE_CLOUD_PRINT },
    { "advancedSectionTitleContent",
      IDS_OPTIONS_ADVANCED_SECTION_TITLE_CONTENT },
    { "advancedSectionTitleLanguages",
      IDS_OPTIONS_ADVANCED_SECTION_TITLE_LANGUAGES },
    { "advancedSectionTitleNetwork",
      IDS_OPTIONS_ADVANCED_SECTION_TITLE_NETWORK },
    { "advancedSectionTitlePrivacy",
      IDS_OPTIONS_ADVANCED_SECTION_TITLE_PRIVACY },
    { "advancedSectionTitleSystem", IDS_OPTIONS_ADVANCED_SECTION_TITLE_SYSTEM },
    { "autoOpenFileTypesInfo", IDS_OPTIONS_OPEN_FILE_TYPES_AUTOMATICALLY },
    { "autoOpenFileTypesResetToDefault",
      IDS_OPTIONS_AUTOOPENFILETYPES_RESETTODEFAULT },
    { "autofillEnabled", IDS_OPTIONS_AUTOFILL_ENABLE },
    { "autologinEnabled", IDS_OPTIONS_PASSWORDS_AUTOLOGIN },
    { "certificatesManageButton", IDS_OPTIONS_CERTIFICATES_MANAGE_BUTTON },
    { "changeHomePage", IDS_OPTIONS_CHANGE_HOME_PAGE },
    { "childLabel", IDS_PROFILES_LIST_CHILD_LABEL },
    { "currentUserOnly", IDS_OPTIONS_CURRENT_USER_ONLY },
    { "customizeSync", IDS_OPTIONS_CUSTOMIZE_SYNC_BUTTON_LABEL },
    { "defaultBrowserUnknown",
      IDS_OPTIONS_DEFAULTBROWSER_UNKNOWN, IDS_PRODUCT_NAME },
    { "defaultBrowserUseAsDefault", IDS_OPTIONS_DEFAULTBROWSER_USEASDEFAULT },
    { "defaultFontSizeLabel", IDS_OPTIONS_DEFAULT_FONT_SIZE_LABEL },
    { "defaultSearchManageEngines", IDS_OPTIONS_DEFAULTSEARCH_MANAGE_ENGINES },
    { "defaultZoomFactorLabel", IDS_OPTIONS_DEFAULT_ZOOM_LEVEL_LABEL },
    { "disableWebServices", IDS_OPTIONS_DISABLE_WEB_SERVICES },
    { "doNotTrack", IDS_OPTIONS_ENABLE_DO_NOT_TRACK },
    { "doNotTrackConfirmDisable",
      IDS_OPTIONS_ENABLE_DO_NOT_TRACK_BUBBLE_DISABLE },
    { "doNotTrackConfirmEnable",
      IDS_OPTIONS_ENABLE_DO_NOT_TRACK_BUBBLE_ENABLE },
    { "doNotTrackConfirmMessage", IDS_OPTIONS_ENABLE_DO_NOT_TRACK_BUBBLE_TEXT },
    { "downloadLocationAskForSaveLocation",
      IDS_OPTIONS_DOWNLOADLOCATION_ASKFORSAVELOCATION },
    { "downloadLocationBrowseTitle",
      IDS_OPTIONS_DOWNLOADLOCATION_BROWSE_TITLE },
    { "downloadLocationChangeButton",
      IDS_OPTIONS_DOWNLOADLOCATION_CHANGE_BUTTON },
    { "downloadLocationGroupName", IDS_OPTIONS_DOWNLOADLOCATION_GROUP_NAME },
    { "easyUnlockDescription", IDS_OPTIONS_EASY_UNLOCK_DESCRIPTION,
      device_type_resource_id },
    { "easyUnlockRequireProximityLabel",
      IDS_OPTIONS_EASY_UNLOCK_REQUIRE_PROXIMITY_LABEL,
      device_type_resource_id },
    { "easyUnlockSectionTitle", IDS_OPTIONS_EASY_UNLOCK_SECTION_TITLE },
    { "easyUnlockSetupButton", IDS_OPTIONS_EASY_UNLOCK_SETUP_BUTTON },
    { "easyUnlockSetupIntro", IDS_OPTIONS_EASY_UNLOCK_SETUP_INTRO,
      device_type_resource_id },
    { "enableLogging", IDS_OPTIONS_ENABLE_LOGGING },
    { "extensionControlled", IDS_OPTIONS_TAB_EXTENSION_CONTROLLED },
    { "extensionDisable", IDS_OPTIONS_TAB_EXTENSION_CONTROLLED_DISABLE },
    { "fontSettingsCustomizeFontsButton",
      IDS_OPTIONS_FONTSETTINGS_CUSTOMIZE_FONTS_BUTTON },
    { "fontSizeLabelCustom", IDS_OPTIONS_FONT_SIZE_LABEL_CUSTOM },
    { "fontSizeLabelLarge", IDS_OPTIONS_FONT_SIZE_LABEL_LARGE },
    { "fontSizeLabelMedium", IDS_OPTIONS_FONT_SIZE_LABEL_MEDIUM },
    { "fontSizeLabelSmall", IDS_OPTIONS_FONT_SIZE_LABEL_SMALL },
    { "fontSizeLabelVeryLarge", IDS_OPTIONS_FONT_SIZE_LABEL_VERY_LARGE },
    { "fontSizeLabelVerySmall", IDS_OPTIONS_FONT_SIZE_LABEL_VERY_SMALL },
    { "googleNowLauncherEnable", IDS_OPTIONS_ENABLE_GOOGLE_NOW },
    { "hideAdvancedSettings", IDS_SETTINGS_HIDE_ADVANCED_SETTINGS },
    { "homePageNtp", IDS_OPTIONS_HOMEPAGE_NTP },
    { "homePageShowHomeButton", IDS_OPTIONS_TOOLBAR_SHOW_HOME_BUTTON },
    { "homePageUseNewTab", IDS_OPTIONS_HOMEPAGE_USE_NEWTAB },
    { "homePageUseURL", IDS_OPTIONS_HOMEPAGE_USE_URL },
    { "hotwordAlwaysOnAudioHistoryDescription",
      IDS_HOTWORD_ALWAYS_ON_AUDIO_HISTORY_DESCRIPTION },
    { "hotwordAlwaysOnDesc", IDS_HOTWORD_SEARCH_ALWAYS_ON_DESCRIPTION },
    { "hotwordAudioHistoryManage", IDS_HOTWORD_AUDIO_HISTORY_MANAGE_LINK },
    { "hotwordAudioLoggingEnable", IDS_HOTWORD_AUDIO_LOGGING_ENABLE },
    { "hotwordConfirmDisable", IDS_HOTWORD_CONFIRM_BUBBLE_DISABLE },
    { "hotwordConfirmEnable", IDS_HOTWORD_CONFIRM_BUBBLE_ENABLE },
    { "hotwordConfirmMessage", IDS_HOTWORD_SEARCH_PREF_DESCRIPTION },
    { "hotwordNoDSPDesc", IDS_HOTWORD_SEARCH_NO_DSP_DESCRIPTION },
    { "hotwordRetrainLink", IDS_HOTWORD_RETRAIN_LINK },
    { "hotwordSearchEnable", IDS_HOTWORD_SEARCH_PREF_CHKBOX },
    { "importData", IDS_OPTIONS_IMPORT_DATA_BUTTON },
    { "improveBrowsingExperience", IDS_OPTIONS_IMPROVE_BROWSING_EXPERIENCE },
    { "languageAndSpellCheckSettingsButton",
      IDS_OPTIONS_SETTINGS_LANGUAGE_AND_INPUT_SETTINGS },
#if defined(OS_CHROMEOS)
    { "languageSectionLabel", IDS_OPTIONS_ADVANCED_LANGUAGE_LABEL,
      IDS_SHORT_PRODUCT_OS_NAME },
#else
    { "languageSectionLabel", IDS_OPTIONS_ADVANCED_LANGUAGE_LABEL,
      IDS_SHORT_PRODUCT_NAME },
#endif
    { "linkDoctorPref", IDS_OPTIONS_LINKDOCTOR_PREF },
    { "manageAutofillSettings", IDS_OPTIONS_MANAGE_AUTOFILL_SETTINGS_LINK },
    { "manageLanguages", IDS_OPTIONS_TRANSLATE_MANAGE_LANGUAGES },
    { "managePasswords", IDS_OPTIONS_PASSWORDS_MANAGE_PASSWORDS_LINK },
    { "metricsReportingResetRestart", IDS_OPTIONS_ENABLE_LOGGING_RESTART },
    { "networkPredictionEnabledDescription",
      IDS_NETWORK_PREDICTION_ENABLED_DESCRIPTION },
    { "passwordManagerEnabled", GetPasswordManagerSettingsStringId(
        ProfileSyncServiceFactory::GetForProfile(Profile::FromWebUI(web_ui())))
    },
    { "passwordsAndAutofillGroupName",
      IDS_OPTIONS_PASSWORDS_AND_FORMS_GROUP_NAME },
    { "privacyClearDataButton", IDS_OPTIONS_PRIVACY_CLEAR_DATA_BUTTON },
    { "privacyContentSettingsButton",
      IDS_OPTIONS_PRIVACY_CONTENT_SETTINGS_BUTTON },
    { "profileAddPersonEnable", IDS_PROFILE_ADD_PERSON_ENABLE },
    { "profileBrowserGuestEnable", IDS_PROFILE_BROWSER_GUEST_ENABLE },
    { "profilesCreate", IDS_PROFILES_CREATE_BUTTON_LABEL },
    { "profilesDelete", IDS_PROFILES_DELETE_BUTTON_LABEL },
    { "profilesDeleteSingle", IDS_PROFILES_DELETE_SINGLE_BUTTON_LABEL },
    { "profilesListItemCurrent", IDS_PROFILES_LIST_ITEM_CURRENT },
    { "profilesManage", IDS_PROFILES_MANAGE_BUTTON_LABEL },
    { "profilesSingleUser", IDS_PROFILES_SINGLE_USER_MESSAGE,
      IDS_PRODUCT_NAME },
    { "proxiesLabelExtension", IDS_OPTIONS_EXTENSION_PROXIES_LABEL },
    { "proxiesLabelSystem", IDS_OPTIONS_SYSTEM_PROXIES_LABEL,
      IDS_PRODUCT_NAME },
    { "resetProfileSettings", IDS_RESET_PROFILE_SETTINGS_BUTTON },
    { "resetProfileSettingsDescription",
      IDS_RESET_PROFILE_SETTINGS_DESCRIPTION },
    { "resetProfileSettingsSectionTitle",
      IDS_RESET_PROFILE_SETTINGS_SECTION_TITLE },
    { "safeBrowsingEnableExtendedReporting",
      IDS_OPTIONS_SAFEBROWSING_ENABLE_EXTENDED_REPORTING },
    { "safeBrowsingEnableProtection",
      IDS_OPTIONS_SAFEBROWSING_ENABLEPROTECTION },
    { "sectionTitleAppearance", IDS_APPEARANCE_GROUP_NAME },
    { "sectionTitleDefaultBrowser", IDS_OPTIONS_DEFAULTBROWSER_GROUP_NAME },
    { "sectionTitleProxy", IDS_OPTIONS_PROXY_GROUP_NAME },
    { "sectionTitleSearch", IDS_OPTIONS_DEFAULTSEARCH_GROUP_NAME },
    { "sectionTitleStartup", IDS_OPTIONS_STARTUP_GROUP_NAME },
    { "sectionTitleSync", IDS_SYNC_OPTIONS_GROUP_NAME },
    { "sectionTitleUsers", IDS_PROFILES_OPTIONS_GROUP_NAME },
    { "settingsTitle", IDS_SETTINGS_TITLE },
    { "showAdvancedSettings", IDS_SETTINGS_SHOW_ADVANCED_SETTINGS },
    { "spellingConfirmDisable", IDS_CONTENT_CONTEXT_SPELLING_BUBBLE_DISABLE },
    { "spellingConfirmEnable", IDS_CONTENT_CONTEXT_SPELLING_BUBBLE_ENABLE },
    { "spellingConfirmMessage", IDS_CONTENT_CONTEXT_SPELLING_BUBBLE_TEXT },
    { "spellingPref", IDS_OPTIONS_SPELLING_PREF },
    { "startupRestoreLastSession", IDS_OPTIONS_STARTUP_RESTORE_LAST_SESSION },
    { "startupSetPages", IDS_OPTIONS_STARTUP_SET_PAGES },
    { "startupShowNewTab", IDS_OPTIONS_STARTUP_SHOW_NEWTAB },
    { "startupShowPages", IDS_OPTIONS_STARTUP_SHOW_PAGES },
    { "suggestPref", IDS_OPTIONS_SUGGEST_PREF },
    { "supervisedUserLabel", IDS_PROFILES_LIST_LEGACY_SUPERVISED_USER_LABEL },
    { "syncButtonTextInProgress", IDS_SYNC_NTP_SETUP_IN_PROGRESS },
    { "syncButtonTextSignIn", IDS_SYNC_START_SYNC_BUTTON_LABEL,
      IDS_SHORT_PRODUCT_NAME },
    { "syncButtonTextStop", IDS_SYNC_STOP_SYNCING_BUTTON_LABEL },
    { "syncOverview", IDS_SYNC_OVERVIEW },
    { "tabsToLinksPref", IDS_OPTIONS_TABS_TO_LINKS_PREF },
    { "themesGallery", IDS_THEMES_GALLERY_BUTTON },
    { "themesGalleryURL", IDS_THEMES_GALLERY_URL },
    { "themesReset", IDS_THEMES_RESET_BUTTON },
    { "toolbarShowBookmarksBar", IDS_OPTIONS_TOOLBAR_SHOW_BOOKMARKS_BAR },
    { "toolbarShowHomeButton", IDS_OPTIONS_TOOLBAR_SHOW_HOME_BUTTON },
    { "translateEnableTranslate",
      IDS_OPTIONS_TRANSLATE_ENABLE_TRANSLATE },
#if defined(OS_CHROMEOS)
    { "accessibilityAlwaysShowMenu",
      IDS_OPTIONS_SETTINGS_ACCESSIBILITY_SHOULD_ALWAYS_SHOW_MENU },
    { "accessibilityAutoclick",
       IDS_OPTIONS_SETTINGS_ACCESSIBILITY_AUTOCLICK_DESCRIPTION },
    { "accessibilityAutoclickDropdown",
      IDS_OPTIONS_SETTINGS_ACCESSIBILITY_AUTOCLICK_DROPDOWN_DESCRIPTION },
    { "accessibilityExplanation",
      IDS_OPTIONS_SETTINGS_ACCESSIBILITY_EXPLANATION },
    { "accessibilityHighContrast",
      IDS_OPTIONS_SETTINGS_ACCESSIBILITY_HIGH_CONTRAST_DESCRIPTION },
    { "accessibilityLargeCursor",
      IDS_OPTIONS_SETTINGS_ACCESSIBILITY_LARGE_CURSOR_DESCRIPTION },
    { "accessibilityScreenMagnifier",
      IDS_OPTIONS_SETTINGS_ACCESSIBILITY_SCREEN_MAGNIFIER_DESCRIPTION },
    { "accessibilityScreenMagnifierCenterFocus",
      IDS_OPTIONS_SETTINGS_ACCESSIBILITY_SCREEN_MAGNIFIER_CENTER_FOCUS },
    { "accessibilityScreenMagnifierFull",
      IDS_OPTIONS_SETTINGS_ACCESSIBILITY_SCREEN_MAGNIFIER_FULL },
    { "accessibilityScreenMagnifierOff",
      IDS_OPTIONS_SETTINGS_ACCESSIBILITY_SCREEN_MAGNIFIER_OFF },
    { "accessibilityScreenMagnifierPartial",
      IDS_OPTIONS_SETTINGS_ACCESSIBILITY_SCREEN_MAGNIFIER_PARTIAL },
    { "accessibilitySettings",
      IDS_OPTIONS_SETTINGS_ACCESSIBILITY_SETTINGS },
    { "accessibilitySpokenFeedback",
      IDS_OPTIONS_SETTINGS_ACCESSIBILITY_SPOKEN_FEEDBACK_DESCRIPTION },
    { "accessibilityStickyKeys",
      IDS_OPTIONS_SETTINGS_ACCESSIBILITY_STICKY_KEYS_DESCRIPTION },
    { "accessibilityTapDragging",
      IDS_OPTIONS_SETTINGS_ACCESSIBILITY_TOUCHPAD_TAP_DRAGGING_DESCRIPTION },
    { "accessibilityVirtualKeyboard",
      IDS_OPTIONS_SETTINGS_ACCESSIBILITY_VIRTUAL_KEYBOARD_DESCRIPTION },
    { "androidAppsTitle", IDS_OPTIONS_ARC_TITLE },
    { "androidAppsEnabled", IDS_OPTIONS_ARC_ENABLE },
    { "autoclickDelayExtremelyShort",
      IDS_OPTIONS_SETTINGS_ACCESSIBILITY_AUTOCLICK_DELAY_EXTREMELY_SHORT },
    { "autoclickDelayLong",
      IDS_OPTIONS_SETTINGS_ACCESSIBILITY_AUTOCLICK_DELAY_LONG },
    { "autoclickDelayShort",
      IDS_OPTIONS_SETTINGS_ACCESSIBILITY_AUTOCLICK_DELAY_SHORT },
    { "autoclickDelayVeryLong",
      IDS_OPTIONS_SETTINGS_ACCESSIBILITY_AUTOCLICK_DELAY_VERY_LONG },
    { "autoclickDelayVeryShort",
      IDS_OPTIONS_SETTINGS_ACCESSIBILITY_AUTOCLICK_DELAY_VERY_SHORT },
    { "changePicture", IDS_OPTIONS_CHANGE_PICTURE },
    { "changePictureCaption", IDS_OPTIONS_CHANGE_PICTURE_CAPTION },
    { "consumerManagementDescription",
      IDS_OPTIONS_CONSUMER_MANAGEMENT_DESCRIPTION },
    { "consumerManagementEnrollButton",
      IDS_OPTIONS_CONSUMER_MANAGEMENT_ENROLL_BUTTON },
    { "consumerManagementEnrollingButton",
      IDS_OPTIONS_CONSUMER_MANAGEMENT_ENROLLING_BUTTON },
    { "consumerManagementUnenrollButton",
      IDS_OPTIONS_CONSUMER_MANAGEMENT_UNENROLL_BUTTON },
    { "consumerManagementUnenrollingButton",
      IDS_OPTIONS_CONSUMER_MANAGEMENT_UNENROLLING_BUTTON },
    { "datetimeTitle", IDS_OPTIONS_SETTINGS_SECTION_TITLE_DATETIME },
    { "deviceControlTitle", IDS_OPTIONS_DEVICE_CONTROL_SECTION_TITLE },
    { "deviceGroupDescription", IDS_OPTIONS_DEVICE_GROUP_DESCRIPTION },
    { "deviceGroupPointer", IDS_OPTIONS_DEVICE_GROUP_POINTER_SECTION },
    { "disableGData", IDS_OPTIONS_DISABLE_GDATA },
    { "displayOptions", IDS_OPTIONS_SETTINGS_DISPLAY_OPTIONS_BUTTON_LABEL },
    { "enableContentProtectionAttestation",
      IDS_OPTIONS_ENABLE_CONTENT_PROTECTION_ATTESTATION },
    { "enableScreenlock", IDS_OPTIONS_ENABLE_SCREENLOCKER_CHECKBOX },
    { "factoryResetDataRestart", IDS_RELAUNCH_BUTTON },
    { "factoryResetDescription", IDS_OPTIONS_FACTORY_RESET_DESCRIPTION,
      IDS_SHORT_PRODUCT_NAME },
    { "factoryResetHeading", IDS_OPTIONS_FACTORY_RESET_HEADING },
    { "factoryResetHelpUrl", IDS_FACTORY_RESET_HELP_URL },
    { "factoryResetRestart", IDS_OPTIONS_FACTORY_RESET_BUTTON },
    { "factoryResetTitle", IDS_OPTIONS_FACTORY_RESET },
    { "factoryResetWarning", IDS_OPTIONS_FACTORY_RESET_WARNING },
    { "internetOptionsButtonTitle", IDS_OPTIONS_INTERNET_OPTIONS_BUTTON_TITLE },
    { "keyboardSettingsButtonTitle",
      IDS_OPTIONS_DEVICE_GROUP_KEYBOARD_SETTINGS_BUTTON_TITLE },
    { "manageAccountsButtonTitle", IDS_OPTIONS_ACCOUNTS_BUTTON_TITLE },
    { "mouseSpeed", IDS_OPTIONS_SETTINGS_MOUSE_SPEED_DESCRIPTION },
    { "noPointingDevices", IDS_OPTIONS_NO_POINTING_DEVICES },
    { "powerSettingsButton",
      IDS_OPTIONS_DEVICE_GROUP_POWER_SETTINGS_BUTTON },
    { "resolveTimezoneByGeoLocation",
      IDS_OPTIONS_RESOLVE_TIMEZONE_BY_GEOLOCATION_DESCRIPTION },
    { "sectionTitleDevice", IDS_OPTIONS_DEVICE_GROUP_NAME },
    { "sectionTitleInternet", IDS_OPTIONS_INTERNET_OPTIONS_GROUP_LABEL },
    { "syncButtonTextStart", IDS_SYNC_SETUP_BUTTON_LABEL },
    { "thirdPartyImeConfirmDisable", IDS_CANCEL },
    { "thirdPartyImeConfirmEnable", IDS_OK },
    { "thirdPartyImeConfirmMessage",
      IDS_OPTIONS_SETTINGS_LANGUAGES_THIRD_PARTY_WARNING_MESSAGE },
    { "timezone", IDS_OPTIONS_SETTINGS_TIMEZONE_DESCRIPTION },
    { "touchpadSpeed", IDS_OPTIONS_SETTINGS_TOUCHPAD_SPEED_DESCRIPTION },
    { "use24HourClock", IDS_OPTIONS_SETTINGS_USE_24HOUR_CLOCK_DESCRIPTION },
    { "wakeOnWifiLabel", IDS_OPTIONS_SETTINGS_WAKE_ON_WIFI_DESCRIPTION },
#else
    { "gpuModeCheckbox",
      IDS_OPTIONS_SYSTEM_ENABLE_HARDWARE_ACCELERATION_MODE },
    { "gpuModeResetRestart",
      IDS_OPTIONS_SYSTEM_ENABLE_HARDWARE_ACCELERATION_MODE_RESTART },
    { "proxiesConfigureButton", IDS_OPTIONS_PROXIES_CONFIGURE_BUTTON },
    { "syncButtonTextStart", IDS_SYNC_SETUP_BUTTON_LABEL },
#endif  // defined(OS_CHROMEOS)

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
    { "showWindowDecorations", IDS_SHOW_WINDOW_DECORATIONS },
    { "themesNativeButton", IDS_THEMES_GTK_BUTTON },
    { "themesSetClassic", IDS_THEMES_SET_CLASSIC },
#else
    { "themes", IDS_THEMES_GROUP_NAME },
#endif  // defined(OS_LINUX) && !defined(OS_CHROMEOS)

#if defined(OS_CHROMEOS) && defined(USE_ASH)
    { "setWallpaper", IDS_SET_WALLPAPER_BUTTON },
#endif  // defined(OS_CHROMEOS) && defined(USE_ASH)

#if !defined(OS_MACOSX) && !defined(OS_CHROMEOS)
    { "backgroundModeCheckbox", IDS_OPTIONS_SYSTEM_ENABLE_BACKGROUND_MODE },
#endif  // defined(OS_MACOSX) && !defined(OS_CHROMEOS)

#if defined(ENABLE_SETTINGS_APP)
    { "profilesAppListSwitch", IDS_SETTINGS_APP_PROFILES_SWITCH_BUTTON_LABEL },
#endif  // defined(ENABLE_SETTINGS_APP)

#if defined(ENABLE_SERVICE_DISCOVERY)
    { "cloudPrintDevicesPageButton", IDS_LOCAL_DISCOVERY_DEVICES_PAGE_BUTTON },
    { "cloudPrintEnableNotificationsLabel",
      IDS_LOCAL_DISCOVERY_NOTIFICATIONS_ENABLE_CHECKBOX_LABEL },
#endif  // defined(ENABLE_SERVICE_DISCOVERY)
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
  base::DictionaryValue* app_values = NULL;
  CHECK(values->GetDictionary(kSettingsAppKey, &app_values));
  RegisterStrings(app_values, app_resources, arraysize(app_resources));
#endif

  RegisterStrings(values, resources, arraysize(resources));
  RegisterTitle(values, "doNotTrackConfirmOverlay",
                IDS_OPTIONS_ENABLE_DO_NOT_TRACK_BUBBLE_TITLE);
  RegisterTitle(values, "spellingConfirmOverlay",
                IDS_CONTENT_CONTEXT_SPELLING_ASK_GOOGLE);
#if defined(ENABLE_PRINT_PREVIEW)
  RegisterCloudPrintValues(values);
#endif

  values->SetString("syncLearnMoreURL", chrome::kSyncLearnMoreURL);
  base::string16 omnibox_url = base::ASCIIToUTF16(chrome::kOmniboxLearnMoreURL);
  values->SetString(
      "defaultSearchGroupLabel",
      l10n_util::GetStringFUTF16(IDS_SEARCH_PREF_EXPLANATION, omnibox_url));
  values->SetString("hotwordLearnMoreURL", chrome::kHotwordLearnMoreURL);
  RegisterTitle(values, "hotwordConfirmOverlay",
                IDS_HOTWORD_CONFIRM_BUBBLE_TITLE);
  values->SetString("hotwordManageAudioHistoryURL",
                    chrome::kManageAudioHistoryURL);
  base::string16 supervised_user_dashboard =
      base::ASCIIToUTF16(chrome::kLegacySupervisedUserManagementURL);
  values->SetString("profilesSupervisedDashboardTip",
      l10n_util::GetStringFUTF16(
          IDS_PROFILES_LEGACY_SUPERVISED_USER_DASHBOARD_TIP,
          supervised_user_dashboard));

#if defined(OS_CHROMEOS)
  Profile* profile = Profile::FromWebUI(web_ui());
  std::string username = profile->GetProfileUserName();
  if (username.empty()) {
    const user_manager::User* user =
        chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
    if (user && (user->GetType() != user_manager::USER_TYPE_GUEST))
      username = user->email();
  }
  if (!username.empty())
    username = gaia::SanitizeEmail(gaia::CanonicalizeEmail(username));

  values->SetString("username", username);
#endif
  // Pass along sync status early so it will be available during page init.
  values->Set("syncData", GetSyncStateDictionary().release());

  values->SetString("privacyLearnMoreURL", chrome::kPrivacyLearnMoreURL);

  values->SetString("doNotTrackLearnMoreURL", chrome::kDoNotTrackLearnMoreURL);

  values->SetBoolean(
      "metricsReportingEnabledAtStart",
      ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled());

#if defined(OS_CHROMEOS)
  // TODO(pastarmovj): replace this with a call to the CrosSettings list
  // handling functionality to come.
  values->Set("timezoneList", chromeos::system::GetTimezoneList().release());

  values->SetString("accessibilityLearnMoreURL",
                    chrome::kChromeAccessibilityHelpURL);

  std::string settings_url = std::string("chrome-extension://") +
      extension_misc::kChromeVoxExtensionId +
      chrome::kChromeAccessibilitySettingsURL;

  values->SetString("accessibilitySettingsURL",
                    settings_url);

  values->SetString("contentProtectionAttestationLearnMoreURL",
                    chrome::kAttestationForContentProtectionLearnMoreURL);

  // Creates magnifierList.
  scoped_ptr<base::ListValue> magnifier_list(new base::ListValue);

  scoped_ptr<base::ListValue> option_full(new base::ListValue);
  option_full->AppendInteger(ui::MAGNIFIER_FULL);
  option_full->AppendString(l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_ACCESSIBILITY_SCREEN_MAGNIFIER_FULL));
  magnifier_list->Append(option_full.release());

  scoped_ptr<base::ListValue> option_partial(new base::ListValue);
  option_partial->AppendInteger(ui::MAGNIFIER_PARTIAL);
  option_partial->Append(new base::StringValue(l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_ACCESSIBILITY_SCREEN_MAGNIFIER_PARTIAL)));
  magnifier_list->Append(option_partial.release());

  values->Set("magnifierList", magnifier_list.release());
#endif

#if defined(OS_MACOSX)
  values->SetString("macPasswordsWarning",
      l10n_util::GetStringUTF16(IDS_OPTIONS_PASSWORDS_MAC_WARNING));
  values->SetBoolean("multiple_profiles",
      g_browser_process->profile_manager()->GetNumberOfProfiles() > 1);
#endif

  if (ShouldShowMultiProfilesUserList())
    values->Set("profilesInfo", GetProfilesInfoList().release());

  // Profile deletion is not allowed for any users using Metro mode.
  bool allow_deletion = true;
#if defined(USE_ASH)
  allow_deletion = allow_deletion && !ash::Shell::HasInstance();
#endif
  values->SetBoolean("allowProfileDeletion", allow_deletion);

  values->SetBoolean("profileIsGuest",
                     Profile::FromWebUI(web_ui())->IsOffTheRecord());

  values->SetBoolean("profileIsSupervised",
                     Profile::FromWebUI(web_ui())->IsSupervised());

#if !defined(OS_CHROMEOS)
  values->SetBoolean(
      "gpuEnabledAtStart",
      g_browser_process->gpu_mode_manager()->initial_gpu_mode_pref());
#endif

#if defined(ENABLE_SERVICE_DISCOVERY)
  values->SetBoolean("cloudPrintHideNotificationsCheckbox",
                     !cloud_print::PrivetNotificationService::IsEnabled());
#endif

  values->SetBoolean("cloudPrintShowMDnsOptions",
                     cloud_print_mdns_ui_enabled_);

  values->SetString("cloudPrintLearnMoreURL", chrome::kCloudPrintLearnMoreURL);

  values->SetString("languagesLearnMoreURL",
                    chrome::kLanguageSettingsLearnMoreUrl);

  values->SetBoolean(
      "easyUnlockAllowed",
      EasyUnlockService::Get(Profile::FromWebUI(web_ui()))->IsAllowed());
  values->SetString("easyUnlockLearnMoreURL", chrome::kEasyUnlockLearnMoreUrl);
  values->SetBoolean("easyUnlockProximityDetectionAllowed",
                     base::CommandLine::ForCurrentProcess()->HasSwitch(
                         proximity_auth::switches::kEnableProximityDetection));

#if defined(OS_CHROMEOS)
  values->SetBoolean("consumerManagementEnabled",
                     base::CommandLine::ForCurrentProcess()->HasSwitch(
                         chromeos::switches::kEnableConsumerManagement));

  RegisterTitle(values, "thirdPartyImeConfirmOverlay",
                IDS_OPTIONS_SETTINGS_LANGUAGES_THIRD_PARTY_WARNING_TITLE);
  values->SetBoolean("usingNewProfilesUI", false);
#else
  values->SetBoolean("usingNewProfilesUI", true);
#endif

  values->SetBoolean("showSetDefault", ShouldShowSetDefaultBrowser());

  values->SetBoolean("allowAdvancedSettings", ShouldAllowAdvancedSettings());

#if defined(OS_CHROMEOS)
  values->SetBoolean(
      "showWakeOnWifi",
      chromeos::WakeOnWifiManager::Get()->WakeOnWifiSupported() &&
      chromeos::switches::WakeOnWifiEnabled());
  const bool have_disable_time_zone_tracking_option_switch =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kDisableTimeZoneTrackingOption);
  values->SetBoolean("enableTimeZoneTrackingOption",
                     !have_disable_time_zone_tracking_option_switch &&
                         !chromeos::system::HasSystemTimezonePolicy());
  values->SetBoolean("resolveTimezoneByGeolocationInitialValue",
                     Profile::FromWebUI(web_ui())->GetPrefs()->GetBoolean(
                         prefs::kResolveTimezoneByGeolocation));
  values->SetBoolean("enableLanguageOptionsImeMenu",
                     chromeos::switches::IsImeMenuEnabled());
#endif
}

#if defined(ENABLE_PRINT_PREVIEW)
void BrowserOptionsHandler::RegisterCloudPrintValues(
    base::DictionaryValue* values) {
  values->SetString("cloudPrintOptionLabel",
                    l10n_util::GetStringFUTF16(
                        IDS_CLOUD_PRINT_CHROMEOS_OPTION_LABEL,
                        l10n_util::GetStringUTF16(IDS_GOOGLE_CLOUD_PRINT)));
}
#endif  // defined(ENABLE_PRINT_PREVIEW)

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
      "deleteProfile",
      base::Bind(&BrowserOptionsHandler::DeleteProfile,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "themesReset",
      base::Bind(&BrowserOptionsHandler::ThemesReset,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "requestProfilesInfo",
      base::Bind(&BrowserOptionsHandler::HandleRequestProfilesInfo,
                 base::Unretained(this)));
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  web_ui()->RegisterMessageCallback(
      "themesSetNative",
      base::Bind(&BrowserOptionsHandler::ThemesSetNative,
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
#if defined(OS_WIN) || defined(OS_MACOSX)
  web_ui()->RegisterMessageCallback(
      "showManageSSLCertificates",
      base::Bind(&BrowserOptionsHandler::ShowManageSSLCertificates,
                 base::Unretained(this)));
#endif
#if defined(OS_CHROMEOS)
  web_ui()->RegisterMessageCallback(
      "openWallpaperManager",
      base::Bind(&BrowserOptionsHandler::HandleOpenWallpaperManager,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "virtualKeyboardChange",
      base::Bind(&BrowserOptionsHandler::VirtualKeyboardChangeCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
       "onPowerwashDialogShow",
       base::Bind(&BrowserOptionsHandler::OnPowerwashDialogShow,
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
  web_ui()->RegisterMessageCallback(
      "showNetworkProxySettings",
      base::Bind(&BrowserOptionsHandler::ShowNetworkProxySettings,
                 base::Unretained(this)));
#endif  // defined(OS_CHROMEOS)

#if defined(ENABLE_SERVICE_DISCOVERY)
  if (cloud_print_mdns_ui_enabled_) {
    web_ui()->RegisterMessageCallback(
        "showCloudPrintDevicesPage",
        base::Bind(&BrowserOptionsHandler::ShowCloudPrintDevicesPage,
                   base::Unretained(this)));
  }
#endif
  web_ui()->RegisterMessageCallback(
      "requestGoogleNowAvailable",
      base::Bind(&BrowserOptionsHandler::HandleRequestGoogleNowAvailable,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "requestHotwordAvailable",
      base::Bind(&BrowserOptionsHandler::HandleRequestHotwordAvailable,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "launchHotwordAudioVerificationApp",
      base::Bind(
          &BrowserOptionsHandler::HandleLaunchHotwordAudioVerificationApp,
          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "launchEasyUnlockSetup",
      base::Bind(&BrowserOptionsHandler::HandleLaunchEasyUnlockSetup,
               base::Unretained(this)));
#if defined(OS_WIN)
  web_ui()->RegisterMessageCallback(
      "refreshExtensionControlIndicators",
      base::Bind(
          &BrowserOptionsHandler::HandleRefreshExtensionControlIndicators,
          base::Unretained(this)));
#endif  // defined(OS_WIN)
  web_ui()->RegisterMessageCallback("metricsReportingCheckboxChanged",
      base::Bind(&BrowserOptionsHandler::HandleMetricsReportingChange,
                 base::Unretained(this)));
}

void BrowserOptionsHandler::Uninitialize() {
  registrar_.RemoveAll();
  g_browser_process->profile_manager()->
      GetProfileInfoCache().RemoveObserver(this);
#if defined(OS_WIN)
  ExtensionRegistry::Get(Profile::FromWebUI(web_ui()))->RemoveObserver(this);
#endif
#if defined(OS_CHROMEOS)
  policy::ConsumerManagementService* consumer_management =
      g_browser_process->platform_part()->browser_policy_connector_chromeos()->
          GetConsumerManagementService();
  if (consumer_management)
    consumer_management->RemoveObserver(this);
#endif
}

void BrowserOptionsHandler::OnStateChanged() {
  UpdateSyncState();
}

void BrowserOptionsHandler::GoogleSigninSucceeded(const std::string& account_id,
                                                  const std::string& username,
                                                  const std::string& password) {
  OnStateChanged();
}

void BrowserOptionsHandler::GoogleSignedOut(const std::string& account_id,
                                            const std::string& username) {
  OnStateChanged();
}

void BrowserOptionsHandler::PageLoadStarted() {
  page_initialized_ = false;
}

void BrowserOptionsHandler::InitializeHandler() {
  Profile* profile = Profile::FromWebUI(web_ui());
  PrefService* prefs = profile->GetPrefs();
  ChromeZoomLevelPrefs* zoom_level_prefs = profile->GetZoomLevelPrefs();
  // Only regular profiles are able to edit default zoom level, or delete per-
  // host zoom levels, via the settings menu. We only require a zoom_level_prefs
  // if the profile is able to change these preference types.
  DCHECK(zoom_level_prefs ||
         profile->GetProfileType() != Profile::REGULAR_PROFILE);
  if (zoom_level_prefs) {
    default_zoom_level_subscription_ =
        zoom_level_prefs->RegisterDefaultZoomLevelCallback(
            base::Bind(&BrowserOptionsHandler::SetupPageZoomSelector,
                       base::Unretained(this)));
  }

  g_browser_process->policy_service()->AddObserver(
      policy::POLICY_DOMAIN_CHROME, this);

  g_browser_process->profile_manager()->GetProfileInfoCache().AddObserver(this);

  ProfileSyncService* sync_service(
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile));
  // TODO(blundell): Use a ScopedObserver to observe the PSS so that cleanup on
  // destruction is automatic.
  if (sync_service)
    sync_service->AddObserver(this);

  SigninManagerBase* signin_manager(
      SigninManagerFactory::GetInstance()->GetForProfile(profile));
  if (signin_manager)
    signin_observer_.Add(signin_manager);

  // Create our favicon data source.
  content::URLDataSource::Add(
      profile, new FaviconSource(profile, FaviconSource::FAVICON));

  default_browser_policy_.Init(
      prefs::kDefaultBrowserSettingEnabled,
      g_browser_process->local_state(),
      base::Bind(&BrowserOptionsHandler::UpdateDefaultBrowserState,
                 base::Unretained(this)));

#if defined(OS_CHROMEOS)
  registrar_.Add(this, chrome::NOTIFICATION_LOGIN_USER_IMAGE_CHANGED,
                 content::NotificationService::AllSources());
#endif
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
                 content::Source<ThemeService>(
                     ThemeServiceFactory::GetForProfile(profile)));
  registrar_.Add(this, chrome::NOTIFICATION_GLOBAL_ERRORS_CHANGED,
                 content::Source<Profile>(profile));
  AddTemplateUrlServiceObserver();

#if defined(OS_WIN)
  ExtensionRegistry::Get(Profile::FromWebUI(web_ui()))->AddObserver(this);
#endif

  // No preferences below this point may be modified by guest profiles.
  if (Profile::FromWebUI(web_ui())->IsGuestSession())
    return;

  auto_open_files_.Init(
      prefs::kDownloadExtensionsToOpen, prefs,
      base::Bind(&BrowserOptionsHandler::SetupAutoOpenFileTypes,
                 base::Unretained(this)));
  profile_pref_registrar_.Init(prefs);
  profile_pref_registrar_.Add(
      prefs::kNetworkPredictionOptions,
      base::Bind(&BrowserOptionsHandler::SetupNetworkPredictionControl,
                 base::Unretained(this)));
  profile_pref_registrar_.Add(
      prefs::kWebKitDefaultFontSize,
      base::Bind(&BrowserOptionsHandler::SetupFontSizeSelector,
                 base::Unretained(this)));
  profile_pref_registrar_.Add(
      prefs::kWebKitDefaultFixedFontSize,
      base::Bind(&BrowserOptionsHandler::SetupFontSizeSelector,
                 base::Unretained(this)));
  profile_pref_registrar_.Add(
      prefs::kSupervisedUsers,
      base::Bind(&BrowserOptionsHandler::SetupManagingSupervisedUsers,
                 base::Unretained(this)));
  profile_pref_registrar_.Add(
      prefs::kSigninAllowed,
      base::Bind(&BrowserOptionsHandler::OnSigninAllowedPrefChange,
                 base::Unretained(this)));
  profile_pref_registrar_.Add(
      prefs::kEasyUnlockPairing,
      base::Bind(&BrowserOptionsHandler::SetupEasyUnlock,
                 base::Unretained(this)));

#if defined(OS_WIN)
  profile_pref_registrar_.Add(
      prefs::kURLsToRestoreOnStartup,
      base::Bind(&BrowserOptionsHandler::SetupExtensionControlledIndicators,
                 base::Unretained(this)));
  profile_pref_registrar_.Add(
      prefs::kHomePage,
      base::Bind(&BrowserOptionsHandler::SetupExtensionControlledIndicators,
                 base::Unretained(this)));
#endif  // defined(OS_WIN)

#if defined(OS_CHROMEOS)
  if (!policy_registrar_) {
    policy_registrar_.reset(new policy::PolicyChangeRegistrar(
        policy::ProfilePolicyConnectorFactory::GetForBrowserContext(profile)
            ->policy_service(),
        policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string())));
    policy_registrar_->Observe(
        policy::key::kUserAvatarImage,
        base::Bind(&BrowserOptionsHandler::OnUserImagePolicyChanged,
                   base::Unretained(this)));
    policy_registrar_->Observe(
        policy::key::kWallpaperImage,
        base::Bind(&BrowserOptionsHandler::OnWallpaperPolicyChanged,
                   base::Unretained(this)));
  }
  chromeos::CrosSettings::Get()->AddSettingsObserver(
      chromeos::kSystemTimezonePolicy,
      base::Bind(&BrowserOptionsHandler::OnSystemTimezonePolicyChanged,
                 weak_ptr_factory_.GetWeakPtr()));
#else  // !defined(OS_CHROMEOS)
  profile_pref_registrar_.Add(
      proxy_config::prefs::kProxy,
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
  SetupMetricsReportingCheckbox();
  SetupNetworkPredictionControl();
  SetupFontSizeSelector();
  SetupPageZoomSelector();
  SetupAutoOpenFileTypes();
  SetupProxySettingsSection();
  SetupManageCertificatesSection();
  SetupManagingSupervisedUsers();
  SetupEasyUnlock();
  SetupExtensionControlledIndicators();

#if defined(OS_CHROMEOS)
  SetupAccessibilityFeatures();
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  enable_factory_reset_ = !connector->IsEnterpriseManaged() &&
      !user_manager::UserManager::Get()->IsLoggedInAsGuest() &&
      !user_manager::UserManager::Get()->IsLoggedInAsSupervisedUser();
  if (enable_factory_reset_) {
    web_ui()->CallJavascriptFunction(
        "BrowserOptions.enableFactoryResetSection");
  }

  Profile* profile = Profile::FromWebUI(web_ui());
  OnAccountPictureManagedChanged(
      policy::ProfilePolicyConnectorFactory::GetForBrowserContext(profile)
          ->policy_service()
          ->GetPolicies(policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME,
                                                std::string()))
          .Get(policy::key::kUserAvatarImage));

  OnWallpaperManagedChanged(
      chromeos::WallpaperManager::Get()->IsPolicyControlled(
          user_manager::UserManager::Get()->GetActiveUser()->GetAccountId()));

  policy::ConsumerManagementService* consumer_management =
      g_browser_process->platform_part()->browser_policy_connector_chromeos()->
          GetConsumerManagementService();
  if (consumer_management) {
    OnConsumerManagementStatusChanged();
    consumer_management->AddObserver(this);
  }

  if (!arc::ArcBridgeService::GetEnabled(
          base::CommandLine::ForCurrentProcess()) ||
      arc::ArcAuthService::IsOptInVerificationDisabled()) {
    web_ui()->CallJavascriptFunction("BrowserOptions.hideAndroidAppsSection");
  }
#endif
}

bool BrowserOptionsHandler::ShouldShowSetDefaultBrowser() {
#if defined(OS_CHROMEOS)
  // We're always the default browser on ChromeOS.
  return false;
#else
  Profile* profile = Profile::FromWebUI(web_ui());
  return !profile->IsGuestSession();
#endif
}

bool BrowserOptionsHandler::ShouldShowMultiProfilesUserList() {
#if defined(OS_CHROMEOS)
  // On Chrome OS we use different UI for multi-profiles.
  return false;
#else
  Profile* profile = Profile::FromWebUI(web_ui());
  if (profile->IsGuestSession())
    return false;
  return profiles::IsMultipleProfilesEnabled();
#endif
}

bool BrowserOptionsHandler::ShouldAllowAdvancedSettings() {
#if defined(OS_CHROMEOS)
  // ChromeOS handles guest-mode restrictions in a different manner.
  return true;
#else
  return !Profile::FromWebUI(web_ui())->IsGuestSession();
#endif
}

void BrowserOptionsHandler::UpdateDefaultBrowserState() {
  default_browser_worker_->StartCheckIsDefault();
}

void BrowserOptionsHandler::BecomeDefaultBrowser(const base::ListValue* args) {
  // If the default browser setting is managed then we should not be able to
  // call this function.
  if (default_browser_policy_.IsManaged())
    return;

  content::RecordAction(UserMetricsAction("Options_SetAsDefaultBrowser"));
  UMA_HISTOGRAM_COUNTS("Settings.StartSetAsDefault", true);

  // Callback takes care of updating UI.
  default_browser_worker_->StartSetAsDefault();

  // If the user attempted to make Chrome the default browser, notify
  // them when this changes.
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

  if (state == ShellIntegration::STATE_IS_DEFAULT) {
    status_string_id = IDS_OPTIONS_DEFAULTBROWSER_DEFAULT;
    // Notify the user in the future if Chrome ceases to be the user's chosen
    // default browser.
    PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();
    prefs->SetBoolean(prefs::kCheckDefaultBrowser, true);
  } else if (state == ShellIntegration::STATE_NOT_DEFAULT) {
    if (ShellIntegration::CanSetAsDefaultBrowser() ==
            ShellIntegration::SET_DEFAULT_NOT_ALLOWED) {
      status_string_id = IDS_OPTIONS_DEFAULTBROWSER_SXS;
    } else {
      status_string_id = IDS_OPTIONS_DEFAULTBROWSER_NOTDEFAULT;
    }
  } else if (state == ShellIntegration::STATE_UNKNOWN) {
    status_string_id = IDS_OPTIONS_DEFAULTBROWSER_UNKNOWN;
  } else {
    return;  // Still processing.
  }

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
  base::ListValue search_engines;
  TemplateURLService::TemplateURLVector model_urls(
      template_url_service_->GetTemplateURLs());
  for (size_t i = 0; i < model_urls.size(); ++i) {
    if (!model_urls[i]->ShowInDefaultList(
            template_url_service_->search_terms_data()))
      continue;

    base::DictionaryValue* entry = new base::DictionaryValue();
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
          template_url_service_->is_default_search_managed() ||
          template_url_service_->IsExtensionControlledDefaultSearch()));

  SetupExtensionControlledIndicators();

  HandleRequestHotwordAvailable(nullptr);
  HandleRequestGoogleNowAvailable(nullptr);
}

void BrowserOptionsHandler::SetDefaultSearchEngine(
    const base::ListValue* args) {
  int selected_index = -1;
  if (!ExtractIntegerValue(args, &selected_index)) {
    NOTREACHED();
    return;
  }

  TemplateURLService::TemplateURLVector model_urls(
      template_url_service_->GetTemplateURLs());
  if (selected_index >= 0 &&
      selected_index < static_cast<int>(model_urls.size()))
    template_url_service_->SetUserSelectedDefaultSearchProvider(
        model_urls[selected_index]);

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

void BrowserOptionsHandler::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  SetupExtensionControlledIndicators();
}

void BrowserOptionsHandler::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    extensions::UnloadedExtensionInfo::Reason reason) {
  SetupExtensionControlledIndicators();
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
    case chrome::NOTIFICATION_GLOBAL_ERRORS_CHANGED:
      // Update our sync/signin status display.
      OnStateChanged();
      break;
    default:
      NOTREACHED();
  }
}

void BrowserOptionsHandler::OnProfileAdded(const base::FilePath& profile_path) {
  SendProfilesInfo();
}

void BrowserOptionsHandler::OnProfileWasRemoved(
    const base::FilePath& profile_path,
    const base::string16& profile_name) {
  SendProfilesInfo();
}

void BrowserOptionsHandler::OnProfileNameChanged(
    const base::FilePath& profile_path,
    const base::string16& old_profile_name) {
  SendProfilesInfo();
}

void BrowserOptionsHandler::OnProfileAvatarChanged(
    const base::FilePath& profile_path) {
  SendProfilesInfo();
}

scoped_ptr<base::ListValue> BrowserOptionsHandler::GetProfilesInfoList() {
  ProfileInfoCache& cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  scoped_ptr<base::ListValue> profile_info_list(new base::ListValue);
  base::FilePath current_profile_path =
      web_ui()->GetWebContents()->GetBrowserContext()->GetPath();

  for (size_t i = 0, e = cache.GetNumberOfProfiles(); i < e; ++i) {
    // The items in |profile_value| are also described in
    // chrome/browser/resources/options/browser_options.js in a @typedef for
    // Profile. Please update it whenever you add or remove any keys here.

    base::DictionaryValue* profile_value = new base::DictionaryValue();
    profile_value->SetString("name", cache.GetNameOfProfileAtIndex(i));
    base::FilePath profile_path = cache.GetPathOfProfileAtIndex(i);
    profile_value->Set("filePath", base::CreateFilePathValue(profile_path));
    profile_value->SetBoolean("isCurrentProfile",
                              profile_path == current_profile_path);
    profile_value->SetBoolean("isSupervised",
                              cache.ProfileIsSupervisedAtIndex(i));
    profile_value->SetBoolean("isChild", cache.ProfileIsChildAtIndex(i));

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
                               profiles::GetDefaultAvatarIconUrl(icon_index));
    }

    profile_info_list->Append(profile_value);
  }

  return profile_info_list;
}

void BrowserOptionsHandler::SendProfilesInfo() {
  if (!ShouldShowMultiProfilesUserList())
    return;
  web_ui()->CallJavascriptFunction("BrowserOptions.setProfilesInfo",
                                   *GetProfilesInfoList());
}

void BrowserOptionsHandler::DeleteProfile(const base::ListValue* args) {
  DCHECK(args);
  const base::Value* file_path_value;
  if (!args->Get(0, &file_path_value)) {
    NOTREACHED();
    return;
  }

  base::FilePath file_path;
  if (!base::GetValueAsFilePath(*file_path_value, &file_path)) {
    NOTREACHED();
    return;
  }

  helper::DeleteProfileAtPath(file_path, web_ui());
}

void BrowserOptionsHandler::ObserveThemeChanged() {
  Profile* profile = Profile::FromWebUI(web_ui());
  ThemeService* theme_service = ThemeServiceFactory::GetForProfile(profile);
  bool is_system_theme = false;

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  bool profile_is_supervised = profile->IsSupervised();
  is_system_theme = theme_service->UsingSystemTheme();
  base::FundamentalValue native_theme_enabled(!is_system_theme &&
                                              !profile_is_supervised);
  web_ui()->CallJavascriptFunction("BrowserOptions.setNativeThemeButtonEnabled",
                                   native_theme_enabled);
#endif

  bool is_classic_theme = !is_system_theme &&
                          theme_service->UsingDefaultTheme();
  base::FundamentalValue enabled(!is_classic_theme);
  web_ui()->CallJavascriptFunction("BrowserOptions.setThemesResetButtonEnabled",
                                   enabled);
}

void BrowserOptionsHandler::ThemesReset(const base::ListValue* args) {
  Profile* profile = Profile::FromWebUI(web_ui());
  content::RecordAction(UserMetricsAction("Options_ThemesReset"));
  ThemeServiceFactory::GetForProfile(profile)->UseDefaultTheme();
}

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
void BrowserOptionsHandler::ThemesSetNative(const base::ListValue* args) {
  content::RecordAction(UserMetricsAction("Options_GtkThemeSet"));
  Profile* profile = Profile::FromWebUI(web_ui());
  ThemeServiceFactory::GetForProfile(profile)->UseSystemTheme();
}
#endif

#if defined(OS_CHROMEOS)
void BrowserOptionsHandler::UpdateAccountPicture() {
  std::string email =
      user_manager::UserManager::Get()->GetLoggedInUser()->email();
  if (!email.empty()) {
    web_ui()->CallJavascriptFunction("BrowserOptions.updateAccountPicture");
    base::StringValue email_value(email);
    web_ui()->CallJavascriptFunction("BrowserOptions.updateAccountPicture",
                                     email_value);
  }
}

void BrowserOptionsHandler::OnAccountPictureManagedChanged(bool managed) {
  web_ui()->CallJavascriptFunction("BrowserOptions.setAccountPictureManaged",
                                   base::FundamentalValue(managed));
}

void BrowserOptionsHandler::OnWallpaperManagedChanged(bool managed) {
  web_ui()->CallJavascriptFunction("BrowserOptions.setWallpaperManaged",
                                   base::FundamentalValue(managed));
}

void BrowserOptionsHandler::OnSystemTimezonePolicyChanged() {
  web_ui()->CallJavascriptFunction(
      "BrowserOptions.setSystemTimezoneManaged",
      base::FundamentalValue(chromeos::system::HasSystemTimezonePolicy()));
}
#endif

scoped_ptr<base::DictionaryValue>
BrowserOptionsHandler::GetSyncStateDictionary() {
  // The items which are to be written into |sync_status| are also described in
  // chrome/browser/resources/options/browser_options.js in @typedef
  // for SyncStatus. Please update it whenever you add or remove any keys here.
  scoped_ptr<base::DictionaryValue> sync_status(new base::DictionaryValue);
  Profile* profile = Profile::FromWebUI(web_ui());
  if (profile->IsGuestSession()) {
    // Cannot display signin status when running in guest mode on chromeos
    // because there is no SigninManager.
    sync_status->SetBoolean("signinAllowed", false);
    return sync_status;
  }

  sync_status->SetBoolean("supervisedUser", profile->IsSupervised());
  sync_status->SetBoolean("childUser", profile->IsChild());

  bool signout_prohibited = false;
#if !defined(OS_CHROMEOS)
  // Signout is not allowed if the user has policy (crbug.com/172204).
  signout_prohibited =
      SigninManagerFactory::GetForProfile(profile)->IsSignoutProhibited();
#endif

  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile);
  SigninManagerBase* signin = SigninManagerFactory::GetForProfile(profile);
  DCHECK(signin);
  sync_status->SetBoolean("signoutAllowed", !signout_prohibited);
  sync_status->SetBoolean("signinAllowed", signin->IsSigninAllowed());
  sync_status->SetBoolean("syncSystemEnabled", (service != NULL));
  sync_status->SetBoolean("setupCompleted",
                          service && service->IsFirstSetupComplete());
  sync_status->SetBoolean("setupInProgress",
      service && !service->IsManaged() && service->IsFirstSetupInProgress());

  base::string16 status_label;
  base::string16 link_label;
  bool status_has_error =
      sync_ui_util::GetStatusLabels(profile, service, *signin,
                                    sync_ui_util::WITH_HTML, &status_label,
                                    &link_label) == sync_ui_util::SYNC_ERROR;
  sync_status->SetString("statusText", status_label);
  sync_status->SetString("actionLinkText", link_label);
  sync_status->SetBoolean("hasError", status_has_error);

  sync_status->SetBoolean("managed", service && service->IsManaged());
  sync_status->SetBoolean("signedIn", signin->IsAuthenticated());
  sync_status->SetBoolean("hasUnrecoverableError",
                          service && service->HasUnrecoverableError());

  return sync_status;
}

void BrowserOptionsHandler::HandleSelectDownloadLocation(
    const base::ListValue* args) {
  PrefService* pref_service = Profile::FromWebUI(web_ui())->GetPrefs();
  select_folder_dialog_ = ui::SelectFileDialog::Create(
      this, new ChromeSelectFilePolicy(web_ui()->GetWebContents()));
  ui::SelectFileDialog::FileTypeInfo info;
  info.allowed_paths = ui::SelectFileDialog::FileTypeInfo::NATIVE_OR_DRIVE_PATH;
  select_folder_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_FOLDER,
      l10n_util::GetStringUTF16(IDS_OPTIONS_DOWNLOADLOCATION_BROWSE_TITLE),
      pref_service->GetFilePath(prefs::kDownloadDefaultDirectory),
      &info,
      0,
      base::FilePath::StringType(),
      web_ui()->GetWebContents()->GetTopLevelNativeWindow(),
      NULL);
}

void BrowserOptionsHandler::FileSelected(const base::FilePath& path, int index,
                                         void* params) {
  content::RecordAction(UserMetricsAction("Options_SetDownloadDirectory"));
  PrefService* pref_service = Profile::FromWebUI(web_ui())->GetPrefs();
  pref_service->SetFilePath(prefs::kDownloadDefaultDirectory, path);
  pref_service->SetFilePath(prefs::kSaveFileDefaultDirectory, path);
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

void BrowserOptionsHandler::OnUserImagePolicyChanged(
    const base::Value* previous_policy,
    const base::Value* current_policy) {
  const bool had_policy = previous_policy;
  const bool has_policy = current_policy;
  if (had_policy != has_policy)
    OnAccountPictureManagedChanged(has_policy);
}

void BrowserOptionsHandler::OnWallpaperPolicyChanged(
    const base::Value* previous_policy,
    const base::Value* current_policy) {
  const bool had_policy = previous_policy;
  const bool has_policy = current_policy;
  if (had_policy != has_policy)
    OnWallpaperManagedChanged(has_policy);
}

void BrowserOptionsHandler::OnPowerwashDialogShow(
     const base::ListValue* args) {
  UMA_HISTOGRAM_ENUMERATION(
      "Reset.ChromeOS.PowerwashDialogShown",
      chromeos::reset::DIALOG_FROM_OPTIONS,
      chromeos::reset::DIALOG_VIEW_TYPE_SIZE);
}

void BrowserOptionsHandler::OnConsumerManagementStatusChanged() {
  const std::string& status = g_browser_process->platform_part()->
      browser_policy_connector_chromeos()->GetConsumerManagementService()->
          GetStatusString();
  web_ui()->CallJavascriptFunction(
      "BrowserOptions.setConsumerManagementStatus", base::StringValue(status));
}

#endif  // defined(OS_CHROMEOS)

void BrowserOptionsHandler::UpdateSyncState() {
  web_ui()->CallJavascriptFunction("BrowserOptions.updateSyncState",
                                   *GetSyncStateDictionary());

  // A change in sign-in state also affects how hotwording and audio history are
  // displayed. Hide all hotwording and re-display properly.
  web_ui()->CallJavascriptFunction(
      "BrowserOptions.setAllHotwordSectionsVisible",
      base::FundamentalValue(false));
  HandleRequestHotwordAvailable(nullptr);
}

void BrowserOptionsHandler::OnSigninAllowedPrefChange() {
  UpdateSyncState();
}

void BrowserOptionsHandler::HandleAutoOpenButton(const base::ListValue* args) {
  content::RecordAction(UserMetricsAction("Options_ResetAutoOpenFiles"));
  DownloadManager* manager = BrowserContext::GetDownloadManager(
      web_ui()->GetWebContents()->GetBrowserContext());
  if (manager)
    DownloadPrefs::FromDownloadManager(manager)->ResetAutoOpen();
}

void BrowserOptionsHandler::HandleDefaultFontSize(const base::ListValue* args) {
  int font_size;
  if (ExtractIntegerValue(args, &font_size)) {
    if (font_size > 0) {
      PrefService* pref_service = Profile::FromWebUI(web_ui())->GetPrefs();
      pref_service->SetInteger(prefs::kWebKitDefaultFontSize, font_size);
      SetupFontSizeSelector();
    }
  }
}

void BrowserOptionsHandler::HandleDefaultZoomFactor(
    const base::ListValue* args) {
  double zoom_factor;
  if (ExtractDoubleValue(args, &zoom_factor)) {
    Profile::FromWebUI(web_ui())->GetZoomLevelPrefs()->SetDefaultZoomLevelPref(
        content::ZoomFactorToZoomLevel(zoom_factor));
  }
}

void BrowserOptionsHandler::HandleRestartBrowser(const base::ListValue* args) {
#if defined(OS_WIN)
  // On Windows Breakpad will upload crash reports if the breakpad pipe name
  // environment variable is defined. So we undefine this environment variable
  // before restarting, as the restarted processes will inherit their
  // environment variables from ours, thus suppressing crash uploads.
  if (!ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled()) {
    HMODULE exe_module = GetModuleHandle(chrome::kBrowserProcessExecutableName);
    if (exe_module) {
      typedef void (__cdecl *ClearBreakpadPipeEnvVar)();
      ClearBreakpadPipeEnvVar clear = reinterpret_cast<ClearBreakpadPipeEnvVar>(
          GetProcAddress(exe_module, "ClearBreakpadPipeEnvironmentVariable"));
      if (clear)
        clear();
    }
  }
#endif

  chrome::AttemptRestart();
}

void BrowserOptionsHandler::HandleRequestProfilesInfo(
    const base::ListValue* args) {
  SendProfilesInfo();
}

#if !defined(OS_CHROMEOS)
void BrowserOptionsHandler::ShowNetworkProxySettings(
    const base::ListValue* args) {
  content::RecordAction(UserMetricsAction("Options_ShowProxySettings"));
  AdvancedOptionsUtilities::ShowNetworkProxySettings(
      web_ui()->GetWebContents());
}
#endif

#if defined(OS_WIN) || defined(OS_MACOSX)
void BrowserOptionsHandler::ShowManageSSLCertificates(
    const base::ListValue* args) {
  content::RecordAction(UserMetricsAction("Options_ManageSSLCertificates"));
  AdvancedOptionsUtilities::ShowManageSSLCertificates(
      web_ui()->GetWebContents());
}
#endif

#if defined(ENABLE_SERVICE_DISCOVERY)

void BrowserOptionsHandler::ShowCloudPrintDevicesPage(
    const base::ListValue* args) {
  content::RecordAction(UserMetricsAction("Options_CloudPrintDevicesPage"));
  // Navigate in current tab to devices page.
  OpenURLParams params(
      GURL(chrome::kChromeUIDevicesURL), Referrer(),
      CURRENT_TAB, ui::PAGE_TRANSITION_LINK, false);
  web_ui()->GetWebContents()->OpenURL(params);
}

#endif

void BrowserOptionsHandler::SetHotwordAudioHistorySectionVisible(
    const base::string16& audio_history_state,
    bool success, bool logging_enabled) {
  bool visible = logging_enabled && success;
  web_ui()->CallJavascriptFunction(
      "BrowserOptions.setAudioHistorySectionVisible",
      base::FundamentalValue(visible),
      base::StringValue(audio_history_state));
}

void BrowserOptionsHandler::HandleRequestGoogleNowAvailable(
    const base::ListValue* args) {
  bool is_search_provider_google = false;
  if (template_url_service_ && template_url_service_->loaded()) {
    const TemplateURL* default_url =
        template_url_service_->GetDefaultSearchProvider();
    if (default_url && default_url->HasGoogleBaseURLs(
            template_url_service_->search_terms_data())) {
      is_search_provider_google = true;
    }
  }

  std::string group = base::FieldTrialList::FindFullName("GoogleNowExtension");
  bool has_field_trial = !group.empty() && group != "Disabled";

  bool should_show = is_search_provider_google && has_field_trial;
  web_ui()->CallJavascriptFunction(
      "BrowserOptions.setNowSectionVisible",
      base::FundamentalValue(should_show));
}

void BrowserOptionsHandler::HandleRequestHotwordAvailable(
    const base::ListValue* args) {
  Profile* profile = Profile::FromWebUI(web_ui());

  bool is_search_provider_google = false;
  // The check for default search provider is only valid if the
  // |template_url_service_| has loaded already.
  if (template_url_service_ && template_url_service_->loaded()) {
    const TemplateURL* default_url =
        template_url_service_->GetDefaultSearchProvider();
    if (default_url && default_url->HasGoogleBaseURLs(
            template_url_service_->search_terms_data())) {
      is_search_provider_google = true;
    } else {
      // If the user has chosen a default search provide other than Google, turn
      // off hotwording since other providers don't provide that functionality.
      HotwordService* hotword_service =
        HotwordServiceFactory::GetForProfile(profile);
      if (hotword_service)
        hotword_service->DisableHotwordPreferences();
    }
  }

  // |is_search_provider_google| may be false because |template_url_service_|
  // does not exist yet or because the user selected a different search
  // provider. In either case it does not make sense to show the hotwording
  // options.
  if (!is_search_provider_google) {
    web_ui()->CallJavascriptFunction(
        "BrowserOptions.setAllHotwordSectionsVisible",
        base::FundamentalValue(false));
    return;
  }

  // Don't need to check the field trial here since |IsHotwordAllowed| also
  // checks it.
  if (HotwordServiceFactory::IsHotwordAllowed(profile)) {
    // Update the current error value.
    HotwordServiceFactory::IsServiceAvailable(profile);
    int error = HotwordServiceFactory::GetCurrentError(profile);

    std::string function_name;
    bool always_on = false;
    SigninManagerBase* signin = SigninManagerFactory::GetForProfile(profile);
    bool authenticated = signin && signin->IsAuthenticated();
    if (HotwordServiceFactory::IsAlwaysOnAvailable() && authenticated) {
      function_name = "BrowserOptions.showHotwordAlwaysOnSection";
      always_on = true;
      // Show the retrain link if always-on is enabled.
      if (profile->GetPrefs()->GetBoolean(
              prefs::kHotwordAlwaysOnSearchEnabled)) {
        web_ui()->CallJavascriptFunction(
            "BrowserOptions.setHotwordRetrainLinkVisible",
            base::FundamentalValue(true));
      }
    } else {
      function_name = "BrowserOptions.showHotwordNoDspSection";
    }

    // Audio history should be displayed if it's enabled regardless of the
    // hotword error state if the user is signed in. If the user is not signed
    // in, audio history is meaningless. This is only displayed if always-on
    // hotwording is available.
    if (authenticated && always_on) {
      std::string user_display_name =
          signin->GetAuthenticatedAccountInfo().email;
      DCHECK(!user_display_name.empty());
      base::string16 audio_history_state =
          l10n_util::GetStringFUTF16(IDS_HOTWORD_AUDIO_HISTORY_ENABLED,
          base::ASCIIToUTF16(user_display_name));
      HotwordService* hotword_service =
          HotwordServiceFactory::GetForProfile(profile);
      if (hotword_service) {
        hotword_service->GetAudioHistoryHandler()->GetAudioHistoryEnabled(
            base::Bind(
                &BrowserOptionsHandler::SetHotwordAudioHistorySectionVisible,
                weak_ptr_factory_.GetWeakPtr(),
                audio_history_state));
      }
    }

    if (!error) {
      web_ui()->CallJavascriptFunction(function_name);
    } else {
      base::string16 hotword_help_url =
          base::ASCIIToUTF16(chrome::kHotwordLearnMoreURL);
      base::StringValue error_message(l10n_util::GetStringUTF16(error));
      if (error == IDS_HOTWORD_GENERIC_ERROR_MESSAGE) {
        error_message = base::StringValue(
            l10n_util::GetStringFUTF16(error, hotword_help_url));
      }
      web_ui()->CallJavascriptFunction(function_name, error_message);
    }
  }
}

void BrowserOptionsHandler::HandleLaunchHotwordAudioVerificationApp(
    const base::ListValue* args) {
  Profile* profile = Profile::FromWebUI(web_ui());

  bool retrain = false;
  bool success = args->GetBoolean(0, &retrain);
  DCHECK(success);
  HotwordService::LaunchMode launch_mode =
      HotwordService::HOTWORD_AND_AUDIO_HISTORY;

  if (retrain) {
    DCHECK(profile->GetPrefs()->GetBoolean(
        prefs::kHotwordAlwaysOnSearchEnabled));
    DCHECK(profile->GetPrefs()->GetBoolean(
        prefs::kHotwordAudioLoggingEnabled));

    launch_mode = HotwordService::RETRAIN;
  } else if (profile->GetPrefs()->GetBoolean(
      prefs::kHotwordAudioLoggingEnabled)) {
    DCHECK(!profile->GetPrefs()->GetBoolean(
        prefs::kHotwordAlwaysOnSearchEnabled));
    launch_mode = HotwordService::HOTWORD_ONLY;
  } else {
    DCHECK(!profile->GetPrefs()->GetBoolean(
        prefs::kHotwordAlwaysOnSearchEnabled));
  }

  HotwordService* hotword_service =
      HotwordServiceFactory::GetForProfile(profile);
  if (!hotword_service)
    return;

  hotword_service->OptIntoHotwording(launch_mode);
}

void BrowserOptionsHandler::HandleLaunchEasyUnlockSetup(
    const base::ListValue* args) {
  EasyUnlockService::Get(Profile::FromWebUI(web_ui()))->LaunchSetup();
}

void BrowserOptionsHandler::HandleRefreshExtensionControlIndicators(
    const base::ListValue* args) {
  SetupExtensionControlledIndicators();
}

#if defined(OS_CHROMEOS)
void BrowserOptionsHandler::HandleOpenWallpaperManager(
    const base::ListValue* args) {
  ash::Shell::GetInstance()->user_wallpaper_delegate()->OpenSetWallpaperPage();
}

void BrowserOptionsHandler::VirtualKeyboardChangeCallback(
    const base::ListValue* args) {
  bool enabled = false;
  args->GetBoolean(0, &enabled);

  chromeos::accessibility::EnableVirtualKeyboard(enabled);
}

void BrowserOptionsHandler::PerformFactoryResetRestart(
    const base::ListValue* args) {
  if (!enable_factory_reset_)
    return;

  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
  prefs->CommitPendingWrite();

  // Perform sign out. Current chrome process will then terminate, new one will
  // be launched (as if it was a restart).
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->RequestRestart();
}

void BrowserOptionsHandler::SetupAccessibilityFeatures() {
  PrefService* pref_service = g_browser_process->local_state();
  base::FundamentalValue virtual_keyboard_enabled(
      pref_service->GetBoolean(prefs::kAccessibilityVirtualKeyboardEnabled));
  web_ui()->CallJavascriptFunction(
      "BrowserOptions.setVirtualKeyboardCheckboxState",
      virtual_keyboard_enabled);
}
#endif

void BrowserOptionsHandler::SetupMetricsReportingSettingVisibility() {
#if defined(GOOGLE_CHROME_BUILD)
  // Don't show the reporting setting if we are in the guest mode.
  if (Profile::FromWebUI(web_ui())->IsGuestSession()) {
    base::FundamentalValue visible(false);
    web_ui()->CallJavascriptFunction(
        "BrowserOptions.setMetricsReportingSettingVisibility", visible);
  }
#endif
}

void BrowserOptionsHandler::SetupNetworkPredictionControl() {
  PrefService* pref_service = Profile::FromWebUI(web_ui())->GetPrefs();

  base::DictionaryValue dict;
  dict.SetInteger("value",
                  pref_service->GetInteger(prefs::kNetworkPredictionOptions));
  dict.SetBoolean("disabled",
                  !pref_service->IsUserModifiablePreference(
                      prefs::kNetworkPredictionOptions));

  web_ui()->CallJavascriptFunction("BrowserOptions.setNetworkPredictionValue",
                                   dict);
}

void BrowserOptionsHandler::SetupFontSizeSelector() {
  PrefService* pref_service = Profile::FromWebUI(web_ui())->GetPrefs();
  const PrefService::Preference* default_font_size =
      pref_service->FindPreference(prefs::kWebKitDefaultFontSize);
  const PrefService::Preference* default_fixed_font_size =
      pref_service->FindPreference(prefs::kWebKitDefaultFixedFontSize);

  base::DictionaryValue dict;
  dict.SetInteger("value",
                  pref_service->GetInteger(prefs::kWebKitDefaultFontSize));

  // The font size control displays the value of the default font size, but
  // setting it alters both the default font size and the default fixed font
  // size. So it must be disabled when either of those prefs is not user
  // modifiable.
  dict.SetBoolean("disabled",
      !default_font_size->IsUserModifiable() ||
      !default_fixed_font_size->IsUserModifiable());

  // This is a poor man's version of CoreOptionsHandler::CreateValueForPref,
  // adapted to consider two prefs. It may be better to refactor
  // CreateValueForPref so it can be called from here.
  if (default_font_size->IsManaged() || default_fixed_font_size->IsManaged()) {
      dict.SetString("controlledBy", "policy");
  } else if (default_font_size->IsExtensionControlled() ||
             default_fixed_font_size->IsExtensionControlled()) {
      dict.SetString("controlledBy", "extension");
  }

  web_ui()->CallJavascriptFunction("BrowserOptions.setFontSize", dict);
}

void BrowserOptionsHandler::SetupPageZoomSelector() {
  double default_zoom_level =
      content::HostZoomMap::GetDefaultForBrowserContext(
          Profile::FromWebUI(web_ui()))->GetDefaultZoomLevel();
  double default_zoom_factor =
      content::ZoomLevelToZoomFactor(default_zoom_level);

  // Generate a vector of zoom factors from an array of known presets along with
  // the default factor added if necessary.
  std::vector<double> zoom_factors =
      ui_zoom::PageZoom::PresetZoomFactors(default_zoom_factor);

  // Iterate through the zoom factors and and build the contents of the
  // selector that will be sent to the javascript handler.
  // Each item in the list has the following parameters:
  // 1. Title (string).
  // 2. Value (double).
  // 3. Is selected? (bool).
  base::ListValue zoom_factors_value;
  for (std::vector<double>::const_iterator i = zoom_factors.begin();
       i != zoom_factors.end(); ++i) {
    base::ListValue* option = new base::ListValue();
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
  // Disable the button if proxy settings are managed by a sysadmin, overridden
  // by an extension, or the browser is running in Windows Ash (on Windows the
  // proxy settings dialog will open on the Windows desktop and be invisible
  // to a user in Ash).
  bool is_win_ash = false;
#if defined(OS_WIN)
  chrome::HostDesktopType desktop_type = helper::GetDesktopType(web_ui());
  is_win_ash = (desktop_type == chrome::HOST_DESKTOP_TYPE_ASH);
#endif
  PrefService* pref_service = Profile::FromWebUI(web_ui())->GetPrefs();
  const PrefService::Preference* proxy_config =
      pref_service->FindPreference(proxy_config::prefs::kProxy);
  bool is_extension_controlled = (proxy_config &&
                                  proxy_config->IsExtensionControlled());

  base::FundamentalValue disabled(is_win_ash || (proxy_config &&
                                  !proxy_config->IsUserModifiable()));
  base::FundamentalValue extension_controlled(is_extension_controlled);
  web_ui()->CallJavascriptFunction("BrowserOptions.setupProxySettingsButton",
                                   disabled, extension_controlled);

#if defined(OS_WIN)
  SetupExtensionControlledIndicators();
#endif  // defined(OS_WIN)

#endif  // !defined(OS_CHROMEOS)
}

void BrowserOptionsHandler::SetupManageCertificatesSection() {
#if defined(OS_WIN)
  // Disable the button if the settings page is displayed in Windows Ash,
  // otherwise the proxy settings dialog will open on the Windows desktop and
  // be invisible to a user in Ash.
  if (helper::GetDesktopType(web_ui()) == chrome::HOST_DESKTOP_TYPE_ASH) {
    base::FundamentalValue enabled(false);
    web_ui()->CallJavascriptFunction("BrowserOptions.enableCertificateButton",
                                     enabled);
  }
#endif  // defined(OS_WIN)
}

void BrowserOptionsHandler::SetupManagingSupervisedUsers() {
  bool has_users = !Profile::FromWebUI(web_ui())->
      GetPrefs()->GetDictionary(prefs::kSupervisedUsers)->empty();
  base::FundamentalValue has_users_value(has_users);
  web_ui()->CallJavascriptFunction(
      "BrowserOptions.updateManagesSupervisedUsers",
      has_users_value);
}

void BrowserOptionsHandler::SetupEasyUnlock() {
  base::FundamentalValue is_enabled(
      EasyUnlockService::Get(Profile::FromWebUI(web_ui()))->IsEnabled());
  web_ui()->CallJavascriptFunction(
      "BrowserOptions.updateEasyUnlock",
      is_enabled);
}

void BrowserOptionsHandler::SetupExtensionControlledIndicators() {
#if defined(OS_WIN)
  base::DictionaryValue extension_controlled;

  // Check if an extension is overriding the Search Engine.
  const extensions::Extension* extension =
      extensions::GetExtensionOverridingSearchEngine(
          Profile::FromWebUI(web_ui()));
  AppendExtensionData("searchEngine", extension, &extension_controlled);

  // Check if an extension is overriding the Home page.
  extension = extensions::GetExtensionOverridingHomepage(
      Profile::FromWebUI(web_ui()));
  AppendExtensionData("homePage", extension, &extension_controlled);

  // Check if an extension is overriding the Startup pages.
  extension = extensions::GetExtensionOverridingStartupPages(
      Profile::FromWebUI(web_ui()));
  AppendExtensionData("startUpPage", extension, &extension_controlled);

  // Check if an extension is overriding the NTP page.
  GURL ntp_url(chrome::kChromeUINewTabURL);
  bool ignored_param;
  extension = NULL;
  content::BrowserURLHandler::GetInstance()->RewriteURLIfNecessary(
      &ntp_url,
      web_ui()->GetWebContents()->GetBrowserContext(),
      &ignored_param);
  if (ntp_url.SchemeIs("chrome-extension")) {
    using extensions::ExtensionRegistry;
    ExtensionRegistry* registry = ExtensionRegistry::Get(
        Profile::FromWebUI(web_ui()));
    extension = registry->GetExtensionById(ntp_url.host(),
                                           ExtensionRegistry::ENABLED);
  }
  AppendExtensionData("newTabPage", extension, &extension_controlled);

  // Check if an extension is overwriting the proxy setting.
  extension = extensions::GetExtensionOverridingProxy(
      Profile::FromWebUI(web_ui()));
  AppendExtensionData("proxy", extension, &extension_controlled);

  web_ui()->CallJavascriptFunction("BrowserOptions.toggleExtensionIndicators",
                                   extension_controlled);
#endif  // defined(OS_WIN)
}

void BrowserOptionsHandler::SetupMetricsReportingCheckbox() {
// As the metrics and crash reporting checkbox only exists for official builds
// it doesn't need to be set up for non-official builds.
#if defined(GOOGLE_CHROME_BUILD)
  bool checked =
      ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled();
  bool policy_managed = IsMetricsReportingPolicyManaged();
  bool owner_managed = false;
#if defined(OS_CHROMEOS)
  owner_managed = !IsDeviceOwnerProfile();
#endif  // defined(OS_CHROMEOS)
  SetMetricsReportingCheckbox(checked, policy_managed, owner_managed);
#endif  // defined(GOOGLE_CHROME_BUILD)
}

void BrowserOptionsHandler::HandleMetricsReportingChange(
    const base::ListValue* args) {
  bool enable;
  if (!args->GetBoolean(0, &enable))
    return;
  // Decline the change if current user shouldn't be able to change metrics
  // reporting.
  if (!IsDeviceOwnerProfile() || IsMetricsReportingPolicyManaged()) {
    NotifyUIOfMetricsReportingChange(
        ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled());
    return;
  }

// For Chrome OS updating device settings will notify an observer to update
// metrics pref, however we still need to call |InitiateMetricsReportingChange|
// with a proper callback so that UI gets updated in case of failure to update
// the metrics pref.
// TODO(gayane): Don't call |InitiateMetricsReportingChange| twice so that
// metrics service pref changes only as a result of device settings change for
// Chrome OS .crbug.com/552550.
#if defined(OS_CHROMEOS)
  chromeos::CrosSettings::Get()->SetBoolean(chromeos::kStatsReportingPref,
                                            enable);
#endif  // defined(OS_CHROMEOS)
  InitiateMetricsReportingChange(
      enable,
      base::Bind(&BrowserOptionsHandler::NotifyUIOfMetricsReportingChange,
                 weak_ptr_factory_.GetWeakPtr()));
}

void BrowserOptionsHandler::NotifyUIOfMetricsReportingChange(bool enabled) {
  SetMetricsReportingCheckbox(enabled, IsMetricsReportingPolicyManaged(),
                              !IsDeviceOwnerProfile());
}

void BrowserOptionsHandler::SetMetricsReportingCheckbox(bool checked,
                                                        bool policy_managed,
                                                        bool owner_managed) {
  web_ui()->CallJavascriptFunction(
      "BrowserOptions.setMetricsReportingCheckboxState",
      base::FundamentalValue(checked), base::FundamentalValue(policy_managed),
      base::FundamentalValue(owner_managed));
}

void BrowserOptionsHandler::OnPolicyUpdated(const policy::PolicyNamespace& ns,
                                            const policy::PolicyMap& previous,
                                            const policy::PolicyMap& current) {
  std::set<std::string> different_keys;
  current.GetDifferingKeys(previous, &different_keys);
  if (ContainsKey(different_keys, policy::key::kMetricsReportingEnabled))
    SetupMetricsReportingCheckbox();
}

bool BrowserOptionsHandler::IsDeviceOwnerProfile() {
#if defined(OS_CHROMEOS)
  return chromeos::ProfileHelper::IsOwnerProfile(Profile::FromWebUI(web_ui()));
#else
  return true;
#endif
}

}  // namespace options
