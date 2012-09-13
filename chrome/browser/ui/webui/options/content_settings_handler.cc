// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/content_settings_handler.h"

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/content_settings_details.h"
#include "chrome/browser/content_settings/content_settings_utils.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "chrome/browser/intents/web_intents_util.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/desktop_notification_service_factory.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/content_settings_pattern.h"
#include "chrome/common/extensions/permissions/api_permission.h"
#include "chrome/common/extensions/extension_set.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/content_switches.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/user_manager.h"
#endif

using content::UserMetricsAction;
using extensions::APIPermission;

namespace {

enum ExContentSettingsTypeEnum {
  EX_CONTENT_SETTINGS_TYPE_PEPPER_FLASH_CAMERAMIC =
      CONTENT_SETTINGS_NUM_TYPES,
  EX_CONTENT_SETTINGS_NUM_TYPES
};

typedef std::map<ContentSettingsPattern, ContentSetting> OnePatternSettings;
typedef std::map<ContentSettingsPattern, OnePatternSettings>
    AllPatternsSettings;

// The AppFilter is used in AddExceptionsGrantedByHostedApps() to choose
// extensions which should have their extent displayed.
typedef bool (*AppFilter)(const extensions::Extension& app, Profile* profile);

const char* kDisplayPattern = "displayPattern";
const char* kSetting = "setting";
const char* kOrigin = "origin";
const char* kSource = "source";
const char* kAppName = "appName";
const char* kAppId = "appId";
const char* kEmbeddingOrigin = "embeddingOrigin";
const char* kDefaultProviderID = "default";
const char* kPreferencesSource = "preferences";

std::string ContentSettingToString(ContentSetting setting) {
  switch (setting) {
    case CONTENT_SETTING_ALLOW:
      return "allow";
    case CONTENT_SETTING_ASK:
      return "ask";
    case CONTENT_SETTING_BLOCK:
      return "block";
    case CONTENT_SETTING_SESSION_ONLY:
      return "session";
    case CONTENT_SETTING_DEFAULT:
      return "default";
    case CONTENT_SETTING_NUM_SETTINGS:
      NOTREACHED();
  }

  return "";
}

ContentSetting ContentSettingFromString(const std::string& name) {
  if (name == "allow")
    return CONTENT_SETTING_ALLOW;
  if (name == "ask")
    return CONTENT_SETTING_ASK;
  if (name == "block")
    return CONTENT_SETTING_BLOCK;
  if (name == "session")
    return CONTENT_SETTING_SESSION_ONLY;

  NOTREACHED() << name << " is not a recognized content setting.";
  return CONTENT_SETTING_DEFAULT;
}

// Create a DictionaryValue* that will act as a data source for a single row
// in a HostContentSettingsMap-controlled exceptions table (e.g., cookies).
// Ownership of the pointer is passed to the caller.
DictionaryValue* GetExceptionForPage(
    const ContentSettingsPattern& pattern,
    const ContentSetting& setting,
    const std::string& provider_name) {
  DictionaryValue* exception = new DictionaryValue();
  exception->SetString(kDisplayPattern, pattern.ToString());
  exception->SetString(kSetting, ContentSettingToString(setting));
  exception->SetString(kSource, provider_name);
  return exception;
}

// Create a DictionaryValue* that will act as a data source for a single row
// in the Geolocation exceptions table. Ownership of the pointer is passed to
// the caller.
DictionaryValue* GetGeolocationExceptionForPage(
    const ContentSettingsPattern& origin,
    const ContentSettingsPattern& embedding_origin,
    ContentSetting setting) {
  DictionaryValue* exception = new DictionaryValue();
  exception->SetString(kSetting, ContentSettingToString(setting));
  exception->SetString(kOrigin, origin.ToString());
  exception->SetString(kEmbeddingOrigin, embedding_origin.ToString());
  return exception;
}

// Create a DictionaryValue* that will act as a data source for a single row
// in the desktop notifications exceptions table. Ownership of the pointer is
// passed to the caller.
DictionaryValue* GetNotificationExceptionForPage(
    const ContentSettingsPattern& pattern,
    ContentSetting setting,
    const std::string& provider_name) {
  DictionaryValue* exception = new DictionaryValue();
  exception->SetString(kDisplayPattern, pattern.ToString());
  exception->SetString(kSetting, ContentSettingToString(setting));
  exception->SetString(kOrigin, pattern.ToString());
  exception->SetString(kSource, provider_name);
  return exception;
}

// Returns true whenever the hosted |app|'s extent enjoys protected storage
// under the current |profile|.
// Must have the AppFilter signature.
bool HasProtectedStorage(const extensions::Extension& app, Profile* profile) {
  ExtensionSpecialStoragePolicy* policy =
      profile->GetExtensionSpecialStoragePolicy();
  return policy->NeedsProtection(&app);
}

// Returns true whenever the |extension| is hosted and has |permission|.
// Must have the AppFilter signature.
template <APIPermission::ID permission>
bool HostedAppHasPermission(
    const extensions::Extension& extension, Profile* /*profile*/) {
    return extension.is_hosted_app() && extension.HasAPIPermission(permission);
}

// Add an "Allow"-entry to the list of |exceptions| for a |url_pattern| from
// the web extent of a hosted |app|.
void AddExceptionForHostedApp(const std::string& url_pattern,
    const extensions::Extension& app, ListValue* exceptions) {
  DictionaryValue* exception = new DictionaryValue();
  exception->SetString(kDisplayPattern, url_pattern);
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
void AddExceptionsGrantedByHostedApps(
    Profile* profile, AppFilter app_filter, ListValue* exceptions) {
  const ExtensionService* extension_service = profile->GetExtensionService();
  // After ExtensionSystem::Init has been called at the browser's start,
  // GetExtensionService() should not return NULL, so this is safe:
  const ExtensionSet* extensions = extension_service->extensions();

  for (ExtensionSet::const_iterator extension = extensions->begin();
       extension != extensions->end(); ++extension) {
    if (!app_filter(**extension, profile)) continue;

    URLPatternSet web_extent = (*extension)->web_extent();
    // Add patterns from web extent.
    for (URLPatternSet::const_iterator pattern = web_extent.begin();
         pattern != web_extent.end(); ++pattern) {
      std::string url_pattern = pattern->GetAsString();
      AddExceptionForHostedApp(url_pattern, **extension, exceptions);
    }
    // Retrieve the launch URL.
    std::string launch_url_string = (*extension)->launch_web_url();
    GURL launch_url(launch_url_string);
    // Skip adding the launch URL if it is part of the web extent.
    if (web_extent.MatchesURL(launch_url)) continue;
    AddExceptionForHostedApp(launch_url_string, **extension, exceptions);
  }
}

ContentSetting FlashPermissionToContentSetting(
    PP_Flash_BrowserOperations_Permission permission) {
  switch (permission) {
    case PP_FLASH_BROWSEROPERATIONS_PERMISSION_DEFAULT:
      return CONTENT_SETTING_DEFAULT;
    case PP_FLASH_BROWSEROPERATIONS_PERMISSION_ALLOW:
      return CONTENT_SETTING_ALLOW;
    case PP_FLASH_BROWSEROPERATIONS_PERMISSION_BLOCK:
      return CONTENT_SETTING_BLOCK;
    case PP_FLASH_BROWSEROPERATIONS_PERMISSION_ASK:
      return CONTENT_SETTING_ASK;
    default:
      NOTREACHED();
      return CONTENT_SETTING_DEFAULT;
  }
}

PP_Flash_BrowserOperations_Permission FlashPermissionFromContentSetting(
    ContentSetting setting) {
  switch (setting) {
    case CONTENT_SETTING_DEFAULT:
      return PP_FLASH_BROWSEROPERATIONS_PERMISSION_DEFAULT;
    case CONTENT_SETTING_ALLOW:
      return PP_FLASH_BROWSEROPERATIONS_PERMISSION_ALLOW;
    case CONTENT_SETTING_BLOCK:
      return PP_FLASH_BROWSEROPERATIONS_PERMISSION_BLOCK;
    case CONTENT_SETTING_ASK:
      return PP_FLASH_BROWSEROPERATIONS_PERMISSION_ASK;
    default:
      NOTREACHED();
      return PP_FLASH_BROWSEROPERATIONS_PERMISSION_DEFAULT;
  }
}

std::string CanonicalizeHost(const std::string& host) {
  url_canon::CanonHostInfo info;
  return net::CanonicalizeHost(host, &info);
}

bool IsValidHost(const std::string& host) {
  std::string canonicalized_host = CanonicalizeHost(host);
  return !canonicalized_host.empty();
}

}  // namespace

namespace options {

class ContentSettingsHandler::ExContentSettingsType {
 public:
  explicit ExContentSettingsType(int value) : value_(value) {
    DCHECK(value_ < EX_CONTENT_SETTINGS_NUM_TYPES);
  }
  explicit ExContentSettingsType(ContentSettingsType type) : value_(type) {}
  explicit ExContentSettingsType(ExContentSettingsTypeEnum type)
      : value_(type) {}

  bool IsExtraContentSettingsType() const {
    return value_ >= CONTENT_SETTINGS_NUM_TYPES;
  }

  operator int() const { return value_; }

  ContentSettingsType ToContentSettingsType() const {
    DCHECK(value_ < CONTENT_SETTINGS_NUM_TYPES);
    return static_cast<ContentSettingsType>(value_);
  }

 private:
  int value_;
};

ContentSettingsHandler::CachedPepperFlashSettings::CachedPepperFlashSettings()
    : default_permission(PP_FLASH_BROWSEROPERATIONS_PERMISSION_BLOCK),
      initialized(false),
      last_refresh_request_id(0) {
}

ContentSettingsHandler::CachedPepperFlashSettings::~CachedPepperFlashSettings()
    {
}

struct ContentSettingsHandler::ExContentSettingsTypeNameEntry {
  int type;
  const char* name;
};

const ContentSettingsHandler::ExContentSettingsTypeNameEntry
    ContentSettingsHandler::kExContentSettingsTypeGroupNames[] = {
  {CONTENT_SETTINGS_TYPE_COOKIES, "cookies"},
  {CONTENT_SETTINGS_TYPE_IMAGES, "images"},
  {CONTENT_SETTINGS_TYPE_JAVASCRIPT, "javascript"},
  {CONTENT_SETTINGS_TYPE_PLUGINS, "plugins"},
  {CONTENT_SETTINGS_TYPE_POPUPS, "popups"},
  {CONTENT_SETTINGS_TYPE_GEOLOCATION, "location"},
  {CONTENT_SETTINGS_TYPE_NOTIFICATIONS, "notifications"},
  {CONTENT_SETTINGS_TYPE_INTENTS, "intents"},
  {CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE, "auto-select-certificate"},
  {CONTENT_SETTINGS_TYPE_FULLSCREEN, "fullscreen"},
  {CONTENT_SETTINGS_TYPE_MOUSELOCK, "mouselock"},
  {CONTENT_SETTINGS_TYPE_MIXEDSCRIPT, "mixed-script"},
  {CONTENT_SETTINGS_TYPE_PROTOCOL_HANDLERS, "register-protocol-handler"},
  {EX_CONTENT_SETTINGS_TYPE_PEPPER_FLASH_CAMERAMIC, "pepper-flash-cameramic"},
  {CONTENT_SETTINGS_TYPE_MEDIASTREAM, "media-stream"},
  {CONTENT_SETTINGS_TYPE_PPAPI_BROKER, "ppapi-broker"},
};

ContentSettingsHandler::ContentSettingsHandler() {
}

ContentSettingsHandler::~ContentSettingsHandler() {
}

void ContentSettingsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
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
    { "cookies_show_app_cookies", IDS_COOKIES_SHOW_APP_COOKIES_BUTTON },
    { "flash_storage_settings", IDS_FLASH_STORAGE_SETTINGS },
    { "flash_storage_url", IDS_FLASH_STORAGE_URL },
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
    // Intents filter.
    { "webIntentsTabLabel", IDS_WEB_INTENTS_TAB_LABEL },
    { "allowWebIntents", IDS_ALLOW_WEB_INTENTS },
    // Fullscreen filter.
    { "fullscreen_tab_label", IDS_FULLSCREEN_TAB_LABEL },
    { "fullscreen_header", IDS_FULLSCREEN_HEADER },
    // Mouse Lock filter.
    { "mouselock_tab_label", IDS_MOUSE_LOCK_TAB_LABEL },
    { "mouselock_header", IDS_MOUSE_LOCK_HEADER },
    { "mouselock_allow", IDS_MOUSE_LOCK_ALLOW_RADIO },
    { "mouselock_ask", IDS_MOUSE_LOCK_ASK_RADIO },
    { "mouselock_block", IDS_MOUSE_LOCK_BLOCK_RADIO },
    // Pepper Flash camera and microphone filter.
    { "pepperFlashCameramicTabLabel", IDS_PEPPER_FLASH_CAMERAMIC_TAB_LABEL },
    // The header has to be named as <content_type_name>_header.
    { "pepper-flash-cameramic_header", IDS_PEPPER_FLASH_CAMERAMIC_HEADER },
    { "pepperFlashCameramicAsk", IDS_PEPPER_FLASH_CAMERAMIC_ASK_RADIO },
    { "pepperFlashCameramicBlock", IDS_PEPPER_FLASH_CAMERAMIC_BLOCK_RADIO },
#if defined(OS_CHROMEOS)
    // Protected Content filter
    { "protectedContentTabLabel", IDS_PROTECTED_CONTENT_TAB_LABEL },
    { "protectedContentInfo", IDS_PROTECTED_CONTENT_INFO },
    { "protectedContentEnable", IDS_PROTECTED_CONTENT_ENABLE},
#endif  // defined(OS_CHROMEOS)
    // Media stream capture device filter.
    { "mediaStreamTabLabel", IDS_MEDIA_STREAM_TAB_LABEL },
    { "media-stream_header", IDS_MEDIA_STREAM_HEADER },
    { "mediaStreamAsk", IDS_MEDIA_STREAM_ASK_RADIO },
    { "mediaStreamBlock", IDS_MEDIA_STREAM_BLOCK_RADIO },
    // PPAPI broker filter.
    { "ppapi_broker_tab_label", IDS_PPAPI_BROKER_TAB_LABEL },
    { "ppapi_broker_allow", IDS_PPAPI_BROKER_ALLOW_RADIO },
    { "ppapi_broker_ask", IDS_PPAPI_BROKER_ASK_RADIO },
    { "ppapi_broker_block", IDS_PPAPI_BROKER_BLOCK_RADIO },
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
  RegisterTitle(localized_strings, "pepper-flash-cameramic",
                IDS_PEPPER_FLASH_CAMERAMIC_TAB_LABEL);
  RegisterTitle(localized_strings, "media-stream",
                IDS_MEDIA_STREAM_TAB_LABEL);

  Profile* profile = Profile::FromWebUI(web_ui());
  localized_strings->SetBoolean(
      "enable_web_intents",
      web_intents::IsWebIntentsEnabledForProfile(profile));
}

void ContentSettingsHandler::InitializeHandler() {
  notification_registrar_.Add(
      this, chrome::NOTIFICATION_PROFILE_CREATED,
      content::NotificationService::AllSources());
  notification_registrar_.Add(
      this, chrome::NOTIFICATION_PROFILE_DESTROYED,
      content::NotificationService::AllSources());

  notification_registrar_.Add(
      this, chrome::NOTIFICATION_CONTENT_SETTINGS_CHANGED,
      content::NotificationService::AllSources());
  notification_registrar_.Add(
      this, chrome::NOTIFICATION_DESKTOP_NOTIFICATION_SETTINGS_CHANGED,
      content::NotificationService::AllSources());
  Profile* profile = Profile::FromWebUI(web_ui());
  notification_registrar_.Add(
      this, chrome::NOTIFICATION_PROTOCOL_HANDLER_REGISTRY_CHANGED,
      content::Source<Profile>(profile));

  PrefService* prefs = profile->GetPrefs();
  pref_change_registrar_.Init(prefs);
  pref_change_registrar_.Add(prefs::kGeolocationContentSettings, this);
  pref_change_registrar_.Add(prefs::kPepperFlashSettingsEnabled, this);

  flash_settings_manager_.reset(new PepperFlashSettingsManager(this, profile));
}

void ContentSettingsHandler::InitializePage() {
  UpdateHandlersEnabledRadios();
  UpdateAllExceptionsViewsFromModel();

  flash_cameramic_settings_ = CachedPepperFlashSettings();
  RefreshFlashSettingsCache(true);
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

    case chrome::NOTIFICATION_CONTENT_SETTINGS_CHANGED: {
      // Filter out notifications from other profiles.
      HostContentSettingsMap* map =
          content::Source<HostContentSettingsMap>(source).ptr();
      if (map != GetContentSettingsMap() &&
          map != GetOTRContentSettingsMap())
        break;

      const ContentSettingsDetails* settings_details =
          content::Details<const ContentSettingsDetails>(details).ptr();

      // TODO(estade): we pretend update_all() is always true.
      if (settings_details->update_all_types()) {
        UpdateAllExceptionsViewsFromModel();
      } else {
        UpdateExceptionsViewFromModel(
            ExContentSettingsType(settings_details->type()));
      }
      break;
    }

    case chrome::NOTIFICATION_PREF_CHANGED: {
      const std::string& pref_name =
          *content::Details<std::string>(details).ptr();
      if (pref_name == prefs::kGeolocationContentSettings)
        UpdateGeolocationExceptionsView();
      else if (pref_name == prefs::kPepperFlashSettingsEnabled)
        RefreshFlashSettingsCache(false);

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

    default:
      OptionsPageUIHandler::Observe(type, source, details);
  }
}

void ContentSettingsHandler::OnGetPermissionSettingsCompleted(
    uint32 request_id,
    bool success,
    PP_Flash_BrowserOperations_Permission default_permission,
    const ppapi::FlashSiteSettings& sites) {
  if (success &&
      request_id == flash_cameramic_settings_.last_refresh_request_id) {
    flash_cameramic_settings_.default_permission = default_permission;
    flash_cameramic_settings_.sites.clear();
    for (ppapi::FlashSiteSettings::const_iterator iter = sites.begin();
         iter != sites.end(); ++iter) {
      if (IsValidHost(iter->site))
        flash_cameramic_settings_.sites[iter->site] = iter->permission;
    }
    UpdateExceptionsViewFromModel(
        ExContentSettingsType(EX_CONTENT_SETTINGS_TYPE_PEPPER_FLASH_CAMERAMIC));

    if (!flash_cameramic_settings_.initialized) {
      web_ui()->CallJavascriptFunction(
          "ContentSettings.enablePepperFlashCameraMicSettings");
      flash_cameramic_settings_.initialized = true;
    }
  }
}

void ContentSettingsHandler::UpdateSettingDefaultFromModel(
    const ExContentSettingsType& type) {
  DictionaryValue filter_settings;
  std::string provider_id;
  filter_settings.SetString(
      ExContentSettingsTypeToGroupName(type) + ".value",
      GetSettingDefaultFromModel(type, &provider_id));
  filter_settings.SetString(
      ExContentSettingsTypeToGroupName(type) + ".managedBy", provider_id);

  web_ui()->CallJavascriptFunction(
      "ContentSettings.setContentFilterSettingsValue", filter_settings);
}

std::string ContentSettingsHandler::GetSettingDefaultFromModel(
    const ExContentSettingsType& type, std::string* provider_id) {
  Profile* profile = Profile::FromWebUI(web_ui());
  ContentSetting default_setting;
  if (type == CONTENT_SETTINGS_TYPE_NOTIFICATIONS) {
    default_setting =
        DesktopNotificationServiceFactory::GetForProfile(profile)->
            GetDefaultContentSetting(provider_id);
  } else if (type == EX_CONTENT_SETTINGS_TYPE_PEPPER_FLASH_CAMERAMIC) {
    default_setting = FlashPermissionToContentSetting(
        flash_cameramic_settings_.default_permission);
    *provider_id = kDefaultProviderID;
  } else {
    default_setting =
        profile->GetHostContentSettingsMap()->
            GetDefaultContentSetting(type.ToContentSettingsType(), provider_id);
  }

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
       type < EX_CONTENT_SETTINGS_NUM_TYPES; ++type) {
    // The content settings type CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE
    // is supposed to be set by policy only. Hence there is no user facing UI
    // for this content type and we skip it here.
    if (type == CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE)
      continue;
    // The RPH settings are retrieved separately.
    if (type == CONTENT_SETTINGS_TYPE_PROTOCOL_HANDLERS)
      continue;
    UpdateExceptionsViewFromModel(ExContentSettingsType(type));
  }
}

void ContentSettingsHandler::UpdateAllOTRExceptionsViewsFromModel() {
  for (int type = CONTENT_SETTINGS_TYPE_DEFAULT + 1;
       type < EX_CONTENT_SETTINGS_NUM_TYPES; ++type) {
    UpdateOTRExceptionsViewFromModel(ExContentSettingsType(type));
  }
}

void ContentSettingsHandler::UpdateExceptionsViewFromModel(
    const ExContentSettingsType& type) {
  switch (type) {
    case CONTENT_SETTINGS_TYPE_GEOLOCATION:
      UpdateGeolocationExceptionsView();
      break;
    case CONTENT_SETTINGS_TYPE_NOTIFICATIONS:
      UpdateNotificationExceptionsView();
      break;
    case EX_CONTENT_SETTINGS_TYPE_PEPPER_FLASH_CAMERAMIC:
      UpdateFlashCameraMicExceptionsView();
      break;
    case CONTENT_SETTINGS_TYPE_INTENTS:
      // Don't update intents settings at this point.
      // Turn on when enable_web_intents_tag is enabled.
      break;
    case CONTENT_SETTINGS_TYPE_MIXEDSCRIPT:
      // We don't yet support exceptions for mixed scripting.
      break;
    case CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE:
      break;
    case CONTENT_SETTINGS_TYPE_PROTOCOL_HANDLERS:
      break;
    default:
      UpdateExceptionsViewFromHostContentSettingsMap(
          type.ToContentSettingsType());
      break;
  }
}

void ContentSettingsHandler::UpdateOTRExceptionsViewFromModel(
    const ExContentSettingsType& type) {
  switch (type) {
    case CONTENT_SETTINGS_TYPE_GEOLOCATION:
    case CONTENT_SETTINGS_TYPE_NOTIFICATIONS:
    case CONTENT_SETTINGS_TYPE_INTENTS:
    case CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE:
    case CONTENT_SETTINGS_TYPE_MIXEDSCRIPT:
    case EX_CONTENT_SETTINGS_TYPE_PEPPER_FLASH_CAMERAMIC:
      break;
    default:
      UpdateExceptionsViewFromOTRHostContentSettingsMap(
          type.ToContentSettingsType());
      break;
  }
}

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
  for (ContentSettingsForOneType::iterator i =
           all_settings.begin();
       i != all_settings.end();
       ++i) {
    // Don't add default settings.
    if (i->primary_pattern == ContentSettingsPattern::Wildcard() &&
        i->secondary_pattern == ContentSettingsPattern::Wildcard() &&
        i->source != kPreferencesSource) {
      continue;
    }
    all_patterns_settings[i->primary_pattern][i->secondary_pattern] =
        i->setting;
  }

  ListValue exceptions;
  AddExceptionsGrantedByHostedApps(
      profile,
      HostedAppHasPermission<APIPermission::kGeolocation>,
      &exceptions);

  for (AllPatternsSettings::iterator i = all_patterns_settings.begin();
       i != all_patterns_settings.end();
       ++i) {
    const ContentSettingsPattern& primary_pattern = i->first;
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

      exceptions.Append(
          GetGeolocationExceptionForPage(primary_pattern, j->first, j->second));
    }
  }

  StringValue type_string(
      ContentSettingsTypeToGroupName(CONTENT_SETTINGS_TYPE_GEOLOCATION));
  web_ui()->CallJavascriptFunction("ContentSettings.setExceptions",
                                   type_string, exceptions);

  // This is mainly here to keep this function ideologically parallel to
  // UpdateExceptionsViewFromHostContentSettingsMap().
  UpdateSettingDefaultFromModel(
      ExContentSettingsType(CONTENT_SETTINGS_TYPE_GEOLOCATION));
}

void ContentSettingsHandler::UpdateNotificationExceptionsView() {
  Profile* profile = Profile::FromWebUI(web_ui());
  DesktopNotificationService* service =
      DesktopNotificationServiceFactory::GetForProfile(profile);

  ContentSettingsForOneType settings;
  service->GetNotificationsSettings(&settings);

  ListValue exceptions;
  AddExceptionsGrantedByHostedApps(profile,
      HostedAppHasPermission<APIPermission::kNotification>,
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

  StringValue type_string(
      ContentSettingsTypeToGroupName(CONTENT_SETTINGS_TYPE_NOTIFICATIONS));
  web_ui()->CallJavascriptFunction("ContentSettings.setExceptions",
                                   type_string, exceptions);

  // This is mainly here to keep this function ideologically parallel to
  // UpdateExceptionsViewFromHostContentSettingsMap().
  UpdateSettingDefaultFromModel(
      ExContentSettingsType(CONTENT_SETTINGS_TYPE_NOTIFICATIONS));
}

void ContentSettingsHandler::UpdateFlashCameraMicExceptionsView() {
  ListValue exceptions;
  for (CachedPepperFlashSettings::SiteMap::iterator iter =
           flash_cameramic_settings_.sites.begin();
       iter != flash_cameramic_settings_.sites.end(); ++iter) {
    DictionaryValue* exception = new DictionaryValue();
    exception->SetString(kDisplayPattern, iter->first);
    exception->SetString(
        kSetting,
        ContentSettingToString(FlashPermissionToContentSetting(iter->second)));
    exception->SetString(kSource, "preference");
    exceptions.Append(exception);
  }

  StringValue type_string(ExContentSettingsTypeToGroupName(
      ExContentSettingsType(EX_CONTENT_SETTINGS_TYPE_PEPPER_FLASH_CAMERAMIC)));
  web_ui()->CallJavascriptFunction("ContentSettings.setExceptions",
                                   type_string, exceptions);

  UpdateSettingDefaultFromModel(
      ExContentSettingsType(EX_CONTENT_SETTINGS_TYPE_PEPPER_FLASH_CAMERAMIC));
}

void ContentSettingsHandler::UpdateExceptionsViewFromHostContentSettingsMap(
    ContentSettingsType type) {
  ContentSettingsForOneType entries;
  GetContentSettingsMap()->GetSettingsForOneType(type, "", &entries);

  ListValue exceptions;
  for (size_t i = 0; i < entries.size(); ++i) {
    // Skip default settings from extensions and policy, and the default content
    // settings; all of them will affect the default setting UI.
    if (entries[i].primary_pattern == ContentSettingsPattern::Wildcard() &&
        entries[i].secondary_pattern == ContentSettingsPattern::Wildcard() &&
        entries[i].source != "preference") {
      continue;
    }
    // The content settings UI does not support secondary content settings
    // pattern yet. For content settings set through the content settings UI the
    // secondary pattern is by default a wildcard pattern. Hence users are not
    // able to modify content settings with a secondary pattern other than the
    // wildcard pattern. So only show settings that the user is able to modify.
    // TODO(bauerb): Support a read-only view for those patterns.
    if (entries[i].secondary_pattern == ContentSettingsPattern::Wildcard()) {
      // Media Stream is using compound values for exceptions, which are
      // granted as |CONTENT_SETTING_ALLOW|.
      ContentSetting content_setting = CONTENT_SETTING_DEFAULT;
      if (type == CONTENT_SETTINGS_TYPE_MEDIASTREAM)
        content_setting = CONTENT_SETTING_ALLOW;
      else
        content_setting = entries[i].setting;

      exceptions.Append(GetExceptionForPage(entries[i].primary_pattern,
                                            content_setting,
                                            entries[i].source));
    } else {
      LOG(ERROR) << "Secondary content settings patterns are not "
                 << "supported by the content settings UI";
    }
  }

  StringValue type_string(ContentSettingsTypeToGroupName(type));
  web_ui()->CallJavascriptFunction("ContentSettings.setExceptions", type_string,
                                   exceptions);

  UpdateExceptionsViewFromOTRHostContentSettingsMap(type);

  // TODO(koz): The default for fullscreen is always 'ask'.
  // http://crbug.com/104683
  if (type == CONTENT_SETTINGS_TYPE_FULLSCREEN)
    return;

  // The default may also have changed (we won't get a separate notification).
  // If it hasn't changed, this call will be harmless.
  UpdateSettingDefaultFromModel(ExContentSettingsType(type));
}

void ContentSettingsHandler::UpdateExceptionsViewFromOTRHostContentSettingsMap(
    ContentSettingsType type) {
  const HostContentSettingsMap* otr_settings_map = GetOTRContentSettingsMap();
  if (!otr_settings_map)
    return;

  ContentSettingsForOneType otr_entries;
  otr_settings_map->GetSettingsForOneType(type, "", &otr_entries);

  ListValue otr_exceptions;
  for (size_t i = 0; i < otr_entries.size(); ++i) {
    // Off-the-record HostContentSettingsMap contains incognito content settings
    // as well as normal content settings. Here, we use the incongnito settings
    // only.
    if (!otr_entries[i].incognito)
      continue;
    // The content settings UI does not support secondary content settings
    // pattern yet. For content settings set through the content settings UI the
    // secondary pattern is by default a wildcard pattern. Hence users are not
    // able to modify content settings with a secondary pattern other than the
    // wildcard pattern. So only show settings that the user is able to modify.
    // TODO(bauerb): Support a read-only view for those patterns.
    if (otr_entries[i].secondary_pattern ==
        ContentSettingsPattern::Wildcard()) {
      ContentSetting content_setting = CONTENT_SETTING_DEFAULT;
      // Media Stream is using compound values for its exceptions and arbitrary
      // values for its default setting. And all the exceptions are granted as
      // |CONTENT_SETTING_ALLOW|.
      if (type == CONTENT_SETTINGS_TYPE_MEDIASTREAM &&
          otr_entries[i].primary_pattern != ContentSettingsPattern::Wildcard())
        content_setting = CONTENT_SETTING_ALLOW;
      else
        content_setting = otr_entries[i].setting;

      otr_exceptions.Append(GetExceptionForPage(otr_entries[i].primary_pattern,
                                                content_setting,
                                                otr_entries[i].source));
    } else {
      LOG(ERROR) << "Secondary content settings patterns are not "
                 << "supported by the content settings UI";
    }
  }

  StringValue type_string(ContentSettingsTypeToGroupName(type));
  web_ui()->CallJavascriptFunction("ContentSettings.setOTRExceptions",
                                   type_string, otr_exceptions);
}

void ContentSettingsHandler::RemoveGeolocationException(
    const ListValue* args, size_t arg_index) {
  Profile* profile = Profile::FromWebUI(web_ui());
  std::string origin;
  std::string embedding_origin;
  bool rv = args->GetString(arg_index++, &origin);
  DCHECK(rv);
  rv = args->GetString(arg_index++, &embedding_origin);
  DCHECK(rv);

  profile->GetHostContentSettingsMap()->
      SetContentSetting(ContentSettingsPattern::FromString(origin),
                        ContentSettingsPattern::FromString(embedding_origin),
                        CONTENT_SETTINGS_TYPE_GEOLOCATION,
                        std::string(),
                        CONTENT_SETTING_DEFAULT);
}

void ContentSettingsHandler::RemoveNotificationException(
    const ListValue* args, size_t arg_index) {
  Profile* profile = Profile::FromWebUI(web_ui());
  std::string origin;
  std::string setting;
  bool rv = args->GetString(arg_index++, &origin);
  DCHECK(rv);
  rv = args->GetString(arg_index++, &setting);
  DCHECK(rv);
  ContentSetting content_setting = ContentSettingFromString(setting);

  DCHECK(content_setting == CONTENT_SETTING_ALLOW ||
         content_setting == CONTENT_SETTING_BLOCK);
  DesktopNotificationServiceFactory::GetForProfile(profile)->
      ClearSetting(ContentSettingsPattern::FromString(origin));
}

void ContentSettingsHandler::RemoveFlashCameraMicException(
    const ListValue* args, size_t arg_index) {
  std::string mode;
  bool rv = args->GetString(arg_index++, &mode);
  DCHECK(rv);
  DCHECK_EQ(mode, "normal");

  std::string pattern;
  rv = args->GetString(arg_index++, &pattern);
  DCHECK(rv);

  CachedPepperFlashSettings::SiteMap::iterator iter =
      flash_cameramic_settings_.sites.find(pattern);
  if (iter != flash_cameramic_settings_.sites.end()) {
    flash_cameramic_settings_.sites.erase(iter);
    ppapi::FlashSiteSettings site_settings(
        1,
        ppapi::FlashSiteSetting(pattern,
                                PP_FLASH_BROWSEROPERATIONS_PERMISSION_DEFAULT));
    flash_settings_manager_->SetSitePermission(
        PP_FLASH_BROWSEROPERATIONS_SETTINGTYPE_CAMERAMIC,
        site_settings);
  } else {
    NOTREACHED();
  }

  UpdateFlashCameraMicExceptionsView();
}

void ContentSettingsHandler::RemoveExceptionFromHostContentSettingsMap(
    const ListValue* args, size_t arg_index,
    const ExContentSettingsType& type) {
  std::string mode;
  bool rv = args->GetString(arg_index++, &mode);
  DCHECK(rv);

  std::string pattern;
  rv = args->GetString(arg_index++, &pattern);
  DCHECK(rv);

  HostContentSettingsMap* settings_map =
      mode == "normal" ? GetContentSettingsMap() :
                         GetOTRContentSettingsMap();
  if (settings_map) {
    settings_map->SetWebsiteSetting(
        ContentSettingsPattern::FromString(pattern),
        ContentSettingsPattern::Wildcard(),
        type.ToContentSettingsType(),
        "",
        NULL);
  }
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
  Profile* profile = Profile::FromWebUI(web_ui());
  HostContentSettingsMap* map = GetContentSettingsMap();
  if (content_type != CONTENT_SETTINGS_TYPE_PLUGINS)
    return;
  const int kDefaultWhitelistVersion = 1;
  PrefService* prefs = profile->GetPrefs();
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
        Value::CreateIntegerValue(CONTENT_SETTING_ALLOW));
  }
  prefs->SetInteger(prefs::kContentSettingsDefaultWhitelistVersion,
                    kDefaultWhitelistVersion);
}

void ContentSettingsHandler::SetContentFilter(const ListValue* args) {
  DCHECK_EQ(2U, args->GetSize());
  std::string group, setting;
  if (!(args->GetString(0, &group) &&
        args->GetString(1, &setting))) {
    NOTREACHED();
    return;
  }

  ContentSetting default_setting = ContentSettingFromString(setting);
  ExContentSettingsType content_type =
      ExContentSettingsTypeFromGroupName(group);
  Profile* profile = Profile::FromWebUI(web_ui());

#if defined(OS_CHROMEOS)
  // ChromeOS special case : in Guest mode settings are opened in Incognito
  // mode, so we need original profile to actually modify settings.
  if (chromeos::UserManager::Get()->IsLoggedInAsGuest())
    profile = profile->GetOriginalProfile();
#endif

  if (content_type == CONTENT_SETTINGS_TYPE_NOTIFICATIONS) {
    DesktopNotificationServiceFactory::GetForProfile(profile)->
        SetDefaultContentSetting(default_setting);
  } else if (content_type == EX_CONTENT_SETTINGS_TYPE_PEPPER_FLASH_CAMERAMIC) {
    flash_cameramic_settings_.default_permission =
        FlashPermissionFromContentSetting(default_setting);
    flash_settings_manager_->SetDefaultPermission(
        PP_FLASH_BROWSEROPERATIONS_SETTINGTYPE_CAMERAMIC,
        flash_cameramic_settings_.default_permission, false);
    RefreshFlashSettingsCache(true);
  } else {
    HostContentSettingsMap* map = profile->GetHostContentSettingsMap();
    ContentSettingsType converted_type = content_type.ToContentSettingsType();
    ApplyWhitelist(converted_type, default_setting);
    map->SetDefaultContentSetting(converted_type, default_setting);
  }
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
    case CONTENT_SETTINGS_TYPE_INTENTS:
      content::RecordAction(
          UserMetricsAction("Options_DefaultHandlersSettingChanged"));
      break;
    case CONTENT_SETTINGS_TYPE_MOUSELOCK:
      content::RecordAction(
          UserMetricsAction("Options_DefaultMouseLockSettingChanged"));
      break;
    case EX_CONTENT_SETTINGS_TYPE_PEPPER_FLASH_CAMERAMIC:
      content::RecordAction(
          UserMetricsAction("Options_DefaultFlashCameraMicSettingChanged"));
      break;
    case CONTENT_SETTINGS_TYPE_MEDIASTREAM:
      content::RecordAction(
          UserMetricsAction("Options_DefaultMediaStreamSettingChanged"));
      break;
    default:
      break;
  }
}

void ContentSettingsHandler::RemoveException(const ListValue* args) {
  size_t arg_i = 0;
  std::string type_string;
  CHECK(args->GetString(arg_i++, &type_string));

  ExContentSettingsType type = ExContentSettingsTypeFromGroupName(type_string);
  switch (type) {
    case CONTENT_SETTINGS_TYPE_GEOLOCATION:
      RemoveGeolocationException(args, arg_i);
      break;
    case CONTENT_SETTINGS_TYPE_NOTIFICATIONS:
      RemoveNotificationException(args, arg_i);
      break;
    case EX_CONTENT_SETTINGS_TYPE_PEPPER_FLASH_CAMERAMIC:
      RemoveFlashCameraMicException(args, arg_i);
      break;
    default:
      RemoveExceptionFromHostContentSettingsMap(args, arg_i, type);
      break;
  }
}

void ContentSettingsHandler::SetException(const ListValue* args) {
  size_t arg_i = 0;
  std::string type_string;
  CHECK(args->GetString(arg_i++, &type_string));
  std::string mode;
  CHECK(args->GetString(arg_i++, &mode));
  std::string pattern;
  CHECK(args->GetString(arg_i++, &pattern));
  std::string setting;
  CHECK(args->GetString(arg_i++, &setting));

  ExContentSettingsType type = ExContentSettingsTypeFromGroupName(type_string);
  if (type == CONTENT_SETTINGS_TYPE_GEOLOCATION ||
      type == CONTENT_SETTINGS_TYPE_NOTIFICATIONS ||
      type == CONTENT_SETTINGS_TYPE_MEDIASTREAM) {
    NOTREACHED();
  } else if (type == EX_CONTENT_SETTINGS_TYPE_PEPPER_FLASH_CAMERAMIC) {
    DCHECK(IsValidHost(pattern));

    if (flash_cameramic_settings_.sites.find(pattern) ==
        flash_cameramic_settings_.sites.end()) {
      pattern = CanonicalizeHost(pattern);
    }
    PP_Flash_BrowserOperations_Permission permission =
        FlashPermissionFromContentSetting(ContentSettingFromString(setting));
    flash_cameramic_settings_.sites[pattern] = permission;
    ppapi::FlashSiteSettings
        site_settings(1, ppapi::FlashSiteSetting(pattern, permission));
    flash_settings_manager_->SetSitePermission(
        PP_FLASH_BROWSEROPERATIONS_SETTINGTYPE_CAMERAMIC,
        site_settings);
    UpdateFlashCameraMicExceptionsView();
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
                                    type.ToContentSettingsType(),
                                    "",
                                    ContentSettingFromString(setting));
  }
}

void ContentSettingsHandler::CheckExceptionPatternValidity(
    const ListValue* args) {
  size_t arg_i = 0;
  std::string type_string;
  CHECK(args->GetString(arg_i++, &type_string));
  std::string mode_string;
  CHECK(args->GetString(arg_i++, &mode_string));
  std::string pattern_string;
  CHECK(args->GetString(arg_i++, &pattern_string));

  ExContentSettingsType type = ExContentSettingsTypeFromGroupName(type_string);
  bool is_valid = false;
  if (type == EX_CONTENT_SETTINGS_TYPE_PEPPER_FLASH_CAMERAMIC) {
    is_valid = IsValidHost(pattern_string);
  } else {
    ContentSettingsPattern pattern =
        ContentSettingsPattern::FromString(pattern_string);
    is_valid = pattern.IsValid();
  }

  scoped_ptr<Value> type_value(Value::CreateStringValue(type_string));
  scoped_ptr<Value> mode_value(Value::CreateStringValue(mode_string));
  scoped_ptr<Value> pattern_value(Value::CreateStringValue(pattern_string));
  scoped_ptr<Value> valid_value(Value::CreateBooleanValue(is_valid));

  web_ui()->CallJavascriptFunction(
      "ContentSettings.patternValidityCheckComplete",
      *type_value.get(),
      *mode_value.get(),
      *pattern_value.get(),
      *valid_value.get());
}

// static
std::string ContentSettingsHandler::ContentSettingsTypeToGroupName(
    ContentSettingsType type) {
  return ExContentSettingsTypeToGroupName(ExContentSettingsType(type));
}

HostContentSettingsMap* ContentSettingsHandler::GetContentSettingsMap() {
  return Profile::FromWebUI(web_ui())->GetHostContentSettingsMap();
}

ProtocolHandlerRegistry* ContentSettingsHandler::GetProtocolHandlerRegistry() {
  return Profile::FromWebUI(web_ui())->GetProtocolHandlerRegistry();
}

HostContentSettingsMap*
    ContentSettingsHandler::GetOTRContentSettingsMap() {
  Profile* profile = Profile::FromWebUI(web_ui());
  if (profile->HasOffTheRecordProfile())
    return profile->GetOffTheRecordProfile()->GetHostContentSettingsMap();
  return NULL;
}

void ContentSettingsHandler::RefreshFlashSettingsCache(bool force) {
  if (force || !flash_cameramic_settings_.initialized) {
    flash_cameramic_settings_.last_refresh_request_id =
        flash_settings_manager_->GetPermissionSettings(
            PP_FLASH_BROWSEROPERATIONS_SETTINGTYPE_CAMERAMIC);
  }
}

// static
ContentSettingsHandler::ExContentSettingsType
    ContentSettingsHandler::ExContentSettingsTypeFromGroupName(
        const std::string& name) {
  COMPILE_ASSERT(arraysize(kExContentSettingsTypeGroupNames) ==
                     EX_CONTENT_SETTINGS_NUM_TYPES,
                 MISSING_CONTENT_SETTINGS_TYPE);

  for (size_t i = 0; i < arraysize(kExContentSettingsTypeGroupNames); ++i) {
    if (name == kExContentSettingsTypeGroupNames[i].name)
      return ExContentSettingsType(kExContentSettingsTypeGroupNames[i].type);
  }

  NOTREACHED() << name << " is not a recognized content settings type.";
  return ExContentSettingsType(CONTENT_SETTINGS_TYPE_DEFAULT);
}

// static
std::string ContentSettingsHandler::ExContentSettingsTypeToGroupName(
    const ExContentSettingsType& type) {
  for (size_t i = 0; i < arraysize(kExContentSettingsTypeGroupNames); ++i) {
    if (type == kExContentSettingsTypeGroupNames[i].type)
      return kExContentSettingsTypeGroupNames[i].name;
  }

  NOTREACHED();
  return std::string();
}

}  // namespace options
