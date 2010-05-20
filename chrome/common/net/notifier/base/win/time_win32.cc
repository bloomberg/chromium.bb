// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Windows specific time functions.

#include <time.h>
#include <windows.h>

#include "chrome/common/net/notifier/base/time.h"

#include "talk/base/common.h"

namespace notifier {

time64 FileTimeToTime64(const FILETIME& file_time) {
  return static_cast<time64>(file_time.dwHighDateTime) << 32 |
      file_time.dwLowDateTime;
}

time64 GetCurrent100NSTime() {
  // In order to get the 100ns time we shouldn't use SystemTime as it's
  // granularity is 1 ms. Below is the correct implementation. On the other
  // hand the system clock granularity is 15 ms, so we are not gaining much by
  // having the timestamp in nano-sec. If we decide to go with ms, divide
  // "time64 time" by 10000.
  FILETIME file_time;
  ::GetSystemTimeAsFileTime(&file_time);

  time64 time = FileTimeToTime64(file_time);
  return time;
}

}  // namespace notifier
