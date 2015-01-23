// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TIMER_RTC_ALARM_H_
#define COMPONENTS_TIMER_RTC_ALARM_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "components/timers/alarm_timer.h"

namespace base {
class MessageLoopProxy;
}

namespace timers {
// This class manages a Real Time Clock (RTC) alarm, a feature that is available
// from linux version 3.11 onwards.  It creates a file descriptor for the RTC
// alarm timer and then watches that file descriptor to see when it can be read
// without blocking, indicating that the timer has fired.
//
// A major problem for this class is that watching file descriptors is only
// available on a MessageLoopForIO but there is no guarantee the timer is going
// to be created on one.  To get around this, the timer has a dedicated thread
// with a MessageLoopForIO that posts tasks back to the thread that started the
// timer.
//
// This class is designed to work together with the AlarmTimer class and is
// tested through the AlarmTimer unit tests.  DO NOT TRY TO USE THIS CLASS
// DIRECTLY.  Or use it anyway, I'm a comment not a cop.
class RtcAlarm : public AlarmTimer::Delegate,
                 public base::MessageLoopForIO::Watcher {
 public:
  RtcAlarm();

  // AlarmTimer::Delegate overrides.
  bool Init(base::WeakPtr<AlarmTimer> timer) override;
  void Stop() override;
  void Reset(base::TimeDelta delay) override;

  // base::MessageLoopForIO::Watcher overrides.
  void OnFileCanReadWithoutBlocking(int fd) override;
  void OnFileCanWriteWithoutBlocking(int fd) override;

 protected:
  // Needs to be protected because AlarmTimer::Delegate is a refcounted class.
  ~RtcAlarm() override;

 private:
  // Actually performs the system calls to set up the timer.  This must be
  // called on a MessageLoopForIO.
  void ResetImpl(base::TimeDelta delay, int event_id);

  // Callback that is run when the timer fires.  Must be run on
  // |origin_message_loop_|.
  void OnTimerFired(int event_id);

  // File descriptor associated with the alarm timer.
  int alarm_fd_;

  // Message loop which initially started the timer.
  scoped_refptr<base::MessageLoopProxy> origin_message_loop_;

  // The parent timer that should be informed when the timer fires.  We may end
  // up outliving the parent so we need to ensure the reference is valid before
  // we try to call it.
  base::WeakPtr<AlarmTimer> parent_;

  // Manages watching file descriptors.
  scoped_ptr<base::MessageLoopForIO::FileDescriptorWatcher> fd_watcher_;

  // These two variables are used for coordinating between the thread that
  // started the timer and the IO thread being used to watch the timer file
  // descriptor.  When Reset() is called, the original thread increments
  // |origin_event_id_| and binds its value to ResetImpl(), which gets posted to
  // the IO thread.  When the IO thread runs, it saves this value in
  // |io_event_id_|.  Later, when the timer fires, the IO thread binds the value
  // of |io_event_id_| to OnTimerFired() and posts it to the original thread.
  // When the original thread runs OnTimerFired(), it calls
  // parent_->OnTimerFired() only if |origin_event_id_| matches the event id
  // that was passed in to it.  This is used to get around a race condition
  // where the user resets the timer on the original thread, while the event is
  // being fired on the IO thread at the same time.
  int origin_event_id_;
  int io_event_id_;

  DISALLOW_COPY_AND_ASSIGN(RtcAlarm);
};

}  // namespace timers
#endif  // COMPONENTS_TIMER_RTC_ALARM_H_
