// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system/timezone_util.h"

#include <string>

#include "base/i18n/rtl.h"
#include "base/lazy_instance.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/values.h"
#include "chromeos/settings/timezone_settings.h"
#include "grit/generated_resources.h"
#include "third_party/icu/source/common/unicode/ures.h"
#include "third_party/icu/source/common/unicode/utypes.h"
#include "third_party/icu/source/i18n/unicode/calendar.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

struct UResClose {
  inline void operator() (UResourceBundle* b) const {
    ures_close(b);
  }
};

static base::LazyInstance<base::Lock>::Leaky
    g_timezone_bundle_lock = LAZY_INSTANCE_INITIALIZER;

// Returns an exemplary city in the given timezone.
base::string16 GetExemplarCity(const icu::TimeZone& zone) {
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

  // Resource keys for timezones use ':' in place of '/'.
  ReplaceSubstringsAfterOffset(&zone_id_str, 0, "/", ":");
  scoped_ptr<UResourceBundle, UResClose> zone_item(
      ures_getByKey(zone_strings, zone_id_str.c_str(), NULL, &status));
  icu::UnicodeString city;
  if (!U_FAILURE(status)) {
    city = icu::ures_getUnicodeStringByKey(zone_item.get(), "ec", &status);
    if (U_SUCCESS(status))
      return base::string16(city.getBuffer(), city.length());
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
  return base::ASCIIToUTF16(zone_id_str);
}

// Gets the given timezone's name for visualization.
base::string16 GetTimezoneName(const icu::TimeZone& timezone) {
  // Instead of using the raw_offset, use the offset in effect now.
  // For instance, US Pacific Time, the offset shown will be -7 in summer
  // while it'll be -8 in winter.
  int raw_offset, dst_offset;
  UDate now = icu::Calendar::getNow();
  UErrorCode status = U_ZERO_ERROR;
  timezone.getOffset(now, false, raw_offset, dst_offset, status);
  DCHECK(U_SUCCESS(status));
  int offset = raw_offset + dst_offset;
  // |offset| is in msec.
  int minute_offset = std::abs(offset) / 60000;
  int hour_offset = minute_offset / 60;
  int min_remainder = minute_offset % 60;
  // Some timezones have a non-integral hour offset. So, we need to use hh:mm
  // form.
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
  base::string16 result(l10n_util::GetStringFUTF16(
      IDS_OPTIONS_SETTINGS_TIMEZONE_DISPLAY_TEMPLATE,
      base::ASCIIToUTF16(offset_str),
      base::string16(name.getBuffer(), name.length()),
      GetExemplarCity(timezone)));
  base::i18n::AdjustStringForLocaleDirection(&result);
  return result;
}

}  // namespace

namespace chromeos {
namespace system {

// Creates a list of pairs of each timezone's ID and name.
scoped_ptr<base::ListValue> GetTimezoneList() {
  const std::vector<icu::TimeZone*> &timezones =
      chromeos::system::TimezoneSettings::GetInstance()->GetTimezoneList();
  scoped_ptr<base::ListValue> timezoneList(new base::ListValue());
  for (std::vector<icu::TimeZone*>::const_iterator iter = timezones.begin();
       iter != timezones.end(); ++iter) {
    const icu::TimeZone* timezone = *iter;
    base::ListValue* option = new base::ListValue();
    option->Append(new base::StringValue(
        chromeos::system::TimezoneSettings::GetTimezoneID(*timezone)));
    option->Append(new base::StringValue(GetTimezoneName(*timezone)));
    timezoneList->Append(option);
  }
  return timezoneList.Pass();
}

}  // namespace system
}  // namespace chromeos
