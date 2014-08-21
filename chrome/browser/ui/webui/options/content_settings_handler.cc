// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/content_settings_handler.h"

#include <algorithm>
#include <map>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/content_settings_details.h"
#include "chrome/browser/content_settings/content_settings_provider.h"
#include "chrome/browser/content_settings/content_settings_utils.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "chrome/browser/notifications/desktop_notification_profile_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/content_settings_pattern.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/google/core/browser/google_util.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/page_zoom.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "components/user_manager/user_manager.h"
#endif

using base::UserMetricsAction;
using content_settings::ContentSettingToString;
using content_settings::ContentSettingFromString;
using extensions::APIPermission;

namespace {

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

// The AppFilter is used in AddExceptionsGrantedByHostedApps() to choose
// extensions which should have their extent displayed.
typedef bool (*AppFilter)(const extensions::Extension& app,
                          content::BrowserContext* profile);

const char kExceptionsLearnMoreUrl[] =
    "https://support.google.com/chrome/?p=settings_manage_exceptions";

const char* kSetting = "setting";
const char* kOrigin = "origin";
const char* kSource = "source";
const char* kAppName = "appName";
const char* kAppId = "appId";
const char* kEmbeddingOrigin = "embeddingOrigin";
const char* kPreferencesSource = "preference";
const char* kVideoSetting = "video";
const char* kZoom = "zoom";

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
  {CONTENT_SETTINGS_TYPE_MEDIASTREAM, "media-stream"},
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
};

// A pseudo content type. We use it to display data like a content setting even
// though it is not a real content setting.
const char* kZoomContentType = "zoomlevels";

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

// Create a DictionaryValue* that will act as a data source for a single row
// in a HostContentSettingsMap-controlled exceptions table (e.g., cookies).
// Ownership of the pointer is passed to the caller.
base::DictionaryValue* GetExceptionForPage(
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
  exception->SetString(kSetting, ContentSettingToString(setting));
  exception->SetString(kSource, provider_name);
  return exception;
}

// Create a DictionaryValue* that will act as a data source for a single row
// in the Geolocation exceptions table. Ownership of the pointer is passed to
// the caller.
base::DictionaryValue* GetGeolocationExceptionForPage(
    const ContentSettingsPattern& origin,
    const ContentSettingsPattern& embedding_origin,
    ContentSetting setting) {
  base::DictionaryValue* exception = new base::DictionaryValue();
  exception->SetString(kSetting, ContentSettingToString(setting));
  exception->SetString(kOrigin, origin.ToString());
  exception->SetString(kEmbeddingOrigin, embedding_origin.ToString());
  return exception;
}

// Create a DictionaryValue* that will act as a data source for a single row
// in the desktop notifications exceptions table. Ownership of the pointer is
// passed to the caller.
base::DictionaryValue* GetNotificationExceptionForPage(
    const ContentSettingsPattern& pattern,
    ContentSetting setting,
    const std::string& provider_name) {
  base::DictionaryValue* exception = new base::DictionaryValue();
  exception->SetString(kSetting, ContentSettingToString(setting));
  exception->SetString(kOrigin, pattern.ToString());
  exception->SetString(kSource, provider_name);
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
  exception->SetString(kSetting, ContentSettingToString(CONTENT_SETTING_ALLOW));
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

// Sort ZoomLevelChanges by host and scheme
// (a.com < http://a.com < https://a.com < b.com).
bool HostZoomSort(const content::HostZoomMap::ZoomLevelChange& a,
                  const content::HostZoomMap::ZoomLevelChange& b) {
  return a.host == b.host ? a.scheme < b.scheme : a.host < b.host;
}

}  // namespace

namespace options {

ContentSettingsHandler::MediaSettingsInfo::MediaSettingsInfo()
    : flash_default_setting(CONTENT_SETTING_DEFAULT),
      flash_settings_initialized(false),
      last_flash_refresh_request_id(0),
      show_flash_default_link(false),
      show_flash_exceptions_link(false),
      default_setting(CONTENT_SETTING_DEFAULT),
      policy_disable_audio(false),
      policy_disable_video(false),
      default_setting_initialized(false),
      exceptions_initialized(false) {
}

ContentSettingsHandler::MediaSettingsInfo::~MediaSettingsInfo() {
}

ContentSettingsHandler::ContentSettingsHandler() : observer_(this) {
}

ContentSettingsHandler::~ContentSettingsHandler() {
}

void ContentSettingsHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  static OptionsStringResource resources[] = {
    { "allowException", IDS_EXCEPTIONS_ALLOW_BUTTON },
    { "blockException", IDS_EXCEPTIONS_BLOCK_BUTTON },
    { "sessionException", IDS_EXCEPTIONS_SESSION_ONLY_BUTTON },
    { "askException", IDS_EXCEPTIONS_ASK_BUTTON },
    { "otr_exceptions_explanation", IDS_EXCEPTIONS_OTR_LABEL },
    { "addNewExceptionInstructions", IDS_EXCEPTIONS_ADD_NEW_INSTRUCTIONS },
    { "manageExceptions", IDS_EXCEPTIONS_MANAGE },
    { "manage_handlers", IDS_HANDLERS_MANAGE },
    { "exceptionPatternHeader", IDS_EXCEPTIONS_PATTERN_HEADER },
    { "exceptionBehaviorHeader", IDS_EXCEPTIONS_ACTION_HEADER },
    { "exceptionZoomHeader", IDS_EXCEPTIONS_ZOOM_HEADER },
    { "embeddedOnHost", IDS_EXCEPTIONS_GEOLOCATION_EMBEDDED_ON_HOST },
    // Cookies filter.
    { "cookies_tab_label", IDS_COOKIES_TAB_LABEL },
    { "cookies_header", IDS_COOKIES_HEADER },
    { "cookies_allow", IDS_COOKIES_ALLOW_RADIO },
    { "cookies_block", IDS_COOKIES_BLOCK_RADIO },
    { "cookies_session_only", IDS_COOKIES_SESSION_ONLY_RADIO },
    { "cookies_block_3rd_party", IDS_COOKIES_BLOCK_3RDPARTY_CHKBOX },
    { "cookies_clear_when_close", IDS_COOKIES_CLEAR_WHEN_CLOSE_CHKBOX },
    { "cookies_lso_clear_when_close", IDS_COOKIES_LSO_CLEAR_WHEN_CLOSE_CHKBOX },
    { "cookies_show_cookies", IDS_COOKIES_SHOW_COOKIES_BUTTON },
    { "flash_storage_settings", IDS_FLASH_STORAGE_SETTINGS },
    { "flash_storage_url", IDS_FLASH_STORAGE_URL },
#if defined(ENABLE_GOOGLE_NOW)
    { "googleGeolocationAccessEnable",
       IDS_GEOLOCATION_GOOGLE_ACCESS_ENABLE_CHKBOX },
#endif
    // Image filter.
    { "images_tab_label", IDS_IMAGES_TAB_LABEL },
    { "images_header", IDS_IMAGES_HEADER },
    { "images_allow", IDS_IMAGES_LOAD_RADIO },
    { "images_block", IDS_IMAGES_NOLOAD_RADIO },
    // JavaScript filter.
    { "javascript_tab_label", IDS_JAVASCRIPT_TAB_LABEL },
    { "javascript_header", IDS_JAVASCRIPT_HEADER },
    { "javascript_allow", IDS_JS_ALLOW_RADIO },
    { "javascript_block", IDS_JS_DONOTALLOW_RADIO },
    // Plug-ins filter.
    { "plugins_tab_label", IDS_PLUGIN_TAB_LABEL },
    { "plugins_header", IDS_PLUGIN_HEADER },
    { "plugins_ask", IDS_PLUGIN_ASK_RADIO },
    { "plugins_allow", IDS_PLUGIN_LOAD_RADIO },
    { "plugins_block", IDS_PLUGIN_NOLOAD_RADIO },
    { "disableIndividualPlugins", IDS_PLUGIN_SELECTIVE_DISABLE },
    // Pop-ups filter.
    { "popups_tab_label", IDS_POPUP_TAB_LABEL },
    { "popups_header", IDS_POPUP_HEADER },
    { "popups_allow", IDS_POPUP_ALLOW_RADIO },
    { "popups_block", IDS_POPUP_BLOCK_RADIO },
    // Location filter.
    { "location_tab_label", IDS_GEOLOCATION_TAB_LABEL },
    { "location_header", IDS_GEOLOCATION_HEADER },
    { "location_allow", IDS_GEOLOCATION_ALLOW_RADIO },
    { "location_ask", IDS_GEOLOCATION_ASK_RADIO },
    { "location_block", IDS_GEOLOCATION_BLOCK_RADIO },
    { "set_by", IDS_GEOLOCATION_SET_BY_HOVER },
    // Notifications filter.
    { "notifications_tab_label", IDS_NOTIFICATIONS_TAB_LABEL },
    { "notifications_header", IDS_NOTIFICATIONS_HEADER },
    { "notifications_allow", IDS_NOTIFICATIONS_ALLOW_RADIO },
    { "notifications_ask", IDS_NOTIFICATIONS_ASK_RADIO },
    { "notifications_block", IDS_NOTIFICATIONS_BLOCK_RADIO },
    // Fullscreen filter.
    { "fullscreen_tab_label", IDS_FULLSCREEN_TAB_LABEL },
    { "fullscreen_header", IDS_FULLSCREEN_HEADER },
    // Mouse Lock filter.
    { "mouselock_tab_label", IDS_MOUSE_LOCK_TAB_LABEL },
    { "mouselock_header", IDS_MOUSE_LOCK_HEADER },
    { "mouselock_allow", IDS_MOUSE_LOCK_ALLOW_RADIO },
    { "mouselock_ask", IDS_MOUSE_LOCK_ASK_RADIO },
    { "mouselock_block", IDS_MOUSE_LOCK_BLOCK_RADIO },
#if defined(OS_CHROMEOS) || defined(OS_WIN)
    // Protected Content filter
    { "protectedContentTabLabel", IDS_PROTECTED_CONTENT_TAB_LABEL },
    { "protectedContentInfo", IDS_PROTECTED_CONTENT_INFO },
    { "protectedContentEnable", IDS_PROTECTED_CONTENT_ENABLE },
    { "protectedContent_header", IDS_PROTECTED_CONTENT_HEADER },
#endif  // defined(OS_CHROMEOS) || defined(OS_WIN)
    // Media stream capture device filter.
    { "mediaStreamTabLabel", IDS_MEDIA_STREAM_TAB_LABEL },
    { "media-stream_header", IDS_MEDIA_STREAM_HEADER },
    { "mediaStreamAsk", IDS_MEDIA_STREAM_ASK_RADIO },
    { "mediaStreamBlock", IDS_MEDIA_STREAM_BLOCK_RADIO },
    { "mediaStreamAudioAsk", IDS_MEDIA_STREAM_ASK_AUDIO_ONLY_RADIO },
    { "mediaStreamAudioBlock", IDS_MEDIA_STREAM_BLOCK_AUDIO_ONLY_RADIO },
    { "mediaStreamVideoAsk", IDS_MEDIA_STREAM_ASK_VIDEO_ONLY_RADIO },
    { "mediaStreamVideoBlock", IDS_MEDIA_STREAM_BLOCK_VIDEO_ONLY_RADIO },
    { "mediaStreamBubbleAudio", IDS_MEDIA_STREAM_AUDIO_MANAGED },
    { "mediaStreamBubbleVideo", IDS_MEDIA_STREAM_VIDEO_MANAGED },
    { "mediaAudioExceptionHeader", IDS_MEDIA_AUDIO_EXCEPTION_HEADER },
    { "mediaVideoExceptionHeader", IDS_MEDIA_VIDEO_EXCEPTION_HEADER },
    { "mediaPepperFlashDefaultDivergedLabel",
      IDS_MEDIA_PEPPER_FLASH_DEFAULT_DIVERGED_LABEL },
    { "mediaPepperFlashExceptionsDivergedLabel",
      IDS_MEDIA_PEPPER_FLASH_EXCEPTIONS_DIVERGED_LABEL },
    { "mediaPepperFlashChangeLink", IDS_MEDIA_PEPPER_FLASH_CHANGE_LINK },
    { "mediaPepperFlashGlobalPrivacyURL", IDS_FLASH_GLOBAL_PRIVACY_URL },
    { "mediaPepperFlashWebsitePrivacyURL", IDS_FLASH_WEBSITE_PRIVACY_URL },
    // PPAPI broker filter.
    { "ppapi-broker_header", IDS_PPAPI_BROKER_HEADER },
    { "ppapiBrokerTabLabel", IDS_PPAPI_BROKER_TAB_LABEL },
    { "ppapi_broker_allow", IDS_PPAPI_BROKER_ALLOW_RADIO },
    { "ppapi_broker_ask", IDS_PPAPI_BROKER_ASK_RADIO },
    { "ppapi_broker_block", IDS_PPAPI_BROKER_BLOCK_RADIO },
    // Multiple automatic downloads
    { "multiple-automatic-downloads_header",
      IDS_AUTOMATIC_DOWNLOADS_TAB_LABEL },
    { "multiple-automatic-downloads_allow",
      IDS_AUTOMATIC_DOWNLOADS_ALLOW_RADIO },
    { "multiple-automatic-downloads_ask",
      IDS_AUTOMATIC_DOWNLOADS_ASK_RADIO },
    { "multiple-automatic-downloads_block",
      IDS_AUTOMATIC_DOWNLOADS_BLOCK_RADIO },
    // MIDI system exclusive messages
    { "midi-sysex_header", IDS_MIDI_SYSEX_TAB_LABEL },
    { "midiSysExAllow", IDS_MIDI_SYSEX_ALLOW_RADIO },
    { "midiSysExAsk", IDS_MIDI_SYSEX_ASK_RADIO },
    { "midiSysExBlock", IDS_MIDI_SYSEX_BLOCK_RADIO },
    // Push messaging strings
    { "push-messaging_header", IDS_PUSH_MESSAGES_TAB_LABEL },
    { "pushMessagingAllow", IDS_PUSH_MESSSAGING_ALLOW_RADIO },
    { "pushMessagingAsk", IDS_PUSH_MESSSAGING_ASK_RADIO },
    { "pushMessagingBlock", IDS_PUSH_MESSSAGING_BLOCK_RADIO },
    { "zoomlevels_header", IDS_ZOOMLEVELS_HEADER_AND_TAB_LABEL },
    { "zoomLevelsManage", IDS_ZOOMLEVELS_MANAGE_BUTTON },
  };

  RegisterStrings(localized_strings, resources, arraysize(resources));
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
  RegisterTitle(localized_strings, "media-stream",
                IDS_MEDIA_STREAM_TAB_LABEL);
  RegisterTitle(localized_strings, "ppapi-broker",
                IDS_PPAPI_BROKER_TAB_LABEL);
  RegisterTitle(localized_strings, "multiple-automatic-downloads",
                IDS_AUTOMATIC_DOWNLOADS_TAB_LABEL);
  RegisterTitle(localized_strings, "midi-sysex",
                IDS_MIDI_SYSEX_TAB_LABEL);
  RegisterTitle(localized_strings, "zoomlevels",
                IDS_ZOOMLEVELS_HEADER_AND_TAB_LABEL);

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

  notification_registrar_.Add(
      this, chrome::NOTIFICATION_DESKTOP_NOTIFICATION_SETTINGS_CHANGED,
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
      base::Bind(&ContentSettingsHandler::UpdateMediaSettingsView,
                 base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kVideoCaptureAllowed,
      base::Bind(&ContentSettingsHandler::UpdateMediaSettingsView,
                 base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kEnableDRM,
      base::Bind(
          &ContentSettingsHandler::UpdateProtectedContentExceptionsButton,
          base::Unretained(this)));

  content::HostZoomMap* host_zoom_map =
      content::HostZoomMap::GetForBrowserContext(context);
  host_zoom_map_subscription_ =
      host_zoom_map->AddZoomLevelChangedCallback(
          base::Bind(&ContentSettingsHandler::OnZoomLevelChanged,
                     base::Unretained(this)));

  flash_settings_manager_.reset(new PepperFlashSettingsManager(this, context));
  observer_.Add(Profile::FromWebUI(web_ui())->GetHostContentSettingsMap());
}

void ContentSettingsHandler::InitializePage() {
  media_settings_ = MediaSettingsInfo();
  RefreshFlashMediaSettings();

  UpdateHandlersEnabledRadios();
  UpdateAllExceptionsViewsFromModel();
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
  if (details.update_all_types())
    UpdateAllExceptionsViewsFromModel();
  else
    UpdateExceptionsViewFromModel(details.type());
}

void ContentSettingsHandler::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_PROFILE_DESTROYED: {
      if (content::Source<Profile>(source).ptr()->IsOffTheRecord()) {
        web_ui()->CallJavascriptFunction(
            "ContentSettingsExceptionsArea.OTRProfileDestroyed");
      }
      break;
    }

    case chrome::NOTIFICATION_PROFILE_CREATED: {
      if (content::Source<Profile>(source).ptr()->IsOffTheRecord())
        UpdateAllOTRExceptionsViewsFromModel();
      break;
    }

    case chrome::NOTIFICATION_DESKTOP_NOTIFICATION_SETTINGS_CHANGED: {
      UpdateNotificationExceptionsView();
      break;
    }

    case chrome::NOTIFICATION_PROTOCOL_HANDLER_REGISTRY_CHANGED: {
      UpdateHandlersEnabledRadios();
      break;
    }
  }
}

void ContentSettingsHandler::OnGetPermissionSettingsCompleted(
    uint32 request_id,
    bool success,
    PP_Flash_BrowserOperations_Permission default_permission,
    const ppapi::FlashSiteSettings& sites) {
  if (success && request_id == media_settings_.last_flash_refresh_request_id) {
    media_settings_.flash_settings_initialized = true;
    media_settings_.flash_default_setting =
        PepperFlashContentSettingsUtils::FlashPermissionToContentSetting(
            default_permission);
    PepperFlashContentSettingsUtils::FlashSiteSettingsToMediaExceptions(
        sites, &media_settings_.flash_exceptions);
    PepperFlashContentSettingsUtils::SortMediaExceptions(
        &media_settings_.flash_exceptions);

    UpdateFlashMediaLinksVisibility();
  }
}

void ContentSettingsHandler::UpdateSettingDefaultFromModel(
    ContentSettingsType type) {
  base::DictionaryValue filter_settings;
  std::string provider_id;
  filter_settings.SetString(ContentSettingsTypeToGroupName(type) + ".value",
                            GetSettingDefaultFromModel(type, &provider_id));
  filter_settings.SetString(
      ContentSettingsTypeToGroupName(type) + ".managedBy", provider_id);

  web_ui()->CallJavascriptFunction(
      "ContentSettings.setContentFilterSettingsValue", filter_settings);
}

void ContentSettingsHandler::UpdateMediaSettingsView() {
  PrefService* prefs = user_prefs::UserPrefs::Get(GetBrowserContext(web_ui()));
  bool audio_disabled = !prefs->GetBoolean(prefs::kAudioCaptureAllowed) &&
      prefs->IsManagedPreference(prefs::kAudioCaptureAllowed);
  bool video_disabled = !prefs->GetBoolean(prefs::kVideoCaptureAllowed) &&
      prefs->IsManagedPreference(prefs::kVideoCaptureAllowed);

  media_settings_.policy_disable_audio = audio_disabled;
  media_settings_.policy_disable_video = video_disabled;
  media_settings_.default_setting =
      GetContentSettingsMap()->GetDefaultContentSetting(
          CONTENT_SETTINGS_TYPE_MEDIASTREAM, NULL);
  media_settings_.default_setting_initialized = true;
  UpdateFlashMediaLinksVisibility();

  base::DictionaryValue media_ui_settings;
  media_ui_settings.SetBoolean("cameraDisabled", video_disabled);
  media_ui_settings.SetBoolean("micDisabled", audio_disabled);

  // In case only video is enabled change the text appropriately.
  if (audio_disabled && !video_disabled) {
    media_ui_settings.SetString("askText", "mediaStreamVideoAsk");
    media_ui_settings.SetString("blockText", "mediaStreamVideoBlock");
    media_ui_settings.SetBoolean("showBubble", true);
    media_ui_settings.SetString("bubbleText", "mediaStreamBubbleAudio");

    web_ui()->CallJavascriptFunction("ContentSettings.updateMediaUI",
                                     media_ui_settings);
    return;
  }

  // In case only audio is enabled change the text appropriately.
  if (video_disabled && !audio_disabled) {
    base::DictionaryValue media_ui_settings;
    media_ui_settings.SetString("askText", "mediaStreamAudioAsk");
    media_ui_settings.SetString("blockText", "mediaStreamAudioBlock");
    media_ui_settings.SetBoolean("showBubble", true);
    media_ui_settings.SetString("bubbleText", "mediaStreamBubbleVideo");

    web_ui()->CallJavascriptFunction("ContentSettings.updateMediaUI",
                                     media_ui_settings);
    return;
  }

  if (audio_disabled && video_disabled) {
    // Fake policy controlled default because the user can not change anything
    // until both audio and video are blocked.
    base::DictionaryValue filter_settings;
    std::string group_name =
        ContentSettingsTypeToGroupName(CONTENT_SETTINGS_TYPE_MEDIASTREAM);
    filter_settings.SetString(group_name + ".value",
                              ContentSettingToString(CONTENT_SETTING_BLOCK));
    filter_settings.SetString(group_name + ".managedBy", "policy");
    web_ui()->CallJavascriptFunction(
        "ContentSettings.setContentFilterSettingsValue", filter_settings);
  }

  media_ui_settings.SetString("askText", "mediaStreamAsk");
  media_ui_settings.SetString("blockText", "mediaStreamBlock");
  media_ui_settings.SetBoolean("showBubble", false);
  media_ui_settings.SetString("bubbleText", std::string());

  web_ui()->CallJavascriptFunction("ContentSettings.updateMediaUI",
                                   media_ui_settings);
}

std::string ContentSettingsHandler::GetSettingDefaultFromModel(
    ContentSettingsType type, std::string* provider_id) {
  Profile* profile = Profile::FromWebUI(web_ui());
  ContentSetting default_setting;
  default_setting =
      profile->GetHostContentSettingsMap()->GetDefaultContentSetting(
          type, provider_id);

  return ContentSettingToString(default_setting);
}

void ContentSettingsHandler::UpdateHandlersEnabledRadios() {
  base::FundamentalValue handlers_enabled(
      GetProtocolHandlerRegistry()->enabled());

  web_ui()->CallJavascriptFunction(
      "ContentSettings.updateHandlersEnabledRadios",
      handlers_enabled);
}

void ContentSettingsHandler::UpdateAllExceptionsViewsFromModel() {
  for (int type = CONTENT_SETTINGS_TYPE_DEFAULT + 1;
       type < CONTENT_SETTINGS_NUM_TYPES; ++type) {
    UpdateExceptionsViewFromModel(static_cast<ContentSettingsType>(type));
  }
  // Zoom levels are not actually a content type so we need to handle them
  // separately.
  UpdateZoomLevelsExceptionsView();
}

void ContentSettingsHandler::UpdateAllOTRExceptionsViewsFromModel() {
  for (int type = CONTENT_SETTINGS_TYPE_DEFAULT + 1;
       type < CONTENT_SETTINGS_NUM_TYPES; ++type) {
    UpdateOTRExceptionsViewFromModel(static_cast<ContentSettingsType>(type));
  }
}

void ContentSettingsHandler::UpdateExceptionsViewFromModel(
    ContentSettingsType type) {
  switch (type) {
    case CONTENT_SETTINGS_TYPE_GEOLOCATION:
      UpdateGeolocationExceptionsView();
      break;
    case CONTENT_SETTINGS_TYPE_NOTIFICATIONS:
      UpdateNotificationExceptionsView();
      break;
    case CONTENT_SETTINGS_TYPE_MEDIASTREAM:
      UpdateMediaSettingsView();
      break;
    case CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC:
    case CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA:
      UpdateMediaExceptionsView();
      break;
    case CONTENT_SETTINGS_TYPE_MIXEDSCRIPT:
      // We don't yet support exceptions for mixed scripting.
      break;
    case CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE:
      // The content settings type CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE
      // is supposed to be set by policy only. Hence there is no user facing UI
      // for this content type and we skip it here.
      break;
    case CONTENT_SETTINGS_TYPE_PROTOCOL_HANDLERS:
      // The RPH settings are retrieved separately.
      break;
    case CONTENT_SETTINGS_TYPE_MIDI_SYSEX:
      UpdateMIDISysExExceptionsView();
      break;
    case CONTENT_SETTINGS_TYPE_SSL_CERT_DECISIONS:
      // The content settings type CONTENT_SETTINGS_TYPE_SSL_CERT_DECISIONS is
      // supposed to be set by flags and field trials only, thus there is no
      // user facing UI for this content type and we skip it here.
      break;
#if defined(OS_WIN)
    case CONTENT_SETTINGS_TYPE_METRO_SWITCH_TO_DESKTOP:
      break;
#endif
    default:
      UpdateExceptionsViewFromHostContentSettingsMap(type);
      break;
  }
}

void ContentSettingsHandler::UpdateOTRExceptionsViewFromModel(
    ContentSettingsType type) {
  switch (type) {
    case CONTENT_SETTINGS_TYPE_GEOLOCATION:
    case CONTENT_SETTINGS_TYPE_NOTIFICATIONS:
    case CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE:
    case CONTENT_SETTINGS_TYPE_MIXEDSCRIPT:
#if defined(OS_WIN)
    case CONTENT_SETTINGS_TYPE_METRO_SWITCH_TO_DESKTOP:
#endif
    case CONTENT_SETTINGS_TYPE_MEDIASTREAM:
    case CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC:
    case CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA:
    case CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS:
    case CONTENT_SETTINGS_TYPE_MIDI_SYSEX:
    case CONTENT_SETTINGS_TYPE_SSL_CERT_DECISIONS:
      break;
    default:
      UpdateExceptionsViewFromOTRHostContentSettingsMap(type);
      break;
  }
}

// TODO(estade): merge with GetExceptionsFromHostContentSettingsMap.
void ContentSettingsHandler::UpdateGeolocationExceptionsView() {
  Profile* profile = Profile::FromWebUI(web_ui());
  HostContentSettingsMap* map = profile->GetHostContentSettingsMap();

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
        GetNotificationExceptionForPage(i->primary_pattern, i->setting,
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

void ContentSettingsHandler::UpdateMediaExceptionsView() {
  base::ListValue media_exceptions;
  GetExceptionsFromHostContentSettingsMap(
      GetContentSettingsMap(),
      CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC,
      &media_exceptions);

  base::ListValue video_exceptions;
  GetExceptionsFromHostContentSettingsMap(
      GetContentSettingsMap(),
      CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA,
      &video_exceptions);

  // Merge the |video_exceptions| list to |media_exceptions| list.
  std::map<std::string, base::DictionaryValue*> entries_map;
  for (base::ListValue::const_iterator media_entry(media_exceptions.begin());
       media_entry != media_exceptions.end(); ++media_entry) {
    base::DictionaryValue* media_dict = NULL;
    if (!(*media_entry)->GetAsDictionary(&media_dict))
      NOTREACHED();

    media_dict->SetString(kVideoSetting,
                          ContentSettingToString(CONTENT_SETTING_ASK));

    std::string media_origin;
    media_dict->GetString(kOrigin, &media_origin);
    entries_map[media_origin] = media_dict;
  }

  for (base::ListValue::iterator video_entry = video_exceptions.begin();
       video_entry != video_exceptions.end(); ++video_entry) {
    base::DictionaryValue* video_dict = NULL;
    if (!(*video_entry)->GetAsDictionary(&video_dict))
      NOTREACHED();

    std::string video_origin;
    std::string video_setting;
    video_dict->GetString(kOrigin, &video_origin);
    video_dict->GetString(kSetting, &video_setting);

    std::map<std::string, base::DictionaryValue*>::iterator iter =
        entries_map.find(video_origin);
    if (iter == entries_map.end()) {
      base::DictionaryValue* exception = new base::DictionaryValue();
      exception->SetString(kOrigin, video_origin);
      exception->SetString(kSetting,
                           ContentSettingToString(CONTENT_SETTING_ASK));
      exception->SetString(kVideoSetting, video_setting);
      exception->SetString(kSource, kPreferencesSource);

      // Append the new entry to the list and map.
      media_exceptions.Append(exception);
      entries_map[video_origin] = exception;
    } else {
      // Modify the existing entry.
      iter->second->SetString(kVideoSetting, video_setting);
    }
  }

  media_settings_.exceptions.clear();
  for (base::ListValue::const_iterator media_entry = media_exceptions.begin();
       media_entry != media_exceptions.end(); ++media_entry) {
    base::DictionaryValue* media_dict = NULL;
    bool result = (*media_entry)->GetAsDictionary(&media_dict);
    DCHECK(result);

    std::string origin;
    std::string audio_setting;
    std::string video_setting;
    media_dict->GetString(kOrigin, &origin);
    media_dict->GetString(kSetting, &audio_setting);
    media_dict->GetString(kVideoSetting, &video_setting);
    media_settings_.exceptions.push_back(MediaException(
        ContentSettingsPattern::FromString(origin),
        ContentSettingFromString(audio_setting),
        ContentSettingFromString(video_setting)));
  }
  PepperFlashContentSettingsUtils::SortMediaExceptions(
      &media_settings_.exceptions);
  media_settings_.exceptions_initialized = true;
  UpdateFlashMediaLinksVisibility();

  base::StringValue type_string(
       ContentSettingsTypeToGroupName(CONTENT_SETTINGS_TYPE_MEDIASTREAM));
  web_ui()->CallJavascriptFunction("ContentSettings.setExceptions",
                                   type_string, media_exceptions);

  UpdateSettingDefaultFromModel(CONTENT_SETTINGS_TYPE_MEDIASTREAM);
}

void ContentSettingsHandler::UpdateMIDISysExExceptionsView() {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kEnableWebMIDI)) {
    web_ui()->CallJavascriptFunction(
        "ContentSettings.showExperimentalWebMIDISettings",
        base::FundamentalValue(true));
  }

  UpdateSettingDefaultFromModel(CONTENT_SETTINGS_TYPE_MIDI_SYSEX);
  UpdateExceptionsViewFromHostContentSettingsMap(
      CONTENT_SETTINGS_TYPE_MIDI_SYSEX);
}

void ContentSettingsHandler::UpdateZoomLevelsExceptionsView() {
  base::ListValue zoom_levels_exceptions;

  content::HostZoomMap* host_zoom_map =
      content::HostZoomMap::GetForBrowserContext(GetBrowserContext(web_ui()));
  content::HostZoomMap::ZoomLevelVector zoom_levels(
      host_zoom_map->GetAllZoomLevels());
  std::sort(zoom_levels.begin(), zoom_levels.end(), HostZoomSort);

  for (content::HostZoomMap::ZoomLevelVector::const_iterator i =
           zoom_levels.begin();
       i != zoom_levels.end();
       ++i) {
    scoped_ptr<base::DictionaryValue> exception(new base::DictionaryValue);
    switch (i->mode) {
      case content::HostZoomMap::ZOOM_CHANGED_FOR_HOST:
        exception->SetString(kOrigin, i->host);
        break;
      case content::HostZoomMap::ZOOM_CHANGED_FOR_SCHEME_AND_HOST:
        // These are not stored in preferences and get cleared on next browser
        // start. Therefore, we don't care for them.
        break;
      case content::HostZoomMap::ZOOM_CHANGED_TEMPORARY_ZOOM:
        NOTREACHED();
    }
    exception->SetString(kSetting,
                         ContentSettingToString(CONTENT_SETTING_DEFAULT));

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
  std::vector<std::vector<base::Value*> > all_provider_exceptions;
  all_provider_exceptions.resize(HostContentSettingsMap::NUM_PROVIDER_TYPES);

  for (AllPatternsSettings::iterator i = all_patterns_settings.begin();
       i != all_patterns_settings.end();
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
    std::vector<base::Value*>* this_provider_exceptions =
        &all_provider_exceptions.at(
            HostContentSettingsMap::GetProviderTypeFromSource(source));

    // Add the "parent" entry for the non-embedded setting.
    ContentSetting parent_setting =
        parent == one_settings.end() ? CONTENT_SETTING_DEFAULT : parent->second;
    const ContentSettingsPattern& secondary_pattern =
        parent == one_settings.end() ? primary_pattern : parent->first;
    this_provider_exceptions->push_back(GetExceptionForPage(primary_pattern,
                                                            secondary_pattern,
                                                            parent_setting,
                                                            source));

    // Add the "children" for any embedded settings.
    for (OnePatternSettings::const_iterator j = one_settings.begin();
         j != one_settings.end(); ++j) {
      // Skip the non-embedded setting which we already added above.
      if (j == parent)
        continue;

      ContentSetting content_setting = j->second;
      this_provider_exceptions->push_back(GetExceptionForPage(
          primary_pattern,
          j->first,
          content_setting,
          source));
    }
  }

  for (size_t i = 0; i < all_provider_exceptions.size(); ++i) {
    for (size_t j = 0; j < all_provider_exceptions[i].size(); ++j) {
      exceptions->Append(all_provider_exceptions[i][j]);
    }
  }
}

void ContentSettingsHandler::RemoveNotificationException(
    const base::ListValue* args) {
  Profile* profile = Profile::FromWebUI(web_ui());

  std::string origin;
  std::string setting;
  bool rv = args->GetString(1, &origin);
  DCHECK(rv);
  rv = args->GetString(2, &setting);
  DCHECK(rv);
  ContentSetting content_setting = ContentSettingFromString(setting);

  DCHECK(content_setting == CONTENT_SETTING_ALLOW ||
         content_setting == CONTENT_SETTING_BLOCK);
  DesktopNotificationProfileUtil::ClearSetting(profile,
      ContentSettingsPattern::FromString(origin));
}

void ContentSettingsHandler::RemoveMediaException(const base::ListValue* args) {
  std::string mode;
  bool rv = args->GetString(1, &mode);
  DCHECK(rv);

  std::string pattern;
  rv = args->GetString(2, &pattern);
  DCHECK(rv);

  HostContentSettingsMap* settings_map =
      mode == "normal" ? GetContentSettingsMap() :
                         GetOTRContentSettingsMap();
  if (settings_map) {
    settings_map->SetWebsiteSetting(ContentSettingsPattern::FromString(pattern),
                                    ContentSettingsPattern::Wildcard(),
                                    CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC,
                                    std::string(),
                                    NULL);
    settings_map->SetWebsiteSetting(ContentSettingsPattern::FromString(pattern),
                                    ContentSettingsPattern::Wildcard(),
                                    CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA,
                                    std::string(),
                                    NULL);
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
    settings_map->SetWebsiteSetting(
        ContentSettingsPattern::FromString(pattern),
        secondary_pattern.empty() ?
            ContentSettingsPattern::Wildcard() :
            ContentSettingsPattern::FromString(secondary_pattern),
        type,
        std::string(),
        NULL);
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

  content::HostZoomMap* host_zoom_map =
      content::HostZoomMap::GetForBrowserContext(GetBrowserContext(web_ui()));
  double default_level = host_zoom_map->GetDefaultZoomLevel();
  host_zoom_map->SetZoomLevelForHost(pattern, default_level);
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

void ContentSettingsHandler::ApplyWhitelist(ContentSettingsType content_type,
                                            ContentSetting default_setting) {
  HostContentSettingsMap* map = GetContentSettingsMap();
  if (content_type != CONTENT_SETTINGS_TYPE_PLUGINS)
    return;
  const int kDefaultWhitelistVersion = 1;
  PrefService* prefs = user_prefs::UserPrefs::Get(GetBrowserContext(web_ui()));
  int version = prefs->GetInteger(
      prefs::kContentSettingsDefaultWhitelistVersion);
  if (version >= kDefaultWhitelistVersion)
    return;
  ContentSetting old_setting =
      map->GetDefaultContentSetting(CONTENT_SETTINGS_TYPE_PLUGINS, NULL);
  // TODO(bauerb): Remove this once the Google Talk plug-in works nicely with
  // click-to-play (b/6090625).
  if (old_setting == CONTENT_SETTING_ALLOW &&
      default_setting == CONTENT_SETTING_ASK) {
    map->SetWebsiteSetting(
        ContentSettingsPattern::Wildcard(),
        ContentSettingsPattern::Wildcard(),
        CONTENT_SETTINGS_TYPE_PLUGINS,
        "google-talk",
        new base::FundamentalValue(CONTENT_SETTING_ALLOW));
  }
  prefs->SetInteger(prefs::kContentSettingsDefaultWhitelistVersion,
                    kDefaultWhitelistVersion);
}

void ContentSettingsHandler::SetContentFilter(const base::ListValue* args) {
  DCHECK_EQ(2U, args->GetSize());
  std::string group, setting;
  if (!(args->GetString(0, &group) &&
        args->GetString(1, &setting))) {
    NOTREACHED();
    return;
  }

  ContentSetting default_setting = ContentSettingFromString(setting);
  ContentSettingsType content_type = ContentSettingsTypeFromGroupName(group);
  Profile* profile = Profile::FromWebUI(web_ui());

#if defined(OS_CHROMEOS)
  // ChromeOS special case : in Guest mode settings are opened in Incognito
  // mode, so we need original profile to actually modify settings.
  if (user_manager::UserManager::Get()->IsLoggedInAsGuest())
    profile = profile->GetOriginalProfile();
#endif


  HostContentSettingsMap* map = profile->GetHostContentSettingsMap();
  ApplyWhitelist(content_type, default_setting);
  map->SetDefaultContentSetting(content_type, default_setting);

  switch (content_type) {
    case CONTENT_SETTINGS_TYPE_COOKIES:
      content::RecordAction(
          UserMetricsAction("Options_DefaultCookieSettingChanged"));
      break;
    case CONTENT_SETTINGS_TYPE_IMAGES:
      content::RecordAction(
          UserMetricsAction("Options_DefaultImagesSettingChanged"));
      break;
    case CONTENT_SETTINGS_TYPE_JAVASCRIPT:
      content::RecordAction(
          UserMetricsAction("Options_DefaultJavaScriptSettingChanged"));
      break;
    case CONTENT_SETTINGS_TYPE_PLUGINS:
      content::RecordAction(
          UserMetricsAction("Options_DefaultPluginsSettingChanged"));
      break;
    case CONTENT_SETTINGS_TYPE_POPUPS:
      content::RecordAction(
          UserMetricsAction("Options_DefaultPopupsSettingChanged"));
      break;
    case CONTENT_SETTINGS_TYPE_NOTIFICATIONS:
      content::RecordAction(
          UserMetricsAction("Options_DefaultNotificationsSettingChanged"));
      break;
    case CONTENT_SETTINGS_TYPE_GEOLOCATION:
      content::RecordAction(
          UserMetricsAction("Options_DefaultGeolocationSettingChanged"));
      break;
    case CONTENT_SETTINGS_TYPE_MOUSELOCK:
      content::RecordAction(
          UserMetricsAction("Options_DefaultMouseLockSettingChanged"));
      break;
    case CONTENT_SETTINGS_TYPE_MEDIASTREAM:
      content::RecordAction(
          UserMetricsAction("Options_DefaultMediaStreamMicSettingChanged"));
      break;
    case CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS:
      content::RecordAction(
          UserMetricsAction("Options_DefaultMultipleAutomaticDLSettingChange"));
      break;
    case CONTENT_SETTINGS_TYPE_MIDI_SYSEX:
      content::RecordAction(
          UserMetricsAction("Options_DefaultMIDISysExSettingChanged"));
      break;
    case CONTENT_SETTINGS_TYPE_PUSH_MESSAGING:
      content::RecordAction(
          UserMetricsAction("Options_DefaultPushMessagingSettingChanged"));
      break;
    default:
      break;
  }
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

  ContentSettingsType type = ContentSettingsTypeFromGroupName(type_string);
  switch (type) {
    case CONTENT_SETTINGS_TYPE_NOTIFICATIONS:
      RemoveNotificationException(args);
      break;
    case CONTENT_SETTINGS_TYPE_MEDIASTREAM:
      RemoveMediaException(args);
      break;
    default:
      RemoveExceptionFromHostContentSettingsMap(args, type);
      break;
  }
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
      type == CONTENT_SETTINGS_TYPE_NOTIFICATIONS ||
      type == CONTENT_SETTINGS_TYPE_MEDIASTREAM ||
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
    settings_map->SetContentSetting(ContentSettingsPattern::FromString(pattern),
                                    ContentSettingsPattern::Wildcard(),
                                    type,
                                    std::string(),
                                    ContentSettingFromString(setting));
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
  return Profile::FromWebUI(web_ui())->GetHostContentSettingsMap();
}

ProtocolHandlerRegistry* ContentSettingsHandler::GetProtocolHandlerRegistry() {
  return ProtocolHandlerRegistryFactory::GetForBrowserContext(
      GetBrowserContext(web_ui()));
}

HostContentSettingsMap*
    ContentSettingsHandler::GetOTRContentSettingsMap() {
  Profile* profile = Profile::FromWebUI(web_ui());
  if (profile->HasOffTheRecordProfile())
    return profile->GetOffTheRecordProfile()->GetHostContentSettingsMap();
  return NULL;
}

void ContentSettingsHandler::RefreshFlashMediaSettings() {
  media_settings_.flash_settings_initialized = false;

  media_settings_.last_flash_refresh_request_id =
      flash_settings_manager_->GetPermissionSettings(
          PP_FLASH_BROWSEROPERATIONS_SETTINGTYPE_CAMERAMIC);
}

void ContentSettingsHandler::OnPepperFlashPrefChanged() {
  ShowFlashMediaLink(DEFAULT_SETTING, false);
  ShowFlashMediaLink(EXCEPTIONS, false);

  PrefService* prefs = user_prefs::UserPrefs::Get(GetBrowserContext(web_ui()));
  if (prefs->GetBoolean(prefs::kPepperFlashSettingsEnabled))
    RefreshFlashMediaSettings();
  else
    media_settings_.flash_settings_initialized = false;
}

void ContentSettingsHandler::OnZoomLevelChanged(
    const content::HostZoomMap::ZoomLevelChange& change) {
  UpdateZoomLevelsExceptionsView();
}

void ContentSettingsHandler::ShowFlashMediaLink(LinkType link_type, bool show) {
  bool& show_link = link_type == DEFAULT_SETTING ?
      media_settings_.show_flash_default_link :
      media_settings_.show_flash_exceptions_link;
  if (show_link != show) {
    web_ui()->CallJavascriptFunction(
        link_type == DEFAULT_SETTING ?
            "ContentSettings.showMediaPepperFlashDefaultLink" :
            "ContentSettings.showMediaPepperFlashExceptionsLink",
        base::FundamentalValue(show));
    show_link = show;
  }
}

void ContentSettingsHandler::UpdateFlashMediaLinksVisibility() {
  if (!media_settings_.flash_settings_initialized ||
      !media_settings_.default_setting_initialized ||
      !media_settings_.exceptions_initialized) {
    return;
  }

  // Flash won't send us notifications when its settings get changed, which
  // means the Flash settings in |media_settings_| may be out-dated, especially
  // after we show links to change Flash settings.
  // In order to avoid confusion, we won't hide the links once they are showed.
  // One exception is that we will hide them when Pepper Flash is disabled
  // (handled in OnPepperFlashPrefChanged()).
  if (media_settings_.show_flash_default_link &&
      media_settings_.show_flash_exceptions_link) {
    return;
  }

  if (!media_settings_.show_flash_default_link) {
    // If both audio and video capture are disabled by policy, the link
    // shouldn't be showed. Flash conforms to the policy in this case because
    // it cannot open those devices. We don't have to look at the Flash
    // settings.
    if (!(media_settings_.policy_disable_audio &&
          media_settings_.policy_disable_video) &&
        media_settings_.flash_default_setting !=
            media_settings_.default_setting) {
      ShowFlashMediaLink(DEFAULT_SETTING, true);
    }
  }
  if (!media_settings_.show_flash_exceptions_link) {
    // If audio or video capture is disabled by policy, we skip comparison of
    // exceptions for audio or video capture, respectively.
    if (!PepperFlashContentSettingsUtils::AreMediaExceptionsEqual(
            media_settings_.default_setting,
            media_settings_.exceptions,
            media_settings_.flash_default_setting,
            media_settings_.flash_exceptions,
            media_settings_.policy_disable_audio,
            media_settings_.policy_disable_video)) {
      ShowFlashMediaLink(EXCEPTIONS, true);
    }
  }
}

void ContentSettingsHandler::UpdateProtectedContentExceptionsButton() {
  PrefService* prefs = user_prefs::UserPrefs::Get(GetBrowserContext(web_ui()));
  // Exceptions apply only when the feature is enabled.
  bool enable_exceptions = prefs->GetBoolean(prefs::kEnableDRM);
  web_ui()->CallJavascriptFunction(
      "ContentSettings.enableProtectedContentExceptions",
      base::FundamentalValue(enable_exceptions));
}

}  // namespace options
