// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Time functions

#include <time.h>
#include <windows.h>

#include "chrome/browser/sync/notifier/base/time.h"

#include "chrome/browser/sync/notifier/base/utils.h"
#include "talk/base/common.h"
#include "talk/base/logging.h"

namespace notifier {

time64 FileTimeToTime64(const FILETIME& file_time) {
  return static_cast<time64>(file_time.dwHighDateTime) << 32 |
         file_time.dwLowDateTime;
}

void Time64ToFileTime(const time64& time, FILETIME* ft) {
  ASSERT(ft);

  ft->dwHighDateTime = static_cast<DWORD>(time >> 32);
  ft->dwLowDateTime = static_cast<DWORD>(time & 0xffffffff);
}

void TmTimeToSystemTime(const struct tm& tm, SYSTEMTIME* sys_time) {
  ASSERT(sys_time);

  SetZero(*sys_time);
  // tm's year is 1900 based, systemtime's year is absolute
  sys_time->wYear = tm.tm_year + 1900;
  // tm's month is 0 based, but systemtime's month is 1 based
  sys_time->wMonth = tm.tm_mon + 1;
  sys_time->wDay = tm.tm_mday;
  sys_time->wDayOfWeek = tm.tm_wday;
  sys_time->wHour = tm.tm_hour;
  sys_time->wMinute = tm.tm_min;
  sys_time->wSecond = tm.tm_sec;
}

void SystemTimeToTmTime(const SYSTEMTIME& sys_time, struct tm* tm) {
  ASSERT(tm);

  SetZero(*tm);
  // tm's year is 1900 based, systemtime's year is absolute
  tm->tm_year = sys_time.wYear - 1900;
  // tm's month is 0 based, but systemtime's month is 1 based
  tm->tm_mon = sys_time.wMonth - 1;
  tm->tm_mday = sys_time.wDay;
  tm->tm_wday = sys_time.wDayOfWeek;
  tm->tm_hour = sys_time.wHour;
  tm->tm_min = sys_time.wMinute;
  tm->tm_sec = sys_time.wSecond;
}

time64 GetCurrent100NSTime() {
  // In order to get the 100ns time we shouldn't use SystemTime
  // as it's granularity is 1 ms. Below is the correct implementation.
  // On the other hand the system clock granularity is 15 ms, so we
  // are not gaining much by having the timestamp in nano-sec
  // If we decise to go with ms, divide "time64 time" by 10000

  FILETIME file_time;
  ::GetSystemTimeAsFileTime(&file_time);

  time64 time = FileTimeToTime64(file_time);
  return time;
}

time64 TmToTime64(const struct tm& tm) {
  SYSTEMTIME sys_time;
  TmTimeToSystemTime(tm, &sys_time);

  FILETIME file_time;
  SetZero(file_time);
  if (!::SystemTimeToFileTime(&sys_time, &file_time)) {
    return 0;
  }

  return FileTimeToTime64(file_time);
}

bool Time64ToTm(time64 t, struct tm* tm) {
  ASSERT(tm);

  FILETIME file_time;
  SetZero(file_time);
  Time64ToFileTime(t, &file_time);

  SYSTEMTIME sys_time;
  SetZero(sys_time);
  if (!::FileTimeToSystemTime(&file_time, &sys_time)) {
    return false;
  }

  SystemTimeToTmTime(sys_time, tm);

  return true;
}

bool UtcTimeToLocalTime(struct tm* tm) {
  ASSERT(tm);

  SYSTEMTIME utc_time;
  TmTimeToSystemTime(*tm, &utc_time);

  TIME_ZONE_INFORMATION time_zone;
  if (::GetTimeZoneInformation(&time_zone) == TIME_ZONE_ID_INVALID) {
    return false;
  }

  SYSTEMTIME local_time;
  if (!::SystemTimeToTzSpecificLocalTime(&time_zone, &utc_time, &local_time)) {
    return false;
  }

  SystemTimeToTmTime(local_time, tm);

  return true;
}

bool LocalTimeToUtcTime(struct tm* tm) {
  ASSERT(tm);

  SYSTEMTIME local_time;
  TmTimeToSystemTime(*tm, &local_time);

  // Get the bias, which when added to local, gives UTC
  TIME_ZONE_INFORMATION time_zone;
  if (::GetTimeZoneInformation(&time_zone) == TIME_ZONE_ID_INVALID) {
    return false;
  }

  // By negating the biases, we can get translation from UTC to local
  time_zone.Bias *= -1;
  time_zone.DaylightBias *= -1;
  time_zone.StandardBias *= -1;  // this is 0 but negating for completness

  // We'll tell SystemTimeToTzSpecificLocalTime that the local time is actually
  // UTC. With the negated bias, the "local" time that the API returns will
  // actually be UTC. Casting the const off because
  // SystemTimeToTzSpecificLocalTime's definition requires it, although the
  // value is not modified.
  SYSTEMTIME utc_time;
  if (!::SystemTimeToTzSpecificLocalTime(&time_zone, &local_time, &utc_time)) {
    return false;
  }

  SystemTimeToTmTime(utc_time, tm);

  return true;
}

}  // namespace notifier
