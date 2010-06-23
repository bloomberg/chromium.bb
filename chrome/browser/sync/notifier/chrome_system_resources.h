// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Simple system resources class that uses the current message loop
// for scheduling.  Assumes the current message loop is already
// running.

#ifndef CHROME_BROWSER_SYNC_NOTIFIER_CHROME_SYSTEM_RESOURCES_H_
#define CHROME_BROWSER_SYNC_NOTIFIER_CHROME_SYSTEM_RESOURCES_H_

#include "google/cacheinvalidation/invalidation-client.h"

namespace sync_notifier {

// TODO(akalin): Add a NonThreadSafe member to this class and use it.

class ChromeSystemResources : public invalidation::SystemResources {
 public:
  ChromeSystemResources();

  ~ChromeSystemResources();

  virtual invalidation::Time current_time();

  virtual void StartScheduler();

  // We assume that the current message loop is stopped shortly after
  // this is called, i.e. that any in-flight delayed tasks won't get
  // run.
  //
  // TODO(akalin): Make sure that the above actually holds.  Use a
  // ScopedRunnableMethodFactory for better safety.
  virtual void StopScheduler();

  virtual void ScheduleWithDelay(invalidation::TimeDelta delay,
                                 invalidation::Closure* task);

  virtual void ScheduleImmediately(invalidation::Closure* task);

  virtual void Log(LogLevel level, const char* file, int line,
                   const char* format, ...);

 private:
  bool scheduler_active_;
};

}  // namespace sync_notifier

#endif  // CHROME_BROWSER_SYNC_NOTIFIER_CHROME_SYSTEM_RESOURCES_H_
