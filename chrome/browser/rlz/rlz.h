// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RLZ_RLZ_H_
#define CHROME_BROWSER_RLZ_RLZ_H_
#pragma once

#include "build/build_config.h"

#if defined(OS_WIN)

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "base/task.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "rlz/win/lib/rlz_lib.h"

// RLZ is a library which is used to measure distribution scenarios.
// Its job is to record certain lifetime events in the registry and to send
// them encoded as a compact string at most twice. The sent data does
// not contain information that can be used to identify a user or to infer
// browsing habits. The API in this file is a wrapper around the open source
// RLZ library which can be found at http://code.google.com/p/rlz.
//
// For partner or bundled installs, the RLZ might send more information
// according to the terms disclosed in the EULA.

class RLZTracker : public NotificationObserver {
 public:
  // Initializes the RLZ library services for use in chrome. Schedules a
  // delayed task (delayed by |delay| seconds) that performs the ping and
  // registers some events when 'first-run' is true.
  //
  // If the chrome brand is organic (no partners) then the pings don't occur.
  static bool InitRlzDelayed(bool first_run, int delay,
                             bool google_default_search,
                             bool google_default_homepage);

  // Records an RLZ event. Some events can be access point independent.
  // Returns false it the event could not be recorded. Requires write access
  // to the HKCU registry hive on windows.
  static bool RecordProductEvent(rlz_lib::Product product,
                                 rlz_lib::AccessPoint point,
                                 rlz_lib::Event event_id);

  // Get the RLZ value of the access point.
  // Returns false if the rlz string could not be obtained. In some cases
  // an empty string can be returned which is not an error.
  static bool GetAccessPointRlz(rlz_lib::AccessPoint point, std::wstring* rlz);

  // Invoked during shutdown to clean up any state created by RLZTracker.
  static void CleanupRlz();

  // This method is public for use by the Singleton class.
  static RLZTracker* GetInstance();

  // The following methods are made protected so that they can be used for
  // testing purposes. Production code should never need to call these.
 protected:
  RLZTracker();
  ~RLZTracker();

  // This is a temporary constant used here until the home page change is
  // committed, which will happen before 2011-09-01. This constant will be
  // replaced with PageTransition::HOME_PAGE.
  static const int RLZ_PAGETRANSITION_HOME_PAGE = 0x02000000;

  // Thread function entry point, see ScheduleFinancialPing(). Assumes argument
  // is a pointer to an RLZTracker.
  static void _cdecl PingNow(void* tracker);

  // Performs initialization of RLZ tracker that is purposefully delayed so
  // that it does not interfere with chrome startup time.
  virtual void DelayedInit();

  // NotificationObserver implementation:
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  // Used by test code to override the default RLZTracker instance returned
  // by GetInstance().
  void set_tracker(RLZTracker* tracker) {
    tracker_ = tracker;
  }

 private:
  friend struct DefaultSingletonTraits<RLZTracker>;
  friend class base::RefCountedThreadSafe<RLZTracker>;

  // Implementation called from InitRlzDelayed() static method.
  bool Init(bool first_run, int delay, bool google_default_search,
            bool google_default_homepage);

  // Implementation called from RecordProductEvent() static method.
  bool GetAccessPointRlzImpl(rlz_lib::AccessPoint point, std::wstring* rlz);

  // Schedules the delayed initialization. This method is virtual to allow
  // tests to override how the scheduling is done.
  virtual void ScheduleDelayedInit(int delay);

  // Schedules a call to rlz_lib::SendFinancialPing(). This method is virtual
  // to allow tests to override how the scheduling is done.
  virtual void ScheduleFinancialPing();

  // Schedules a call to GetAccessPointRlz() on the I/O thread if the current
  // thread is not already the I/O thread, otherwise does nothing. Returns
  // true if the call was scheduled, and false otherwise. This method is
  // virtual to allow tests to override how the scheduling is done.
  virtual bool ScheduleGetAccessPointRlz(rlz_lib::AccessPoint point);

  // Sends the financial ping to the RLZ servers and invalidates the RLZ string
  // cache since the response from the RLZ server may have changed then.
  void PingNowImpl();

  // Sends the financial ping to the RLZ servers. This method is virtual to
  // allow tests to override.
  virtual bool SendFinancialPing(const std::wstring& brand,
                                 const std::wstring& lang,
                                 const std::wstring& referral,
                                 bool exclude_id);

  // Tracker used for testing purposes only. If this value is non-NULL, it
  // will be returned from GetInstance() instead of the regular singleton.
  static RLZTracker* tracker_;

  // Configuation data for RLZ tracker. Set by call to Init().
  bool first_run_;
  bool send_ping_immediately_;
  bool google_default_search_;
  bool google_default_homepage_;

  // Keeps track if the RLZ tracker has already performed its delayed
  // initialization.
  bool already_ran_;

  // Keeps a cache of RLZ access point strings, since they rarely change.
  // The cache must be protected by a lock since it may be accessed from
  // the UI thread for reading and the IO thread for reading and/or writing.
  base::Lock cache_lock_;
  std::map<rlz_lib::AccessPoint, std::wstring> rlz_cache_;

  // Keeps track of whether the omnibox or host page have been used.
  bool omnibox_used_;
  bool homepage_used_;

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(RLZTracker);
};

// The RLZTracker is a singleton object that outlives any runnable tasks
// that will be queued up.
DISABLE_RUNNABLE_METHOD_REFCOUNT(RLZTracker);

#endif  // defined(OS_WIN)

#endif  // CHROME_BROWSER_RLZ_RLZ_H_
