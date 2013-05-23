// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process_metrics.h"

#include <sys/time.h>

namespace base {

int64 TimeValToMicroseconds(const struct timeval& tv) {
  static const int kMicrosecondsPerSecond = 1000000;
  int64 ret = tv.tv_sec;  // Avoid (int * int) integer overflow.
  ret *= kMicrosecondsPerSecond;
  ret += tv.tv_usec;
  return ret;
}

ProcessMetrics::~ProcessMetrics() { }

}  // namespace base
