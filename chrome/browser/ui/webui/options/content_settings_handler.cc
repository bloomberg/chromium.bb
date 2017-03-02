// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/content_settings_handler.h"

#include <stddef.h>

#include <algorithm>
#include <map>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/i18n/number_formatting.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/web_site_settings_uma_util.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "chrome/browser/notifications/desktop_notification_profile_util.h"
#include "chrome/browser/permissions/chooser_context_base.h"
#include "chrome/browser/permissions/permission_uma_util.h"
#include "chrome/browser/permissions/permission_util.h"
#include "chrome/browser/plugins/plugin_utils.h"
#include "chrome/browser/plugins/plugins_field_trial.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/webui/site_settings_helper.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/locale_settings.h"
#include "components/content_settings/core/browser/content_settings_details.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/browser/website_settings_info.h"
#include "components/content_settings/core/browser/website_settings_registry.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/google/core/browser/google_util.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/page_zoom.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "ppapi/features/features.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "components/user_manager/user_manager.h"
#endif

using base::UserMetricsAction;
using content_settings::ContentSettingToString;
using content_settings::ContentSettingFromString;
using extensions::APIPermission;

namespace options {

namespace {

struct ContentSettingWithExceptions {
  ContentSettingWithExceptions(bool otr, UserMetricsAction action)
      : has_otr_exceptions(otr), uma(action) {}
  bool has_otr_exceptions;
  UserMetricsAction uma;
};

// The AppFilter is used in AddExceptionsGrantedByHostedApps() to choose
// extensions which should have their extent displayed.
typedef bool (*AppFilter)(const extensions::Extension& app,
                          content::BrowserContext* profile);

const char kAppName[] = "appName";
const char kAppId[] = "appId";
const char kZoom[] = "zoom";

// A pseudo content type. We use it to display data like a content setting even
// though it is not a real content setting.
const char kZoomContentType[] = "zoomlevels";

// Maps from a content settings type to a content setting with exceptions
// struct.
typedef std::map<ContentSettingsType, ContentSettingWithExceptions>
    ExceptionsInfoMap;

const ExceptionsInfoMap& GetExceptionsInfoMap() {
  CR_DEFINE_STATIC_LOCAL(ExceptionsInfoMap, exceptions_info_map, ());
  if (exceptions_info_map.empty()) {
    // With OTR exceptions.
    exceptions_info_map.insert(std::make_pair(
        CONTENT_SETTINGS_TYPE_COOKIES,
        ContentSettingWithExceptions(
            true, UserMetricsAction("Options_DefaultCookieSettingChanged"))));
    exceptions_info_map.insert(std::make_pair(
        CONTENT_SETTINGS_TYPE_IMAGES,
        ContentSettingWithExceptions(
            true, UserMetricsAction("Options_DefaultImagesSettingChanged"))));
    exceptions_info_map.insert(std::make_pair(
        CONTENT_SETTINGS_TYPE_JAVASCRIPT,
        ContentSettingWithExceptions(
            true,
            UserMetricsAction("Options_DefaultJavaScriptSettingChanged"))));
    exceptions_info_map.insert(std::make_pair(
        CONTENT_SETTINGS_TYPE_PLUGINS,
        ContentSettingWithExceptions(
            true, UserMetricsAction("Options_DefaultPluginsSettingChanged"))));
    exceptions_info_map.insert(std::make_pair(
        CONTENT_SETTINGS_TYPE_POPUPS,
        ContentSettingWithExceptions(
            true, UserMetricsAction("Options_DefaultPopupsSettingChanged"))));
    exceptions_info_map.insert(std::make_pair(
        CONTENT_SETTINGS_TYPE_PPAPI_BROKER,
        ContentSettingWithExceptions(
            true,
            UserMetricsAction("Options_DefaultPPAPIBrokerSettingChanged"))));
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
    exceptions_info_map.insert(std::make_pair(
        CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER,
        ContentSettingWithExceptions(
            true,
            UserMetricsAction(
                "Options_DefaultProtectedMediaIdentifierSettingChanged"))));
#endif

    // Without OTR exceptions.
    exceptions_info_map.insert(std::make_pair(
        CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
        ContentSettingWithExceptions(
            false,
            UserMetricsAction("Options_DefaultNotificationsSettingChanged"))));
    exceptions_info_map.insert(std::make_pair(
        CONTENT_SETTINGS_TYPE_GEOLOCATION,
        ContentSettingWithExceptions(
            false,
            UserMetricsAction("Options_DefaultGeolocationSettingChanged"))));
    exceptions_info_map.insert(std::make_pair(
        CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC,
        ContentSettingWithExceptions(
            false,
            UserMetricsAction("Options_DefaultMediaStreamMicSettingChanged"))));
    exceptions_info_map.insert(std::make_pair(
        CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA,
        ContentSettingWithExceptions(
            false, UserMetricsAction(
                       "Options_DefaultMediaStreamCameraSettingChanged"))));
    exceptions_info_map.insert(std::make_pair(
        CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS,
        ContentSettingWithExceptions(
            false, UserMetricsAction(
                       "Options_DefaultMultipleAutomaticDLSettingChange"))));
    exceptions_info_map.insert(std::make_pair(
        CONTENT_SETTINGS_TYPE_MIDI_SYSEX,
        ContentSettingWithExceptions(
            false,
            UserMetricsAction("Options_DefaultMIDISysExSettingChanged"))));
    exceptions_info_map.insert(std::make_pair(
        CONTENT_SETTINGS_TYPE_BACKGROUND_SYNC,
        ContentSettingWithExceptions(
            false,
            UserMetricsAction("Options_DefaultBackgroundSyncSettingChanged"))));
  }

  return exceptions_info_map;
}

content::BrowserContext* GetBrowserContext(content::WebUI* web_ui) {
  return web_ui->GetWebContents()->GetBrowserContext();
}

// Create a DictionaryValue* that will act as a data source for a single row
// in the Geolocation exceptions table.
std::unique_ptr<base::DictionaryValue> GetGeolocationExceptionForPage(
    const ContentSettingsPattern& origin,
    const ContentSettingsPattern& embedding_origin,
    ContentSetting setting) {
  base::DictionaryValue* exception = new base::DictionaryValue();

  std::string setting_string =
      content_settings::ContentSettingToString(setting);
  DCHECK(!setting_string.empty());

  exception->SetString(site_settings::kSetting, setting_string);
  exception->SetString(site_settings::kOrigin, origin.ToString());
  exception->SetString(
      site_settings::kEmbeddingOrigin, embedding_origin.ToString());
  return base::WrapUnique(exception);
}

// Create a DictionaryValue* that will act as a data source for a single row
// in the desktop notifications exceptions table.
std::unique_ptr<base::DictionaryValue> GetNotificationExceptionForPage(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSetting setting,
    const std::string& provider_name) {
  std::string embedding_origin;
  if (secondary_pattern != ContentSettingsPattern::Wildcard())
    embedding_origin = secondary_pattern.ToString();

  base::DictionaryValue* exception = new base::DictionaryValue();

  std::string setting_string =
      content_settings::ContentSettingToString(setting);
  DCHECK(!setting_string.empty());

  exception->SetString(site_settings::kSetting, setting_string);
  exception->SetString(site_settings::kOrigin, primary_pattern.ToString());
  exception->SetString(site_settings::kEmbeddingOrigin, embedding_origin);
  exception->SetString(site_settings::kSource, provider_name);
  return base::WrapUnique(exception);
}

// Returns true whenever the |extension| is hosted and has |permission|.
// Must have the AppFilter signature.
template <APIPermission::ID permission>
bool HostedAppHasPermission(const extensions::Extension& extension,
                            content::BrowserContext* /* context */) {
  return extension.is_hosted_app() &&
         extension.permissions_data()->HasAPIPermission(permission);
}

// Add an "Allow"-entry to the list of |exceptions| for a |url_pattern| from
// the web extent of a hosted |app|.
void AddExceptionForHostedApp(const std::string& url_pattern,
    const extensions::Extension& app, base::ListValue* exceptions) {
  std::unique_ptr<base::DictionaryValue> exception(new base::DictionaryValue());

  std::string setting_string =
      content_settings::ContentSettingToString(CONTENT_SETTING_ALLOW);
  DCHECK(!setting_string.empty());

  exception->SetString(site_settings::kSetting, setting_string);
  exception->SetString(site_settings::kOrigin, url_pattern);
  exception->SetString(site_settings::kEmbeddingOrigin, url_pattern);
  exception->SetString(site_settings::kSource, "HostedApp");
  exception->SetString(kAppName, app.name());
  exception->SetString(kAppId, app.id());
  exceptions->Append(std::move(exception));
}

// Asks the |profile| for hosted apps which have the |permission| set, and
// adds their web extent and launch URL to the |exceptions| list.
void AddExceptionsGrantedByHostedApps(content::BrowserContext* context,
                                      AppFilter app_filter,
                                      base::ListValue* exceptions) {
  const extensions::ExtensionSet& extensions =
      extensions::ExtensionRegistry::Get(context)->enabled_extensions();
  for (extensions::ExtensionSet::const_iterator extension = extensions.begin();
       extension != extensions.end(); ++extension) {
    if (!app_filter(*extension->get(), context))
      continue;

    extensions::URLPatternSet web_extent = (*extension)->web_extent();
    // Add patterns from web extent.
    for (extensions::URLPatternSet::const_iterator pattern = web_extent.begin();
         pattern != web_extent.end(); ++pattern) {
      std::string url_pattern = pattern->GetAsString();
      AddExceptionForHostedApp(url_pattern, *extension->get(), exceptions);
    }
    // Retrieve the launch URL.
    GURL launch_url =
        extensions::AppLaunchInfo::GetLaunchWebURL(extension->get());
    // Skip adding the launch URL if it is part of the web extent.
    if (web_extent.MatchesURL(launch_url))
      continue;
    AddExceptionForHostedApp(launch_url.spec(), *extension->get(), exceptions);
  }
}

}  // namespace

ContentSettingsHandler::MediaSettingsInfo::MediaSettingsInfo() {
}

ContentSettingsHandler::MediaSettingsInfo::~MediaSettingsInfo() {
}

ContentSettingsHandler::MediaSettingsInfo::ForFlash::ForFlash()
    : default_setting(CONTENT_SETTING_DEFAULT),
      initialized(false),
      last_refresh_request_id(0) {
}

ContentSettingsHandler::MediaSettingsInfo::ForFlash::~ForFlash() {
}

ContentSettingsHandler::MediaSettingsInfo::ForFlash&
    ContentSettingsHandler::MediaSettingsInfo::forFlash() {
  return flash_settings_;
}

ContentSettingsHandler::MediaSettingsInfo::ForOneType&
    ContentSettingsHandler::MediaSettingsInfo::forType(
        ContentSettingsType type) {
  if (type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC)
    return mic_settings_;
  else if (type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA)
    return camera_settings_;

  NOTREACHED();
  return mic_settings_;
}

ContentSettingsHandler::MediaSettingsInfo::ForOneType::ForOneType()
    : show_flash_default_link(false),
      show_flash_exceptions_link(false),
      default_setting(CONTENT_SETTING_DEFAULT),
      policy_disable(false),
      default_setting_initialized(false),
      exceptions_initialized(false) {
}

ContentSettingsHandler::MediaSettingsInfo::ForOneType::~ForOneType() {
}

ContentSettingsHandler::ContentSettingsHandler() : observer_(this) {
}

ContentSettingsHandler::~ContentSettingsHandler() {
}

void ContentSettingsHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  static OptionsStringResource resources[] = {
    {"allowException", IDS_EXCEPTIONS_ALLOW_BUTTON},
    {"blockException", IDS_EXCEPTIONS_BLOCK_BUTTON},
    {"sessionException", IDS_EXCEPTIONS_SESSION_ONLY_BUTTON},
    {"askException", IDS_EXCEPTIONS_ASK_BUTTON},
    {"otrExceptionsExplanation", IDS_EXCEPTIONS_OTR_LABEL},
    {"addNewExceptionInstructions", IDS_EXCEPTIONS_ADD_NEW_INSTRUCTIONS},
    {"manageExceptions", IDS_EXCEPTIONS_MANAGE},
    {"manageHandlers", IDS_HANDLERS_MANAGE},
    {"exceptionPatternHeader", IDS_EXCEPTIONS_PATTERN_HEADER},
    {"exceptionBehaviorHeader", IDS_EXCEPTIONS_ACTION_HEADER},
    {"exceptionUsbDeviceHeader", IDS_EXCEPTIONS_USB_DEVICE_HEADER},
    {"exceptionZoomHeader", IDS_EXCEPTIONS_ZOOM_HEADER},
    {"embeddedOnHost", IDS_EXCEPTIONS_GEOLOCATION_EMBEDDED_ON_HOST},
    // Cookies filter.
    {"cookiesTabLabel", IDS_COOKIES_TAB_LABEL},
    {"cookiesHeader", IDS_COOKIES_HEADER},
    {"cookiesAllow", IDS_COOKIES_ALLOW_RADIO},
    {"cookiesBlock", IDS_COOKIES_BLOCK_RADIO},
    {"cookiesSessionOnly", IDS_COOKIES_SESSION_ONLY_RADIO},
    {"cookiesBlock3rdParty", IDS_COOKIES_BLOCK_3RDPARTY_CHKBOX},
    {"cookiesShowCookies", IDS_COOKIES_SHOW_COOKIES_BUTTON},
    {"flashStorageSettings", IDS_FLASH_STORAGE_SETTINGS},
    {"flashStorageUrl", IDS_FLASH_STORAGE_URL},
#if BUILDFLAG(ENABLE_GOOGLE_NOW)
    {"googleGeolocationAccessEnable",
     IDS_GEOLOCATION_GOOGLE_ACCESS_ENABLE_CHKBOX},
#endif
    // Image filter.
    {"imagesTabLabel", IDS_IMAGES_TAB_LABEL},
    {"imagesHeader", IDS_IMAGES_HEADER},
    {"imagesAllow", IDS_IMAGES_LOAD_RADIO},
    {"imagesBlock", IDS_IMAGES_NOLOAD_RADIO},
    // JavaScript filter.
    {"javascriptTabLabel", IDS_JAVASCRIPT_TAB_LABEL},
    {"javascriptHeader", IDS_JAVASCRIPT_HEADER},
    {"javascriptAllow", IDS_JS_ALLOW_RADIO},
    {"javascriptBlock", IDS_JS_DONOTALLOW_RADIO},
    // Plugins filter.
    {"pluginsTabLabel", IDS_FLASH_TAB_LABEL},
    {"pluginsHeader", IDS_FLASH_HEADER},
    {"pluginsAllow", IDS_FLASH_ALLOW_RADIO},
    {"pluginsBlock", IDS_FLASH_BLOCK_RADIO},
    // Pop-ups filter.
    {"popupsTabLabel", IDS_POPUP_TAB_LABEL},
    {"popupsHeader", IDS_POPUP_HEADER},
    {"popupsAllow", IDS_POPUP_ALLOW_RADIO},
    {"popupsBlock", IDS_POPUP_BLOCK_RADIO},
    // Location filter.
    {"locationTabLabel", IDS_GEOLOCATION_TAB_LABEL},
    {"locationHeader", IDS_GEOLOCATION_HEADER},
    {"locationAllow", IDS_GEOLOCATION_ALLOW_RADIO},
    {"locationAsk", IDS_GEOLOCATION_ASK_RADIO},
    {"locationBlock", IDS_GEOLOCATION_BLOCK_RADIO},
    {"setBy", IDS_GEOLOCATION_SET_BY_HOVER},
    // Notifications filter.
    {"notificationsTabLabel", IDS_NOTIFICATIONS_TAB_LABEL},
    {"notificationsHeader", IDS_NOTIFICATIONS_HEADER},
    {"notificationsAllow", IDS_NOTIFICATIONS_ALLOW_RADIO},
    {"notificationsAsk", IDS_NOTIFICATIONS_ASK_RADIO},
    {"notificationsBlock", IDS_NOTIFICATIONS_BLOCK_RADIO},
    // Protected Content filter
    {"protectedContentTabLabel", IDS_PROTECTED_CONTENT_TAB_LABEL},
    {"protectedContentEnableCheckbox", IDS_PROTECTED_CONTENT_ENABLE_CHECKBOX},
#if defined(OS_CHROMEOS) || defined(OS_WIN)
    {"protectedContentInfo", IDS_PROTECTED_CONTENT_INFO},
    {"protectedContentEnableIdentifiersCheckbox",
     IDS_PROTECTED_CONTENT_ENABLE_IDENTIFIERS_CHECKBOX},
    {"protectedContentHeader", IDS_PROTECTED_CONTENT_HEADER},
#endif  // defined(OS_CHROMEOS) || defined(OS_WIN)
    // Microphone filter.
    {"mediaStreamMicTabLabel", IDS_MEDIA_STREAM_MIC_TAB_LABEL},
    {"mediaStreamMicHeader", IDS_MEDIA_STREAM_MIC_HEADER},
    {"mediaStreamMicAsk", IDS_MEDIA_STREAM_ASK_AUDIO_ONLY_RADIO},
    {"mediaStreamMicBlock", IDS_MEDIA_STREAM_BLOCK_AUDIO_ONLY_RADIO},
    // Camera filter.
    {"mediaStreamCameraTabLabel", IDS_MEDIA_STREAM_CAMERA_TAB_LABEL},
    {"mediaStreamCameraHeader", IDS_MEDIA_STREAM_CAMERA_HEADER},
    {"mediaStreamCameraAsk", IDS_MEDIA_STREAM_ASK_VIDEO_ONLY_RADIO},
    {"mediaStreamCameraBlock", IDS_MEDIA_STREAM_BLOCK_VIDEO_ONLY_RADIO},
    // Flash media settings.
    {"mediaPepperFlashMicDefaultDivergedLabel",
     IDS_MEDIA_PEPPER_FLASH_MIC_DEFAULT_DIVERGED_LABEL},
    {"mediaPepperFlashCameraDefaultDivergedLabel",
     IDS_MEDIA_PEPPER_FLASH_CAMERA_DEFAULT_DIVERGED_LABEL},
    {"mediaPepperFlashMicExceptionsDivergedLabel",
     IDS_MEDIA_PEPPER_FLASH_MIC_EXCEPTIONS_DIVERGED_LABEL},
    {"mediaPepperFlashCameraExceptionsDivergedLabel",
     IDS_MEDIA_PEPPER_FLASH_CAMERA_EXCEPTIONS_DIVERGED_LABEL},
    {"mediaPepperFlashChangeLink", IDS_MEDIA_PEPPER_FLASH_CHANGE_LINK},
    {"mediaPepperFlashGlobalPrivacyURL", IDS_FLASH_GLOBAL_PRIVACY_URL},
    {"mediaPepperFlashWebsitePrivacyURL", IDS_FLASH_WEBSITE_PRIVACY_URL},
    // PPAPI broker filter.
    {"ppapiBrokerHeader", IDS_PPAPI_BROKER_HEADER},
    {"ppapiBrokerTabLabel", IDS_PPAPI_BROKER_TAB_LABEL},
    {"ppapiBrokerAllow", IDS_PPAPI_BROKER_ALLOW_RADIO},
    {"ppapiBrokerAsk", IDS_PPAPI_BROKER_ASK_RADIO},
    {"ppapiBrokerBlock", IDS_PPAPI_BROKER_BLOCK_RADIO},
    // Multiple automatic downloads.
    {"multipleAutomaticDownloadsTabLabel", IDS_AUTOMATIC_DOWNLOADS_TAB_LABEL},
    {"multipleAutomaticDownloadsHeader", IDS_AUTOMATIC_DOWNLOADS_TAB_LABEL},
    {"multipleAutomaticDownloadsAllow", IDS_AUTOMATIC_DOWNLOADS_ALLOW_RADIO},
    {"multipleAutomaticDownloadsAsk", IDS_AUTOMATIC_DOWNLOADS_ASK_RADIO},
    {"multipleAutomaticDownloadsBlock", IDS_AUTOMATIC_DOWNLOADS_BLOCK_RADIO},
    // MIDI system exclusive messages.
    {"midiSysexHeader", IDS_MIDI_SYSEX_TAB_LABEL},
    {"midiSysExAllow", IDS_MIDI_SYSEX_ALLOW_RADIO},
    {"midiSysExAsk", IDS_MIDI_SYSEX_ASK_RADIO},
    {"midiSysExBlock", IDS_MIDI_SYSEX_BLOCK_RADIO},
    // Push messaging strings.
    {"pushMessagingHeader", IDS_PUSH_MESSAGES_TAB_LABEL},
    {"pushMessagingAllow", IDS_PUSH_MESSSAGING_ALLOW_RADIO},
    {"pushMessagingAsk", IDS_PUSH_MESSSAGING_ASK_RADIO},
    {"pushMessagingBlock", IDS_PUSH_MESSSAGING_BLOCK_RADIO},
    // USB devices.
    {"usbDevicesHeader", IDS_USB_DEVICES_HEADER_AND_TAB_LABEL},
    {"usbDevicesManage", IDS_USB_DEVICES_MANAGE_BUTTON},
    // Background sync.
    {"backgroundSyncHeader", IDS_BACKGROUND_SYNC_HEADER},
    {"backgroundSyncAllow", IDS_BACKGROUND_SYNC_ALLOW_RADIO},
    {"backgroundSyncBlock", IDS_BACKGROUND_SYNC_BLOCK_RADIO},
    // Zoom levels.
    {"zoomlevelsHeader", IDS_ZOOMLEVELS_HEADER_AND_TAB_LABEL},
    {"zoomLevelsManage", IDS_ZOOMLEVELS_MANAGE_BUTTON},
    // PDF Plugin filter.
    {"pdfTabLabel", IDS_PDF_TAB_LABEL},
    {"pdfEnable", IDS_PDF_ENABLE_CHECKBOX},
  };

  RegisterStrings(localized_strings, resources, arraysize(resources));

  // TODO(tommycli): When the HTML5 By Default feature flag is on, we want to
  // display strings that begin with "Ask...", even though the setting remains
  // DETECT. Once this feature is finalized, then we migrate the setting to ASK.
  Profile* profile = Profile::FromWebUI(web_ui());
  bool is_hbd = PluginUtils::ShouldPreferHtmlOverPlugins(
      HostContentSettingsMapFactory::GetForProfile(profile));
  static OptionsStringResource flash_strings[] = {
      {"pluginsDetectImportantContent",
       is_hbd ? IDS_FLASH_ASK_RECOMMENDED_RADIO
              : IDS_FLASH_DETECT_RECOMMENDED_RADIO},
      {"detectException",
       is_hbd ? IDS_EXCEPTIONS_ASK_BUTTON
              : IDS_EXCEPTIONS_DETECT_IMPORTANT_CONTENT_BUTTON},
  };
  RegisterStrings(localized_strings, flash_strings, arraysize(flash_strings));

  PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();
  const base::Value* default_pref = prefs->GetDefaultPrefValue(
      content_settings::WebsiteSettingsRegistry::GetInstance()
          ->Get(CONTENT_SETTINGS_TYPE_PLUGINS)
          ->default_value_pref_name());

  int default_value = CONTENT_SETTING_DEFAULT;
  bool success = default_pref->GetAsInteger(&default_value);
  DCHECK(success);
  DCHECK_NE(CONTENT_SETTING_DEFAULT, default_value);

  RegisterTitle(localized_strings, "contentSettingsPage",
                IDS_CONTENT_SETTINGS_TITLE);

  // Register titles for each of the individual settings whose exception
  // dialogs will be processed by |ContentSettingsHandler|.
  RegisterTitle(localized_strings, "cookies",
                IDS_COOKIES_TAB_LABEL);
  RegisterTitle(localized_strings, "images",
                IDS_IMAGES_TAB_LABEL);
  RegisterTitle(localized_strings, "javascript",
                IDS_JAVASCRIPT_TAB_LABEL);
  RegisterTitle(localized_strings, "plugins", IDS_FLASH_TAB_LABEL);
  RegisterTitle(localized_strings, "popups",
                IDS_POPUP_TAB_LABEL);
  RegisterTitle(localized_strings, "location",
                IDS_GEOLOCATION_TAB_LABEL);
  RegisterTitle(localized_strings, "notifications",
                IDS_NOTIFICATIONS_TAB_LABEL);
#if defined(OS_CHROMEOS)
  RegisterTitle(localized_strings, "protectedContent",
                IDS_PROTECTED_CONTENT_TAB_LABEL);
#endif
  RegisterTitle(localized_strings, "media-stream-mic",
                IDS_MEDIA_STREAM_MIC_TAB_LABEL);
  RegisterTitle(localized_strings, "media-stream-camera",
                IDS_MEDIA_STREAM_CAMERA_TAB_LABEL);
  RegisterTitle(localized_strings, "ppapi-broker",
                IDS_PPAPI_BROKER_TAB_LABEL);
  RegisterTitle(localized_strings, "multiple-automatic-downloads",
                IDS_AUTOMATIC_DOWNLOADS_TAB_LABEL);
  RegisterTitle(localized_strings, "midi-sysex",
                IDS_MIDI_SYSEX_TAB_LABEL);
  RegisterTitle(localized_strings, "usb-devices",
                IDS_USB_DEVICES_HEADER_AND_TAB_LABEL);
  RegisterTitle(localized_strings, "background-sync",
                IDS_BACKGROUND_SYNC_HEADER);
  RegisterTitle(localized_strings, "zoomlevels",
                IDS_ZOOMLEVELS_HEADER_AND_TAB_LABEL);

  localized_strings->SetString("exceptionsLearnMoreUrl",
                               chrome::kContentSettingsExceptionsLearnMoreURL);
}

void ContentSettingsHandler::InitializeHandler() {
  notification_registrar_.Add(
      this, chrome::NOTIFICATION_PROFILE_CREATED,
      content::NotificationService::AllSources());
  notification_registrar_.Add(
      this, chrome::NOTIFICATION_PROFILE_DESTROYED,
      content::NotificationService::AllSources());

  content::BrowserContext* context = GetBrowserContext(web_ui());
  notification_registrar_.Add(
      this, chrome::NOTIFICATION_PROTOCOL_HANDLER_REGISTRY_CHANGED,
      content::Source<content::BrowserContext>(context));

  PrefService* prefs = user_prefs::UserPrefs::Get(context);
  pref_change_registrar_.Init(prefs);
  pref_change_registrar_.Add(
      prefs::kPepperFlashSettingsEnabled,
      base::Bind(&ContentSettingsHandler::OnPepperFlashPrefChanged,
                 base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kAudioCaptureAllowed,
      base::Bind(&ContentSettingsHandler::UpdateSettingDefaultFromModel,
                 base::Unretained(this),
                 CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC));
  pref_change_registrar_.Add(
      prefs::kAudioCaptureAllowedUrls,
      base::Bind(&ContentSettingsHandler::UpdateExceptionsViewFromModel,
                 base::Unretained(this),
                 CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC));
  pref_change_registrar_.Add(
      prefs::kVideoCaptureAllowed,
      base::Bind(&ContentSettingsHandler::UpdateSettingDefaultFromModel,
                 base::Unretained(this),
                 CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA));
  pref_change_registrar_.Add(
      prefs::kVideoCaptureAllowedUrls,
      base::Bind(&ContentSettingsHandler::UpdateExceptionsViewFromModel,
                 base::Unretained(this),
                 CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA));
  pref_change_registrar_.Add(
      prefs::kEnableDRM,
      base::Bind(
          &ContentSettingsHandler::UpdateProtectedContentExceptionsButton,
          base::Unretained(this)));

  // Here we only subscribe to the HostZoomMap for the default storage partition
  // since we don't allow the user to manage the zoom levels for apps.
  // We're only interested in zoom-levels that are persisted, since the user
  // is given the opportunity to view/delete these in the content-settings page.
  host_zoom_map_subscription_ =
      content::HostZoomMap::GetDefaultForBrowserContext(context)
          ->AddZoomLevelChangedCallback(
              base::Bind(&ContentSettingsHandler::OnZoomLevelChanged,
                         base::Unretained(this)));

  flash_settings_manager_.reset(new PepperFlashSettingsManager(this, context));

  Profile* profile = Profile::FromWebUI(web_ui());
  observer_.Add(HostContentSettingsMapFactory::GetForProfile(profile));
  if (profile->HasOffTheRecordProfile()) {
    auto* map = HostContentSettingsMapFactory::GetForProfile(
        profile->GetOffTheRecordProfile());
    if (!observer_.IsObserving(map))
      observer_.Add(map);
  }
}

void ContentSettingsHandler::InitializePage() {
  media_settings_.reset(new MediaSettingsInfo());
  RefreshFlashMediaSettings();

  UpdateHandlersEnabledRadios();
  UpdateAllExceptionsViewsFromModel();
  UpdateAllChooserExceptionsViewsFromModel();
  UpdateProtectedContentExceptionsButton();
}

void ContentSettingsHandler::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    std::string resource_identifier) {
  const ContentSettingsDetails details(
      primary_pattern, secondary_pattern, content_type, resource_identifier);
  // TODO(estade): we pretend update_all() is always true.
  if (details.update_all_types()) {
    UpdateAllExceptionsViewsFromModel();
    UpdateAllChooserExceptionsViewsFromModel();
  } else {
    if (base::ContainsKey(GetExceptionsInfoMap(), details.type()))
      UpdateExceptionsViewFromModel(details.type());
  }
}

void ContentSettingsHandler::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_PROFILE_DESTROYED: {
      Profile* profile = content::Source<Profile>(source).ptr();
      HostContentSettingsMap* settings_map =
          HostContentSettingsMapFactory::GetForProfile(profile);
      if (profile->IsOffTheRecord() &&
          observer_.IsObserving(settings_map)) {
        web_ui()->CallJavascriptFunctionUnsafe(
            "ContentSettingsExceptionsArea.OTRProfileDestroyed");
        observer_.Remove(settings_map);
      }
      break;
    }

    case chrome::NOTIFICATION_PROFILE_CREATED: {
      Profile* profile = content::Source<Profile>(source).ptr();
      if (profile->IsOffTheRecord()) {
        UpdateAllOTRExceptionsViewsFromModel();
        UpdateAllOTRChooserExceptionsViewsFromModel();
        observer_.Add(HostContentSettingsMapFactory::GetForProfile(profile));
      }
      break;
    }

    case chrome::NOTIFICATION_PROTOCOL_HANDLER_REGISTRY_CHANGED: {
      UpdateHandlersEnabledRadios();
      break;
    }
  }
}

void ContentSettingsHandler::OnGetPermissionSettingsCompleted(
    uint32_t request_id,
    bool success,
    PP_Flash_BrowserOperations_Permission default_permission,
    const ppapi::FlashSiteSettings& sites) {
  MediaSettingsInfo::ForFlash& settings = media_settings_->forFlash();
  if (success && request_id == settings.last_refresh_request_id) {
    settings.initialized = true;
    settings.default_setting =
        PepperFlashContentSettingsUtils::FlashPermissionToContentSetting(
            default_permission);
    PepperFlashContentSettingsUtils::FlashSiteSettingsToMediaExceptions(
        sites, &settings.exceptions);
    PepperFlashContentSettingsUtils::SortMediaExceptions(
        &settings.exceptions);

    UpdateFlashMediaLinksVisibility(CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC);
    UpdateFlashMediaLinksVisibility(CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA);
  }
}

void ContentSettingsHandler::UpdateSettingDefaultFromModel(
    ContentSettingsType type) {
  std::string provider_id;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(GetProfile());
  ContentSetting default_setting =
      host_content_settings_map->GetDefaultContentSetting(type, &provider_id);

#if BUILDFLAG(ENABLE_PLUGINS)
  default_setting = PluginsFieldTrial::EffectiveContentSetting(
      host_content_settings_map, type, default_setting);
#endif

  // Camera and microphone default content settings cannot be set by the policy.
  // However, the policy can disable them. Treat this case visually in the same
  // way as if the policy set the default setting to BLOCK. Furthermore, compare
  // the settings with Flash settings and show links to the Flash settings site
  // if they differ.
  if (type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC ||
      type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA) {
    UpdateMediaSettingsFromPrefs(type);
    if (media_settings_->forType(type).policy_disable) {
      default_setting = CONTENT_SETTING_BLOCK;
      provider_id = site_settings::kPolicyProviderId;
    }
  }

  base::DictionaryValue filter_settings;

  std::string setting_string =
      content_settings::ContentSettingToString(default_setting);
  DCHECK(!setting_string.empty());

  filter_settings.SetString(
      site_settings::ContentSettingsTypeToGroupName(type) + ".value",
      setting_string);
  filter_settings.SetString(
      site_settings::ContentSettingsTypeToGroupName(type) + ".managedBy",
      provider_id);

  web_ui()->CallJavascriptFunctionUnsafe(
      "ContentSettings.setContentFilterSettingsValue", filter_settings);
}

void ContentSettingsHandler::UpdateMediaSettingsFromPrefs(
    ContentSettingsType type) {
  PrefService* prefs = user_prefs::UserPrefs::Get(GetBrowserContext(web_ui()));
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(GetProfile());
  MediaSettingsInfo::ForOneType& settings = media_settings_->forType(type);
  std::string policy_pref = (type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC)
      ? prefs::kAudioCaptureAllowed
      : prefs::kVideoCaptureAllowed;

  settings.policy_disable = !prefs->GetBoolean(policy_pref) &&
      prefs->IsManagedPreference(policy_pref);
  settings.default_setting =
      settings_map->GetDefaultContentSetting(type, nullptr);
  settings.default_setting_initialized = true;

  UpdateFlashMediaLinksVisibility(type);
  UpdateMediaDeviceDropdownVisibility(type);
}

void ContentSettingsHandler::UpdateHandlersEnabledRadios() {
  base::Value handlers_enabled(GetProtocolHandlerRegistry()->enabled());

  web_ui()->CallJavascriptFunctionUnsafe(
      "ContentSettings.updateHandlersEnabledRadios", handlers_enabled);
}

void ContentSettingsHandler::UpdateAllExceptionsViewsFromModel() {
  const ExceptionsInfoMap& exceptions_info_map = GetExceptionsInfoMap();
  for (const auto& exceptions_info_pair : exceptions_info_map)
    UpdateExceptionsViewFromModel(exceptions_info_pair.first);

  // Zoom levels are not actually a content type so we need to handle them
  // separately.
  UpdateZoomLevelsExceptionsView();
}

void ContentSettingsHandler::UpdateAllOTRExceptionsViewsFromModel() {
  const ExceptionsInfoMap& exceptions_info_map = GetExceptionsInfoMap();
  for (const auto& exceptions_info_pair : exceptions_info_map) {
    if (exceptions_info_pair.second.has_otr_exceptions) {
      UpdateExceptionsViewFromOTRHostContentSettingsMap(
          exceptions_info_pair.first);
    }
  }
}

void ContentSettingsHandler::UpdateExceptionsViewFromModel(
    ContentSettingsType type) {
  if (type == CONTENT_SETTINGS_TYPE_GEOLOCATION) {
    UpdateGeolocationExceptionsView();
  } else if (type == CONTENT_SETTINGS_TYPE_NOTIFICATIONS) {
    UpdateNotificationExceptionsView();
  } else if (type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC ||
             type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA) {
    CompareMediaExceptionsWithFlash(type);
    UpdateExceptionsViewFromHostContentSettingsMap(type);
  } else if (type == CONTENT_SETTINGS_TYPE_MIDI_SYSEX) {
    UpdateMIDISysExExceptionsView();
  } else {
    UpdateExceptionsViewFromHostContentSettingsMap(type);
  }
}

// TODO(estade): merge with GetExceptionsFromHostContentSettingsMap.
void ContentSettingsHandler::UpdateGeolocationExceptionsView() {
  Profile* profile = Profile::FromWebUI(web_ui());
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile);

  ContentSettingsForOneType all_settings;
  map->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_GEOLOCATION,
      std::string(),
      &all_settings);

  // Group geolocation settings by primary_pattern.
  site_settings::AllPatternsSettings all_patterns_settings;
  for (ContentSettingsForOneType::iterator i = all_settings.begin();
       i != all_settings.end(); ++i) {
    // Don't add default settings.
    if (i->primary_pattern == ContentSettingsPattern::Wildcard() &&
        i->secondary_pattern == ContentSettingsPattern::Wildcard() &&
        i
          ->source != site_settings::kPreferencesSource) {
      continue;
    }
    all_patterns_settings[std::make_pair(i->primary_pattern, i->source)]
        [i->secondary_pattern] = i->setting;
  }

  base::ListValue exceptions;
  AddExceptionsGrantedByHostedApps(
      profile,
      HostedAppHasPermission<APIPermission::kGeolocation>,
      &exceptions);

  for (site_settings::AllPatternsSettings::iterator i =
      all_patterns_settings.begin(); i != all_patterns_settings.end(); ++i) {
    const ContentSettingsPattern& primary_pattern = i->first.first;
    const site_settings::OnePatternSettings& one_settings = i->second;

    site_settings::OnePatternSettings::const_iterator parent =
        one_settings.find(primary_pattern);

    // Add the "parent" entry for the non-embedded setting.
    ContentSetting parent_setting =
        parent == one_settings.end() ? CONTENT_SETTING_DEFAULT : parent->second;
    exceptions.Append(GetGeolocationExceptionForPage(primary_pattern,
                                                     primary_pattern,
                                                     parent_setting));

    // Add the "children" for any embedded settings.
    for (site_settings::OnePatternSettings::const_iterator j =
            one_settings.begin();
         j != one_settings.end();
         ++j) {
      // Skip the non-embedded setting which we already added above.
      if (j == parent)
        continue;

      exceptions.Append(GetGeolocationExceptionForPage(
          primary_pattern, j->first, j->second));
    }
  }

  base::StringValue type_string(site_settings::ContentSettingsTypeToGroupName(
      CONTENT_SETTINGS_TYPE_GEOLOCATION));
  web_ui()->CallJavascriptFunctionUnsafe("ContentSettings.setExceptions",
                                         type_string, exceptions);

  // This is mainly here to keep this function ideologically parallel to
  // UpdateExceptionsViewFromHostContentSettingsMap().
  UpdateSettingDefaultFromModel(CONTENT_SETTINGS_TYPE_GEOLOCATION);
}

void ContentSettingsHandler::UpdateNotificationExceptionsView() {
  Profile* profile = Profile::FromWebUI(web_ui());
  ContentSettingsForOneType settings;
  DesktopNotificationProfileUtil::GetNotificationsSettings(profile, &settings);

  base::ListValue exceptions;
  AddExceptionsGrantedByHostedApps(
      profile,
      HostedAppHasPermission<APIPermission::kNotifications>,
      &exceptions);

  for (ContentSettingsForOneType::const_iterator i =
           settings.begin();
       i != settings.end();
       ++i) {
    // Don't add default settings.
    if (i->primary_pattern == ContentSettingsPattern::Wildcard() &&
        i->secondary_pattern == ContentSettingsPattern::Wildcard() &&
        i
          ->source != site_settings::kPreferencesSource) {
      continue;
    }

    exceptions.Append(
        GetNotificationExceptionForPage(i->primary_pattern,
                                        i->secondary_pattern,
                                        i->setting,
                                        i->source));
  }

  base::StringValue type_string(site_settings::ContentSettingsTypeToGroupName(
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS));
  web_ui()->CallJavascriptFunctionUnsafe("ContentSettings.setExceptions",
                                         type_string, exceptions);

  // This is mainly here to keep this function ideologically parallel to
  // UpdateExceptionsViewFromHostContentSettingsMap().
  UpdateSettingDefaultFromModel(CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
}

void ContentSettingsHandler::CompareMediaExceptionsWithFlash(
    ContentSettingsType type) {
  MediaSettingsInfo::ForOneType& settings = media_settings_->forType(type);
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(GetProfile());
  const auto* extension_registry =
      extensions::ExtensionRegistry::Get(GetProfile());
  base::ListValue exceptions;
  site_settings::GetExceptionsFromHostContentSettingsMap(
      settings_map, type, extension_registry, web_ui(), /*incognito=*/false,
      /*filter=*/nullptr, &exceptions);

  settings.exceptions.clear();
  for (base::ListValue::const_iterator entry = exceptions.begin();
       entry != exceptions.end(); ++entry) {
    base::DictionaryValue* dict = nullptr;
    bool valid_dict = (*entry)->GetAsDictionary(&dict);
    DCHECK(valid_dict);

    std::string origin;
    std::string setting;
    dict->GetString(site_settings::kOrigin, &origin);
    dict->GetString(site_settings::kSetting, &setting);

    ContentSetting setting_type;
    bool result =
        content_settings::ContentSettingFromString(setting, &setting_type);
    DCHECK(result);

    settings.exceptions.push_back(MediaException(
        ContentSettingsPattern::FromString(origin),
        setting_type));
  }

  PepperFlashContentSettingsUtils::SortMediaExceptions(
      &settings.exceptions);

  settings.exceptions_initialized = true;
  UpdateFlashMediaLinksVisibility(type);
}

void ContentSettingsHandler::UpdateMIDISysExExceptionsView() {
  UpdateSettingDefaultFromModel(CONTENT_SETTINGS_TYPE_MIDI_SYSEX);
  UpdateExceptionsViewFromHostContentSettingsMap(
      CONTENT_SETTINGS_TYPE_MIDI_SYSEX);
}

void ContentSettingsHandler::UpdateAllChooserExceptionsViewsFromModel() {
  for (const site_settings::ChooserTypeNameEntry& chooser_type :
      site_settings::kChooserTypeGroupNames)
    UpdateChooserExceptionsViewFromModel(chooser_type);
}

void ContentSettingsHandler::UpdateAllOTRChooserExceptionsViewsFromModel() {
  for (const site_settings::ChooserTypeNameEntry& chooser_type :
      site_settings::kChooserTypeGroupNames)
    UpdateOTRChooserExceptionsViewFromModel(chooser_type);
}

void ContentSettingsHandler::UpdateChooserExceptionsViewFromModel(
    const site_settings::ChooserTypeNameEntry& chooser_type) {
  base::ListValue exceptions;
  site_settings::GetChooserExceptionsFromProfile(
      Profile::FromWebUI(web_ui()), false, chooser_type, &exceptions);
  base::StringValue type_string(chooser_type.name);
  web_ui()->CallJavascriptFunctionUnsafe("ContentSettings.setExceptions",
                                         type_string, exceptions);

  UpdateOTRChooserExceptionsViewFromModel(chooser_type);
}

void ContentSettingsHandler::UpdateOTRChooserExceptionsViewFromModel(
    const site_settings::ChooserTypeNameEntry& chooser_type) {
  if (!Profile::FromWebUI(web_ui())->HasOffTheRecordProfile())
    return;

  base::ListValue exceptions;
  site_settings::GetChooserExceptionsFromProfile(
      Profile::FromWebUI(web_ui()), true, chooser_type, &exceptions);
  base::StringValue type_string(chooser_type.name);
  web_ui()->CallJavascriptFunctionUnsafe("ContentSettings.setOTRExceptions",
                                         type_string, exceptions);
}

void ContentSettingsHandler::UpdateZoomLevelsExceptionsView() {
  base::ListValue zoom_levels_exceptions;

  content::HostZoomMap* host_zoom_map =
      content::HostZoomMap::GetDefaultForBrowserContext(
          GetBrowserContext(web_ui()));
  content::HostZoomMap::ZoomLevelVector zoom_levels(
      host_zoom_map->GetAllZoomLevels());

  // Sort ZoomLevelChanges by host and scheme
  // (a.com < http://a.com < https://a.com < b.com).
  std::sort(zoom_levels.begin(), zoom_levels.end(),
            [](const content::HostZoomMap::ZoomLevelChange& a,
               const content::HostZoomMap::ZoomLevelChange& b) {
              return a.host == b.host ? a.scheme < b.scheme : a.host < b.host;
            });

  for (content::HostZoomMap::ZoomLevelVector::const_iterator i =
           zoom_levels.begin();
       i != zoom_levels.end();
       ++i) {
    std::unique_ptr<base::DictionaryValue> exception(new base::DictionaryValue);
    switch (i->mode) {
      case content::HostZoomMap::ZOOM_CHANGED_FOR_HOST: {
        exception->SetString(site_settings::kOrigin, i->host);
        std::string host = i->host;
        if (host == content::kUnreachableWebDataURL) {
          host =
              l10n_util::GetStringUTF8(IDS_ZOOMLEVELS_CHROME_ERROR_PAGES_LABEL);
        }
        exception->SetString(site_settings::kOrigin, host);
        break;
      }
      case content::HostZoomMap::ZOOM_CHANGED_FOR_SCHEME_AND_HOST:
        // These are not stored in preferences and get cleared on next browser
        // start. Therefore, we don't care for them.
        continue;
      case content::HostZoomMap::PAGE_SCALE_IS_ONE_CHANGED:
        continue;
      case content::HostZoomMap::ZOOM_CHANGED_TEMPORARY_ZOOM:
        NOTREACHED();
    }

    std::string setting_string =
        content_settings::ContentSettingToString(CONTENT_SETTING_DEFAULT);
    DCHECK(!setting_string.empty());

    exception->SetString(site_settings::kSetting, setting_string);

    // Calculate the zoom percent from the factor. Round up to the nearest whole
    // number.
    int zoom_percent = static_cast<int>(
        content::ZoomLevelToZoomFactor(i->zoom_level) * 100 + 0.5);
    exception->SetString(kZoom, base::FormatPercent(zoom_percent));
    exception->SetString(
        site_settings::kSource, site_settings::kPreferencesSource);
    // Append the new entry to the list and map.
    zoom_levels_exceptions.Append(std::move(exception));
  }

  base::StringValue type_string(kZoomContentType);
  web_ui()->CallJavascriptFunctionUnsafe("ContentSettings.setExceptions",
                                         type_string, zoom_levels_exceptions);
}

void ContentSettingsHandler::UpdateExceptionsViewFromHostContentSettingsMap(
    ContentSettingsType type) {
  base::ListValue exceptions;
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(GetProfile());
  const auto* extension_registry =
      extensions::ExtensionRegistry::Get(GetProfile());
  site_settings::GetExceptionsFromHostContentSettingsMap(
      settings_map, type, extension_registry, web_ui(), /*incognito=*/false,
      /*filter=*/nullptr, &exceptions);
  base::StringValue type_string(
      site_settings::ContentSettingsTypeToGroupName(type));
  web_ui()->CallJavascriptFunctionUnsafe("ContentSettings.setExceptions",
                                         type_string, exceptions);

  UpdateExceptionsViewFromOTRHostContentSettingsMap(type);

#if defined(OS_CHROMEOS)
  // Also the default for protected contents is managed in another place.
  if (type == CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER)
    return;
#endif

  // The default may also have changed (we won't get a separate notification).
  // If it hasn't changed, this call will be harmless.
  UpdateSettingDefaultFromModel(type);
}

void ContentSettingsHandler::UpdateExceptionsViewFromOTRHostContentSettingsMap(
    ContentSettingsType type) {
  const HostContentSettingsMap* otr_settings_map =
      HostContentSettingsMapFactory::GetForProfile(GetOTRProfile());
  if (!otr_settings_map)
    return;
  const auto* extension_registry =
      extensions::ExtensionRegistry::Get(GetOTRProfile());
  base::ListValue exceptions;
  site_settings::GetExceptionsFromHostContentSettingsMap(
      otr_settings_map, type, extension_registry, web_ui(), /*incognito=*/true,
      /*filter=*/nullptr, &exceptions);
  base::StringValue type_string(
      site_settings::ContentSettingsTypeToGroupName(type));
  web_ui()->CallJavascriptFunctionUnsafe("ContentSettings.setOTRExceptions",
                                         type_string, exceptions);
}

void ContentSettingsHandler::RemoveExceptionFromHostContentSettingsMap(
    const base::ListValue* args,
    ContentSettingsType type) {
  std::string mode;
  bool rv = args->GetString(1, &mode);
  DCHECK(rv);

  std::string pattern;
  rv = args->GetString(2, &pattern);
  DCHECK(rv);

  // The fourth argument to this handler is optional.
  std::string secondary_pattern_string;
  if (args->GetSize() >= 4U) {
    rv = args->GetString(3, &secondary_pattern_string);
    DCHECK(rv);
  }

  Profile* profile = mode == "normal" ? GetProfile() : GetOTRProfile();
  if (!profile)
    return;

  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  ContentSettingsPattern primary_pattern =
      ContentSettingsPattern::FromString(pattern);
  ContentSettingsPattern secondary_pattern =
      secondary_pattern_string.empty()
          ? ContentSettingsPattern::Wildcard()
          : ContentSettingsPattern::FromString(secondary_pattern_string);
  PermissionUtil::ScopedRevocationReporter scoped_revocation_reporter(
      profile, primary_pattern, secondary_pattern, type,
      PermissionSourceUI::SITE_SETTINGS);

  settings_map->SetContentSettingCustomScope(
      primary_pattern, secondary_pattern, type, std::string(),
      CONTENT_SETTING_DEFAULT);
}

void ContentSettingsHandler::RemoveZoomLevelException(
    const base::ListValue* args) {
  std::string mode;
  bool rv = args->GetString(1, &mode);
  DCHECK(rv);

  std::string pattern;
  rv = args->GetString(2, &pattern);
  DCHECK(rv);

  if (pattern ==
          l10n_util::GetStringUTF8(IDS_ZOOMLEVELS_CHROME_ERROR_PAGES_LABEL)) {
    pattern = content::kUnreachableWebDataURL;
  }

  content::HostZoomMap* host_zoom_map;
  host_zoom_map =
    content::HostZoomMap::GetDefaultForBrowserContext(
        GetBrowserContext(web_ui()));
  double default_level = host_zoom_map->GetDefaultZoomLevel();
  host_zoom_map->SetZoomLevelForHost(pattern, default_level);
}

void ContentSettingsHandler::RemoveChooserException(
    const site_settings::ChooserTypeNameEntry* chooser_type,
    const base::ListValue* args) {
  std::string mode;
  bool rv = args->GetString(1, &mode);
  DCHECK(rv);

  std::string requesting_origin_string;
  rv = args->GetString(2, &requesting_origin_string);
  DCHECK(rv);
  GURL requesting_origin(requesting_origin_string);
  DCHECK(requesting_origin.is_valid());

  std::string embedding_origin_string;
  rv = args->GetString(3, &embedding_origin_string);
  DCHECK(rv);
  GURL embedding_origin(embedding_origin_string);
  DCHECK(embedding_origin.is_valid());

  const base::DictionaryValue* object = nullptr;
  rv = args->GetDictionary(4, &object);
  DCHECK(rv);

  Profile* profile = Profile::FromWebUI(web_ui());
  if (mode != "normal")
    profile = profile->GetOffTheRecordProfile();

  ChooserContextBase* chooser_context = chooser_type->get_context(profile);
  chooser_context->RevokeObjectPermission(requesting_origin, embedding_origin,
                                          *object);
  UpdateChooserExceptionsViewFromModel(*chooser_type);
  // TODO(reillyg): Create metrics for revocations. crbug.com/556845
}

void ContentSettingsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("setContentFilter",
      base::Bind(&ContentSettingsHandler::SetContentFilter,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("removeException",
      base::Bind(&ContentSettingsHandler::RemoveException,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("setException",
      base::Bind(&ContentSettingsHandler::SetException,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("checkExceptionPatternValidity",
      base::Bind(&ContentSettingsHandler::CheckExceptionPatternValidity,
                 base::Unretained(this)));
}

void ContentSettingsHandler::SetContentFilter(const base::ListValue* args) {
  DCHECK_EQ(2U, args->GetSize());
  std::string group, setting;
  if (!(args->GetString(0, &group) &&
        args->GetString(1, &setting))) {
    NOTREACHED();
    return;
  }

  ContentSetting default_setting;
  bool result =
      content_settings::ContentSettingFromString(setting, &default_setting);
  DCHECK(result);

  ContentSettingsType content_type =
      site_settings::ContentSettingsTypeFromGroupName(group);
  Profile* profile = Profile::FromWebUI(web_ui());

#if defined(OS_CHROMEOS)
  // ChromeOS special case : in Guest mode settings are opened in Incognito
  // mode, so we need original profile to actually modify settings.
  if (user_manager::UserManager::Get()->IsLoggedInAsGuest())
    profile = profile->GetOriginalProfile();
#endif

  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  map->SetDefaultContentSetting(content_type, default_setting);

  const ExceptionsInfoMap& exceptions_info_map = GetExceptionsInfoMap();
  const auto& it = exceptions_info_map.find(content_type);
  if (it != exceptions_info_map.end())
    content::RecordAction(it->second.uma);
}

void ContentSettingsHandler::RemoveException(const base::ListValue* args) {
  std::string type_string;
  CHECK(args->GetString(0, &type_string));

  // Zoom levels are no actual content type so we need to handle them
  // separately. They would not be recognized by
  // ContentSettingsTypeFromGroupName.
  if (type_string == kZoomContentType) {
    RemoveZoomLevelException(args);
    return;
  }

  const site_settings::ChooserTypeNameEntry* chooser_type =
      site_settings::ChooserTypeFromGroupName(type_string);
  if (chooser_type) {
    RemoveChooserException(chooser_type, args);
    return;
  }

  ContentSettingsType type =
      site_settings::ContentSettingsTypeFromGroupName(type_string);
  RemoveExceptionFromHostContentSettingsMap(args, type);

  WebSiteSettingsUmaUtil::LogPermissionChange(
      type, ContentSetting::CONTENT_SETTING_DEFAULT);
}

void ContentSettingsHandler::SetException(const base::ListValue* args) {
  std::string type_string;
  CHECK(args->GetString(0, &type_string));
  std::string mode;
  CHECK(args->GetString(1, &mode));
  std::string pattern;
  CHECK(args->GetString(2, &pattern));
  std::string setting;
  CHECK(args->GetString(3, &setting));

  ContentSettingsType type =
      site_settings::ContentSettingsTypeFromGroupName(type_string);

  DCHECK(type != CONTENT_SETTINGS_TYPE_GEOLOCATION &&
         type != CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC &&
         type != CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA);

  Profile* profile = mode == "normal" ? GetProfile() : GetOTRProfile();

  // The profile could be nullptr if the mode was OTR but the OTR profile
  // got destroyed before we received this message.
  if (!profile)
    return;

  ContentSetting setting_type;
  bool result =
      content_settings::ContentSettingFromString(setting, &setting_type);
  DCHECK(result);

  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile);

  PermissionUtil::ScopedRevocationReporter scoped_revocation_reporter(
      profile, ContentSettingsPattern::FromString(pattern),
      ContentSettingsPattern::Wildcard(), type,
      PermissionSourceUI::SITE_SETTINGS);

  settings_map->SetContentSettingCustomScope(
      ContentSettingsPattern::FromString(pattern),
      ContentSettingsPattern::Wildcard(), type, std::string(), setting_type);
  WebSiteSettingsUmaUtil::LogPermissionChange(type, setting_type);
}

void ContentSettingsHandler::CheckExceptionPatternValidity(
    const base::ListValue* args) {
  std::string type_string;
  CHECK(args->GetString(0, &type_string));
  std::string mode_string;
  CHECK(args->GetString(1, &mode_string));
  std::string pattern_string;
  CHECK(args->GetString(2, &pattern_string));

  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromString(pattern_string);

  web_ui()->CallJavascriptFunctionUnsafe(
      "ContentSettings.patternValidityCheckComplete",
      base::StringValue(type_string), base::StringValue(mode_string),
      base::StringValue(pattern_string), base::Value(pattern.IsValid()));
}

Profile* ContentSettingsHandler::GetProfile() {
  return Profile::FromWebUI(web_ui());
}

ProtocolHandlerRegistry* ContentSettingsHandler::GetProtocolHandlerRegistry() {
  return ProtocolHandlerRegistryFactory::GetForBrowserContext(
      GetBrowserContext(web_ui()));
}

Profile* ContentSettingsHandler::GetOTRProfile() {
  return GetProfile()->HasOffTheRecordProfile()
             ? GetProfile()->GetOffTheRecordProfile()
             : nullptr;
}

void ContentSettingsHandler::RefreshFlashMediaSettings() {
  MediaSettingsInfo::ForFlash& settings = media_settings_->forFlash();
  settings.initialized = false;

  settings.last_refresh_request_id =
      flash_settings_manager_->GetPermissionSettings(
          PP_FLASH_BROWSEROPERATIONS_SETTINGTYPE_CAMERAMIC);
}

void ContentSettingsHandler::OnPepperFlashPrefChanged() {
  ShowFlashMediaLink(
      DEFAULT_SETTING, CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC, false);
  ShowFlashMediaLink(
      DEFAULT_SETTING, CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA, false);
  ShowFlashMediaLink(
      EXCEPTIONS, CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC, false);
  ShowFlashMediaLink(
      EXCEPTIONS, CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA, false);

  PrefService* prefs = user_prefs::UserPrefs::Get(GetBrowserContext(web_ui()));
  if (prefs->GetBoolean(prefs::kPepperFlashSettingsEnabled))
    RefreshFlashMediaSettings();
  else
    media_settings_->forFlash().initialized = false;
}

void ContentSettingsHandler::OnZoomLevelChanged(
    const content::HostZoomMap::ZoomLevelChange& change) {
  UpdateZoomLevelsExceptionsView();
}

void ContentSettingsHandler::ShowFlashMediaLink(
    LinkType link_type, ContentSettingsType content_type, bool show) {
  MediaSettingsInfo::ForOneType& settings =
      media_settings_->forType(content_type);

  bool& show_link = link_type == DEFAULT_SETTING ?
      settings.show_flash_default_link :
      settings.show_flash_exceptions_link;

  if (show_link != show) {
    web_ui()->CallJavascriptFunctionUnsafe(
        "ContentSettings.showMediaPepperFlashLink",
        base::StringValue(link_type == DEFAULT_SETTING ? "default"
                                                       : "exceptions"),
        base::StringValue(content_type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC
                              ? "mic"
                              : "camera"),
        base::Value(show));
    show_link = show;
  }
}

void ContentSettingsHandler::UpdateFlashMediaLinksVisibility(
    ContentSettingsType type) {
  MediaSettingsInfo::ForOneType& settings = media_settings_->forType(type);
  MediaSettingsInfo::ForFlash& flash_settings = media_settings_->forFlash();

  if (!flash_settings.initialized)
    return;

  // We handle four cases - default settings and exceptions for microphone
  // and camera. We use the following criteria to determine whether to show
  // the links.
  //
  // 1. Flash won't send us notifications when its settings get changed, which
  // means the Flash settings in |media_settings_| may be out-dated, especially
  // after we show links to change Flash settings.
  // In order to avoid confusion, we won't hide the links once they are showed.
  // One exception is that we will hide them when Pepper Flash is disabled
  // (handled in OnPepperFlashPrefChanged()).
  //
  // 2. If audio or video capture are disabled by policy, the respective link
  // shouldn't be showed. Flash conforms to the policy in this case because
  // it cannot open those devices.
  //
  // 3. Otherwise, we show the link if the corresponding setting is different
  // in HostContentSettingsMap than it is in Flash.
  if (settings.policy_disable)
    return;

  if (settings.default_setting_initialized &&
      !settings.show_flash_default_link &&
      (flash_settings.default_setting !=
       settings.default_setting)) {
    ShowFlashMediaLink(DEFAULT_SETTING, type, true);
  }

  if (settings.exceptions_initialized &&
      !settings.show_flash_exceptions_link &&
      !PepperFlashContentSettingsUtils::AreMediaExceptionsEqual(
          settings.default_setting,
          settings.exceptions,
          flash_settings.default_setting,
          flash_settings.exceptions)) {
    ShowFlashMediaLink(EXCEPTIONS, type, true);
  }
}

void ContentSettingsHandler::UpdateMediaDeviceDropdownVisibility(
    ContentSettingsType type) {
  MediaSettingsInfo::ForOneType& settings = media_settings_->forType(type);

  web_ui()->CallJavascriptFunctionUnsafe(
      "ContentSettings.setDevicesMenuVisibility",
      base::StringValue(site_settings::ContentSettingsTypeToGroupName(type)),
      base::Value(!settings.policy_disable));
}

void ContentSettingsHandler::UpdateProtectedContentExceptionsButton() {
#if defined(OS_CHROMEOS)
  // Guests cannot modify exceptions. UIAccountTweaks will disabled the button.
  if (user_manager::UserManager::Get()->IsLoggedInAsGuest())
    return;
#endif

  // Exceptions apply only when the feature is enabled.
  PrefService* prefs = user_prefs::UserPrefs::Get(GetBrowserContext(web_ui()));
  bool enable_exceptions = prefs->GetBoolean(prefs::kEnableDRM);
  web_ui()->CallJavascriptFunctionUnsafe(
      "ContentSettings.enableProtectedContentExceptions",
      base::Value(enable_exceptions));
}

}  // namespace options
