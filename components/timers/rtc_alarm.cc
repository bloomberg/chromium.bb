// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/timers/rtc_alarm.h"

#include <sys/timerfd.h>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/threading/thread.h"

namespace timers {

namespace {
// Helper class to ensure that the IO thread we will use for watching file
// descriptors is started only once.
class IOThreadStartHelper {
 public:
  IOThreadStartHelper() : thread_(new base::Thread("RTC Alarm IO Thread")) {
    CHECK(thread_->StartWithOptions(
        base::Thread::Options(base::MessageLoop::TYPE_IO, 0)));
  }
  ~IOThreadStartHelper() {}

  base::Thread& operator*() const { return *thread_.get(); }

  base::Thread* operator->() const { return thread_.get(); }

 private:
  scoped_ptr<base::Thread> thread_;
};

base::LazyInstance<IOThreadStartHelper> g_io_thread = LAZY_INSTANCE_INITIALIZER;

}  // namespace

RtcAlarm::RtcAlarm()
    : alarm_fd_(timerfd_create(CLOCK_REALTIME_ALARM, 0)),
      origin_event_id_(0),
      io_event_id_(0) {
}

RtcAlarm::~RtcAlarm() {
  if (alarm_fd_ != -1)
    close(alarm_fd_);
}

bool RtcAlarm::Init(base::WeakPtr<AlarmTimer> parent) {
  parent_ = parent;

  return alarm_fd_ != -1;
}

void RtcAlarm::Stop() {
  // Make sure that we stop the RTC from a MessageLoopForIO.
  if (!base::MessageLoopForIO::IsCurrent()) {
    g_io_thread.Get()->task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&RtcAlarm::Stop, scoped_refptr<RtcAlarm>(this)));
    return;
  }

  // Stop watching for events.
  fd_watcher_.reset();

  // Now clear the timer.
  DCHECK_NE(alarm_fd_, -1);
  itimerspec blank_time = {};
  timerfd_settime(alarm_fd_, 0, &blank_time, NULL);
}

void RtcAlarm::Reset(base::TimeDelta delay) {
  // Get a proxy for the current message loop.  When the timer fires, we will
  // post tasks to this proxy to let the parent timer know.
  origin_message_loop_ = base::MessageLoopProxy::current();

  // Increment the event id.  Used to invalidate any events that have been
  // queued but not yet run since the last time Reset() was called.
  origin_event_id_++;

  // Calling timerfd_settime with a zero delay actually clears the timer so if
  // the user has requested a zero delay timer, we need to handle it
  // differently.  We queue the task here but we still go ahead and call
  // timerfd_settime with the zero delay anyway to cancel any previous delay
  // that might have been programmed.
  if (delay <= base::TimeDelta::FromMicroseconds(0)) {
    // The timerfd_settime documentation is vague on what happens when it is
    // passed a negative delay.  We can sidestep the issue by ensuring that the
    // delay is 0.
    delay = base::TimeDelta::FromMicroseconds(0);
    origin_message_loop_->PostTask(FROM_HERE,
                                   base::Bind(&RtcAlarm::OnTimerFired,
                                              scoped_refptr<RtcAlarm>(this),
                                              origin_event_id_));
  }

  // Make sure that we are running on a MessageLoopForIO.
  if (!base::MessageLoopForIO::IsCurrent()) {
    g_io_thread.Get()->task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&RtcAlarm::ResetImpl,
                   scoped_refptr<RtcAlarm>(this),
                   delay,
                   origin_event_id_));
    return;
  }

  ResetImpl(delay, origin_event_id_);
}

void RtcAlarm::OnFileCanReadWithoutBlocking(int fd) {
  DCHECK_EQ(alarm_fd_, fd);

  // Read from the fd to ack the event.
  char val[sizeof(uint64_t)];
  base::ReadFromFD(alarm_fd_, val, sizeof(uint64_t));

  // Make sure that the parent timer is informed on the proper message loop.
  if (origin_message_loop_->RunsTasksOnCurrentThread()) {
    OnTimerFired(io_event_id_);
    return;
  }

  origin_message_loop_->PostTask(FROM_HERE,
                                 base::Bind(&RtcAlarm::OnTimerFired,
                                            scoped_refptr<RtcAlarm>(this),
                                            io_event_id_));
}

void RtcAlarm::OnFileCanWriteWithoutBlocking(int fd) {
  NOTREACHED();
}

void RtcAlarm::ResetImpl(base::TimeDelta delay, int event_id) {
  DCHECK(base::MessageLoopForIO::IsCurrent());
  DCHECK_NE(alarm_fd_, -1);

  // Store the event id in the IO thread variable.  When the timer fires, we
  // will bind this value to the OnTimerFired callback to ensure that we do the
  // right thing if the timer gets reset.
  io_event_id_ = event_id;

  // If we were already watching the fd, this will stop watching it.
  fd_watcher_.reset(new base::MessageLoopForIO::FileDescriptorWatcher);

  // Start watching the fd to see when the timer fires.
  if (!base::MessageLoopForIO::current()->WatchFileDescriptor(
          alarm_fd_,
          false,
          base::MessageLoopForIO::WATCH_READ,
          fd_watcher_.get(),
          this)) {
    LOG(ERROR) << "Error while attempting to watch file descriptor for RTC "
               << "alarm.  Timer will not fire.";
  }

  // Actually set the timer.  This will also clear the pre-existing timer, if
  // any.
  itimerspec alarm_time = {};
  alarm_time.it_value.tv_sec = delay.InSeconds();
  alarm_time.it_value.tv_nsec =
      (delay.InMicroseconds() % base::Time::kMicrosecondsPerSecond) *
      base::Time::kNanosecondsPerMicrosecond;
  if (timerfd_settime(alarm_fd_, 0, &alarm_time, NULL) < 0)
    PLOG(ERROR) << "Error while setting alarm time.  Timer will not fire";
}

void RtcAlarm::OnTimerFired(int event_id) {
  DCHECK(origin_message_loop_->RunsTasksOnCurrentThread());

  // Check to make sure that the timer was not reset in the time between when
  // this task was queued to run and now.  If it was reset, then don't do
  // anything.
  if (event_id != origin_event_id_)
    return;

  if (parent_)
    parent_->OnTimerFired();
}

}  // namespace timers
