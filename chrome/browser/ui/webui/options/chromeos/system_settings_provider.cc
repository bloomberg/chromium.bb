// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/chromeos/system_settings_provider.h"

#include <string>

#include "base/i18n/rtl.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros_settings.h"
#include "chrome/browser/chromeos/cros_settings_names.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "unicode/calendar.h"
#include "unicode/timezone.h"
#include "unicode/ures.h"

namespace {

// TODO(jungshik): Using Enumerate method in ICU gives 600+ timezones.
// Even after filtering out duplicate entries with a strict identity check,
// we still have 400+ zones. Relaxing the criteria for the timezone
// identity is likely to cut down the number to < 100. Until we
// come up with a better list, we hard-code the following list as used by
// Android.
static const char* kTimeZones[] = {
    "Pacific/Majuro",
    "Pacific/Midway",
    "Pacific/Honolulu",
    "America/Anchorage",
    "America/Los_Angeles",
    "America/Tijuana",
    "America/Denver",
    "America/Phoenix",
    "America/Chihuahua",
    "America/Chicago",
    "America/Mexico_City",
    "America/Costa_Rica",
    "America/Regina",
    "America/New_York",
    "America/Bogota",
    "America/Caracas",
    "America/Barbados",
    "America/Manaus",
    "America/Santiago",
    "America/St_Johns",
    "America/Sao_Paulo",
    "America/Araguaina",
    "America/Argentina/Buenos_Aires",
    "America/Godthab",
    "America/Montevideo",
    "Atlantic/South_Georgia",
    "Atlantic/Azores",
    "Atlantic/Cape_Verde",
    "Africa/Casablanca",
    "Europe/London",
    "Europe/Amsterdam",
    "Europe/Belgrade",
    "Europe/Brussels",
    "Europe/Sarajevo",
    "Africa/Windhoek",
    "Africa/Brazzaville",
    "Asia/Amman",
    "Europe/Athens",
    "Asia/Beirut",
    "Africa/Cairo",
    "Europe/Helsinki",
    "Asia/Jerusalem",
    "Europe/Minsk",
    "Africa/Harare",
    "Asia/Baghdad",
    "Europe/Moscow",
    "Asia/Kuwait",
    "Africa/Nairobi",
    "Asia/Tehran",
    "Asia/Baku",
    "Asia/Tbilisi",
    "Asia/Yerevan",
    "Asia/Dubai",
    "Asia/Kabul",
    "Asia/Karachi",
    "Asia/Oral",
    "Asia/Yekaterinburg",
    "Asia/Calcutta",
    "Asia/Colombo",
    "Asia/Katmandu",
    "Asia/Almaty",
    "Asia/Rangoon",
    "Asia/Krasnoyarsk",
    "Asia/Bangkok",
    "Asia/Shanghai",
    "Asia/Hong_Kong",
    "Asia/Irkutsk",
    "Asia/Kuala_Lumpur",
    "Australia/Perth",
    "Asia/Taipei",
    "Asia/Seoul",
    "Asia/Tokyo",
    "Asia/Yakutsk",
    "Australia/Adelaide",
    "Australia/Darwin",
    "Australia/Brisbane",
    "Australia/Hobart",
    "Australia/Sydney",
    "Asia/Vladivostok",
    "Pacific/Guam",
    "Asia/Magadan",
    "Pacific/Auckland",
    "Pacific/Fiji",
    "Pacific/Tongatapu",
};

static base::LazyInstance<base::Lock,
                          base::LeakyLazyInstanceTraits<base::Lock> >
    g_timezone_bundle_lock = LAZY_INSTANCE_INITIALIZER;

struct UResClose {
  inline void operator() (UResourceBundle* b) const {
    ures_close(b);
  }
};

string16 GetExemplarCity(const icu::TimeZone& zone) {
  // TODO(jungshik): After upgrading to ICU 4.6, use U_ICUDATA_ZONE
  static const char* zone_bundle_name = NULL;

  // These will be leaked at the end.
  static UResourceBundle *zone_bundle = NULL;
  static UResourceBundle *zone_strings = NULL;

  UErrorCode status = U_ZERO_ERROR;
  {
    base::AutoLock lock(g_timezone_bundle_lock.Get());
    if (zone_bundle == NULL)
      zone_bundle = ures_open(zone_bundle_name, uloc_getDefault(), &status);

    if (zone_strings == NULL)
      zone_strings = ures_getByKey(zone_bundle, "zone_strings", NULL, &status);
  }

  icu::UnicodeString zone_id;
  zone.getID(zone_id);
  std::string zone_id_str;
  zone_id.toUTF8String(zone_id_str);

  // resource keys for timezones use ':' in place of '/'.
  ReplaceSubstringsAfterOffset(&zone_id_str, 0, "/", ":");
  scoped_ptr_malloc<UResourceBundle, UResClose> zone_item(
      ures_getByKey(zone_strings, zone_id_str.c_str(), NULL, &status));
  icu::UnicodeString city;
  if (!U_FAILURE(status)) {
    city = icu::ures_getUnicodeStringByKey(zone_item.get(), "ec", &status);
    if (U_SUCCESS(status))
      return string16(city.getBuffer(), city.length());
  }

  // Fallback case in case of failure.
  ReplaceSubstringsAfterOffset(&zone_id_str, 0, ":", "/");
  // Take the last component of a timezone id (e.g. 'Baz' in 'Foo/Bar/Baz').
  // Depending on timezones, keeping all but the 1st component
  // (e.g. Bar/Baz) may be better, but our current list does not have
  // any timezone for which that's the case.
  std::string::size_type slash_pos = zone_id_str.rfind('/');
  if (slash_pos != std::string::npos && slash_pos < zone_id_str.size())
    zone_id_str.erase(0, slash_pos + 1);
  // zone id has '_' in place of ' '.
  ReplaceSubstringsAfterOffset(&zone_id_str, 0, "_", " ");
  return ASCIIToUTF16(zone_id_str);
}

}  // namespace anonymous

namespace chromeos {

SystemSettingsProvider::SystemSettingsProvider() {
  for (size_t i = 0; i < arraysize(kTimeZones); i++) {
    timezones_.push_back(icu::TimeZone::createTimeZone(
        icu::UnicodeString(kTimeZones[i], -1, US_INV)));
  }
  system::TimezoneSettings::GetInstance()->AddObserver(this);

}

SystemSettingsProvider::~SystemSettingsProvider() {
  system::TimezoneSettings::GetInstance()->RemoveObserver(this);
  STLDeleteElements(&timezones_);
}

void SystemSettingsProvider::DoSet(const std::string& path,
                                   const base::Value& in_value) {
  // Non-guest users can change the time zone.
  if (UserManager::Get()->IsLoggedInAsGuest())
    return;

  if (path == kSystemTimezone) {
    string16 value;
    if (!in_value.IsType(Value::TYPE_STRING) || !in_value.GetAsString(&value))
      return;
    const icu::TimeZone* timezone = GetTimezone(value);
    if (!timezone)
      return;
    system::TimezoneSettings::GetInstance()->SetTimezone(*timezone);
  }
}

const base::Value* SystemSettingsProvider::Get(const std::string& path) const {
  if (path == kSystemTimezone) {
    // TODO(pastarmovj): Cache this in the local_state instead of locally.
    system_timezone_.reset(base::Value::CreateStringValue(GetKnownTimezoneID(
        system::TimezoneSettings::GetInstance()->GetTimezone())));
    return system_timezone_.get();
  }
  return NULL;
}

// The timezone is always trusted.
bool SystemSettingsProvider::GetTrusted(const std::string& path,
                                        const base::Closure& callback) const {
  return true;
}

bool SystemSettingsProvider::HandlesSetting(const std::string& path) const {
  return path == kSystemTimezone;
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
  // Instead of using the raw_offset, use the offset in effect now.
  // For instance, US Pacific Time, the offset shown will be -7 in summer
  // while it'll be -8 in winter.
  int raw_offset, dst_offset;
  UDate now = icu::Calendar::getNow();
  UErrorCode status = U_ZERO_ERROR;
  timezone.getOffset(now, false, raw_offset, dst_offset, status);
  DCHECK(U_SUCCESS(status));
  int offset = raw_offset + dst_offset;
  // offset is in msec.
  int minute_offset = std::abs(offset) / 60000;
  int hour_offset = minute_offset / 60;
  int min_remainder = minute_offset % 60;
  // Some timezones have a non-integral hour offset. So, we need to
  // use hh:mm form.
  std::string  offset_str = base::StringPrintf(offset >= 0 ?
      "UTC+%d:%02d" : "UTC-%d:%02d", hour_offset, min_remainder);

  // TODO(jungshik): When coming up with a better list of timezones, we also
  // have to come up with better 'display' names. One possibility is to list
  // multiple cities (e.g. "Los Angeles, Vancouver .." in the order of
  // the population of a country the city belongs to.).
  // We can also think of using LONG_GENERIC or LOCATION once we upgrade
  // to ICU 4.6.
  // In the meantime, we use "LONG" name with "Exemplar City" to distinguish
  // multiple timezones with the same "LONG" name but with different
  // rules (e.g. US Mountain Time in Denver vs Phoenix).
  icu::UnicodeString name;
  timezone.getDisplayName(dst_offset != 0, icu::TimeZone::LONG, name);
  string16 result(l10n_util::GetStringFUTF16(
      IDS_OPTIONS_SETTINGS_TIMEZONE_DISPLAY_TEMPLATE, ASCIIToUTF16(offset_str),
      string16(name.getBuffer(), name.length()), GetExemplarCity(timezone)));
  base::i18n::AdjustStringForLocaleDirection(&result);
  return result;
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

string16 SystemSettingsProvider::GetKnownTimezoneID(
    const icu::TimeZone& timezone) const {
  for (std::vector<icu::TimeZone*>::const_iterator iter = timezones_.begin();
       iter != timezones_.end(); ++iter) {
    const icu::TimeZone* known_timezone = *iter;
    if (known_timezone->hasSameRules(timezone))
      return GetTimezoneID(*known_timezone);
  }

  // Not able to find a matching timezone in our list.
  return string16();
}

}  // namespace chromeos
