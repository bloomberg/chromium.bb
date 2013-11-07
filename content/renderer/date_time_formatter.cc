// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/date_time_formatter.h"

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/WebKit/public/platform/WebCString.h"
#include "third_party/WebKit/public/web/WebDateTimeChooserParams.h"
#include "third_party/icu/source/i18n/unicode/smpdtfmt.h"


namespace content {

void DateTimeFormatter::CreatePatternMap() {
  // Initialize all the UI elements with empty patterns,
  // then fill in the ones that are actually date/time inputs and
  // are implemented.
  for (int i = 0 ; i <= ui::TEXT_INPUT_TYPE_MAX; ++i) {
    patterns_[i] = "";
  }
  patterns_[ui::TEXT_INPUT_TYPE_DATE] = "yyyy-MM-dd";
  patterns_[ui::TEXT_INPUT_TYPE_DATE_TIME] =  "yyyy-MM-dd'T'HH:mm'Z'";
  patterns_[ui::TEXT_INPUT_TYPE_DATE_TIME_LOCAL] =  "yyyy-MM-dd'T'HH:mm";
  patterns_[ui::TEXT_INPUT_TYPE_MONTH] = "yyyy-MM";
  patterns_[ui::TEXT_INPUT_TYPE_TIME] = "HH:mm";
  patterns_[ui::TEXT_INPUT_TYPE_WEEK] = "Y-'W'ww";
}

// Returns true if icu_value parses as a valid for the specified date/time
// pattern. The date/time pattern given is for icu::SimpleDateFormat.
static bool TryPattern(const char* pattern,
                       const icu::UnicodeString& icu_value) {
  icu::UnicodeString time_pattern = pattern;
  UErrorCode success = U_ZERO_ERROR;
  icu::SimpleDateFormat formatter(time_pattern, success);
  formatter.parse(icu_value, success);
  return success == U_ZERO_ERROR;
}

// For a time value represented as a string find the longest time
// pattern which matches it. A valid time can have hours and minutes
// or hours, minutes and seconds or hour, minutes, seconds and upto 3
// digits of fractional seconds. Specify step in milliseconds, it is 1000
// times the value specified as "step" in the "<input type=time step=...>
// HTML fragment. A value of 60000 or more indicates that seconds
// are not expected and a value of 1000 or more indicates that fractional
// seconds are not expected.
static const char* FindLongestTimePatternWhichMatches(const std::string& value,
                                                      double step) {
  const char* pattern = "HH:mm";
  if (step >= 60000)
    return pattern;

  icu::UnicodeString icu_value = icu::UnicodeString::fromUTF8(
      icu::StringPiece(value.data(), value.size()));
  const char* last_pattern = pattern;
  pattern = "HH:mm:ss";
  if (!TryPattern(pattern, icu_value))
    return last_pattern;
  if (step >= 1000)
    return pattern;
  last_pattern = pattern;
  pattern = "HH:mm:ss.S";
  if (!TryPattern(pattern, icu_value))
    return last_pattern;
  last_pattern = pattern;
  pattern = "HH:mm:ss.SS";
  if (!TryPattern(pattern, icu_value))
    return last_pattern;
  last_pattern = pattern;
  pattern = "HH:mm:ss.SSS";
  if (!TryPattern(pattern, icu_value))
    return last_pattern;
  return pattern;
}

DateTimeFormatter::DateTimeFormatter(
    const blink::WebDateTimeChooserParams& source)
    : formatted_string_(source.currentValue.utf8()) {
  CreatePatternMap();
  if (source.type == blink::WebDateTimeInputTypeTime)
    time_pattern_ =
        FindLongestTimePatternWhichMatches(formatted_string_, source.step);
  ExtractType(source);
  if (!ParseValues()) {
    type_ = ui::TEXT_INPUT_TYPE_NONE;
    ClearAll();
    LOG(WARNING) << "Problems parsing input <" << formatted_string_ << ">";
  }
}

DateTimeFormatter::DateTimeFormatter(ui::TextInputType type,
                                     int year,
                                     int month,
                                     int day,
                                     int hour,
                                     int minute,
                                     int second,
                                     int milli,
                                     int week_year,
                                     int week)
    : type_(type),
      year_(year),
      month_(month),
      day_(day),
      hour_(hour),
      minute_(minute),
      second_(second),
      milli_(milli),
      week_year_(week_year),
      week_(week) {
  CreatePatternMap();
  if (type_ == ui::TEXT_INPUT_TYPE_TIME && (second != 0 || milli != 0)) {
    if (milli == 0)
      time_pattern_ = "HH:mm:ss";
    else if (milli % 100 == 0)
      time_pattern_ = "HH:mm:ss.S";
    else if (milli % 10 == 0)
      time_pattern_ = "HH:mm:ss.SS";
    else
      time_pattern_ = "HH:mm:ss.SSS";
    pattern_ = &time_pattern_;
  } else {
    pattern_ = type_ > 0 && type_ <= ui::TEXT_INPUT_TYPE_MAX ?
        &patterns_[type_] : &patterns_[ui::TEXT_INPUT_TYPE_NONE];
  }

  formatted_string_ = FormatString();
}

DateTimeFormatter::~DateTimeFormatter() {
}

int DateTimeFormatter::GetYear() const {
  return year_;
}

int DateTimeFormatter::GetMonth() const {
  return month_;
}

int DateTimeFormatter::GetDay() const {
  return day_;
}

int DateTimeFormatter::GetHour() const {
  return hour_;
}

int DateTimeFormatter::GetMinute() const {
  return minute_;
}

int DateTimeFormatter::GetSecond() const {
  return second_;
}

int DateTimeFormatter::GetMilli() const { return milli_; }

int DateTimeFormatter::GetWeekYear() const { return week_year_; }

int DateTimeFormatter::GetWeek() const {
  return week_;
}

ui::TextInputType DateTimeFormatter::GetType() const {
  return type_;
}

const std::string& DateTimeFormatter::GetFormattedValue() const {
  return formatted_string_;
}

const std::string DateTimeFormatter::FormatString() const {
  UErrorCode success = U_ZERO_ERROR;
  if (year_ == 0 && month_ == 0 && day_ == 0 && hour_ == 0 && minute_ == 0 &&
      second_ == 0 && milli_ == 0 && week_year_ == 0 && week_ == 0) {
    return std::string();
  }

  std::string result;
  icu::GregorianCalendar calendar(success);
  if (success <= U_ZERO_ERROR) {
    if (type_ == ui::TEXT_INPUT_TYPE_WEEK) {
      // An ISO week starts with Monday.
      calendar.setFirstDayOfWeek(UCAL_MONDAY);
      // ISO 8601 defines that the week with the year's first Thursday is the
      // first week.
      calendar.setMinimalDaysInFirstWeek(4);
      calendar.set(UCAL_YEAR_WOY, week_year_);
      calendar.set(UCAL_WEEK_OF_YEAR, week_);
    } else {
      calendar.set(UCAL_YEAR, year_);
      calendar.set(UCAL_MONTH, month_);
      calendar.set(UCAL_DATE, day_);
      calendar.set(UCAL_HOUR_OF_DAY, hour_);
      calendar.set(UCAL_MINUTE, minute_);
      calendar.set(UCAL_SECOND, second_);
      calendar.set(UCAL_MILLISECOND, milli_);
    }
    icu::SimpleDateFormat formatter(*pattern_, success);
    icu::UnicodeString formatted_time;
    formatter.format(calendar, formatted_time, NULL, success);
    UTF16ToUTF8(formatted_time.getBuffer(),
                static_cast<size_t>(formatted_time.length()),
                &result);
    if (success <= U_ZERO_ERROR)
      return result;
  }
  LOG(WARNING) << "Calendar not created: error " <<  success;
  return std::string();
}

void DateTimeFormatter::ExtractType(
    const blink::WebDateTimeChooserParams& source) {
  switch (source.type) {
    case blink::WebDateTimeInputTypeDate:
      type_ = ui::TEXT_INPUT_TYPE_DATE;
      break;
    case blink::WebDateTimeInputTypeDateTime:
      type_ = ui::TEXT_INPUT_TYPE_DATE_TIME;
      break;
    case blink::WebDateTimeInputTypeDateTimeLocal:
      type_ = ui::TEXT_INPUT_TYPE_DATE_TIME_LOCAL;
      break;
    case blink::WebDateTimeInputTypeMonth:
      type_ = ui::TEXT_INPUT_TYPE_MONTH;
      break;
    case blink::WebDateTimeInputTypeTime:
      type_ = ui::TEXT_INPUT_TYPE_TIME;
      break;
    case blink::WebDateTimeInputTypeWeek:
      type_ = ui::TEXT_INPUT_TYPE_WEEK;
      break;
    case blink::WebDateTimeInputTypeNone:
    default:
      type_ = ui::TEXT_INPUT_TYPE_NONE;
  }
}

// Not all fields are defined in all configurations and ICU might store
// garbage if success <= U_ZERO_ERROR so the output is sanitized here.
int DateTimeFormatter::ExtractValue(
    const icu::Calendar* calendar, UCalendarDateFields value) const {
  UErrorCode success = U_ZERO_ERROR;
  int result = calendar->get(value, success);
  return (success <= U_ZERO_ERROR) ? result : 0;
}

bool DateTimeFormatter::ParseValues() {
  if (type_ == ui::TEXT_INPUT_TYPE_NONE) {
    ClearAll();
    return false;
  }

  if (formatted_string_.empty()) {
    ClearAll();
    return true;
  }

  UErrorCode success = U_ZERO_ERROR;
  icu::UnicodeString icu_value = icu::UnicodeString::fromUTF8(
      icu::StringPiece(formatted_string_.data(), formatted_string_.size()));
  if (type_ > 0 && type_ <= ui::TEXT_INPUT_TYPE_MAX) {
    const icu::UnicodeString pattern =
        type_ == ui::TEXT_INPUT_TYPE_TIME ? time_pattern_ : patterns_[type_];
    icu::SimpleDateFormat formatter(pattern, success);
    formatter.parse(icu_value, success);
    if (success <= U_ZERO_ERROR) {
      const icu::Calendar* cal = formatter.getCalendar();
      year_ = ExtractValue(cal, UCAL_YEAR);
      month_ = ExtractValue(cal, UCAL_MONTH);
      day_ = ExtractValue(cal, UCAL_DATE);
      hour_ = ExtractValue(cal, UCAL_HOUR_OF_DAY);  // 24h format
      minute_ = ExtractValue(cal, UCAL_MINUTE);
      second_ = ExtractValue(cal, UCAL_SECOND);
      milli_ = ExtractValue(cal, UCAL_MILLISECOND);
      week_year_ = ExtractValue(cal, UCAL_YEAR_WOY);
      week_ = ExtractValue(cal, UCAL_WEEK_OF_YEAR);
    }
  }

  return (success <= U_ZERO_ERROR);
}

void DateTimeFormatter::ClearAll() {
  year_ = 0;
  month_ = 0;
  day_ = 0;
  hour_ = 0;
  minute_ = 0;
  second_ = 0;
  milli_ = 0;
  week_year_ = 0;
  week_ = 0;
}

}  // namespace content
