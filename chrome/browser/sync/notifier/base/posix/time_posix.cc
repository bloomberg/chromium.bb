// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>

#include "chrome/browser/sync/notifier/base/time.h"

namespace notifier {

time64 GetCurrent100NSTime() {
  struct timeval tv;
  struct timezone tz;

  gettimeofday(&tv, &tz);

  time64 retval = tv.tv_sec * kSecsTo100ns;
  retval += tv.tv_usec * kMicrosecsTo100ns;
  retval += kStart100NsTimeToEpoch;
  return retval;
}

time64 TmToTime64(const struct tm& tm) {
  struct tm tm_temp;
  memcpy(&tm_temp, &tm, sizeof(struct tm));
  time_t t = timegm(&tm_temp);
  return t * kSecsTo100ns;
}

bool Time64ToTm(time64 t, struct tm* tm) {
  assert(tm != NULL);
  time_t secs = t / kSecsTo100ns;
  gmtime_r(&secs, tm);
  return true;
}

bool UtcTimeToLocalTime(struct tm* tm) {
  assert(tm != NULL);
  time_t t = timegm(tm);
  localtime_r(&t, tm);
  return true;
}

bool LocalTimeToUtcTime(struct tm* tm) {
  assert(tm != NULL);
  time_t t = mktime(tm);
  gmtime_r(&t, tm);
  return true;
}

}  // namespace notifier
