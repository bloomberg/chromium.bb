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
#include "base/logging.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
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
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
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
#include "components/content_settings/core/browser/plugins_field_trial.h"
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
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "components/user_manager/user_manager.h"
#endif

using base::UserMetricsAction;
using content_settings::ContentSettingToString;
using content_settings::ContentSettingFromString;
using extensions::APIPermission;

namespace options {

// This struct is declared early so that it can used by functions below.
struct ContentSettingsHandler::ChooserTypeNameEntry {
  ContentSettingsType type;
  ChooserContextBase* (*get_context)(Profile*);
  const char* name;
  const char* ui_name_key;
};

namespace {

struct ContentSettingWithExceptions {
  ContentSettingWithExceptions(bool otr, UserMetricsAction action)
      : has_otr_exceptions(otr), uma(action) {}
  bool has_otr_exceptions;
  UserMetricsAction uma;
};

struct ContentSettingsTypeNameEntry {
  ContentSettingsType type;
  const char* name;
};

// Maps from a secondary pattern to a setting.
typedef std::map<ContentSettingsPattern, ContentSetting>
    OnePatternSettings;
// Maps from a primary pattern/source pair to a OnePatternSettings. All the
// mappings in OnePatternSettings share the given primary pattern and source.
typedef std::map<std::pair<ContentSettingsPattern, std::string>,
                 OnePatternSettings>
    AllPatternsSettings;

// Maps from the UI string to the object it represents (for sorting purposes).
typedef std::multimap<std::string, const base::DictionaryValue*> SortedObjects;
// Maps from a secondary URL to the set of objects it has permission to access.
typedef std::map<GURL, SortedObjects> OneOriginObjects;
// Maps from a primary URL/source pair to a OneOriginObjects. All the mappings
// in OneOriginObjects share the given primary URL and source.
typedef std::map<std::pair<GURL, std::string>, OneOriginObjects>
    AllOriginObjects;

// The AppFilter is used in AddExceptionsGrantedByHostedApps() to choose
// extensions which should have their extent displayed.
typedef bool (*AppFilter)(const extensions::Extension& app,
                          content::BrowserContext* profile);

const char kExceptionsLearnMoreUrl[] =
    "https://support.google.com/chrome/?p=settings_manage_exceptions";

const char kSetting[] = "setting";
const char kOrigin[] = "origin";
const char kPolicyProviderId[] = "policy";
const char kSource[] = "source";
const char kAppName[] = "appName";
const char kAppId[] = "appId";
const char kEmbeddingOrigin[] = "embeddingOrigin";
const char kPreferencesSource[] = "preference";
const char kZoom[] = "zoom";
const char kObject[] = "object";
const char kObjectName[] = "objectName";

const ContentSettingsTypeNameEntry kContentSettingsTypeGroupNames[] = {
  {CONTENT_SETTINGS_TYPE_COOKIES, "cookies"},
  {CONTENT_SETTINGS_TYPE_IMAGES, "images"},
  {CONTENT_SETTINGS_TYPE_JAVASCRIPT, "javascript"},
  {CONTENT_SETTINGS_TYPE_PLUGINS, "plugins"},
  {CONTENT_SETTINGS_TYPE_POPUPS, "popups"},
  {CONTENT_SETTINGS_TYPE_GEOLOCATION, "location"},
  {CONTENT_SETTINGS_TYPE_NOTIFICATIONS, "notifications"},
  {CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE, "auto-select-certificate"},
  {CONTENT_SETTINGS_TYPE_FULLSCREEN, "fullscreen"},
  {CONTENT_SETTINGS_TYPE_MOUSELOCK, "mouselock"},
  {CONTENT_SETTINGS_TYPE_PROTOCOL_HANDLERS, "register-protocol-handler"},
  {CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC, "media-stream-mic"},
  {CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA, "media-stream-camera"},
  {CONTENT_SETTINGS_TYPE_PPAPI_BROKER, "ppapi-broker"},
  {CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS, "multiple-automatic-downloads"},
  {CONTENT_SETTINGS_TYPE_MIDI_SYSEX, "midi-sysex"},
  {CONTENT_SETTINGS_TYPE_PUSH_MESSAGING, "push-messaging"},
  {CONTENT_SETTINGS_TYPE_SSL_CERT_DECISIONS, "ssl-cert-decisions"},
#if defined(OS_CHROMEOS)
  {CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER, "protectedContent"},
#endif
  {CONTENT_SETTINGS_TYPE_KEYGEN, "keygen"},
};

ChooserContextBase* GetUsbChooserContext(Profile* profile) {
  return UsbChooserContextFactory::GetForProfile(profile);
}

const ContentSettingsHandler::ChooserTypeNameEntry kChooserTypeGroupNames[] = {
    {CONTENT_SETTINGS_TYPE_USB_CHOOSER_DATA, &GetUsbChooserContext,
     "usb-devices", "name"},
};

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
        CONTENT_SETTINGS_TYPE_FULLSCREEN,
        ContentSettingWithExceptions(
            true,
            UserMetricsAction("Options_DefaultFullScreenSettingChanged"))));
    exceptions_info_map.insert(std::make_pair(
        CONTENT_SETTINGS_TYPE_MOUSELOCK,
        ContentSettingWithExceptions(
            true,
            UserMetricsAction("Options_DefaultMouseLockSettingChanged"))));
    exceptions_info_map.insert(std::make_pair(
        CONTENT_SETTINGS_TYPE_PPAPI_BROKER,
        ContentSettingWithExceptions(
            true,
            UserMetricsAction("Options_DefaultPPAPIBrokerSettingChanged"))));
    exceptions_info_map.insert(std::make_pair(
        CONTENT_SETTINGS_TYPE_PUSH_MESSAGING,
        ContentSettingWithExceptions(
            true,
            UserMetricsAction("Options_DefaultPushMessagingSettingChanged"))));
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
    exceptions_info_map.insert(std::make_pair(
        CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER,
        ContentSettingWithExceptions(
            true,
            UserMetricsAction(
                "Options_DefaultProtectedMediaIdentifierSettingChanged"))));
#endif
    exceptions_info_map.insert(std::make_pair(
        CONTENT_SETTINGS_TYPE_KEYGEN,
        ContentSettingWithExceptions(
            true, UserMetricsAction("Options_DefaultKeygenSettingChanged"))));

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
  }

  return exceptions_info_map;
}

content::BrowserContext* GetBrowserContext(content::WebUI* web_ui) {
  return web_ui->GetWebContents()->GetBrowserContext();
}

ContentSettingsType ContentSettingsTypeFromGroupName(const std::string& name) {
  for (size_t i = 0; i < arraysize(kContentSettingsTypeGroupNames); ++i) {
    if (name == kContentSettingsTypeGroupNames[i].name)
      return kContentSettingsTypeGroupNames[i].type;
  }

  NOTREACHED() << name << " is not a recognized content settings type.";
  return CONTENT_SETTINGS_TYPE_DEFAULT;
}

const ContentSettingsHandler::ChooserTypeNameEntry* ChooserTypeFromGroupName(
    const std::string& name) {
  for (const auto& chooser_type : kChooserTypeGroupNames) {
    if (chooser_type.name == name)
      return &chooser_type;
  }
  return nullptr;
}

// Create a DictionaryValue* that will act as a data source for a single row
// in a HostContentSettingsMap-controlled exceptions table (e.g., cookies).
scoped_ptr<base::DictionaryValue> GetExceptionForPage(
    const ContentSettingsPattern& pattern,
    const ContentSettingsPattern& secondary_pattern,
    const ContentSetting& setting,
    const std::string& provider_name) {
  base::DictionaryValue* exception = new base::DictionaryValue();
  exception->SetString(kOrigin, pattern.ToString());
  exception->SetString(kEmbeddingOrigin,
                       secondary_pattern == ContentSettingsPattern::Wildcard() ?
                           std::string() :
                           secondary_pattern.ToString());

  std::string setting_string =
      content_settings::ContentSettingToString(setting);
  DCHECK(!setting_string.empty());

  exception->SetString(kSetting, setting_string);
  exception->SetString(kSource, provider_name);
  return make_scoped_ptr(exception);
}

// Create a DictionaryValue* that will act as a data source for a single row
// in the Geolocation exceptions table.
scoped_ptr<base::DictionaryValue> GetGeolocationExceptionForPage(
    const ContentSettingsPattern& origin,
    const ContentSettingsPattern& embedding_origin,
    ContentSetting setting) {
  base::DictionaryValue* exception = new base::DictionaryValue();

  std::string setting_string =
      content_settings::ContentSettingToString(setting);
  DCHECK(!setting_string.empty());

  exception->SetString(kSetting, setting_string);
  exception->SetString(kOrigin, origin.ToString());
  exception->SetString(kEmbeddingOrigin, embedding_origin.ToString());
  return make_scoped_ptr(exception);
}

// Create a DictionaryValue* that will act as a data source for a single row
// in the desktop notifications exceptions table.
scoped_ptr<base::DictionaryValue> GetNotificationExceptionForPage(
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

  exception->SetString(kSetting, setting_string);
  exception->SetString(kOrigin, primary_pattern.ToString());
  exception->SetString(kEmbeddingOrigin, embedding_origin);
  exception->SetString(kSource, provider_name);
  return make_scoped_ptr(exception);
}

// Create a DictionaryValue* that will act as a data source for a single row
// in a chooser permission exceptions table.
scoped_ptr<base::DictionaryValue> GetChooserExceptionForPage(
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    const std::string& provider_name,
    const std::string& name,
    const base::DictionaryValue* object) {
  scoped_ptr<base::DictionaryValue> exception(new base::DictionaryValue());

  std::string setting_string =
      content_settings::ContentSettingToString(CONTENT_SETTING_DEFAULT);
  DCHECK(!setting_string.empty());

  exception->SetString(kSetting, setting_string);
  exception->SetString(kOrigin, requesting_origin.spec());
  exception->SetString(kEmbeddingOrigin, embedding_origin.spec());
  exception->SetString(kSource, provider_name);
  if (object) {
    exception->SetString(kObjectName, name);
    exception->Set(kObject, object->CreateDeepCopy());
  }
  return exception;
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
  base::DictionaryValue* exception = new base::DictionaryValue();

  std::string setting_string =
      content_settings::ContentSettingToString(CONTENT_SETTING_ALLOW);
  DCHECK(!setting_string.empty());

  exception->SetString(kSetting, setting_string);
  exception->SetString(kOrigin, url_pattern);
  exception->SetString(kEmbeddingOrigin, url_pattern);
  exception->SetString(kSource, "HostedApp");
  exception->SetString(kAppName, app.name());
  exception->SetString(kAppId, app.id());
  exceptions->Append(exception);
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
    {"detectException", IDS_EXCEPTIONS_DETECT_IMPORTANT_CONTENT_BUTTON},
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
    {"pluginsTabLabel", IDS_PLUGIN_TAB_LABEL},
    {"pluginsHeader", IDS_PLUGIN_HEADER},
    {"pluginsAllow", IDS_PLUGIN_ALLOW_RADIO},
    {"pluginsBlock", IDS_PLUGIN_BLOCK_RADIO},
    {"pluginsDetectImportantContent", IDS_PLUGIN_DETECT_RECOMMENDED_RADIO},
    {"manageIndividualPlugins", IDS_PLUGIN_MANAGE_INDIVIDUAL},
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
    // Fullscreen filter.
    {"fullscreenTabLabel", IDS_FULLSCREEN_TAB_LABEL},
    {"fullscreenHeader", IDS_FULLSCREEN_HEADER},
    // Mouse Lock filter.
    {"mouselockTabLabel", IDS_MOUSE_LOCK_TAB_LABEL},
    {"mouselockHeader", IDS_MOUSE_LOCK_HEADER},
    {"mouselockAllow", IDS_MOUSE_LOCK_ALLOW_RADIO},
    {"mouselockAsk", IDS_MOUSE_LOCK_ASK_RADIO},
    {"mouselockBlock", IDS_MOUSE_LOCK_BLOCK_RADIO},
#if defined(OS_CHROMEOS) || defined(OS_WIN)
    // Protected Content filter
    {"protectedContentTabLabel", IDS_PROTECTED_CONTENT_TAB_LABEL},
    {"protectedContentInfo", IDS_PROTECTED_CONTENT_INFO},
    {"protectedContentEnable", IDS_PROTECTED_CONTENT_ENABLE},
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
    // Multiple automatic downloads
    {"multipleAutomaticDownloadsTabLabel", IDS_AUTOMATIC_DOWNLOADS_TAB_LABEL},
    {"multipleAutomaticDownloadsHeader", IDS_AUTOMATIC_DOWNLOADS_TAB_LABEL},
    {"multipleAutomaticDownloadsAllow", IDS_AUTOMATIC_DOWNLOADS_ALLOW_RADIO},
    {"multipleAutomaticDownloadsAsk", IDS_AUTOMATIC_DOWNLOADS_ASK_RADIO},
    {"multipleAutomaticDownloadsBlock", IDS_AUTOMATIC_DOWNLOADS_BLOCK_RADIO},
    // MIDI system exclusive messages
    {"midiSysexHeader", IDS_MIDI_SYSEX_TAB_LABEL},
    {"midiSysExAllow", IDS_MIDI_SYSEX_ALLOW_RADIO},
    {"midiSysExAsk", IDS_MIDI_SYSEX_ASK_RADIO},
    {"midiSysExBlock", IDS_MIDI_SYSEX_BLOCK_RADIO},
    // Push messaging strings
    {"pushMessagingHeader", IDS_PUSH_MESSAGES_TAB_LABEL},
    {"pushMessagingAllow", IDS_PUSH_MESSSAGING_ALLOW_RADIO},
    {"pushMessagingAsk", IDS_PUSH_MESSSAGING_ASK_RADIO},
    {"pushMessagingBlock", IDS_PUSH_MESSSAGING_BLOCK_RADIO},
    {"usbDevicesHeader", IDS_USB_DEVICES_HEADER_AND_TAB_LABEL},
    {"usbDevicesManage", IDS_USB_DEVICES_MANAGE_BUTTON},
    {"zoomlevelsHeader", IDS_ZOOMLEVELS_HEADER_AND_TAB_LABEL},
    {"zoomLevelsManage", IDS_ZOOMLEVELS_MANAGE_BUTTON},
    // Keygen filter.
    {"keygenTabLabel", IDS_KEYGEN_TAB_LABEL},
    {"keygenHeader", IDS_KEYGEN_HEADER},
    {"keygenAllow", IDS_KEYGEN_ALLOW_RADIO},
    {"keygenBlock", IDS_KEYGEN_DONOTALLOW_RADIO},
  };

  RegisterStrings(localized_strings, resources, arraysize(resources));

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
  RegisterTitle(localized_strings, "plugins",
                IDS_PLUGIN_TAB_LABEL);
  RegisterTitle(localized_strings, "popups",
                IDS_POPUP_TAB_LABEL);
  RegisterTitle(localized_strings, "location",
                IDS_GEOLOCATION_TAB_LABEL);
  RegisterTitle(localized_strings, "notifications",
                IDS_NOTIFICATIONS_TAB_LABEL);
  RegisterTitle(localized_strings, "fullscreen",
                IDS_FULLSCREEN_TAB_LABEL);
  RegisterTitle(localized_strings, "mouselock",
                IDS_MOUSE_LOCK_TAB_LABEL);
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
  RegisterTitle(localized_strings, "zoomlevels",
                IDS_ZOOMLEVELS_HEADER_AND_TAB_LABEL);
  RegisterTitle(localized_strings, "keygen", IDS_KEYGEN_TAB_LABEL);

  localized_strings->SetString("exceptionsLearnMoreUrl",
                               kExceptionsLearnMoreUrl);
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
    auto map = HostContentSettingsMapFactory::GetForProfile(
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

  // Fullscreen and mouselock settings are not shown in simplified fullscreen
  // mode (always allow).
  web_ui()->CallJavascriptFunction(
      "ContentSettings.setExclusiveAccessVisible",
      base::FundamentalValue(
          !ExclusiveAccessManager::IsSimplifiedFullscreenUIEnabled()));
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
    if (ContainsKey(GetExceptionsInfoMap(), details.type()))
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
        web_ui()->CallJavascriptFunction(
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
  ContentSetting default_setting =
      GetContentSettingsMap()->GetDefaultContentSetting(type, &provider_id);

#if defined(ENABLE_PLUGINS)
  default_setting =
      content_settings::PluginsFieldTrial::EffectiveContentSetting(
          type, default_setting);
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
      provider_id = kPolicyProviderId;
    }
  }

  base::DictionaryValue filter_settings;

  std::string setting_string =
      content_settings::ContentSettingToString(default_setting);
  DCHECK(!setting_string.empty());

  filter_settings.SetString(ContentSettingsTypeToGroupName(type) + ".value",
                            setting_string);
  filter_settings.SetString(
      ContentSettingsTypeToGroupName(type) + ".managedBy", provider_id);

  web_ui()->CallJavascriptFunction(
      "ContentSettings.setContentFilterSettingsValue", filter_settings);
}

void ContentSettingsHandler::UpdateMediaSettingsFromPrefs(
    ContentSettingsType type) {
  PrefService* prefs = user_prefs::UserPrefs::Get(GetBrowserContext(web_ui()));
  MediaSettingsInfo::ForOneType& settings = media_settings_->forType(type);
  std::string policy_pref = (type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC)
      ? prefs::kAudioCaptureAllowed
      : prefs::kVideoCaptureAllowed;

  settings.policy_disable = !prefs->GetBoolean(policy_pref) &&
      prefs->IsManagedPreference(policy_pref);
  settings.default_setting =
      GetContentSettingsMap()->GetDefaultContentSetting(type, NULL);
  settings.default_setting_initialized = true;

  UpdateFlashMediaLinksVisibility(type);
  UpdateMediaDeviceDropdownVisibility(type);
}

void ContentSettingsHandler::UpdateHandlersEnabledRadios() {
  base::FundamentalValue handlers_enabled(
      GetProtocolHandlerRegistry()->enabled());

  web_ui()->CallJavascriptFunction(
      "ContentSettings.updateHandlersEnabledRadios",
      handlers_enabled);
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
  AllPatternsSettings all_patterns_settings;
  for (ContentSettingsForOneType::iterator i = all_settings.begin();
       i != all_settings.end(); ++i) {
    // Don't add default settings.
    if (i->primary_pattern == ContentSettingsPattern::Wildcard() &&
        i->secondary_pattern == ContentSettingsPattern::Wildcard() &&
        i->source != kPreferencesSource) {
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

  for (AllPatternsSettings::iterator i = all_patterns_settings.begin();
       i != all_patterns_settings.end(); ++i) {
    const ContentSettingsPattern& primary_pattern = i->first.first;
    const OnePatternSettings& one_settings = i->second;

    OnePatternSettings::const_iterator parent =
        one_settings.find(primary_pattern);

    // Add the "parent" entry for the non-embedded setting.
    ContentSetting parent_setting =
        parent == one_settings.end() ? CONTENT_SETTING_DEFAULT : parent->second;
    exceptions.Append(GetGeolocationExceptionForPage(primary_pattern,
                                                     primary_pattern,
                                                     parent_setting));

    // Add the "children" for any embedded settings.
    for (OnePatternSettings::const_iterator j = one_settings.begin();
         j != one_settings.end();
         ++j) {
      // Skip the non-embedded setting which we already added above.
      if (j == parent)
        continue;

      exceptions.Append(GetGeolocationExceptionForPage(
          primary_pattern, j->first, j->second));
    }
  }

  base::StringValue type_string(
      ContentSettingsTypeToGroupName(CONTENT_SETTINGS_TYPE_GEOLOCATION));
  web_ui()->CallJavascriptFunction("ContentSettings.setExceptions",
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
        i->source != kPreferencesSource) {
      continue;
    }

    exceptions.Append(
        GetNotificationExceptionForPage(i->primary_pattern,
                                        i->secondary_pattern,
                                        i->setting,
                                        i->source));
  }

  base::StringValue type_string(
      ContentSettingsTypeToGroupName(CONTENT_SETTINGS_TYPE_NOTIFICATIONS));
  web_ui()->CallJavascriptFunction("ContentSettings.setExceptions",
                                   type_string, exceptions);

  // This is mainly here to keep this function ideologically parallel to
  // UpdateExceptionsViewFromHostContentSettingsMap().
  UpdateSettingDefaultFromModel(CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
}

void ContentSettingsHandler::CompareMediaExceptionsWithFlash(
    ContentSettingsType type) {
  MediaSettingsInfo::ForOneType& settings = media_settings_->forType(type);

  base::ListValue exceptions;
  GetExceptionsFromHostContentSettingsMap(
      GetContentSettingsMap(),
      type,
      &exceptions);

  settings.exceptions.clear();
  for (base::ListValue::const_iterator entry = exceptions.begin();
       entry != exceptions.end(); ++entry) {
    base::DictionaryValue* dict = nullptr;
    bool valid_dict = (*entry)->GetAsDictionary(&dict);
    DCHECK(valid_dict);

    std::string origin;
    std::string setting;
    dict->GetString(kOrigin, &origin);
    dict->GetString(kSetting, &setting);

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
  for (const ChooserTypeNameEntry& chooser_type : kChooserTypeGroupNames)
    UpdateChooserExceptionsViewFromModel(chooser_type);
}

void ContentSettingsHandler::UpdateAllOTRChooserExceptionsViewsFromModel() {
  for (const ChooserTypeNameEntry& chooser_type : kChooserTypeGroupNames)
    UpdateOTRChooserExceptionsViewFromModel(chooser_type);
}

void ContentSettingsHandler::UpdateChooserExceptionsViewFromModel(
    const ChooserTypeNameEntry& chooser_type) {
  base::ListValue exceptions;
  GetChooserExceptionsFromProfile(false, chooser_type, &exceptions);
  base::StringValue type_string(chooser_type.name);
  web_ui()->CallJavascriptFunction("ContentSettings.setExceptions", type_string,
                                   exceptions);

  UpdateOTRChooserExceptionsViewFromModel(chooser_type);
}

void ContentSettingsHandler::UpdateOTRChooserExceptionsViewFromModel(
    const ChooserTypeNameEntry& chooser_type) {
  if (!Profile::FromWebUI(web_ui())->HasOffTheRecordProfile())
    return;

  base::ListValue exceptions;
  GetChooserExceptionsFromProfile(true, chooser_type, &exceptions);
  base::StringValue type_string(chooser_type.name);
  web_ui()->CallJavascriptFunction("ContentSettings.setOTRExceptions",
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
    scoped_ptr<base::DictionaryValue> exception(new base::DictionaryValue);
    switch (i->mode) {
      case content::HostZoomMap::ZOOM_CHANGED_FOR_HOST: {
        exception->SetString(kOrigin, i->host);
        std::string host = i->host;
        if (host == content::kUnreachableWebDataURL) {
          host =
              l10n_util::GetStringUTF8(IDS_ZOOMLEVELS_CHROME_ERROR_PAGES_LABEL);
        }
        exception->SetString(kOrigin, host);
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

    exception->SetString(kSetting, setting_string);

    // Calculate the zoom percent from the factor. Round up to the nearest whole
    // number.
    int zoom_percent = static_cast<int>(
        content::ZoomLevelToZoomFactor(i->zoom_level) * 100 + 0.5);
    exception->SetString(
        kZoom,
        l10n_util::GetStringFUTF16(IDS_ZOOM_PERCENT,
                                   base::IntToString16(zoom_percent)));
    exception->SetString(kSource, kPreferencesSource);
    // Append the new entry to the list and map.
    zoom_levels_exceptions.Append(exception.release());
  }

  base::StringValue type_string(kZoomContentType);
  web_ui()->CallJavascriptFunction("ContentSettings.setExceptions",
                                   type_string, zoom_levels_exceptions);
}

void ContentSettingsHandler::UpdateExceptionsViewFromHostContentSettingsMap(
    ContentSettingsType type) {
  base::ListValue exceptions;
  GetExceptionsFromHostContentSettingsMap(
      GetContentSettingsMap(), type, &exceptions);
  base::StringValue type_string(ContentSettingsTypeToGroupName(type));
  web_ui()->CallJavascriptFunction("ContentSettings.setExceptions", type_string,
                                   exceptions);

  UpdateExceptionsViewFromOTRHostContentSettingsMap(type);

  // TODO(koz): The default for fullscreen is always 'ask'.
  // http://crbug.com/104683
  if (type == CONTENT_SETTINGS_TYPE_FULLSCREEN)
    return;

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
  const HostContentSettingsMap* otr_settings_map = GetOTRContentSettingsMap();
  if (!otr_settings_map)
    return;
  base::ListValue exceptions;
  GetExceptionsFromHostContentSettingsMap(otr_settings_map, type, &exceptions);
  base::StringValue type_string(ContentSettingsTypeToGroupName(type));
  web_ui()->CallJavascriptFunction("ContentSettings.setOTRExceptions",
                                   type_string, exceptions);
}

void ContentSettingsHandler::GetPolicyAllowedUrls(
    ContentSettingsType type,
    std::vector<scoped_ptr<base::DictionaryValue>>* exceptions) {
  DCHECK(type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC ||
         type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA);

  PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();
  const base::ListValue* policy_urls = prefs->GetList(
      type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC
          ? prefs::kAudioCaptureAllowedUrls
          : prefs::kVideoCaptureAllowedUrls);

  // Convert the URLs to |ContentSettingsPattern|s. Ignore any invalid ones.
  std::vector<ContentSettingsPattern> patterns;
  for (const base::Value* entry : *policy_urls) {
    std::string url;
    bool valid_string = entry->GetAsString(&url);
    if (!valid_string)
      continue;

    ContentSettingsPattern pattern = ContentSettingsPattern::FromString(url);
    if (!pattern.IsValid())
      continue;

    patterns.push_back(pattern);
  }

  // The patterns are shown in the UI in a reverse order defined by
  // |ContentSettingsPattern::operator<|.
  std::sort(
      patterns.begin(), patterns.end(), std::greater<ContentSettingsPattern>());

  for (const ContentSettingsPattern& pattern : patterns) {
    exceptions->push_back(GetExceptionForPage(pattern, ContentSettingsPattern(),
                                              CONTENT_SETTING_ALLOW,
                                              kPolicyProviderId));
  }
}

void ContentSettingsHandler::GetExceptionsFromHostContentSettingsMap(
    const HostContentSettingsMap* map,
    ContentSettingsType type,
    base::ListValue* exceptions) {
  ContentSettingsForOneType entries;
  map->GetSettingsForOneType(type, std::string(), &entries);
  // Group settings by primary_pattern.
  AllPatternsSettings all_patterns_settings;
  for (ContentSettingsForOneType::iterator i = entries.begin();
       i != entries.end(); ++i) {
    // Don't add default settings.
    if (i->primary_pattern == ContentSettingsPattern::Wildcard() &&
        i->secondary_pattern == ContentSettingsPattern::Wildcard() &&
        i->source != kPreferencesSource) {
      continue;
    }

    // Off-the-record HostContentSettingsMap contains incognito content settings
    // as well as normal content settings. Here, we use the incongnito settings
    // only.
    if (map->is_off_the_record() && !i->incognito)
      continue;

    all_patterns_settings[std::make_pair(i->primary_pattern, i->source)]
        [i->secondary_pattern] = i->setting;
  }

  // Keep the exceptions sorted by provider so they will be displayed in
  // precedence order.
  std::vector<scoped_ptr<base::DictionaryValue>>
      all_provider_exceptions[HostContentSettingsMap::NUM_PROVIDER_TYPES];

  // |all_patterns_settings| is sorted from the lowest precedence pattern to
  // the highest (see operator< in ContentSettingsPattern), so traverse it in
  // reverse to show the patterns with the highest precedence (the more specific
  // ones) on the top.
  for (AllPatternsSettings::reverse_iterator i = all_patterns_settings.rbegin();
       i != all_patterns_settings.rend();
       ++i) {
    const ContentSettingsPattern& primary_pattern = i->first.first;
    const OnePatternSettings& one_settings = i->second;

    // The "parent" entry either has an identical primary and secondary pattern,
    // or has a wildcard secondary. The two cases are indistinguishable in the
    // UI.
    OnePatternSettings::const_iterator parent =
        one_settings.find(primary_pattern);
    if (parent == one_settings.end())
      parent = one_settings.find(ContentSettingsPattern::Wildcard());

    const std::string& source = i->first.second;
    auto& this_provider_exceptions = all_provider_exceptions
        [HostContentSettingsMap::GetProviderTypeFromSource(source)];

    // Add the "parent" entry for the non-embedded setting.
    ContentSetting parent_setting =
        parent == one_settings.end() ? CONTENT_SETTING_DEFAULT : parent->second;
    const ContentSettingsPattern& secondary_pattern =
        parent == one_settings.end() ? primary_pattern : parent->first;
    this_provider_exceptions.push_back(GetExceptionForPage(
        primary_pattern, secondary_pattern, parent_setting, source));

    // Add the "children" for any embedded settings.
    for (OnePatternSettings::const_iterator j = one_settings.begin();
         j != one_settings.end(); ++j) {
      // Skip the non-embedded setting which we already added above.
      if (j == parent)
        continue;

      ContentSetting content_setting = j->second;
      this_provider_exceptions.push_back(GetExceptionForPage(
          primary_pattern, j->first, content_setting, source));
    }
  }

  // For camera and microphone, we do not have policy exceptions, but we do have
  // the policy-set allowed URLs, which should be displayed in the same manner.
  if (type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC ||
      type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA) {
    auto& policy_exceptions = all_provider_exceptions
        [HostContentSettingsMap::GetProviderTypeFromSource(kPolicyProviderId)];
    DCHECK(policy_exceptions.empty());
    GetPolicyAllowedUrls(type, &policy_exceptions);
  }

  for (auto& one_provider_exceptions : all_provider_exceptions) {
    for (auto& exception : one_provider_exceptions)
      exceptions->Append(std::move(exception));
  }
}

void ContentSettingsHandler::GetChooserExceptionsFromProfile(
    bool incognito,
    const ChooserTypeNameEntry& chooser_type,
    base::ListValue* exceptions) {
  Profile* profile = Profile::FromWebUI(web_ui());
  if (incognito) {
    if (profile->HasOffTheRecordProfile())
      profile = profile->GetOffTheRecordProfile();
    else
      return;
  }

  ChooserContextBase* chooser_context = chooser_type.get_context(profile);
  std::vector<scoped_ptr<ChooserContextBase::Object>> objects =
      chooser_context->GetAllGrantedObjects();
  AllOriginObjects all_origin_objects;
  for (const auto& object : objects) {
    std::string name;
    bool found = object->object.GetString(chooser_type.ui_name_key, &name);
    DCHECK(found);
    // It is safe for this structure to hold references into |objects| because
    // they are both destroyed at the end of this function.
    all_origin_objects[make_pair(object->requesting_origin,
                                 object->source)][object->embedding_origin]
        .insert(make_pair(name, &object->object));
  }

  // Keep the exceptions sorted by provider so they will be displayed in
  // precedence order.
  std::vector<scoped_ptr<base::DictionaryValue>>
      all_provider_exceptions[HostContentSettingsMap::NUM_PROVIDER_TYPES];

  for (const auto& all_origin_objects_entry : all_origin_objects) {
    const GURL& requesting_origin = all_origin_objects_entry.first.first;
    const std::string& source = all_origin_objects_entry.first.second;
    const OneOriginObjects& one_origin_objects =
        all_origin_objects_entry.second;

    auto& this_provider_exceptions = all_provider_exceptions
        [HostContentSettingsMap::GetProviderTypeFromSource(source)];

    // Add entries for any non-embedded origins.
    bool has_embedded_entries = false;
    for (const auto& one_origin_objects_entry : one_origin_objects) {
      const GURL& embedding_origin = one_origin_objects_entry.first;
      const SortedObjects& sorted_objects = one_origin_objects_entry.second;

      // Skip the embedded settings which will be added below.
      if (requesting_origin != embedding_origin) {
        has_embedded_entries = true;
        continue;
      }

      for (const auto& sorted_objects_entry : sorted_objects) {
        this_provider_exceptions.push_back(GetChooserExceptionForPage(
            requesting_origin, embedding_origin, source,
            sorted_objects_entry.first, sorted_objects_entry.second));
      }
    }

    if (has_embedded_entries) {
      // Add a "parent" entry that simply acts as a heading for all entries
      // where |requesting_origin| has been embedded.
      this_provider_exceptions.push_back(
          GetChooserExceptionForPage(requesting_origin, requesting_origin,
                                     source, std::string(), nullptr));

      // Add the "children" for any embedded settings.
      for (const auto& one_origin_objects_entry : one_origin_objects) {
        const GURL& embedding_origin = one_origin_objects_entry.first;
        const SortedObjects& sorted_objects = one_origin_objects_entry.second;

        // Skip the non-embedded setting which we already added above.
        if (requesting_origin == embedding_origin)
          continue;

        for (const auto& sorted_objects_entry : sorted_objects) {
          this_provider_exceptions.push_back(GetChooserExceptionForPage(
              requesting_origin, embedding_origin, source,
              sorted_objects_entry.first, sorted_objects_entry.second));
        }
      }
    }
  }

  for (auto& one_provider_exceptions : all_provider_exceptions) {
    for (auto& exception : one_provider_exceptions)
      exceptions->Append(std::move(exception));
  }
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
  std::string secondary_pattern;
  if (args->GetSize() >= 4U) {
    rv = args->GetString(3, &secondary_pattern);
    DCHECK(rv);
  }

  HostContentSettingsMap* settings_map =
      mode == "normal" ? GetContentSettingsMap() :
                         GetOTRContentSettingsMap();
  if (settings_map) {
    settings_map->SetContentSetting(
        ContentSettingsPattern::FromString(pattern),
        secondary_pattern.empty()
            ? ContentSettingsPattern::Wildcard()
            : ContentSettingsPattern::FromString(secondary_pattern),
        type, std::string(), CONTENT_SETTING_DEFAULT);
  }
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
    const ChooserTypeNameEntry* chooser_type,
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

  ContentSettingsType content_type = ContentSettingsTypeFromGroupName(group);
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

  const ChooserTypeNameEntry* chooser_type =
      ChooserTypeFromGroupName(type_string);
  if (chooser_type) {
    RemoveChooserException(chooser_type, args);
    return;
  }

  ContentSettingsType type = ContentSettingsTypeFromGroupName(type_string);
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

  ContentSettingsType type = ContentSettingsTypeFromGroupName(type_string);

  if (type == CONTENT_SETTINGS_TYPE_GEOLOCATION ||
      type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC ||
      type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA) {
    NOTREACHED();
  } else {
    HostContentSettingsMap* settings_map =
        mode == "normal" ? GetContentSettingsMap() :
                           GetOTRContentSettingsMap();

    // The settings map could be null if the mode was OTR but the OTR profile
    // got destroyed before we received this message.
    if (!settings_map)
      return;

    ContentSetting setting_type;
    bool result =
        content_settings::ContentSettingFromString(setting, &setting_type);
    DCHECK(result);

    settings_map->SetContentSetting(ContentSettingsPattern::FromString(pattern),
                                    ContentSettingsPattern::Wildcard(),
                                    type,
                                    std::string(),
                                    setting_type);
  }
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

  web_ui()->CallJavascriptFunction(
      "ContentSettings.patternValidityCheckComplete",
      base::StringValue(type_string),
      base::StringValue(mode_string),
      base::StringValue(pattern_string),
      base::FundamentalValue(pattern.IsValid()));
}

// static
std::string ContentSettingsHandler::ContentSettingsTypeToGroupName(
    ContentSettingsType type) {
  for (size_t i = 0; i < arraysize(kContentSettingsTypeGroupNames); ++i) {
    if (type == kContentSettingsTypeGroupNames[i].type)
      return kContentSettingsTypeGroupNames[i].name;
  }

  NOTREACHED();
  return std::string();
}

HostContentSettingsMap* ContentSettingsHandler::GetContentSettingsMap() {
  return HostContentSettingsMapFactory::GetForProfile(
      Profile::FromWebUI(web_ui()));
}

ProtocolHandlerRegistry* ContentSettingsHandler::GetProtocolHandlerRegistry() {
  return ProtocolHandlerRegistryFactory::GetForBrowserContext(
      GetBrowserContext(web_ui()));
}

HostContentSettingsMap*
    ContentSettingsHandler::GetOTRContentSettingsMap() {
  Profile* profile = Profile::FromWebUI(web_ui());
  if (profile->HasOffTheRecordProfile())
    return HostContentSettingsMapFactory::GetForProfile(
        profile->GetOffTheRecordProfile());
  return NULL;
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
    web_ui()->CallJavascriptFunction(
        "ContentSettings.showMediaPepperFlashLink",
        base::StringValue(
            link_type == DEFAULT_SETTING ? "default" : "exceptions"),
        base::StringValue(
            content_type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC
                ? "mic"
                : "camera"),
        base::FundamentalValue(show));
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

  web_ui()->CallJavascriptFunction(
      "ContentSettings.setDevicesMenuVisibility",
      base::StringValue(ContentSettingsTypeToGroupName(type)),
      base::FundamentalValue(!settings.policy_disable));
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
  web_ui()->CallJavascriptFunction(
      "ContentSettings.enableProtectedContentExceptions",
      base::FundamentalValue(enable_exceptions));
}

}  // namespace options
