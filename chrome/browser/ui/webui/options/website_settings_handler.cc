// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/website_settings_handler.h"

#include "chrome/browser/content_settings/content_settings_utils.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_ui.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/base/text/bytes_formatting.h"

namespace {

const int kHttpPort = 80;
const int kHttpsPort = 443;
const char kPreferencesSource[] = "preference";
const char kStorage[] = "storage";
const ContentSettingsType kValidTypes[] = {CONTENT_SETTINGS_TYPE_GEOLOCATION,
                                           CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                                           CONTENT_SETTINGS_TYPE_MEDIASTREAM};
const size_t kValidTypesLength = arraysize(kValidTypes);
}  // namespace

namespace options {

WebsiteSettingsHandler::WebsiteSettingsHandler()
    : observer_(this), weak_ptr_factory_(this) {
}

WebsiteSettingsHandler::~WebsiteSettingsHandler() {
}

void WebsiteSettingsHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  static OptionsStringResource resources[] = {
      {"websitesOptionsPageTabTitle", IDS_WEBSITE_SETTINGS_TITLE},
      {"websitesSettingsEditPage", IDS_WEBSITE_SETTINGS_EDIT_TITLE},
      {"websitesManage", IDS_WEBSITE_SETTINGS_MANAGE},
      {"websitesSearch", IDS_WEBSITE_SETTINGS_SEARCH_ORIGINS},
      {"websitesLabelGeolocation", IDS_WEBSITE_SETTINGS_TYPE_LOCATION},
      {"websitesLabelMediaStream", IDS_WEBSITE_SETTINGS_TYPE_MEDIASTREAM},
      {"websitesLabelNotifications", IDS_WEBSITE_SETTINGS_TYPE_NOTIFICATIONS},
      {"websitesLabelStorage", IDS_WEBSITE_SETTINGS_TYPE_STORAGE},
      {"websitesLocationDescription",
       IDS_WEBSITE_SETTINGS_LOCATION_DESCRIPTION},
      {"websitesMediastreamDescription",
       IDS_WEBSITE_SETTINGS_MEDIASTREAM_DESCRIPTION},
      {"websitesNotificationsDescription",
       IDS_WEBSITE_SETTINGS_NOTIFICATIONS_DESCRIPTION},
  };

  RegisterStrings(localized_strings, resources, arraysize(resources));
  RegisterTitle(
      localized_strings, "websiteSettingsPage", IDS_WEBSITE_SETTINGS_TITLE);
}

void WebsiteSettingsHandler::InitializeHandler() {
  Profile* profile = Profile::FromWebUI(web_ui());
  HostContentSettingsMap* settings = profile->GetHostContentSettingsMap();
  observer_.Add(settings);
}

void WebsiteSettingsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "updateOrigins",
      base::Bind(&WebsiteSettingsHandler::HandleUpdateOrigins,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "updateOriginsSearchResults",
      base::Bind(&WebsiteSettingsHandler::HandleUpdateSearchResults,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "updateLocalStorage",
      base::Bind(&WebsiteSettingsHandler::HandleUpdateLocalStorage,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "getOriginInfo",
      base::Bind(&WebsiteSettingsHandler::HandleGetOriginInfo,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "setOriginPermission",
      base::Bind(&WebsiteSettingsHandler::HandleSetOriginPermission,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "maybeShowEditPage",
      base::Bind(&WebsiteSettingsHandler::HandleMaybeShowEditPage,
                 base::Unretained(this)));
}

// content_settings::Observer implementation.
void WebsiteSettingsHandler::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    std::string resource_identifier) {
  Update();
}

void WebsiteSettingsHandler::HandleUpdateOrigins(const base::ListValue* args) {
  std::string content_setting_name;
  bool rv = args->GetString(0, &content_setting_name);
  DCHECK(rv);

  ContentSettingsType content_type;
  rv = content_settings::GetTypeFromName(content_setting_name, &content_type);
  DCHECK(rv);
  DCHECK_NE(
      kValidTypes + kValidTypesLength,
      std::find(kValidTypes, kValidTypes + kValidTypesLength, content_type));

  last_setting_ = content_setting_name;
  UpdateOrigins();
}

void WebsiteSettingsHandler::HandleUpdateSearchResults(
    const base::ListValue* args) {
  bool rv = args->GetString(0, &last_filter_);
  DCHECK(rv);

  Update();
}

void WebsiteSettingsHandler::HandleUpdateLocalStorage(
    const base::ListValue* args) {
  if (!local_storage_) {
    Profile* profile = Profile::FromWebUI(web_ui());
    local_storage_ = new BrowsingDataLocalStorageHelper(profile);
  }

  last_setting_ = kStorage;

  local_storage_->StartFetching(
      base::Bind(&WebsiteSettingsHandler::OnLocalStorageFetched,
                 weak_ptr_factory_.GetWeakPtr()));
}

void WebsiteSettingsHandler::HandleMaybeShowEditPage(
    const base::ListValue* args) {
  std::string site;
  bool rv = args->GetString(0, &site);
  DCHECK(rv);

  GURL last_site(site);
  if (!last_site.is_valid())
    return;

  last_site_ = last_site;
  base::StringValue site_value(site);
  web_ui()->CallJavascriptFunction("WebsiteSettingsManager.showEditPage",
                                   site_value);
}

void WebsiteSettingsHandler::OnLocalStorageFetched(const std::list<
    BrowsingDataLocalStorageHelper::LocalStorageInfo>& storage) {
  local_storage_list_ = storage;
  UpdateLocalStorage();
}

void WebsiteSettingsHandler::Update() {
  DCHECK(!last_setting_.empty());
  if (last_setting_ == kStorage)
    UpdateLocalStorage();
  else
    UpdateOrigins();
}

void WebsiteSettingsHandler::UpdateOrigins() {
  Profile* profile = Profile::FromWebUI(web_ui());
  HostContentSettingsMap* settings = profile->GetHostContentSettingsMap();

  ContentSettingsForOneType all_settings;
  ContentSettingsType last_setting;
  content_settings::GetTypeFromName(last_setting_, &last_setting);

  if (last_setting == CONTENT_SETTINGS_TYPE_MEDIASTREAM)
    last_setting = CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC;

  settings->GetSettingsForOneType(last_setting, std::string(), &all_settings);

  base::DictionaryValue origins;
  for (ContentSettingsForOneType::const_iterator it = all_settings.begin();
       it != all_settings.end();
       ++it) {
    // Don't add default settings.
    if (it->primary_pattern == ContentSettingsPattern::Wildcard() &&
        it->secondary_pattern == ContentSettingsPattern::Wildcard() &&
        it->source != kPreferencesSource) {
      continue;
    }

    GURL origin_url(it->primary_pattern.ToString());
    std::string origin = origin_url.spec();

    // Hide the port if it is using a standard URL scheme.
    if ((origin_url.SchemeIs(url::kHttpScheme) &&
         origin_url.IntPort() == kHttpPort) ||
        (origin_url.SchemeIs(url::kHttpsScheme) &&
         origin_url.IntPort() == kHttpsPort)) {
      url::Replacements<char> replacements;
      replacements.ClearPort();
      origin = origin_url.ReplaceComponents(replacements).spec();
    }

    // Mediastream isn't set unless both mic and camera are set to the same.
    if (last_setting == CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC) {
      ContentSetting cam_setting =
          settings->GetContentSetting(origin_url,
                                      origin_url,
                                      CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA,
                                      std::string());
      if (it->setting != cam_setting)
        continue;
    }

    if (origin.find(last_filter_) == base::string16::npos)
      continue;

    base::Time last_usage = settings->GetLastUsageByPattern(
        it->primary_pattern, it->secondary_pattern, last_setting);

    base::DictionaryValue* origin_entry = new base::DictionaryValue();
    origin_entry->SetDoubleWithoutPathExpansion("usage",
                                                last_usage.ToDoubleT());
    base::string16 usage_string;
    if (last_usage.ToDoubleT()) {
      usage_string = ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_ELAPSED,
                                            ui::TimeFormat::LENGTH_SHORT,
                                            base::Time::Now() - last_usage);
    }
    origin_entry->SetStringWithoutPathExpansion("usageString", usage_string);

    origins.SetWithoutPathExpansion(origin, origin_entry);
  }

  web_ui()->CallJavascriptFunction("WebsiteSettingsManager.populateOrigins",
                                   origins);
}

void WebsiteSettingsHandler::HandleGetOriginInfo(const base::ListValue* args) {
  std::string url;
  bool rv = args->GetString(0, &url);
  DCHECK(rv);
  GURL origin(url);

  if (!origin.is_valid())
    return;

  GetInfoForOrigin(origin);
}

void WebsiteSettingsHandler::HandleSetOriginPermission(
    const base::ListValue* args) {
  std::string setting_name;
  bool rv = args->GetString(0, &setting_name);
  DCHECK(rv);
  ContentSettingsType settings_type;
  rv = content_settings::GetTypeFromName(setting_name, &settings_type);
  DCHECK(rv);

  std::string value;
  rv = args->GetString(1, &value);
  DCHECK(rv);

  ContentSetting setting = content_settings::ContentSettingFromString(value);
  Profile* profile = Profile::FromWebUI(web_ui());
  HostContentSettingsMap* map = profile->GetHostContentSettingsMap();
  ContentSetting default_value =
      map->GetDefaultContentSetting(settings_type, NULL);

  // Users are not allowed to be the source of the "ask" setting. It is an
  // ephemeral setting which is removed once the question is asked.
  if (setting == CONTENT_SETTING_ASK && setting == default_value)
    setting = CONTENT_SETTING_DEFAULT;

  ContentSettingsPattern primary_pattern;
  ContentSettingsPattern secondary_pattern;
  switch (settings_type) {
    case CONTENT_SETTINGS_TYPE_NOTIFICATIONS:
      primary_pattern = ContentSettingsPattern::FromURLNoWildcard(last_site_);
      secondary_pattern = ContentSettingsPattern::Wildcard();
      break;
    case CONTENT_SETTINGS_TYPE_GEOLOCATION:
      primary_pattern = ContentSettingsPattern::FromURLNoWildcard(last_site_);
      secondary_pattern = ContentSettingsPattern::FromURLNoWildcard(last_site_);
      break;
    case CONTENT_SETTINGS_TYPE_MEDIASTREAM:
      primary_pattern = ContentSettingsPattern::FromURLNoWildcard(last_site_);
      secondary_pattern = ContentSettingsPattern::Wildcard();
      map->SetContentSetting(primary_pattern,
                             secondary_pattern,
                             CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC,
                             std::string(),
                             setting);
      map->SetContentSetting(primary_pattern,
                             secondary_pattern,
                             CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA,
                             std::string(),
                             setting);
      return;
    default:
      NOTREACHED() << "Content settings type not yet supported.";
      break;
  }

  content_settings::SettingInfo info;
  scoped_ptr<base::Value> v(map->GetWebsiteSetting(
      last_site_, last_site_, settings_type, std::string(), &info));
  map->SetNarrowestWebsiteSetting(primary_pattern,
                                  secondary_pattern,
                                  settings_type,
                                  std::string(),
                                  setting,
                                  info);
}

void WebsiteSettingsHandler::GetInfoForOrigin(const GURL& site_url) {
  Profile* profile = Profile::FromWebUI(web_ui());
  HostContentSettingsMap* map = profile->GetHostContentSettingsMap();

  double storage = 0.0;
  for (LocalStorageList::const_iterator it = local_storage_list_.begin();
       it != local_storage_list_.end();
       it++) {
    if (it->origin_url == site_url) {
      storage = static_cast<double>(it->size);
      break;
    }
  }

  base::DictionaryValue* permissions = new base::DictionaryValue;
  for (size_t i = 0; i < arraysize(kValidTypes); ++i) {
    ContentSettingsType permission_type = kValidTypes[i];

    // Append the possible settings.
    base::ListValue* options = new base::ListValue;
    ContentSetting default_value =
        map->GetDefaultContentSetting(permission_type, NULL);
    if (default_value != CONTENT_SETTING_ALLOW &&
        default_value != CONTENT_SETTING_BLOCK) {
      options->AppendString(
          content_settings::ContentSettingToString(default_value));
    }
    options->AppendString(
        content_settings::ContentSettingToString(CONTENT_SETTING_ALLOW));
    options->AppendString(
        content_settings::ContentSettingToString(CONTENT_SETTING_BLOCK));

    ContentSetting permission;
    content_settings::SettingInfo info;
    if (permission_type == CONTENT_SETTINGS_TYPE_MEDIASTREAM) {
      scoped_ptr<base::Value> mic_value(
          map->GetWebsiteSetting(site_url,
                                 site_url,
                                 CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC,
                                 std::string(),
                                 &info));
      ContentSetting mic_setting =
          content_settings::ValueToContentSetting(mic_value.get());
      ContentSetting cam_setting =
          map->GetContentSetting(site_url,
                                 site_url,
                                 CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA,
                                 std::string());

      if (mic_setting != cam_setting)
        permission = CONTENT_SETTING_ASK;
      else
        permission = mic_setting;
    } else {
      scoped_ptr<base::Value> v(map->GetWebsiteSetting(
          site_url, site_url, permission_type, std::string(), &info));
      permission = content_settings::ValueToContentSetting(v.get());
    }

    base::DictionaryValue* permission_info = new base::DictionaryValue;
    permission_info->SetStringWithoutPathExpansion(
        "setting", content_settings::ContentSettingToString(permission));
    permission_info->SetWithoutPathExpansion("options", options);
    permission_info->SetBooleanWithoutPathExpansion(
        "editable", info.source == content_settings::SETTING_SOURCE_USER);
    permissions->SetWithoutPathExpansion(
        content_settings::GetTypeName(permission_type), permission_info);
  }

  base::Value* storage_used = new base::StringValue(l10n_util::GetStringFUTF16(
      IDS_WEBSITE_SETTINGS_STORAGE_USED, ui::FormatBytes(storage)));

  web_ui()->CallJavascriptFunction(
      "WebsiteSettingsEditor.populateOrigin", *storage_used, *permissions);
}

void WebsiteSettingsHandler::UpdateLocalStorage() {
  base::DictionaryValue local_storage_map;
  for (LocalStorageList::const_iterator it = local_storage_list_.begin();
       it != local_storage_list_.end();
       it++) {
    std::string origin = it->origin_url.spec();

    if (origin.find(last_filter_) == base::string16::npos)
      continue;

    base::DictionaryValue* origin_entry = new base::DictionaryValue();
    origin_entry->SetWithoutPathExpansion(
        "usage", new base::FundamentalValue(static_cast<double>(it->size)));
    origin_entry->SetWithoutPathExpansion(
        "usageString", new base::StringValue(ui::FormatBytes(it->size)));
    local_storage_map.SetWithoutPathExpansion(origin, origin_entry);
  }
  web_ui()->CallJavascriptFunction("WebsiteSettingsManager.populateOrigins",
                                   local_storage_map);
}

}  // namespace options
