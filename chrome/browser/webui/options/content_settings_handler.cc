// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webui/options/content_settings_handler.h"

#include "base/callback.h"
#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/content_settings_details.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/geolocation/geolocation_content_settings_map.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/content_settings_helper.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

const char* kDisplayPattern = "displayPattern";
const char* kSetting = "setting";
const char* kOrigin = "origin";
const char* kEmbeddingOrigin = "embeddingOrigin";

const char* const kContentSettingsTypeGroupNames[] = {
  "cookies",
  "images",
  "javascript",
  "plugins",
  "popups",
  "location",
  "notifications",
  "prerender",
};
COMPILE_ASSERT(arraysize(kContentSettingsTypeGroupNames) ==
               CONTENT_SETTINGS_NUM_TYPES,
               invalid_content_settings_type_group_names_size);


ContentSettingsType ContentSettingsTypeFromGroupName(const std::string& name) {

  for (int content_settings_type = CONTENT_SETTINGS_TYPE_COOKIES;
       content_settings_type < CONTENT_SETTINGS_NUM_TYPES;
       ++content_settings_type) {
    if (name == kContentSettingsTypeGroupNames[content_settings_type])
      return static_cast<ContentSettingsType>(content_settings_type);
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

std::string GeolocationExceptionToString(const GURL& origin,
                                         const GURL& embedding_origin) {
  if (origin == embedding_origin)
    return content_settings_helper::OriginToString(origin);

  // TODO(estade): the page needs to use CSS to indent the string.
  std::string indent(" ");
  if (embedding_origin.is_empty()) {
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
      UTF8ToUTF16(content_settings_helper::OriginToString(embedding_origin)));
}

// Create a DictionaryValue* that will act as a data source for a single row
// in a HostContentSettingsMap-controlled exceptions table (e.g., cookies).
// Ownership of the pointer is passed to the caller.
DictionaryValue* GetExceptionForPage(
    const ContentSettingsPattern pattern,
    ContentSetting setting) {
  DictionaryValue* exception = new DictionaryValue();
  exception->Set(
      kDisplayPattern,
      new StringValue(pattern.AsString()));
  exception->Set(
      kSetting,
      new StringValue(ContentSettingToString(setting)));
  return exception;
}

// Create a DictionaryValue* that will act as a data source for a single row
// in the Geolocation exceptions table. Ownership of the pointer is passed to
// the caller.
DictionaryValue* GetGeolocationExceptionForPage(const GURL& origin,
                                                const GURL& embedding_origin,
                                                ContentSetting setting) {
  DictionaryValue* exception = new DictionaryValue();
  exception->Set(
      kDisplayPattern,
      new StringValue(GeolocationExceptionToString(origin, embedding_origin)));
  exception->Set(
      kSetting,
      new StringValue(ContentSettingToString(setting)));
  exception->Set(
      kOrigin,
      new StringValue(origin.spec()));
  exception->Set(
      kEmbeddingOrigin,
      new StringValue(embedding_origin.spec()));
  return exception;
}

// Create a DictionaryValue* that will act as a data source for a single row
// in the desktop notifications exceptions table. Ownership of the pointer is
// passed to the caller.
DictionaryValue* GetNotificationExceptionForPage(
    const GURL& url,
    ContentSetting setting) {
  DictionaryValue* exception = new DictionaryValue();
  exception->Set(
      kDisplayPattern,
      new StringValue(content_settings_helper::OriginToString(url)));
  exception->Set(
      kSetting,
      new StringValue(ContentSettingToString(setting)));
  exception->Set(
      kOrigin,
      new StringValue(url.spec()));
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
    // Cookies filter.
    { "cookies_tab_label", IDS_COOKIES_TAB_LABEL },
    { "cookies_header", IDS_COOKIES_HEADER },
    { "cookies_allow", IDS_COOKIES_ALLOW_RADIO },
    { "cookies_ask", IDS_COOKIES_ASK_EVERY_TIME_RADIO },
    { "cookies_block", IDS_COOKIES_BLOCK_RADIO },
    { "cookies_block_3rd_party", IDS_COOKIES_BLOCK_3RDPARTY_CHKBOX },
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
    { "disable_individual_plugins", IDS_PLUGIN_SELECTIVE_DISABLE },
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
  };

  RegisterStrings(localized_strings, resources, arraysize(resources));
  RegisterTitle(localized_strings, "contentSettingsPage",
                IDS_CONTENT_SETTINGS_TITLE);
  localized_strings->SetBoolean("enable_click_to_play",
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableClickToPlay));
}

void ContentSettingsHandler::Initialize() {
  const HostContentSettingsMap* settings_map = GetContentSettingsMap();
  scoped_ptr<Value> block_3rd_party(Value::CreateBooleanValue(
      settings_map->BlockThirdPartyCookies()));
  web_ui_->CallJavascriptFunction(
      L"ContentSettings.setBlockThirdPartyCookies", *block_3rd_party.get());

  clear_plugin_lso_data_enabled_.Init(prefs::kClearPluginLSODataEnabled,
                                      g_browser_process->local_state(),
                                      this);
  UpdateClearPluginLSOData();

  notification_registrar_.Add(
      this, NotificationType::OTR_PROFILE_CREATED,
      NotificationService::AllSources());
  notification_registrar_.Add(
      this, NotificationType::PROFILE_DESTROYED,
      NotificationService::AllSources());

  UpdateAllExceptionsViewsFromModel();
  notification_registrar_.Add(
      this, NotificationType::CONTENT_SETTINGS_CHANGED,
      NotificationService::AllSources());
  notification_registrar_.Add(
      this, NotificationType::DESKTOP_NOTIFICATION_DEFAULT_CHANGED,
      NotificationService::AllSources());
  notification_registrar_.Add(
      this, NotificationType::DESKTOP_NOTIFICATION_SETTINGS_CHANGED,
      NotificationService::AllSources());

  PrefService* prefs = web_ui_->GetProfile()->GetPrefs();
  pref_change_registrar_.Init(prefs);
  pref_change_registrar_.Add(prefs::kGeolocationDefaultContentSetting, this);
  pref_change_registrar_.Add(prefs::kGeolocationContentSettings, this);
}

void ContentSettingsHandler::Observe(NotificationType type,
                                     const NotificationSource& source,
                                     const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::PROFILE_DESTROYED: {
      Profile* profile = static_cast<Source<Profile> >(source).ptr();
      if (profile->IsOffTheRecord()) {
        web_ui_->CallJavascriptFunction(
            L"ContentSettingsExceptionsArea.OTRProfileDestroyed");
      }
      break;
    }

    case NotificationType::OTR_PROFILE_CREATED: {
      UpdateAllOTRExceptionsViewsFromModel();
      break;
    }

    case NotificationType::CONTENT_SETTINGS_CHANGED: {
      const ContentSettingsDetails* settings_details =
          Details<const ContentSettingsDetails>(details).ptr();

      // TODO(estade): we pretend update_all() is always true.
      if (settings_details->update_all_types())
        UpdateAllExceptionsViewsFromModel();
      else
        UpdateExceptionsViewFromModel(settings_details->type());
      break;
    }

    case NotificationType::PREF_CHANGED: {
      const std::string& pref_name = *Details<std::string>(details).ptr();
      if (pref_name == prefs::kGeolocationDefaultContentSetting)
        UpdateSettingDefaultFromModel(CONTENT_SETTINGS_TYPE_GEOLOCATION);
      else if (pref_name == prefs::kGeolocationContentSettings)
        UpdateGeolocationExceptionsView();
      else if (pref_name == prefs::kClearPluginLSODataEnabled)
        UpdateClearPluginLSOData();
      break;
    }

    case NotificationType::DESKTOP_NOTIFICATION_DEFAULT_CHANGED: {
      UpdateSettingDefaultFromModel(CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
      break;
    }

    case NotificationType::DESKTOP_NOTIFICATION_SETTINGS_CHANGED: {
      UpdateNotificationExceptionsView();
      break;
    }

    default:
      OptionsPageUIHandler::Observe(type, source, details);
  }
}

void ContentSettingsHandler::UpdateClearPluginLSOData() {
  int label_id = clear_plugin_lso_data_enabled_.GetValue() ?
      IDS_COOKIES_LSO_CLEAR_WHEN_CLOSE_CHKBOX :
      IDS_COOKIES_CLEAR_WHEN_CLOSE_CHKBOX;
  scoped_ptr<Value> label(
      Value::CreateStringValue(l10n_util::GetStringUTF16(label_id)));
  web_ui_->CallJavascriptFunction(
      L"ContentSettings.setClearLocalDataOnShutdownLabel", *label);
}

void ContentSettingsHandler::UpdateSettingDefaultFromModel(
    ContentSettingsType type) {
  DictionaryValue filter_settings;
  filter_settings.SetString(ContentSettingsTypeToGroupName(type) + ".value",
      GetSettingDefaultFromModel(type));
  filter_settings.SetBoolean(ContentSettingsTypeToGroupName(type) + ".managed",
      GetDefaultSettingManagedFromModel(type));

  web_ui_->CallJavascriptFunction(
      L"ContentSettings.setContentFilterSettingsValue", filter_settings);
}

std::string ContentSettingsHandler::GetSettingDefaultFromModel(
    ContentSettingsType type) {
  ContentSetting default_setting;
  if (type == CONTENT_SETTINGS_TYPE_GEOLOCATION) {
    default_setting = web_ui_->GetProfile()->
        GetGeolocationContentSettingsMap()->GetDefaultContentSetting();
  } else if (type == CONTENT_SETTINGS_TYPE_NOTIFICATIONS) {
    default_setting = web_ui_->GetProfile()->
        GetDesktopNotificationService()->GetDefaultContentSetting();
  } else {
    default_setting = GetContentSettingsMap()->GetDefaultContentSetting(type);
  }

  return ContentSettingToString(default_setting);
}

bool ContentSettingsHandler::GetDefaultSettingManagedFromModel(
    ContentSettingsType type) {
  if (type == CONTENT_SETTINGS_TYPE_GEOLOCATION) {
    return web_ui_->GetProfile()->
        GetGeolocationContentSettingsMap()->IsDefaultContentSettingManaged();
  } else if (type == CONTENT_SETTINGS_TYPE_NOTIFICATIONS) {
    return web_ui_->GetProfile()->
        GetDesktopNotificationService()->IsDefaultContentSettingManaged();
  } else {
    return GetContentSettingsMap()->IsDefaultContentSettingManaged(type);
  }
}

void ContentSettingsHandler::UpdateAllExceptionsViewsFromModel() {
  for (int type = CONTENT_SETTINGS_TYPE_DEFAULT + 1;
       type < CONTENT_SETTINGS_NUM_TYPES; ++type) {
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
  switch (type) {
    case CONTENT_SETTINGS_TYPE_GEOLOCATION:
      UpdateGeolocationExceptionsView();
      break;
    case CONTENT_SETTINGS_TYPE_NOTIFICATIONS:
      UpdateNotificationExceptionsView();
      break;
    case CONTENT_SETTINGS_TYPE_PRERENDER:
      // Prerender is currently (02/24/2011) an experimental feature which is
      // only turned on via about:flags. There is intentionally no UI in
      // chrome://preferences for CONTENT_SETTINGS_TYPE_PRERENDER.
      // TODO(cbentzel): Change once prerender moves out of about:flags.
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
    case CONTENT_SETTINGS_TYPE_PRERENDER:
      break;
    default:
      UpdateExceptionsViewFromOTRHostContentSettingsMap(type);
      break;
  }
}

void ContentSettingsHandler::UpdateGeolocationExceptionsView() {
  GeolocationContentSettingsMap* map =
      web_ui_->GetProfile()->GetGeolocationContentSettingsMap();
  GeolocationContentSettingsMap::AllOriginsSettings all_settings =
      map->GetAllOriginsSettings();
  GeolocationContentSettingsMap::AllOriginsSettings::const_iterator i;

  ListValue exceptions;
  for (i = all_settings.begin(); i != all_settings.end(); ++i) {
    const GURL& origin = i->first;
    const GeolocationContentSettingsMap::OneOriginSettings& one_settings =
        i->second;

    GeolocationContentSettingsMap::OneOriginSettings::const_iterator parent =
        one_settings.find(origin);

    // Add the "parent" entry for the non-embedded setting.
    ContentSetting parent_setting =
        parent == one_settings.end() ? CONTENT_SETTING_DEFAULT : parent->second;
    exceptions.Append(
        GetGeolocationExceptionForPage(origin, origin, parent_setting));

    // Add the "children" for any embedded settings.
    GeolocationContentSettingsMap::OneOriginSettings::const_iterator j;
    for (j = one_settings.begin(); j != one_settings.end(); ++j) {
      // Skip the non-embedded setting which we already added above.
      if (j == parent)
        continue;

      exceptions.Append(
          GetGeolocationExceptionForPage(origin, j->first, j->second));
    }
  }

  StringValue type_string(
      ContentSettingsTypeToGroupName(CONTENT_SETTINGS_TYPE_GEOLOCATION));
  web_ui_->CallJavascriptFunction(
      L"ContentSettings.setExceptions", type_string, exceptions);

  // This is mainly here to keep this function ideologically parallel to
  // UpdateExceptionsViewFromHostContentSettingsMap().
  UpdateSettingDefaultFromModel(CONTENT_SETTINGS_TYPE_GEOLOCATION);
}

void ContentSettingsHandler::UpdateNotificationExceptionsView() {
  DesktopNotificationService* service =
      web_ui_->GetProfile()->GetDesktopNotificationService();

  std::vector<GURL> allowed(service->GetAllowedOrigins());
  std::vector<GURL> blocked(service->GetBlockedOrigins());

  ListValue exceptions;
  for (size_t i = 0; i < allowed.size(); ++i) {
    exceptions.Append(
        GetNotificationExceptionForPage(allowed[i], CONTENT_SETTING_ALLOW));
  }
  for (size_t i = 0; i < blocked.size(); ++i) {
    exceptions.Append(
        GetNotificationExceptionForPage(blocked[i], CONTENT_SETTING_BLOCK));
  }

  StringValue type_string(
      ContentSettingsTypeToGroupName(CONTENT_SETTINGS_TYPE_NOTIFICATIONS));
  web_ui_->CallJavascriptFunction(
      L"ContentSettings.setExceptions", type_string, exceptions);

  // This is mainly here to keep this function ideologically parallel to
  // UpdateExceptionsViewFromHostContentSettingsMap().
  UpdateSettingDefaultFromModel(CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
}

void ContentSettingsHandler::UpdateExceptionsViewFromHostContentSettingsMap(
    ContentSettingsType type) {
  HostContentSettingsMap::SettingsForOneType entries;
  GetContentSettingsMap()->GetSettingsForOneType(type, "", &entries);

  ListValue exceptions;
  for (size_t i = 0; i < entries.size(); ++i) {
    exceptions.Append(GetExceptionForPage(entries[i].first, entries[i].second));
  }

  StringValue type_string(ContentSettingsTypeToGroupName(type));
  web_ui_->CallJavascriptFunction(
      L"ContentSettings.setExceptions", type_string, exceptions);

  UpdateExceptionsViewFromOTRHostContentSettingsMap(type);

  // The default may also have changed (we won't get a separate notification).
  // If it hasn't changed, this call will be harmless.
  UpdateSettingDefaultFromModel(type);
}

void ContentSettingsHandler::UpdateExceptionsViewFromOTRHostContentSettingsMap(
    ContentSettingsType type) {
  const HostContentSettingsMap* otr_settings_map = GetOTRContentSettingsMap();
  if (!otr_settings_map)
    return;

  HostContentSettingsMap::SettingsForOneType otr_entries;
  otr_settings_map->GetSettingsForOneType(type, "", &otr_entries);

  ListValue otr_exceptions;
  for (size_t i = 0; i < otr_entries.size(); ++i) {
    otr_exceptions.Append(GetExceptionForPage(otr_entries[i].first,
                                              otr_entries[i].second));
  }

  StringValue type_string(ContentSettingsTypeToGroupName(type));
  web_ui_->CallJavascriptFunction(
      L"ContentSettings.setOTRExceptions", type_string, otr_exceptions);
}

void ContentSettingsHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("setContentFilter",
      NewCallback(this,
                  &ContentSettingsHandler::SetContentFilter));
  web_ui_->RegisterMessageCallback("setAllowThirdPartyCookies",
      NewCallback(this,
                  &ContentSettingsHandler::SetAllowThirdPartyCookies));
  web_ui_->RegisterMessageCallback("removeException",
      NewCallback(this,
                  &ContentSettingsHandler::RemoveException));
  web_ui_->RegisterMessageCallback("setException",
      NewCallback(this,
                  &ContentSettingsHandler::SetException));
  web_ui_->RegisterMessageCallback("checkExceptionPatternValidity",
      NewCallback(this,
                  &ContentSettingsHandler::CheckExceptionPatternValidity));
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
  if (content_type == CONTENT_SETTINGS_TYPE_GEOLOCATION) {
    web_ui_->GetProfile()->GetGeolocationContentSettingsMap()->
        SetDefaultContentSetting(default_setting);
  } else if (content_type == CONTENT_SETTINGS_TYPE_NOTIFICATIONS) {
    web_ui_->GetProfile()->GetDesktopNotificationService()->
        SetDefaultContentSetting(default_setting);
  } else {
    GetContentSettingsMap()->
        SetDefaultContentSetting(content_type, default_setting);
  }
}

void ContentSettingsHandler::SetAllowThirdPartyCookies(const ListValue* args) {
  std::wstring allow = ExtractStringValue(args);

  GetContentSettingsMap()->SetBlockThirdPartyCookies(allow == L"true");
}

void ContentSettingsHandler::RemoveException(const ListValue* args) {
  size_t arg_i = 0;
  std::string type_string;
  CHECK(args->GetString(arg_i++, &type_string));

  ContentSettingsType type = ContentSettingsTypeFromGroupName(type_string);
  if (type == CONTENT_SETTINGS_TYPE_GEOLOCATION) {
    std::string origin;
    std::string embedding_origin;
    bool rv = args->GetString(arg_i++, &origin);
    DCHECK(rv);
    rv = args->GetString(arg_i++, &embedding_origin);
    DCHECK(rv);

    web_ui_->GetProfile()->GetGeolocationContentSettingsMap()->
        SetContentSetting(GURL(origin),
                          GURL(embedding_origin),
                          CONTENT_SETTING_DEFAULT);
  } else if (type == CONTENT_SETTINGS_TYPE_NOTIFICATIONS) {
    std::string origin;
    std::string setting;
    bool rv = args->GetString(arg_i++, &origin);
    DCHECK(rv);
    rv = args->GetString(arg_i++, &setting);
    DCHECK(rv);
    ContentSetting content_setting = ContentSettingFromString(setting);
    if (content_setting == CONTENT_SETTING_ALLOW) {
      web_ui_->GetProfile()->GetDesktopNotificationService()->
          ResetAllowedOrigin(GURL(origin));
    } else {
      DCHECK_EQ(content_setting, CONTENT_SETTING_BLOCK);
      web_ui_->GetProfile()->GetDesktopNotificationService()->
          ResetBlockedOrigin(GURL(origin));
    }
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
          ContentSettingsPattern(pattern),
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

  settings_map->SetContentSetting(ContentSettingsPattern(pattern),
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

  ContentSettingsPattern pattern(pattern_string);

  scoped_ptr<Value> mode_value(Value::CreateStringValue(mode_string));
  scoped_ptr<Value> pattern_value(Value::CreateStringValue(pattern_string));
  scoped_ptr<Value> valid_value(Value::CreateBooleanValue(pattern.IsValid()));

  web_ui_->CallJavascriptFunction(
      L"ContentSettings.patternValidityCheckComplete", *type,
                                                       *mode_value.get(),
                                                       *pattern_value.get(),
                                                       *valid_value.get());
}

// static
std::string ContentSettingsHandler::ContentSettingsTypeToGroupName(
    ContentSettingsType type) {
  if (type < CONTENT_SETTINGS_TYPE_COOKIES ||
      type >= CONTENT_SETTINGS_NUM_TYPES) {
    NOTREACHED();
    return "";
  }
  return kContentSettingsTypeGroupNames[type];
}

HostContentSettingsMap* ContentSettingsHandler::GetContentSettingsMap() {
  return web_ui_->GetProfile()->GetHostContentSettingsMap();
}

HostContentSettingsMap*
    ContentSettingsHandler::GetOTRContentSettingsMap() {
  Profile* profile = web_ui_->GetProfile();
  if (profile->HasOffTheRecordProfile())
    return profile->GetOffTheRecordProfile()->GetHostContentSettingsMap();
  return NULL;
}
