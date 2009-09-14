// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/base/time.h"

#include <string>
#include <time.h>

#include "chrome/browser/sync/notifier/base/string.h"
#include "chrome/browser/sync/notifier/base/utils.h"
#include "talk/base/common.h"
#include "talk/base/logging.h"

namespace notifier {

// Get the current time represented in 100NS granularity since epoch
// (Jan 1, 1970).
time64 GetCurrent100NSTimeSinceEpoch() {
  return GetCurrent100NSTime() - kStart100NsTimeToEpoch;
}

char* GetLocalTimeAsString() {
  time64 long_time = GetCurrent100NSTime();
  struct tm now;
  Time64ToTm(long_time, &now);
  char* time_string = asctime(&now);
  if (time_string) {
    int time_len = strlen(time_string);
    if (time_len > 0) {
      time_string[time_len - 1] = 0;  // trim off terminating \n.
    }
  }
  return time_string;
}

// Parses RFC 822 Date/Time format
//    5.  DATE AND TIME SPECIFICATION
//     5.1.  SYNTAX
//
//     date-time   =  [ day "," ] date time        ; dd mm yy
//                                                 ;  hh:mm:ss zzz
//     day         =  "Mon"  / "Tue" /  "Wed"  / "Thu"
//                 /  "Fri"  / "Sat" /  "Sun"
//
//     date        =  1*2DIGIT month 2DIGIT        ; day month year
//                                                 ;  e.g. 20 Jun 82
//
//     month       =  "Jan"  /  "Feb" /  "Mar"  /  "Apr"
//                 /  "May"  /  "Jun" /  "Jul"  /  "Aug"
//                 /  "Sep"  /  "Oct" /  "Nov"  /  "Dec"
//
//     time        =  hour zone                    ; ANSI and Military
//
//     hour        =  2DIGIT ":" 2DIGIT [":" 2DIGIT]
//                                                 ; 00:00:00 - 23:59:59
//
//     zone        =  "UT"  / "GMT"                ; Universal Time
//                                                 ; North American : UT
//                 /  "EST" / "EDT"                ;  Eastern:  - 5/ - 4
//                 /  "CST" / "CDT"                ;  Central:  - 6/ - 5
//                 /  "MST" / "MDT"                ;  Mountain: - 7/ - 6
//                 /  "PST" / "PDT"                ;  Pacific:  - 8/ - 7
//                 /  1ALPHA                       ; Military: Z = UT;
//                                                 ;  A:-1; (J not used)
//                                                 ;  M:-12; N:+1; Y:+12
//                 / ( ("+" / "-") 4DIGIT )        ; Local differential
//                                                 ;  hours+min. (HHMM)
// Return local time if ret_local_time == true, return UTC time otherwise
const int kNumOfDays = 7;
const int kNumOfMonth = 12;
// Note: RFC822 does not include '-' as a separator, but Http Cookies use
// it in the date field, like this: Wdy, DD-Mon-YYYY HH:MM:SS GMT
// This differs from RFC822 only by those dashes. It is legacy quirk from
// old Netscape cookie specification. So it makes sense to expand this
// parser rather then add another one.
// See http://wp.netscape.com/newsref/std/cookie_spec.html
const char kRFC822_DateDelimiters[] = " ,:-";

const char kRFC822_TimeDelimiter[] = ": ";
const char* kRFC822_Day[] = {
  "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"
};
const char* kRFC822_Month[] = {
  "Jan", "Feb", "Mar", "Apr", "May", "Jun",
  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

struct TimeZoneInfo {
  const char* zone_name;
  int hour_dif;
};

const TimeZoneInfo kRFC822_TimeZone[] = {
  { "UT",  0 },
  { "GMT", 0 },
  { "EST", -5 },
  { "EDT", -4 },
  { "CST", -6 },
  { "CDT", -5 },
  { "MST", -7 },
  { "MDT", -6 },
  { "PST", -8 },
  { "PDT", -7 },
  { "A",   -1 },  // Military time zones.
  { "B",   -2 },
  { "C",   -3 },
  { "D",   -4 },
  { "E",   -5 },
  { "F",   -6 },
  { "G",   -7 },
  { "H",   -8 },
  { "I",   -9 },
  { "K",   -10 },
  { "L",   -11 },
  { "M",   -12 },
  { "N",    1 },
  { "O",    2 },
  { "P",    3 },
  { "Q",    4 },
  { "R",    5 },
  { "S",    6 },
  { "T",    7 },
  { "U",    8 },
  { "V",    9 },
  { "W",    10 },
  { "X",    11 },
  { "Y",    12 },
  { "Z",    0 },
};

bool ParseRFC822DateTime(const char* str, struct tm* time,
                         bool ret_local_time) {
  ASSERT(str && *str);
  ASSERT(time);

  std::string str_date(str);
  std::string str_token;
  const char* str_curr = str_date.c_str();

  str_token = SplitOneStringToken(&str_curr, kRFC822_DateDelimiters);
  if (str_token == "") {
    return false;
  }

  for (int i = 0; i < kNumOfDays; ++i) {
    if (str_token == kRFC822_Day[i]) {
      // Skip spaces after ','.
      while (*str_curr == ' ' && *str_curr != '\0') {
        str_curr++;
      }

      str_token = SplitOneStringToken(&str_curr, kRFC822_DateDelimiters);
      if (str_token == "") {
        return false;
      }
      break;
    }
  }

  int day = 0;
  if (!ParseStringToInt(str_token.c_str(), &day, true) || day < 0 || day > 31) {
    return false;
  }

  str_token = SplitOneStringToken(&str_curr, kRFC822_DateDelimiters);
  if (str_token == "") {
    return false;
  }

  int month = -1;
  for (int i = 0; i < kNumOfMonth; ++i) {
    if (str_token == kRFC822_Month[i]) {
      month = i;  // Month is 0 based number.
      break;
    }
  }
  if (month == -1) {  // Month not found.
    return false;
  }

  str_token = SplitOneStringToken(&str_curr, kRFC822_DateDelimiters);
  if (str_token == "") {
    return false;
  }

  int year = 0;
  if (!ParseStringToInt(str_token.c_str(), &year, true)) {
    return false;
  }
  if (year < 100) {   // Two digit year format, convert to 1950 - 2050 range.
    if (year < 50) {
      year += 2000;
    } else {
      year += 1900;
    }
  }

  str_token = SplitOneStringToken(&str_curr, kRFC822_TimeDelimiter);
  if (str_token == "") {
    return false;
  }

  int hour = 0;
  if (!ParseStringToInt(str_token.c_str(), &hour, true) ||
      hour < 0 || hour > 23) {
    return false;
  }

  str_token = SplitOneStringToken(&str_curr, kRFC822_TimeDelimiter);
  if (str_token == "") {
    return false;
  }

  int minute = 0;
  if (!ParseStringToInt(str_token.c_str(), &minute, true) ||
      minute < 0 || minute > 59) {
    return false;
  }

  str_token = SplitOneStringToken(&str_curr, kRFC822_TimeDelimiter);
  if (str_token == "") {
    return false;
  }

  int second = 0;
  // Distingushed between XX:XX and XX:XX:XX time formats.
  if (str_token.size() == 2 && isdigit(str_token[0]) && isdigit(str_token[1])) {
    second = 0;
    if (!ParseStringToInt(str_token.c_str(), &second, true) ||
        second < 0 || second > 59) {
      return false;
    }

    str_token = SplitOneStringToken(&str_curr, kRFC822_TimeDelimiter);
    if (str_token == "") {
      return false;
    }
  }

  int bias = 0;
  if (str_token[0] == '+' || str_token[0] == '-' || isdigit(str_token[0])) {
    // Numeric format.
    int zone = 0;
    if (!ParseStringToInt(str_token.c_str(), &zone, true)) {
      return false;
    }

    // Zone is in HHMM format, need to convert to the number of minutes.
    bias = (zone / 100) * 60 + (zone % 100);
  } else {  // Text format.
    for (size_t i = 0; i < sizeof(kRFC822_TimeZone) / sizeof(TimeZoneInfo);
         ++i) {
      if (str_token == kRFC822_TimeZone[i].zone_name) {
        bias = kRFC822_TimeZone[i].hour_dif * 60;
        break;
      }
    }
  }

  SetZero(*time);
  time->tm_year = year - 1900;
  time->tm_mon = month;
  time->tm_mday = day;
  time->tm_hour = hour;
  time->tm_min = minute;
  time->tm_sec = second;

  time64 time_64 = TmToTime64(*time);
  time_64 = time_64 - bias * kMinsTo100ns;

  if (!Time64ToTm(time_64, time)) {
    return false;
  }

  if (ret_local_time) {
    if (!UtcTimeToLocalTime(time)) {
      return false;
    }
  }

  return true;
}

// Parse a string to time span.
//
// A TimeSpan value can be represented as
//    [d.]hh:mm:ss
//
//    d    = days (optional)
//    hh   = hours as measured on a 24-hour clock
//    mm   = minutes
//    ss   = seconds
bool ParseStringToTimeSpan(const char* str, time64* time_span) {
  ASSERT(str);
  ASSERT(time_span);

  const char kColonDelimitor[] = ":";
  const char kDotDelimitor = '.';

  std::string str_span(str);
  time64 span = 0;

  int idx = str_span.find(kDotDelimitor);
  if (idx != -1) {
    std::string str_day = str_span.substr(0, idx);
    int day = 0;
    if (!ParseStringToInt(str_day.c_str(), &day, true) ||
        day < 0 || day > 365) {
      return false;
    }
    span = day;

    str_span = str_span.substr(idx + 1);
  }

  const char* str_curr = str_span.c_str();
  std::string str_token;

  str_token = SplitOneStringToken(&str_curr, kColonDelimitor);
  if (str_token == "") {
    return false;
  }

  int hour = 0;
  if (!ParseStringToInt(str_token.c_str(), &hour, true) ||
      hour < 0 || hour > 23) {
    return false;
  }
  span = span * 24 + hour;

  str_token = SplitOneStringToken(&str_curr, kColonDelimitor);
  if (str_token == "") {
    return false;
  }

  int minute = 0;
  if (!ParseStringToInt(str_token.c_str(), &minute, true) ||
      minute < 0 || minute > 59) {
    return false;
  }
  span = span * 60 + minute;

  str_token = SplitOneStringToken(&str_curr, kColonDelimitor);
  if (str_token == "") {
    return false;
  }

  int second = 0;
  if (!ParseStringToInt(str_token.c_str(), &second, true) ||
      second < 0 || second > 59) {
    return false;
  }

  *time_span = (span * 60 + second) * kSecsTo100ns;

  return true;
}

}  // namespace notifier
