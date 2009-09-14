// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_NOTIFIER_BASE_TIMER_H_
#define CHROME_BROWSER_SYNC_NOTIFIER_BASE_TIMER_H_

#include "talk/base/task.h"

namespace notifier {

class Timer : private talk_base::Task {
 public:
  Timer(talk_base::Task* parent, int timeout_seconds, bool repeat);
  ~Timer();

  // Call Abort() to stop the timer.
  using talk_base::Task::Abort;

  // Call to find out when the timer is set to go off. Returns int64.
  using talk_base::Task::get_timeout_time;

  // Call to set the timeout interval.
  using talk_base::Task::set_timeout_seconds;

  using talk_base::Task::SignalTimeout;

 private:
  virtual int OnTimeout();
  virtual int ProcessStart();

  bool repeat_;

  DISALLOW_COPY_AND_ASSIGN(Timer);
};

}  // namespace notifier

#endif  // CHROME_BROWSER_SYNC_NOTIFIER_BASE_TIMER_H_
