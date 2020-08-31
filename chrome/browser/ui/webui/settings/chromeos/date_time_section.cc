// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/date_time_section.h"

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/system/timezone_util.h"
#include "chrome/browser/ui/webui/settings/chromeos/date_time_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search_tag_registry.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/settings/system_settings_provider.h"
#include "chromeos/settings/timezone_settings.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

namespace chromeos {
namespace settings {
namespace {

const std::vector<SearchConcept>& GetDateTimeSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_DATE_TIME,
       mojom::kDateAndTimeSectionPath,
       mojom::SearchResultIcon::kClock,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSection,
       {.section = mojom::Section::kDateAndTime}},
      {IDS_OS_SETTINGS_TAG_DATE_TIME_MILITARY_CLOCK,
       mojom::kDateAndTimeSectionPath,
       mojom::SearchResultIcon::kClock,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::k24HourClock},
       {IDS_OS_SETTINGS_TAG_DATE_TIME_MILITARY_CLOCK_ALT1,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetFineGrainedTimeZoneSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_DATE_TIME_ZONE_SUBPAGE,
       mojom::kTimeZoneSubpagePath,
       mojom::SearchResultIcon::kClock,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kTimeZone}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetNoFineGrainedTimeZoneSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_DATE_TIME_ZONE,
       mojom::kDateAndTimeSectionPath,
       mojom::SearchResultIcon::kClock,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kChangeTimeZone}},
  });
  return *tags;
}

}  // namespace

DateTimeSection::DateTimeSection(Profile* profile,
                                 SearchTagRegistry* search_tag_registry)
    : OsSettingsSection(profile, search_tag_registry) {
  registry()->AddSearchTags(GetDateTimeSearchConcepts());

  SystemSettingsProvider provider;
  if (provider.Get(chromeos::kFineGrainedTimeZoneResolveEnabled)->GetBool())
    registry()->AddSearchTags(GetFineGrainedTimeZoneSearchConcepts());
  else
    registry()->AddSearchTags(GetNoFineGrainedTimeZoneSearchConcepts());
}

DateTimeSection::~DateTimeSection() = default;

void DateTimeSection::AddLoadTimeData(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"dateTimePageTitle", IDS_SETTINGS_DATE_TIME},
      {"timeZone", IDS_SETTINGS_TIME_ZONE},
      {"selectTimeZoneResolveMethod",
       IDS_SETTINGS_SELECT_TIME_ZONE_RESOLVE_METHOD},
      {"timeZoneGeolocation", IDS_SETTINGS_TIME_ZONE_GEOLOCATION},
      {"timeZoneButton", IDS_SETTINGS_TIME_ZONE_BUTTON},
      {"timeZoneSubpageTitle", IDS_SETTINGS_TIME_ZONE_SUBPAGE_TITLE},
      {"setTimeZoneAutomaticallyDisabled",
       IDS_SETTINGS_TIME_ZONE_DETECTION_MODE_DISABLED},
      {"setTimeZoneAutomaticallyOn",
       IDS_SETTINGS_TIME_ZONE_DETECTION_SET_AUTOMATICALLY},
      {"setTimeZoneAutomaticallyOff",
       IDS_SETTINGS_TIME_ZONE_DETECTION_CHOOSE_FROM_LIST},
      {"setTimeZoneAutomaticallyIpOnlyDefault",
       IDS_SETTINGS_TIME_ZONE_DETECTION_MODE_IP_ONLY_DEFAULT},
      {"setTimeZoneAutomaticallyWithWiFiAccessPointsData",
       IDS_SETTINGS_TIME_ZONE_DETECTION_MODE_SEND_WIFI_AP},
      {"setTimeZoneAutomaticallyWithAllLocationInfo",
       IDS_SETTINGS_TIME_ZONE_DETECTION_MODE_SEND_ALL_INFO},
      {"use24HourClock", IDS_SETTINGS_USE_24_HOUR_CLOCK},
      {"setDateTime", IDS_SETTINGS_SET_DATE_TIME},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);

  html_source->AddString(
      "timeZoneSettingsLearnMoreURL",
      base::ASCIIToUTF16(base::StringPrintf(
          chrome::kTimeZoneSettingsLearnMoreURL,
          g_browser_process->GetApplicationLocale().c_str())));

  // Set the initial time zone to show.
  html_source->AddString("timeZoneName", system::GetCurrentTimezoneName());
  html_source->AddString(
      "timeZoneID",
      system::TimezoneSettings::GetInstance()->GetCurrentTimezoneID());
  html_source->AddBoolean(
      "timeActionsProtectedForChild",
      base::FeatureList::IsEnabled(features::kParentAccessCodeForTimeChange));
}

void DateTimeSection::AddHandlers(content::WebUI* web_ui) {
  web_ui->AddMessageHandler(std::make_unique<DateTimeHandler>());
}

}  // namespace settings
}  // namespace chromeos
