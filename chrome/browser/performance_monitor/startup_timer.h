// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MONITOR_STARTUP_TIMER_H_
#define CHROME_BROWSER_PERFORMANCE_MONITOR_STARTUP_TIMER_H_

#include "base/time/time.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace performance_monitor {

// This class is responsible for recording the startup and session restore times
// (if applicable) for PerformanceMonitor. This allows us to initialize this
// relatively small object early in the startup process, and start the whole of
// PerformanceMonitor at a later time. StartupTimer will record the times and
// insert them into PerformanceMonitor's database.
class StartupTimer : public content::NotificationObserver {
 public:
  // Indicates the type of startup; i.e. either a normal startup or a testing
  // environment.
  enum StartupType {
    STARTUP_NORMAL,
    STARTUP_TEST
  };

  StartupTimer();
  virtual ~StartupTimer();

  // Inform StartupTimer that the startup process has been completed; stop the
  // timer. Returns false if the timer has already stopped.
  bool SignalStartupComplete(StartupType startup_type);

  // Pauses the timer until UnpauseTimer() is called; any time spent within a
  // pause does not count towards the measured startup time. This will DCHECK if
  // PauseTimer() is called while paused or UnpauseTimer() is called while
  // unpaused.
  static void PauseTimer();
  static void UnpauseTimer();

  // content::NotificationObserver
  // We keep track of whether or not PerformanceMonitor has been started via
  // the PERFORMANCE_MONITOR_INITIALIZED notification; we need to know this so
  // we know when to insert startup data into the database. We either insert
  // data as we gather it (if PerformanceMonitor is started prior to data
  // collection) or at the notification (if PerformanceMonitor is started
  // later).
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  static void SetElapsedSessionRestoreTime(
      const base::TimeDelta& elapsed_session_restore_time);

 private:
  // Insert the elapsed time measures into PerformanceMonitor's database.
  void InsertElapsedStartupTime();
  void InsertElapsedSessionRestoreTime();

  // The time at which the startup process begins (the creation of
  // ChromeBrowserMain).
  base::TimeTicks startup_begin_;

  // The time at which the timer was most recently paused, or null if the timer
  // is not currently paused.
  base::TimeTicks pause_started_;

  // The total duration for which the timer has been paused.
  base::TimeDelta total_pause_;

  // A flag of whether or not this was a "normal" startup (e.g. whether or not
  // this was in a testing environment, which would change the startup time
  // values). If it is not a normal startup, we use a different metric.
  StartupType startup_type_;

  // The total duration of the startup process, minus any pauses.
  base::TimeDelta elapsed_startup_time_;

  // The total duration of the session restore(s), if any occurred. This is
  // independent of the startup time, because:
  // - If the user has auto-restore on, the restore is synchronous, and we pause
  //   the startup timer during the session restore; the restore will not
  //   interfere with startup timing.
  // - If Chrome crashed and the user chooses to restore the crashed session,
  //   then the startup is already completed; the restore will not interfere
  //   with startup timing.
  std::vector<base::TimeDelta> elapsed_session_restore_times_;

  // Flag whether or not PerformanceMonitor has been fully started.
  bool performance_monitor_initialized_;

  content::NotificationRegistrar registrar_;

  // The singleton of this class.
  static StartupTimer* g_startup_timer_;

  DISALLOW_COPY_AND_ASSIGN(StartupTimer);
};

}  // namespace performance_monitor

#endif  // CHROME_BROWSER_PERFORMANCE_MONITOR_STARTUP_TIMER_H_
