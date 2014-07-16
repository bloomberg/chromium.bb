// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/website_settings_handler.h"

#include "chrome/browser/content_settings/content_settings_utils.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_ui.h"
#include "grit/generated_resources.h"

namespace {

const char kPreferencesSource[] = "preference";

}  // namespace

namespace options {

WebsiteSettingsHandler::WebsiteSettingsHandler() {
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
}

void WebsiteSettingsHandler::HandleUpdateOrigins(const base::ListValue* args) {
  std::string content_setting_name;
  bool rv = args->GetString(0, &content_setting_name);
  DCHECK(rv);
  ContentSettingsType content_type;
  content_settings::GetTypeFromName(content_setting_name, &content_type);

  last_setting_ = content_type;

  UpdateOrigins(std::string());
}

void WebsiteSettingsHandler::HandleUpdateSearchResults(
    const base::ListValue* args) {
  std::string filter;
  bool rv = args->GetString(0, &filter);
  DCHECK(rv);

  UpdateOrigins(filter);
}

void WebsiteSettingsHandler::UpdateOrigins(const std::string& filter) {
  Profile* profile = Profile::FromWebUI(web_ui());
  HostContentSettingsMap* settings = profile->GetHostContentSettingsMap();

  ContentSettingsForOneType all_settings;
  settings->GetSettingsForOneType(last_setting_, std::string(), &all_settings);

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

    if (origin.find(filter) == base::string16::npos)
      continue;

    // TODO(dhnishi): Change 0 to a timestamp representing last API usage.
    origins.SetDoubleWithoutPathExpansion(origin, 0);
  }

  web_ui()->CallJavascriptFunction("WebsiteSettingsManager.populateOrigins",
                                   origins);
}

}  // namespace options
