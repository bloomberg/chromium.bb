// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dom_ui/system_options_handler.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/basictypes.h"
#include "base/callback.h"
#include "base/i18n/time_formatting.h"
#include "base/stl_util-inl.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/notification_service.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"

static const char* kTimeZonesUtf8[] = {
    "Pacific/Samoa",
    "US/Hawaii",
    "US/Alaska",
    "US/Pacific",
    "US/Mountain",
    "US/Central",
    "US/Eastern",
    "America/Santiago",
    "America/Sao_Paulo",
    "Atlantic/South_Georgia",
    "Atlantic/Cape_Verde",
    "Europe/London",
    "Europe/Rome",
    "Europe/Helsinki",
    "Europe/Moscow",
    "Asia/Dubai",
    "Asia/Karachi",
    "Asia/Dhaka",
    "Asia/Bangkok",
    "Asia/Hong_Kong",
    "Asia/Tokyo",
    "Australia/Sydney",
    "Asia/Magadan",
    "Pacific/Auckland" };

SystemOptionsHandler::SystemOptionsHandler() {
  // TODO(chocobo): For now, add all the GMT timezones.
  // We may eventually want to use icu::TimeZone::createEnumeration()
  // to list all the timezones and pick the ones we want to show.
  // NOTE: This currently does not handle daylight savings properly
  // b/c this is just a manually selected list of timezones that
  // happen to span the GMT-11 to GMT+12 Today. When daylight savings
  // kick in, this list might have more than one timezone in the same
  // GMT bucket.
  for (size_t i = 0; i < arraysize(kTimeZonesUtf8); i++) {
    timezones_.push_back(icu::TimeZone::createTimeZone(
        icu::UnicodeString::fromUTF8(kTimeZonesUtf8[i])));
  }
}

SystemOptionsHandler::~SystemOptionsHandler() {
  STLDeleteElements(&timezones_);
}

void SystemOptionsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);
  // System page - ChromeOS
  localized_strings->SetString(L"systemPage",
      l10n_util::GetString(IDS_OPTIONS_SYSTEM_TAB_LABEL));
  localized_strings->SetString(L"datetime_title",
      l10n_util::GetString(IDS_OPTIONS_SETTINGS_SECTION_TITLE_DATETIME));
  localized_strings->SetString(L"timezone",
      l10n_util::GetString(IDS_OPTIONS_SETTINGS_TIMEZONE_DESCRIPTION));

  localized_strings->SetString(L"touchpad",
      l10n_util::GetString(IDS_OPTIONS_SETTINGS_SECTION_TITLE_TOUCHPAD));
  localized_strings->SetString(L"enable_tap_to_click",
      l10n_util::GetString(
          IDS_OPTIONS_SETTINGS_TAP_TO_CLICK_ENABLED_DESCRIPTION));
  localized_strings->SetString(L"enable_vert_edge_scroll",
      l10n_util::GetString(
          IDS_OPTIONS_SETTINGS_VERT_EDGE_SCROLL_ENABLED_DESCRIPTION));
  localized_strings->SetString(L"sensitivity",
      l10n_util::GetString(IDS_OPTIONS_SETTINGS_SENSITIVITY_DESCRIPTION));
  localized_strings->SetString(L"speed_factor",
      l10n_util::GetString(IDS_OPTIONS_SETTINGS_SPEED_FACTOR_DESCRIPTION));

  localized_strings->SetString(L"language",
      l10n_util::GetString(IDS_OPTIONS_SETTINGS_SECTION_TITLE_LANGUAGE));
  localized_strings->SetString(L"language_customize",
      l10n_util::GetString(IDS_OPTIONS_SETTINGS_LANGUAGES_CUSTOMIZE));

  localized_strings->SetString(L"accessibility_title",
      l10n_util::GetString(IDS_OPTIONS_SETTINGS_SECTION_TITLE_ACCESSIBILITY));
  localized_strings->SetString(L"accessibility",
      l10n_util::GetString(IDS_OPTIONS_SETTINGS_ACCESSIBILITY_DESCRIPTION));

  localized_strings->Set(L"timezoneList", GetTimezoneList());
}

ListValue* SystemOptionsHandler::GetTimezoneList() {
  ListValue* timezoneList = new ListValue();
  for (std::vector<icu::TimeZone*>::iterator iter = timezones_.begin();
       iter != timezones_.end(); ++iter) {
    const icu::TimeZone* timezone = *iter;
    ListValue* option = new ListValue();
    option->Append(Value::CreateStringValue(GetTimezoneID(timezone)));
    option->Append(Value::CreateStringValue(GetTimezoneName(timezone)));
    timezoneList->Append(option);
  }
  return timezoneList;
}

std::wstring SystemOptionsHandler::GetTimezoneName(
    const icu::TimeZone* timezone) {
  DCHECK(timezone);
  icu::UnicodeString name;
  timezone->getDisplayName(name);
  std::wstring output;
  UTF16ToWide(name.getBuffer(), name.length(), &output);
  int hour_offset = timezone->getRawOffset() / 3600000;
  const wchar_t* format;
  if (hour_offset == 0)
    format = L"(GMT) ";
  else
    format = L"(GMT%+d) ";

  return StringPrintf(format, hour_offset) + output;
}

std::wstring SystemOptionsHandler::GetTimezoneID(
    const icu::TimeZone* timezone) {
  DCHECK(timezone);
  icu::UnicodeString id;
  timezone->getID(id);
  std::wstring output;
  UTF16ToWide(id.getBuffer(), id.length(), &output);
  return output;
}
