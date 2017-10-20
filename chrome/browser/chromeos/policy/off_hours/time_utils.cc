// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/off_hours/time_utils.h"

#include <algorithm>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/common/unicode/utypes.h"
#include "third_party/icu/source/i18n/unicode/gregocal.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace policy {
namespace off_hours {
namespace {
constexpr base::TimeDelta kWeek = base::TimeDelta::FromDays(7);
}  // namespace

bool GetOffsetFromTimezoneToGmt(const std::string& timezone, int* offset) {
  auto zone = base::WrapUnique(
      icu::TimeZone::createTimeZone(icu::UnicodeString::fromUTF8(timezone)));
  if (*zone == icu::TimeZone::getUnknown()) {
    LOG(ERROR) << "Unsupported timezone: " << timezone;
    return false;
  }
  // Time in milliseconds which is added to GMT to get local time.
  int gmt_offset = zone->getRawOffset();
  // Time in milliseconds which is added to local standard time to get local
  // wall clock time.
  int dst_offset = zone->getDSTSavings();
  UErrorCode status = U_ZERO_ERROR;
  std::unique_ptr<icu::GregorianCalendar> gregorian_calendar =
      base::MakeUnique<icu::GregorianCalendar>(*zone, status);
  if (U_FAILURE(status)) {
    LOG(ERROR) << "Gregorian calendar error = " << u_errorName(status);
    return false;
  }
  status = U_ZERO_ERROR;
  UBool day_light = gregorian_calendar->inDaylightTime(status);
  if (U_FAILURE(status)) {
    LOG(ERROR) << "Daylight time error = " << u_errorName(status);
    return false;
  }
  if (day_light)
    gmt_offset += dst_offset;
  // -|gmt_offset| is time which is added to local time to get GMT time.
  *offset = -gmt_offset;
  return true;
}

std::vector<OffHoursInterval> ConvertIntervalsToGmt(
    const std::vector<OffHoursInterval>& intervals,
    const std::string& timezone) {
  int gmt_offset = 0;
  bool no_offset_error = GetOffsetFromTimezoneToGmt(timezone, &gmt_offset);
  if (!no_offset_error)
    return {};
  std::vector<OffHoursInterval> gmt_intervals;
  for (const auto& interval : intervals) {
    // |gmt_offset| is added to input time to get GMT time.
    auto gmt_start = interval.start().AddMilliseconds(gmt_offset);
    auto gmt_end = interval.end().AddMilliseconds(gmt_offset);
    gmt_intervals.push_back(OffHoursInterval(gmt_start, gmt_end));
  }
  return gmt_intervals;
}

base::TimeDelta GetDeltaTillNextOffHours(
    const WeeklyTime& current_time,
    const std::vector<OffHoursInterval>& off_hours_intervals) {
  // "OffHours" intervals repeat every week, therefore the maximum duration till
  // next "OffHours" interval is one week.
  base::TimeDelta till_off_hours = kWeek;
  for (const auto& interval : off_hours_intervals) {
    till_off_hours =
        std::min(till_off_hours, current_time.GetDurationTo(interval.start()));
  }
  return till_off_hours;
}

}  // namespace off_hours
}  // namespace policy
