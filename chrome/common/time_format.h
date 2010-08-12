// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_TIME_FORMAT_H__
#define CHROME_COMMON_TIME_FORMAT_H__
#pragma once

// This file defines methods to format time values as strings.

#include "base/string16.h"

#include "unicode/smpdtfmt.h"

namespace base {
class Time;
class TimeDelta;
}

class TimeFormat {
 public:
  // TimeElapsed, TimeRemaining and TimeRemainingShort functions:
  // These functions return a localized string of approximate time duration. The
  // conditions are simpler than PastTime since these functions are used for
  // in-progress operations and users have different expectations of units.

  // Returns times in elapsed-format: "3 mins ago", "2 days ago".
  static string16 TimeElapsed(const base::TimeDelta& delta);

  // Returns times in remaining-format: "3 mins left", "2 days left".
  static string16 TimeRemaining(const base::TimeDelta& delta);

  // Returns times in short-format: "3 mins", "2 days".
  static string16 TimeRemainingShort(const base::TimeDelta& delta);

  // For displaying a relative time in the past.  This method returns either
  // "Today", "Yesterday", or an empty string if it's older than that.
  //
  // TODO(brettw): This should be able to handle days in the future like
  //    "Tomorrow".
  // TODO(tc): This should be able to do things like "Last week".  This
  //    requires handling singluar/plural for all languages.
  //
  // The second parameter is optional, it is midnight of "Now" for relative day
  // computations: Time::Now().LocalMidnight()
  // If NULL, the current day's midnight will be retrieved, which can be
  // slow. If many items are being processed, it is best to get the current
  // time once at the beginning and pass it for each computation.
  static string16 RelativeDate(const base::Time& time,
                               const base::Time* optional_midnight_today);
};

#endif  // CHROME_COMMON_TIME_FORMAT_H__
