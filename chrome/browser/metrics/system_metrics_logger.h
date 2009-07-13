// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use
// of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_SYSTEM_METRICS_LOGGER_H_
#define CHROME_BROWSER_METRICS_SYSTEM_METRICS_LOGGER_H_

#include "base/basictypes.h"

class Profile;

// This is the abstract base class for a simple class that wraps up some
// calls to chromium metrics logging helper functions.  This design will
// allow for easy mocking in unit tests.

class SystemMetricsLogger {
 public:
  SystemMetricsLogger() {}
  virtual ~SystemMetricsLogger() {}
  virtual void RecordOverviewKeystroke(Profile *profile) = 0;
  virtual void RecordOverviewExitMouse(Profile *profile) = 0;
  virtual void RecordOverviewExitKeystroke(Profile *profile) = 0;
  virtual void RecordWindowCycleKeystroke(Profile *profile) = 0;
  virtual void RecordBootTime(int64 time) = 0;
  virtual void RecordUpTime(int64 time) = 0;
};

#endif  // CHROME_BROWSER_METRICS_SYSTEM_METRICS_LOGGER_H_
