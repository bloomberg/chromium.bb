// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_util.h"

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "base/tracked_objects.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/user_manager.h"
#endif  // OS_CHROMEOS

using content::BrowserThread;

namespace gdata {
namespace util {

namespace {

const char kGDataSpecialRootPath[] = "/special";

const char kGDataMountPointPath[] = "/special/drive";

const int kReadOnlyFilePermissions = base::PLATFORM_FILE_OPEN |
                                     base::PLATFORM_FILE_READ |
                                     base::PLATFORM_FILE_EXCLUSIVE_READ |
                                     base::PLATFORM_FILE_ASYNC;

bool ParseTimezone(const base::StringPiece& timezone,
                   bool ahead,
                   int* out_offset_to_utc_in_minutes) {
  DCHECK(out_offset_to_utc_in_minutes);

  std::vector<base::StringPiece> parts;
  int num_of_token = Tokenize(timezone, ":", &parts);

  int hour = 0;
  if (!base::StringToInt(parts[0], &hour))
    return false;

  int minute = 0;
  if (num_of_token > 1 && !base::StringToInt(parts[1], &minute))
    return false;

  *out_offset_to_utc_in_minutes = (hour * 60 + minute) * (ahead ? +1 : -1);
  return true;
}

}  // namespace

bool IsGDataAvailable(Profile* profile) {
#if defined(OS_CHROMEOS)
  if (!chromeos::UserManager::Get()->IsUserLoggedIn() ||
      chromeos::UserManager::Get()->IsLoggedInAsGuest() ||
      chromeos::UserManager::Get()->IsLoggedInAsDemoUser())
    return false;

  // Do not allow GData for incognito windows / guest mode.
  if (profile->IsOffTheRecord())
    return false;

  // Disable gdata if preference is set.  This can happen with commandline flag
  // --disable-gdata or enterprise policy, or probably with user settings too
  // in the future.
  if (profile->GetPrefs()->GetBoolean(prefs::kDisableGData))
    return false;

  return true;
#else
  // TODO(nhiroki): Check if GData is available or not in a platform
  // independent way (http://crbug.com/147529).
  return false;
#endif  // OS_CHROMEOS
}

bool IsDriveV2ApiEnabled() {
  static bool enabled = CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableDriveV2Api);
  return enabled;
}

base::PlatformFileError DriveFileErrorToPlatformError(
    gdata::DriveFileError error) {
  switch (error) {
    case gdata::DRIVE_FILE_OK:
      return base::PLATFORM_FILE_OK;

    case gdata::DRIVE_FILE_ERROR_FAILED:
      return base::PLATFORM_FILE_ERROR_FAILED;

    case gdata::DRIVE_FILE_ERROR_IN_USE:
      return base::PLATFORM_FILE_ERROR_IN_USE;

    case gdata::DRIVE_FILE_ERROR_EXISTS:
      return base::PLATFORM_FILE_ERROR_EXISTS;

    case gdata::DRIVE_FILE_ERROR_NOT_FOUND:
      return base::PLATFORM_FILE_ERROR_NOT_FOUND;

    case gdata::DRIVE_FILE_ERROR_ACCESS_DENIED:
      return base::PLATFORM_FILE_ERROR_ACCESS_DENIED;

    case gdata::DRIVE_FILE_ERROR_TOO_MANY_OPENED:
      return base::PLATFORM_FILE_ERROR_TOO_MANY_OPENED;

    case gdata::DRIVE_FILE_ERROR_NO_MEMORY:
      return base::PLATFORM_FILE_ERROR_NO_MEMORY;

    case gdata::DRIVE_FILE_ERROR_NO_SPACE:
      return base::PLATFORM_FILE_ERROR_NO_SPACE;

    case gdata::DRIVE_FILE_ERROR_NOT_A_DIRECTORY:
      return base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY;

    case gdata::DRIVE_FILE_ERROR_INVALID_OPERATION:
      return base::PLATFORM_FILE_ERROR_INVALID_OPERATION;

    case gdata::DRIVE_FILE_ERROR_SECURITY:
      return base::PLATFORM_FILE_ERROR_SECURITY;

    case gdata::DRIVE_FILE_ERROR_ABORT:
      return base::PLATFORM_FILE_ERROR_ABORT;

    case gdata::DRIVE_FILE_ERROR_NOT_A_FILE:
      return base::PLATFORM_FILE_ERROR_NOT_A_FILE;

    case gdata::DRIVE_FILE_ERROR_NOT_EMPTY:
      return base::PLATFORM_FILE_ERROR_NOT_EMPTY;

    case gdata::DRIVE_FILE_ERROR_INVALID_URL:
      return base::PLATFORM_FILE_ERROR_INVALID_URL;

    case gdata::DRIVE_FILE_ERROR_NO_CONNECTION:
      return base::PLATFORM_FILE_ERROR_FAILED;
  }

  NOTREACHED();
  return base::PLATFORM_FILE_ERROR_FAILED;
}

bool GetTimeFromString(const base::StringPiece& raw_value,
                       base::Time* parsed_time) {
  base::StringPiece date;
  base::StringPiece time_and_tz;
  base::StringPiece time;
  base::Time::Exploded exploded = {0};
  bool has_timezone = false;
  int offset_to_utc_in_minutes = 0;

  // Splits the string into "date" part and "time" part.
  {
    std::vector<base::StringPiece> parts;
    if (Tokenize(raw_value, "T", &parts) != 2)
      return false;
    date = parts[0];
    time_and_tz = parts[1];
  }

  // Parses timezone suffix on the time part if available.
  {
    std::vector<base::StringPiece> parts;
    if (time_and_tz[time_and_tz.size() - 1] == 'Z') {
      // Timezone is 'Z' (UTC)
      has_timezone = true;
      offset_to_utc_in_minutes = 0;
      time = time_and_tz;
      time.remove_suffix(1);
    } else if (Tokenize(time_and_tz, "+", &parts) == 2) {
      // Timezone is "+hh:mm" format
      if (!ParseTimezone(parts[1], true, &offset_to_utc_in_minutes))
        return false;
      has_timezone = true;
      time = parts[0];
    } else if (Tokenize(time_and_tz, "-", &parts) == 2) {
      // Timezone is "-hh:mm" format
      if (!ParseTimezone(parts[1], false, &offset_to_utc_in_minutes))
        return false;
      has_timezone = true;
      time = parts[0];
    } else {
      // No timezone (uses local timezone)
      time = time_and_tz;
    }
  }

  // Parses the date part.
  {
    std::vector<base::StringPiece> parts;
    if (Tokenize(date, "-", &parts) != 3)
      return false;

    if (!base::StringToInt(parts[0], &exploded.year) ||
        !base::StringToInt(parts[1], &exploded.month) ||
        !base::StringToInt(parts[2], &exploded.day_of_month)) {
      return false;
    }
  }

  // Parses the time part.
  {
    std::vector<base::StringPiece> parts;
    int num_of_token = Tokenize(time, ":", &parts);
    if (num_of_token != 3)
      return false;

    if (!base::StringToInt(parts[0], &exploded.hour) ||
        !base::StringToInt(parts[1], &exploded.minute)) {
      return false;
    }

    std::vector<base::StringPiece> seconds_parts;
    int num_of_seconds_token = Tokenize(parts[2], ".", &seconds_parts);
    if (num_of_seconds_token >= 3)
      return false;

    if (!base::StringToInt(seconds_parts[0], &exploded.second))
        return false;

    // Only accept milli-seconds (3-digits).
    if (num_of_seconds_token > 1 &&
        seconds_parts[1].length() == 3 &&
        !base::StringToInt(seconds_parts[1], &exploded.millisecond)) {
      return false;
    }
  }

  exploded.day_of_week = 0;
  if (!exploded.HasValidValues())
    return false;

  if (has_timezone) {
    *parsed_time = base::Time::FromUTCExploded(exploded);
    if (offset_to_utc_in_minutes != 0)
      *parsed_time -= base::TimeDelta::FromMinutes(offset_to_utc_in_minutes);
  } else {
    *parsed_time = base::Time::FromLocalExploded(exploded);
  }

  return true;
}

std::string FormatTimeAsString(const base::Time& time) {
  base::Time::Exploded exploded;
  time.UTCExplode(&exploded);
  return base::StringPrintf(
      "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
      exploded.year, exploded.month, exploded.day_of_month,
      exploded.hour, exploded.minute, exploded.second, exploded.millisecond);
}

std::string FormatTimeAsStringLocaltime(const base::Time& time) {
  base::Time::Exploded exploded;
  time.LocalExplode(&exploded);

  return base::StringPrintf(
      "%04d-%02d-%02dT%02d:%02d:%02d.%03d",
      exploded.year, exploded.month, exploded.day_of_month,
      exploded.hour, exploded.minute, exploded.second, exploded.millisecond);
}

void PostBlockingPoolSequencedTask(
    const tracked_objects::Location& from_here,
    base::SequencedTaskRunner* blocking_task_runner,
    const base::Closure& task) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  const bool posted = blocking_task_runner->PostTask(from_here, task);
  DCHECK(posted);
}

void PostBlockingPoolSequencedTaskAndReply(
    const tracked_objects::Location& from_here,
    base::SequencedTaskRunner* blocking_task_runner,
    const base::Closure& request_task,
    const base::Closure& reply_task) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  const bool posted = blocking_task_runner->PostTaskAndReply(
      from_here, request_task, reply_task);
  DCHECK(posted);
}

}  // namespace util
}  // namespace gdata
