// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/jankometer.h"

#include <limits>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/metrics/stats_counters.h"
#include "base/pending_task.h"
#include "base/strings/string_util.h"
#include "base/threading/thread.h"
#include "base/threading/watchdog.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_thread.h"

using base::TimeDelta;
using base::TimeTicks;
using content::BrowserThread;

namespace {

// The maximum threshold of delay of the message  before considering it a delay.
// For a debug build, you may want to set IO delay around 500ms.
// For a release build, setting it around 350ms is sensible.
// Visit about:histograms to see what the distribution is on your system, with
// your build. Be sure to do some work to get interesting stats.
// The numbers below came from a warm start (you'll get about 5-10 alarms with
// a cold start), and running the page-cycler for 5 rounds.
#ifdef NDEBUG
const int kMaxUIMessageDelayMs = 350;
const int kMaxIOMessageDelayMs = 200;
#else
const int kMaxUIMessageDelayMs = 500;
const int kMaxIOMessageDelayMs = 400;
#endif

// Maximum processing time (excluding queueing delay) for a message before
// considering it delayed.
const int kMaxMessageProcessingMs = 100;

// TODO(brettw) Consider making this a pref.
const bool kPlaySounds = false;

//------------------------------------------------------------------------------
// Provide a special watchdog to make it easy to set the breakpoint on this
// class only.
class JankWatchdog : public base::Watchdog {
 public:
  JankWatchdog(const TimeDelta& duration,
               const std::string& thread_watched_name,
               bool enabled)
      : Watchdog(duration, thread_watched_name, enabled),
        thread_name_watched_(thread_watched_name),
        alarm_count_(0) {
  }

  virtual ~JankWatchdog() {}

  virtual void Alarm() OVERRIDE {
    // Put break point here if you want to stop threads and look at what caused
    // the jankiness.
    alarm_count_++;
    Watchdog::Alarm();
  }

 private:
  std::string thread_name_watched_;
  int alarm_count_;

  DISALLOW_COPY_AND_ASSIGN(JankWatchdog);
};

class JankObserverHelper {
 public:
  JankObserverHelper(const std::string& thread_name,
                     const TimeDelta& excessive_duration,
                     bool watchdog_enable);
  ~JankObserverHelper();

  void StartProcessingTimers(const TimeDelta& queueing_time);
  void EndProcessingTimers();

  // Indicate if we will bother to measuer this message.
  bool MessageWillBeMeasured();

  static void SetDefaultMessagesToSkip(int count) { discard_count_ = count; }

 private:
  const TimeDelta max_message_delay_;

  // Indicate if we'll bother measuring this message.
  bool measure_current_message_;

  // Down counter which will periodically hit 0, and only then bother to measure
  // the corresponding message.
  int events_till_measurement_;

  // The value to re-initialize events_till_measurement_ after it reaches 0.
  static int discard_count_;

  // Time at which the current message processing began.
  TimeTicks begin_process_message_;

  // Time the current message spent in the queue -- delta between message
  // construction time and message processing time.
  TimeDelta queueing_time_;

  // Counters for the two types of jank we measure.
  base::StatsCounter slow_processing_counter_;  // Msgs w/ long proc time.
  base::StatsCounter queueing_delay_counter_;   // Msgs w/ long queueing delay.
  base::HistogramBase* const process_times_;  // Time spent proc. task.
  base::HistogramBase* const total_times_;  // Total queueing plus proc.
  JankWatchdog total_time_watchdog_;  // Watching for excessive total_time.

  DISALLOW_COPY_AND_ASSIGN(JankObserverHelper);
};

JankObserverHelper::JankObserverHelper(
    const std::string& thread_name,
    const TimeDelta& excessive_duration,
    bool watchdog_enable)
    : max_message_delay_(excessive_duration),
      measure_current_message_(true),
      events_till_measurement_(0),
      slow_processing_counter_(std::string("Chrome.SlowMsg") + thread_name),
      queueing_delay_counter_(std::string("Chrome.DelayMsg") + thread_name),
      process_times_(base::Histogram::FactoryGet(
          std::string("Chrome.ProcMsgL ") + thread_name,
          1, 3600000, 50, base::Histogram::kUmaTargetedHistogramFlag)),
      total_times_(base::Histogram::FactoryGet(
          std::string("Chrome.TotalMsgL ") + thread_name,
          1, 3600000, 50, base::Histogram::kUmaTargetedHistogramFlag)),
      total_time_watchdog_(excessive_duration, thread_name, watchdog_enable) {
  if (discard_count_ > 0) {
    // Select a vaguely random sample-start-point.
    events_till_measurement_ = static_cast<int>(
        (TimeTicks::Now() - TimeTicks()).InSeconds() % (discard_count_ + 1));
  }
}

JankObserverHelper::~JankObserverHelper() {}

// Called when a message has just begun processing, initializes
// per-message variables and timers.
void JankObserverHelper::StartProcessingTimers(const TimeDelta& queueing_time) {
  DCHECK(measure_current_message_);
  begin_process_message_ = TimeTicks::Now();
  queueing_time_ = queueing_time;

  // Simulate arming when the message entered the queue.
  total_time_watchdog_.ArmSomeTimeDeltaAgo(queueing_time_);
  if (queueing_time_ > max_message_delay_) {
    // Message is too delayed.
    queueing_delay_counter_.Increment();
    if (kPlaySounds)
      MessageBeep(MB_ICONASTERISK);
  }
}

// Called when a message has just finished processing, finalizes
// per-message variables and timers.
void JankObserverHelper::EndProcessingTimers() {
  if (!measure_current_message_)
    return;
  total_time_watchdog_.Disarm();
  TimeTicks now = TimeTicks::Now();
  if (begin_process_message_ != TimeTicks()) {
    TimeDelta processing_time = now - begin_process_message_;
    process_times_->AddTime(processing_time);
    total_times_->AddTime(queueing_time_ + processing_time);
  }
  if (now - begin_process_message_ >
      TimeDelta::FromMilliseconds(kMaxMessageProcessingMs)) {
    // Message took too long to process.
    slow_processing_counter_.Increment();
    if (kPlaySounds)
      MessageBeep(MB_ICONHAND);
  }

  // Reset message specific times.
  begin_process_message_ = base::TimeTicks();
  queueing_time_ = base::TimeDelta();
}

bool JankObserverHelper::MessageWillBeMeasured() {
  measure_current_message_ = events_till_measurement_ <= 0;
  if (!measure_current_message_)
    --events_till_measurement_;
  else
    events_till_measurement_ = discard_count_;
  return measure_current_message_;
}

// static
int JankObserverHelper::discard_count_ = 99;  // Measure only 1 in 100.

//------------------------------------------------------------------------------
class IOJankObserver : public base::RefCountedThreadSafe<IOJankObserver>,
                       public base::MessageLoopForIO::IOObserver,
                       public base::MessageLoop::TaskObserver {
 public:
  IOJankObserver(const char* thread_name,
                 TimeDelta excessive_duration,
                 bool watchdog_enable)
      : helper_(thread_name, excessive_duration, watchdog_enable) {}

  // Attaches the observer to the current thread's message loop. You can only
  // attach to the current thread, so this function can be invoked on another
  // thread to attach it.
  void AttachToCurrentThread() {
    base::MessageLoop::current()->AddTaskObserver(this);
    base::MessageLoopForIO::current()->AddIOObserver(this);
  }

  // Detaches the observer to the current thread's message loop.
  void DetachFromCurrentThread() {
    base::MessageLoopForIO::current()->RemoveIOObserver(this);
    base::MessageLoop::current()->RemoveTaskObserver(this);
  }

  virtual void WillProcessIOEvent() OVERRIDE {
    if (!helper_.MessageWillBeMeasured())
      return;
    helper_.StartProcessingTimers(base::TimeDelta());
  }

  virtual void DidProcessIOEvent() OVERRIDE {
    helper_.EndProcessingTimers();
  }

  virtual void WillProcessTask(const base::PendingTask& pending_task) OVERRIDE {
    if (!helper_.MessageWillBeMeasured())
      return;
    base::TimeTicks now = base::TimeTicks::Now();
    const base::TimeDelta queueing_time = now - pending_task.time_posted;
    helper_.StartProcessingTimers(queueing_time);
  }

  virtual void DidProcessTask(const base::PendingTask& pending_task) OVERRIDE {
    helper_.EndProcessingTimers();
  }

 private:
  friend class base::RefCountedThreadSafe<IOJankObserver>;

  virtual ~IOJankObserver() {}

  JankObserverHelper helper_;

  DISALLOW_COPY_AND_ASSIGN(IOJankObserver);
};

//------------------------------------------------------------------------------
class UIJankObserver : public base::RefCountedThreadSafe<UIJankObserver>,
                       public base::MessageLoop::TaskObserver,
                       public base::MessageLoopForUI::Observer {
 public:
  UIJankObserver(const char* thread_name,
                 TimeDelta excessive_duration,
                 bool watchdog_enable)
      : helper_(thread_name, excessive_duration, watchdog_enable) {}

  // Attaches the observer to the current thread's message loop. You can only
  // attach to the current thread, so this function can be invoked on another
  // thread to attach it.
  void AttachToCurrentThread() {
    DCHECK(base::MessageLoopForUI::IsCurrent());
    base::MessageLoopForUI::current()->AddObserver(this);
    base::MessageLoop::current()->AddTaskObserver(this);
  }

  // Detaches the observer to the current thread's message loop.
  void DetachFromCurrentThread() {
    DCHECK(base::MessageLoopForUI::IsCurrent());
    base::MessageLoop::current()->RemoveTaskObserver(this);
    base::MessageLoopForUI::current()->RemoveObserver(this);
  }

  virtual void WillProcessTask(const base::PendingTask& pending_task) OVERRIDE {
    if (!helper_.MessageWillBeMeasured())
      return;
    base::TimeTicks now = base::TimeTicks::Now();
    const base::TimeDelta queueing_time = now - pending_task.time_posted;
    helper_.StartProcessingTimers(queueing_time);
  }

  virtual void DidProcessTask(const base::PendingTask& pending_task) OVERRIDE {
    helper_.EndProcessingTimers();
  }

  virtual void WillProcessEvent(const base::NativeEvent& event) OVERRIDE {
    if (!helper_.MessageWillBeMeasured())
      return;
    // GetMessageTime returns a LONG (signed 32-bit) and GetTickCount returns
    // a DWORD (unsigned 32-bit). They both wrap around when the time is longer
    // than they can hold. I'm not sure if GetMessageTime wraps around to 0,
    // or if the original time comes from GetTickCount, it might wrap around
    // to -1.
    //
    // Therefore, I cast to DWORD so if it wraps to -1 we will correct it. If
    // it doesn't, then our time delta will be negative if a message happens
    // to straddle the wraparound point, it will still be OK.
    DWORD cur_message_issue_time = static_cast<DWORD>(event.time);
    DWORD cur_time = GetTickCount();
    base::TimeDelta queueing_time =
        base::TimeDelta::FromMilliseconds(cur_time - cur_message_issue_time);

    helper_.StartProcessingTimers(queueing_time);
  }

  virtual void DidProcessEvent(const base::NativeEvent& event) OVERRIDE {
    helper_.EndProcessingTimers();
  }

 private:
  friend class base::RefCountedThreadSafe<UIJankObserver>;

  virtual ~UIJankObserver() {}

  JankObserverHelper helper_;

  DISALLOW_COPY_AND_ASSIGN(UIJankObserver);
};

// These objects are created by InstallJankometer and leaked.
const scoped_refptr<UIJankObserver>* ui_observer = NULL;
const scoped_refptr<IOJankObserver>* io_observer = NULL;

}  // namespace

void InstallJankometer(const CommandLine& parsed_command_line) {
  if (ui_observer || io_observer) {
    NOTREACHED() << "Initializing jank-o-meter twice";
    return;
  }

  bool ui_watchdog_enabled = false;
  bool io_watchdog_enabled = false;
  if (parsed_command_line.HasSwitch(switches::kEnableWatchdog)) {
    std::string list =
        parsed_command_line.GetSwitchValueASCII(switches::kEnableWatchdog);
    if (list.npos != list.find("ui"))
      ui_watchdog_enabled = true;
    if (list.npos != list.find("io"))
      io_watchdog_enabled = true;
  }

  if (ui_watchdog_enabled || io_watchdog_enabled)
    JankObserverHelper::SetDefaultMessagesToSkip(0);  // Watch everything.

  // Install on the UI thread.
  ui_observer = new scoped_refptr<UIJankObserver>(
      new UIJankObserver(
          "UI",
          TimeDelta::FromMilliseconds(kMaxUIMessageDelayMs),
          ui_watchdog_enabled));
  (*ui_observer)->AttachToCurrentThread();

  // Now install on the I/O thread. Hiccups on that thread will block
  // interaction with web pages. We must proxy to that thread before we can
  // add our observer.
  io_observer = new scoped_refptr<IOJankObserver>(
      new IOJankObserver(
          "IO",
          TimeDelta::FromMilliseconds(kMaxIOMessageDelayMs),
          io_watchdog_enabled));
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&IOJankObserver::AttachToCurrentThread, io_observer->get()));
}

void UninstallJankometer() {
  if (ui_observer) {
    (*ui_observer)->DetachFromCurrentThread();
    delete ui_observer;
    ui_observer = NULL;
  }
  if (io_observer) {
    // IO thread can't be running when we remove observers.
    DCHECK((!g_browser_process) || !(g_browser_process->io_thread()));
    delete io_observer;
    io_observer = NULL;
  }
}
