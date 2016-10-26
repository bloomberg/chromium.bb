// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system/timezone_util.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "base/i18n/rtl.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/system/timezone_resolver_manager.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/settings/timezone_settings.h"
#include "chromeos/timezone/timezone_request.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
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
  base::ReplaceSubstringsAfterOffset(&zone_id_str, 0, "/", ":");
  std::unique_ptr<UResourceBundle, UResClose> zone_item(
      ures_getByKey(zone_strings, zone_id_str.c_str(), NULL, &status));
  icu::UnicodeString city;
  if (!U_FAILURE(status)) {
    city = icu::ures_getUnicodeStringByKey(zone_item.get(), "ec", &status);
    if (U_SUCCESS(status))
      return base::string16(city.getBuffer(), city.length());
  }

  // Fallback case in case of failure.
  base::ReplaceSubstringsAfterOffset(&zone_id_str, 0, ":", "/");
  // Take the last component of a timezone id (e.g. 'Baz' in 'Foo/Bar/Baz').
  // Depending on timezones, keeping all but the 1st component
  // (e.g. Bar/Baz) may be better, but our current list does not have
  // any timezone for which that's the case.
  std::string::size_type slash_pos = zone_id_str.rfind('/');
  if (slash_pos != std::string::npos && slash_pos < zone_id_str.size())
    zone_id_str.erase(0, slash_pos + 1);
  // zone id has '_' in place of ' '.
  base::ReplaceSubstringsAfterOffset(&zone_id_str, 0, "_", " ");
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
  icu::UnicodeString id;
  icu::UnicodeString name;
  timezone.getID(id);
  if (id == icu::UnicodeString(chromeos::system::kUTCTimezoneName)) {
    name = icu::UnicodeString(
        l10n_util::GetStringUTF8(IDS_OPTIONS_SETTINGS_TIMEZONE_DISPLAY_NAME_UTC)
            .c_str());
  } else {
    timezone.getDisplayName(dst_offset != 0, icu::TimeZone::LONG, name);
  }
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

base::string16 GetCurrentTimezoneName() {
  return GetTimezoneName(
      chromeos::system::TimezoneSettings::GetInstance()->GetTimezone());
}

// Creates a list of pairs of each timezone's ID and name.
std::unique_ptr<base::ListValue> GetTimezoneList() {
  const auto& timezones =
      chromeos::system::TimezoneSettings::GetInstance()->GetTimezoneList();
  std::unique_ptr<base::ListValue> timezoneList(new base::ListValue());
  for (const auto& timezone : timezones) {
    auto option = base::MakeUnique<base::ListValue>();
    option->AppendString(
        chromeos::system::TimezoneSettings::GetTimezoneID(*timezone));
    option->AppendString(GetTimezoneName(*timezone));
    timezoneList->Append(std::move(option));
  }
  return timezoneList;
}

bool HasSystemTimezonePolicy() {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  if (!connector->IsEnterpriseManaged())
    return false;

  std::string policy_timezone;
  if (chromeos::CrosSettings::Get()->GetString(chromeos::kSystemTimezonePolicy,
                                               &policy_timezone) &&
      !policy_timezone.empty()) {
    VLOG(1) << "Refresh TimeZone: TimeZone settings are overridden"
            << " by DevicePolicy.";
    return true;
  }
  return false;
}

void ApplyTimeZone(const TimeZoneResponseData* timezone) {
  if (!g_browser_process->platform_part()
           ->GetTimezoneResolverManager()
           ->ShouldApplyResolvedTimezone()) {
    return;
  }

  if (!timezone->timeZoneId.empty()) {
    VLOG(1) << "Refresh TimeZone: setting timezone to '" << timezone->timeZoneId
            << "'";

    chromeos::system::TimezoneSettings::GetInstance()->SetTimezoneFromID(
        base::UTF8ToUTF16(timezone->timeZoneId));
  }
}

}  // namespace system
}  // namespace chromeos
