// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/certificate/types.h"

#include "platform/api/logging.h"

namespace cast {
namespace certificate {

bool operator<(const DateTime& a, const DateTime& b) {
  if (a.year < b.year) {
    return true;
  } else if (a.year > b.year) {
    return false;
  }
  if (a.month < b.month) {
    return true;
  } else if (a.month > b.month) {
    return false;
  }
  if (a.day < b.day) {
    return true;
  } else if (a.day > b.day) {
    return false;
  }
  if (a.hour < b.hour) {
    return true;
  } else if (a.hour > b.hour) {
    return false;
  }
  if (a.minute < b.minute) {
    return true;
  } else if (a.minute > b.minute) {
    return false;
  }
  if (a.second < b.second) {
    return true;
  } else if (a.second > b.second) {
    return false;
  }
  return false;
}

bool operator>(const DateTime& a, const DateTime& b) {
  return (b < a);
}

bool DateTimeFromSeconds(uint64_t seconds, DateTime* time) {
  struct tm tm = {};
  time_t sec = static_cast<time_t>(seconds);
  OSP_DCHECK_GE(sec, 0);
  OSP_DCHECK_EQ(static_cast<uint64_t>(sec), seconds);
  if (!gmtime_r(&sec, &tm)) {
    return false;
  }

  time->second = tm.tm_sec;
  time->minute = tm.tm_min;
  time->hour = tm.tm_hour;
  time->day = tm.tm_mday;
  time->month = tm.tm_mon + 1;
  time->year = tm.tm_year + 1900;

  return true;
}

}  // namespace certificate
}  // namespace cast
