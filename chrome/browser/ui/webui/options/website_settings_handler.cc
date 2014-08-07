// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/website_settings_handler.h"

#include "chrome/browser/content_settings/content_settings_utils.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_ui.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/time_format.h"
#include "ui/base/text/bytes_formatting.h"

namespace {

const char kPreferencesSource[] = "preference";
const char kStorage[] = "storage";
const ContentSettingsType kValidTypes[] = {CONTENT_SETTINGS_TYPE_GEOLOCATION,
                                           CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                                           CONTENT_SETTINGS_TYPE_MEDIASTREAM};
const size_t kValidTypesLength = arraysize(kValidTypes);
}  // namespace

namespace options {

WebsiteSettingsHandler::WebsiteSettingsHandler() : weak_ptr_factory_(this) {
}

WebsiteSettingsHandler::~WebsiteSettingsHandler() {
}

void WebsiteSettingsHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  static OptionsStringResource resources[] = {
      {"websitesOptionsPageTabTitle", IDS_WEBSITES_SETTINGS_TITLE},
      {"websitesManage", IDS_WEBSITE_SETTINGS_MANAGE},
      {"websitesSearch", IDS_WEBSITE_SETTINGS_SEARCH_ORIGINS},
      {"websitesLabelGeolocation", IDS_WEBSITE_SETTINGS_TYPE_LOCATION},
      {"websitesLabelMediaStream", IDS_WEBSITE_SETTINGS_TYPE_MEDIASTREAM},
      {"websitesLabelNotifications", IDS_WEBSITE_SETTINGS_TYPE_NOTIFICATIONS},
      {"websitesLabelStorage", IDS_WEBSITE_SETTINGS_TYPE_STORAGE},
  };

  RegisterStrings(localized_strings, resources, arraysize(resources));
  RegisterTitle(
      localized_strings, "websiteSettingsPage", IDS_WEBSITES_SETTINGS_TITLE);
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

  if (last_setting_ == kStorage)
    UpdateLocalStorage();
  else
    UpdateOrigins();
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

void WebsiteSettingsHandler::OnLocalStorageFetched(const std::list<
    BrowsingDataLocalStorageHelper::LocalStorageInfo>& storage) {
  local_storage_list_ = storage;
  UpdateLocalStorage();
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

    std::string origin = it->primary_pattern.ToString();

    // Mediastream isn't set unless both mic and camera are set to the same.
    if (last_setting == CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC) {
      GURL origin_url(origin);
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
    origin_entry->SetStringWithoutPathExpansion(
        "usageString",
        ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_ELAPSED,
                               ui::TimeFormat::LENGTH_SHORT,
                               base::Time::Now() - last_usage));
    origins.SetWithoutPathExpansion(origin, origin_entry);
  }

  web_ui()->CallJavascriptFunction("WebsiteSettingsManager.populateOrigins",
                                   origins);
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
