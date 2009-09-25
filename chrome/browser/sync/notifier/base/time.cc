// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/base/time.h"

#include <string>
#include <time.h>

#include "chrome/browser/sync/notifier/base/utils.h"
#include "talk/base/common.h"
#include "talk/base/logging.h"

namespace notifier {

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

}  // namespace notifier
