// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "base/logging.h"
#include "chrome/common/net/notifier/base/time.h"

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

}  // namespace notifier
