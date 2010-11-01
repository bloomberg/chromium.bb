// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dom_ui/system_settings_provider.h"

#include <string>

#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros_settings.h"
#include "chrome/browser/chromeos/cros_settings_names.h"

namespace chromeos {

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

SystemSettingsProvider::SystemSettingsProvider() {
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
  CrosLibrary::Get()->GetSystemLibrary()->AddObserver(this);

}

SystemSettingsProvider::~SystemSettingsProvider() {
  CrosLibrary::Get()->GetSystemLibrary()->RemoveObserver(this);
  STLDeleteElements(&timezones_);
}

void SystemSettingsProvider::DoSet(const std::string& path, Value* in_value) {
  if (path == kSystemTimezone) {
    string16 value;
    if (!in_value || !in_value->IsType(Value::TYPE_STRING) ||
        !in_value->GetAsString(&value))
      return;
    const icu::TimeZone* timezone = GetTimezone(value);
    if (!timezone)
      return;
    CrosLibrary::Get()->GetSystemLibrary()->SetTimezone(timezone);
  }
}

bool SystemSettingsProvider::Get(const std::string& path,
                                 Value** out_value) const {
  if (path == kSystemTimezone) {
    *out_value = Value::CreateStringValue(
        GetTimezoneID(CrosLibrary::Get()->GetSystemLibrary()->GetTimezone()));
    return true;
  }
  return false;
}

bool SystemSettingsProvider::HandlesSetting(const std::string& path) {
  return ::StartsWithASCII(path, std::string("cros.system."), true);
}

void SystemSettingsProvider::TimezoneChanged(const icu::TimeZone& timezone) {
  // Fires system setting change notification.
  CrosSettings::Get()->FireObservers(kSystemTimezone);
}

ListValue* SystemSettingsProvider::GetTimezoneList() {
  ListValue* timezoneList = new ListValue();
  for (std::vector<icu::TimeZone*>::iterator iter = timezones_.begin();
       iter != timezones_.end(); ++iter) {
    const icu::TimeZone* timezone = *iter;
    ListValue* option = new ListValue();
    option->Append(Value::CreateStringValue(GetTimezoneID(*timezone)));
    option->Append(Value::CreateStringValue(GetTimezoneName(*timezone)));
    timezoneList->Append(option);
  }
  return timezoneList;
}

string16 SystemSettingsProvider::GetTimezoneName(
    const icu::TimeZone& timezone) {
  icu::UnicodeString name;
  timezone.getDisplayName(name);
  string16 output(name.getBuffer(), name.length());
  int hour_offset = timezone.getRawOffset() / 3600000;
  const wchar_t* format;
  if (hour_offset == 0)
    format = L"(GMT) ";
  else
    format = L"(GMT%+d) ";
  std::wstring offset = base::StringPrintf(format, hour_offset);
  return WideToUTF16(offset) + output;
}

string16 SystemSettingsProvider::GetTimezoneID(
    const icu::TimeZone& timezone) {
  icu::UnicodeString id;
  timezone.getID(id);
  return string16(id.getBuffer(), id.length());
}

const icu::TimeZone* SystemSettingsProvider::GetTimezone(
    const string16& timezone_id) {
  for (std::vector<icu::TimeZone*>::iterator iter = timezones_.begin();
       iter != timezones_.end(); ++iter) {
    const icu::TimeZone* timezone = *iter;
    if (GetTimezoneID(*timezone) == timezone_id) {
      return timezone;
    }
  }
  return NULL;
}

}  // namespace chromeos
