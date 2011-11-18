// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/content_settings_handler.h"

#include <map>
#include <string>
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
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/desktop_notification_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/content_settings_pattern.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/common/content_switches.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

struct ContentSettingsTypeNameEntry {
  ContentSettingsType type;
  const char* name;
};

typedef std::map<ContentSettingsPattern, ContentSetting> OnePatternSettings;
typedef std::map<ContentSettingsPattern, OnePatternSettings>
    AllPatternsSettings;

const char* kDisplayPattern = "displayPattern";
const char* kSetting = "setting";
const char* kOrigin = "origin";
const char* kSource = "source";
const char* kEmbeddingOrigin = "embeddingOrigin";

const ContentSettingsTypeNameEntry kContentSettingsTypeGroupNames[] = {
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
};
COMPILE_ASSERT(arraysize(kContentSettingsTypeGroupNames) ==
                   CONTENT_SETTINGS_NUM_TYPES,
               MISSING_CONTENT_SETTINGS_TYPE);

ContentSettingsType ContentSettingsTypeFromGroupName(const std::string& name) {
  for (size_t i = 0; i < arraysize(kContentSettingsTypeGroupNames); ++i) {
    if (name == kContentSettingsTypeGroupNames[i].name)
      return kContentSettingsTypeGroupNames[i].type;
  }

  NOTREACHED() << name << " is not a recognized content settings type.";
  return CONTENT_SETTINGS_TYPE_DEFAULT;
}

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

std::string GeolocationExceptionToString(
    const ContentSettingsPattern& origin,
    const ContentSettingsPattern& embedding_origin) {
  if (origin == embedding_origin)
    return origin.ToString();

  // TODO(estade): the page needs to use CSS to indent the string.
  std::string indent(" ");

  if (embedding_origin == ContentSettingsPattern::Wildcard()) {
    // NOTE: As long as the user cannot add/edit entries from the exceptions
    // dialog, it's impossible to actually have a non-default setting for some
    // origin "embedded on any other site", so this row will never appear.  If
    // we add the ability to add/edit exceptions, we'll need to decide when to
    // display this and how "removing" it will function.
    return indent +
        l10n_util::GetStringUTF8(IDS_EXCEPTIONS_GEOLOCATION_EMBEDDED_ANY_OTHER);
  }

  return indent + l10n_util::GetStringFUTF8(
      IDS_EXCEPTIONS_GEOLOCATION_EMBEDDED_ON_HOST,
      UTF8ToUTF16(embedding_origin.ToString()));
}

// Create a DictionaryValue* that will act as a data source for a single row
// in a HostContentSettingsMap-controlled exceptions table (e.g., cookies).
// Ownership of the pointer is passed to the caller.
DictionaryValue* GetExceptionForPage(
    const ContentSettingsPattern& pattern,
    ContentSetting setting,
    std::string provider_name) {
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
  exception->SetString(kDisplayPattern,
                       GeolocationExceptionToString(origin, embedding_origin));
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

}  // namespace

ContentSettingsHandler::ContentSettingsHandler() {
}

ContentSettingsHandler::~ContentSettingsHandler() {
}

void ContentSettingsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  static OptionsStringResource resources[] = {
    { "content_exceptions", IDS_COOKIES_EXCEPTIONS_BUTTON },
    { "allowException", IDS_EXCEPTIONS_ALLOW_BUTTON },
    { "blockException", IDS_EXCEPTIONS_BLOCK_BUTTON },
    { "sessionException", IDS_EXCEPTIONS_SESSION_ONLY_BUTTON },
    { "askException", IDS_EXCEPTIONS_ASK_BUTTON },
    { "addExceptionRow", IDS_EXCEPTIONS_ADD_BUTTON },
    { "removeExceptionRow", IDS_EXCEPTIONS_REMOVE_BUTTON },
    { "editExceptionRow", IDS_EXCEPTIONS_EDIT_BUTTON },
    { "otr_exceptions_explanation", IDS_EXCEPTIONS_OTR_LABEL },
    { "examplePattern", IDS_EXCEPTIONS_PATTERN_EXAMPLE },
    { "addNewExceptionInstructions", IDS_EXCEPTIONS_ADD_NEW_INSTRUCTIONS },
    { "manage_exceptions", IDS_EXCEPTIONS_MANAGE },
    { "manage_handlers", IDS_HANDLERS_MANAGE },
    { "exceptionPatternHeader", IDS_EXCEPTIONS_PATTERN_HEADER },
    { "exceptionBehaviorHeader", IDS_EXCEPTIONS_ACTION_HEADER },
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
    // Notifications filter.
    { "notifications_tab_label", IDS_NOTIFICATIONS_TAB_LABEL },
    { "notifications_header", IDS_NOTIFICATIONS_HEADER },
    { "notifications_allow", IDS_NOTIFICATIONS_ALLOW_RADIO },
    { "notifications_ask", IDS_NOTIFICATIONS_ASK_RADIO },
    { "notifications_block", IDS_NOTIFICATIONS_BLOCK_RADIO },
    // Intents filter.
    { "intentsTabLabel", IDS_INTENTS_TAB_LABEL },
    { "intentsAllow", IDS_INTENTS_ALLOW_RADIO },
    { "intentsAsk", IDS_INTENTS_ASK_RADIO },
    { "intentsBlock", IDS_INTENTS_BLOCK_RADIO },
    { "intents_header", IDS_INTENTS_HEADER },
    // Fullscreen filter.
    { "fullscreen_tab_label", IDS_FULLSCREEN_TAB_LABEL },
    { "fullscreen_header", IDS_FULLSCREEN_HEADER },
  };

  RegisterStrings(localized_strings, resources, arraysize(resources));
  RegisterTitle(localized_strings, "contentSettingsPage",
                IDS_CONTENT_SETTINGS_TITLE);
  localized_strings->SetBoolean("enable_click_to_play",
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableClickToPlay));
  localized_strings->SetBoolean("enable_web_intents",
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableWebIntents));
}

void ContentSettingsHandler::Initialize() {
  notification_registrar_.Add(
      this, chrome::NOTIFICATION_PROFILE_CREATED,
      content::NotificationService::AllSources());
  notification_registrar_.Add(
      this, chrome::NOTIFICATION_PROFILE_DESTROYED,
      content::NotificationService::AllSources());

  UpdateHandlersEnabledRadios();
  UpdateAllExceptionsViewsFromModel();
  notification_registrar_.Add(
      this, chrome::NOTIFICATION_CONTENT_SETTINGS_CHANGED,
      content::NotificationService::AllSources());
  notification_registrar_.Add(
      this, chrome::NOTIFICATION_DESKTOP_NOTIFICATION_SETTINGS_CHANGED,
      content::NotificationService::AllSources());
  Profile* profile = Profile::FromWebUI(web_ui_);
  notification_registrar_.Add(
      this, chrome::NOTIFICATION_PROTOCOL_HANDLER_REGISTRY_CHANGED,
      content::Source<Profile>(profile));

  PrefService* prefs = profile->GetPrefs();
  pref_change_registrar_.Init(prefs);
  pref_change_registrar_.Add(prefs::kGeolocationContentSettings, this);
}

void ContentSettingsHandler::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_PROFILE_DESTROYED: {
      if (content::Source<Profile>(source).ptr()->IsOffTheRecord()) {
        web_ui_->CallJavascriptFunction(
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
      if (settings_details->update_all_types())
        UpdateAllExceptionsViewsFromModel();
      else
        UpdateExceptionsViewFromModel(settings_details->type());
      break;
    }

    case chrome::NOTIFICATION_PREF_CHANGED: {
      const std::string& pref_name =
          *content::Details<std::string>(details).ptr();
      if (pref_name == prefs::kGeolocationContentSettings)
        UpdateGeolocationExceptionsView();
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

void ContentSettingsHandler::UpdateSettingDefaultFromModel(
    ContentSettingsType type) {
  DictionaryValue filter_settings;
  std::string provider_id;
  filter_settings.SetString(ContentSettingsTypeToGroupName(type) + ".value",
                            GetSettingDefaultFromModel(type, &provider_id));
  filter_settings.SetString(
      ContentSettingsTypeToGroupName(type) + ".managedBy",
      provider_id);

  web_ui_->CallJavascriptFunction(
      "ContentSettings.setContentFilterSettingsValue", filter_settings);
}

std::string ContentSettingsHandler::GetSettingDefaultFromModel(
    ContentSettingsType type, std::string* provider_id) {
  Profile* profile = Profile::FromWebUI(web_ui_);
  ContentSetting default_setting;
  if (type == CONTENT_SETTINGS_TYPE_NOTIFICATIONS) {
    default_setting =
        DesktopNotificationServiceFactory::GetForProfile(profile)->
            GetDefaultContentSetting(provider_id);
  } else {
    default_setting =
        profile->GetHostContentSettingsMap()->
            GetDefaultContentSetting(type, provider_id);
  }

  return ContentSettingToString(default_setting);
}

void ContentSettingsHandler::UpdateHandlersEnabledRadios() {
#if defined(ENABLE_REGISTER_PROTOCOL_HANDLER)
  DCHECK(web_ui_);
  base::FundamentalValue handlers_enabled(
      GetProtocolHandlerRegistry()->enabled());

  web_ui_->CallJavascriptFunction("ContentSettings.updateHandlersEnabledRadios",
      handlers_enabled);
#endif  // defined(ENABLE_REGISTER_PROTOCOL_HANDLER)
}

void ContentSettingsHandler::UpdateAllExceptionsViewsFromModel() {
  for (int type = CONTENT_SETTINGS_TYPE_DEFAULT + 1;
       type < CONTENT_SETTINGS_NUM_TYPES; ++type) {
    // The content settings type CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE
    // is supposed to be set by policy only. Hence there is no user facing UI
    // for this content type and we skip it here.
    if (type == CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE)
      continue;
    // TODO(scheib): Mouse lock content settings UI. http://crbug.com/97768
    if (type == CONTENT_SETTINGS_TYPE_MOUSELOCK)
      continue;
    UpdateExceptionsViewFromModel(static_cast<ContentSettingsType>(type));
  }
}

void ContentSettingsHandler::UpdateAllOTRExceptionsViewsFromModel() {
  for (int type = CONTENT_SETTINGS_TYPE_DEFAULT + 1;
       type < CONTENT_SETTINGS_NUM_TYPES; ++type) {
    UpdateOTRExceptionsViewFromModel(static_cast<ContentSettingsType>(type));
  }
}

void ContentSettingsHandler::UpdateExceptionsViewFromModel(
    ContentSettingsType type) {
  // Skip updating intents unless it's enabled from the command line.
  if (type == CONTENT_SETTINGS_TYPE_INTENTS &&
      !CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableWebIntents))
    return;

  switch (type) {
    case CONTENT_SETTINGS_TYPE_GEOLOCATION:
      UpdateGeolocationExceptionsView();
      break;
    case CONTENT_SETTINGS_TYPE_NOTIFICATIONS:
      UpdateNotificationExceptionsView();
      break;
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
    case CONTENT_SETTINGS_TYPE_INTENTS:
    case CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE:
      break;
    default:
      UpdateExceptionsViewFromOTRHostContentSettingsMap(type);
      break;
  }
}

void ContentSettingsHandler::UpdateGeolocationExceptionsView() {
  Profile* profile = Profile::FromWebUI(web_ui_);
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
        i->source != "preferences") {
      continue;
    }
    all_patterns_settings[i->primary_pattern][i->secondary_pattern] =
        i->setting;
  }

  ListValue exceptions;
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
  web_ui_->CallJavascriptFunction("ContentSettings.setExceptions",
                                  type_string, exceptions);

  // This is mainly here to keep this function ideologically parallel to
  // UpdateExceptionsViewFromHostContentSettingsMap().
  UpdateSettingDefaultFromModel(CONTENT_SETTINGS_TYPE_GEOLOCATION);
}

void ContentSettingsHandler::UpdateNotificationExceptionsView() {
  Profile* profile = Profile::FromWebUI(web_ui_);
  DesktopNotificationService* service =
      DesktopNotificationServiceFactory::GetForProfile(profile);

  ContentSettingsForOneType settings;
  service->GetNotificationsSettings(&settings);

  ListValue exceptions;
  for (ContentSettingsForOneType::const_iterator i =
           settings.begin();
       i != settings.end();
       ++i) {
    // Don't add default settings.
    if (i->primary_pattern == ContentSettingsPattern::Wildcard() &&
        i->secondary_pattern == ContentSettingsPattern::Wildcard() &&
        i->source != "preferences") {
      continue;
    }

    exceptions.Append(
        GetNotificationExceptionForPage(i->primary_pattern, i->setting,
                                        i->source));
  }

  StringValue type_string(
      ContentSettingsTypeToGroupName(CONTENT_SETTINGS_TYPE_NOTIFICATIONS));
  web_ui_->CallJavascriptFunction("ContentSettings.setExceptions",
                                  type_string, exceptions);

  // This is mainly here to keep this function ideologically parallel to
  // UpdateExceptionsViewFromHostContentSettingsMap().
  UpdateSettingDefaultFromModel(CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
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
      exceptions.Append(
          GetExceptionForPage(entries[i].primary_pattern, entries[i].setting,
                              entries[i].source));
    } else {
      LOG(ERROR) << "Secondary content settings patterns are not "
                 << "supported by the content settings UI";
    }
  }

  StringValue type_string(ContentSettingsTypeToGroupName(type));
  web_ui_->CallJavascriptFunction("ContentSettings.setExceptions", type_string,
                                  exceptions);

  UpdateExceptionsViewFromOTRHostContentSettingsMap(type);

  // TODO(koz): The default for fullscreen is always 'ask'.
  // http://crbug.com/104683
  if (type == CONTENT_SETTINGS_TYPE_FULLSCREEN)
    return;

  // The default may also have changed (we won't get a separate notification).
  // If it hasn't changed, this call will be harmless.
  UpdateSettingDefaultFromModel(type);
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
      otr_exceptions.Append(
          GetExceptionForPage(otr_entries[i].primary_pattern,
                              otr_entries[i].setting,
                              otr_entries[i].source));
    } else {
      LOG(ERROR) << "Secondary content settings patterns are not "
                 << "supported by the content settings UI";
    }
  }

  StringValue type_string(ContentSettingsTypeToGroupName(type));
  web_ui_->CallJavascriptFunction("ContentSettings.setOTRExceptions",
                                  type_string, otr_exceptions);
}

void ContentSettingsHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("setContentFilter",
      base::Bind(&ContentSettingsHandler::SetContentFilter,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("removeException",
      base::Bind(&ContentSettingsHandler::RemoveException,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("setException",
      base::Bind(&ContentSettingsHandler::SetException,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("checkExceptionPatternValidity",
      base::Bind(&ContentSettingsHandler::CheckExceptionPatternValidity,
                 base::Unretained(this)));
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
  ContentSettingsType content_type = ContentSettingsTypeFromGroupName(group);
  if (content_type == CONTENT_SETTINGS_TYPE_NOTIFICATIONS) {
    Profile* profile = Profile::FromWebUI(web_ui_);
    DesktopNotificationServiceFactory::GetForProfile(profile)->
        SetDefaultContentSetting(default_setting);
  } else {
    GetContentSettingsMap()->
        SetDefaultContentSetting(content_type, default_setting);
  }
}

void ContentSettingsHandler::RemoveException(const ListValue* args) {
  size_t arg_i = 0;
  std::string type_string;
  CHECK(args->GetString(arg_i++, &type_string));

  Profile* profile = Profile::FromWebUI(web_ui_);
  ContentSettingsType type = ContentSettingsTypeFromGroupName(type_string);
  if (type == CONTENT_SETTINGS_TYPE_GEOLOCATION) {
    std::string origin;
    std::string embedding_origin;
    bool rv = args->GetString(arg_i++, &origin);
    DCHECK(rv);
    rv = args->GetString(arg_i++, &embedding_origin);
    DCHECK(rv);

    profile->GetHostContentSettingsMap()->
        SetContentSetting(ContentSettingsPattern::FromString(origin),
                          ContentSettingsPattern::FromString(embedding_origin),
                          CONTENT_SETTINGS_TYPE_GEOLOCATION,
                          std::string(),
                          CONTENT_SETTING_DEFAULT);
  } else if (type == CONTENT_SETTINGS_TYPE_NOTIFICATIONS) {
    std::string origin;
    std::string setting;
    bool rv = args->GetString(arg_i++, &origin);
    DCHECK(rv);
    rv = args->GetString(arg_i++, &setting);
    DCHECK(rv);
    ContentSetting content_setting = ContentSettingFromString(setting);

    DCHECK(content_setting == CONTENT_SETTING_ALLOW ||
           content_setting == CONTENT_SETTING_BLOCK);
    DesktopNotificationServiceFactory::GetForProfile(profile)->
        ClearSetting(ContentSettingsPattern::FromString(origin));
  } else {
    std::string mode;
    bool rv = args->GetString(arg_i++, &mode);
    DCHECK(rv);

    std::string pattern;
    rv = args->GetString(arg_i++, &pattern);
    DCHECK(rv);

    HostContentSettingsMap* settings_map =
        mode == "normal" ? GetContentSettingsMap() :
                           GetOTRContentSettingsMap();
    // The settings map could be null if the mode was OTR but the OTR profile
    // got destroyed before we received this message.
    if (settings_map) {
      settings_map->SetContentSetting(
          ContentSettingsPattern::FromString(pattern),
          ContentSettingsPattern::Wildcard(),
          ContentSettingsTypeFromGroupName(type_string),
          "",
          CONTENT_SETTING_DEFAULT);
    }
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

  ContentSettingsType type = ContentSettingsTypeFromGroupName(type_string);
  if (type == CONTENT_SETTINGS_TYPE_GEOLOCATION ||
      type == CONTENT_SETTINGS_TYPE_NOTIFICATIONS) {
    NOTREACHED();
    return;
  }

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
                                  "",
                                  ContentSettingFromString(setting));
}

void ContentSettingsHandler::CheckExceptionPatternValidity(
    const ListValue* args) {
  size_t arg_i = 0;
  Value* type;
  CHECK(args->Get(arg_i++, &type));
  std::string mode_string;
  CHECK(args->GetString(arg_i++, &mode_string));
  std::string pattern_string;
  CHECK(args->GetString(arg_i++, &pattern_string));

  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromString(pattern_string);

  scoped_ptr<Value> mode_value(Value::CreateStringValue(mode_string));
  scoped_ptr<Value> pattern_value(Value::CreateStringValue(pattern_string));
  scoped_ptr<Value> valid_value(Value::CreateBooleanValue(pattern.IsValid()));

  web_ui_->CallJavascriptFunction(
      "ContentSettings.patternValidityCheckComplete",
      *type,
      *mode_value.get(),
      *pattern_value.get(),
      *valid_value.get());
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
  return Profile::FromWebUI(web_ui_)->GetHostContentSettingsMap();
}

ProtocolHandlerRegistry* ContentSettingsHandler::GetProtocolHandlerRegistry() {
  return Profile::FromWebUI(web_ui_)->GetProtocolHandlerRegistry();
}

HostContentSettingsMap*
    ContentSettingsHandler::GetOTRContentSettingsMap() {
  Profile* profile = Profile::FromWebUI(web_ui_);
  if (profile->HasOffTheRecordProfile())
    return profile->GetOffTheRecordProfile()->GetHostContentSettingsMap();
  return NULL;
}
