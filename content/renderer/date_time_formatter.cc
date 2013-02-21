// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/date_time_formatter.h"

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebCString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDateTimeChooserParams.h"
#include "third_party/icu/public/i18n/unicode/smpdtfmt.h"


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
}

DateTimeFormatter::DateTimeFormatter(
    const WebKit::WebDateTimeChooserParams& source)
  : formatted_string_(source.currentValue.utf8()) {
  CreatePatternMap();
  ExtractType(source);
  if (!ParseValues()) {
    type_ = ui::TEXT_INPUT_TYPE_NONE;
    ClearAll();
    LOG(WARNING) << "Problems parsing input <" << formatted_string_ << ">";
  }
}

DateTimeFormatter::DateTimeFormatter(
    ui::TextInputType type,
    int year, int month, int day, int hour, int minute, int second)
  : type_(type),
    year_(year),
    month_(month),
    day_(day),
    hour_(hour),
    minute_(minute),
    second_(second) {
  CreatePatternMap();
  pattern_ = type_ > 0 && type_ <= ui::TEXT_INPUT_TYPE_MAX ?
      &patterns_[type_] : &patterns_[ui::TEXT_INPUT_TYPE_NONE];

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

ui::TextInputType DateTimeFormatter::GetType() const {
  return type_;
}

const std::string& DateTimeFormatter::GetFormattedValue() const {
  return formatted_string_;
}

const std::string DateTimeFormatter::FormatString() const {
  UErrorCode success = U_ZERO_ERROR;
  if (year_ == 0 && month_ == 0 && day_ == 0 &&
      hour_ == 0 && minute_ == 0 && second_ == 0) {
    return "";
  }

  std::string result;
  const icu::GregorianCalendar calendar(
      year_, month_, day_, hour_, minute_, second_, success);
  if (success <= U_ZERO_ERROR) {
    UDate time = calendar.getTime(success);
    icu::SimpleDateFormat formatter(*pattern_, success);
    icu::UnicodeString formatted_time;
    formatter.format(time, formatted_time, success);
    UTF16ToUTF8(formatted_time.getBuffer(),
                static_cast<size_t>(formatted_time.length()),
                &result);
    if (success <= U_ZERO_ERROR)
      return result;
  }
  LOG(WARNING) << "Calendar not created: error " <<  success;
  return "";
}

void DateTimeFormatter::ExtractType(
    const WebKit::WebDateTimeChooserParams& source) {
  switch (source.type) {
    case WebKit::WebDateTimeInputTypeDate:
      type_ = ui::TEXT_INPUT_TYPE_DATE;
      break;
    case WebKit::WebDateTimeInputTypeDateTime:
      type_ = ui::TEXT_INPUT_TYPE_DATE_TIME;
      break;
    case WebKit::WebDateTimeInputTypeDateTimeLocal:
      type_ = ui::TEXT_INPUT_TYPE_DATE_TIME_LOCAL;
      break;
    case WebKit::WebDateTimeInputTypeMonth:
      type_ = ui::TEXT_INPUT_TYPE_MONTH;
      break;
    case WebKit::WebDateTimeInputTypeTime:
      type_ = ui::TEXT_INPUT_TYPE_TIME;
      break;
    case WebKit::WebDateTimeInputTypeWeek: // Not implemented
    case WebKit::WebDateTimeInputTypeNone:
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
    const icu::UnicodeString pattern = patterns_[type_];
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
}

}  // namespace content
