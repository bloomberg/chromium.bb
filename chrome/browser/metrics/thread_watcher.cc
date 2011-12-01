// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/thread_watcher.h"

#include <math.h>  // ceil

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "base/lazy_instance.h"
#include "base/string_tokenizer.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/metrics/metrics_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/logging_chrome.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

using content::BrowserThread;

namespace {

// The following are unique function names for forcing the crash when a thread
// is unresponsive. This makes it possible to tell from the callstack alone what
// thread was unresponsive.
//
// We disable optimizations for this block of functions so the compiler doesn't
// merge them all together.

// TODO(eroman): What is the equivalent for other compilers?
#if defined(COMPILER_MSVC)
#pragma optimize("", off)
MSVC_PUSH_DISABLE_WARNING(4748)
#endif

void ThreadUnresponsive_UI() {
  CHECK(false);
}

void ThreadUnresponsive_DB() {
  CHECK(false);
}

void ThreadUnresponsive_WEBKIT() {
  CHECK(false);
}

void ThreadUnresponsive_FILE() {
  CHECK(false);
}

void ThreadUnresponsive_PROCESS_LAUNCHER() {
  CHECK(false);
}

void ThreadUnresponsive_CACHE() {
  CHECK(false);
}

void ThreadUnresponsive_IO() {
  CHECK(false);
}

void ThreadUnresponsive_WEB_SOCKET_PROXY() {
  CHECK(false);
}

#if defined(COMPILER_MSVC)
MSVC_POP_WARNING()
#pragma optimize("", on)
#endif

void CrashBecauseThreadWasUnresponsive(BrowserThread::ID thread_id) {
  base::debug::Alias(&thread_id);

  switch (thread_id) {
    case BrowserThread::UI:
      return ThreadUnresponsive_UI();
    case BrowserThread::DB:
      return ThreadUnresponsive_DB();
    case BrowserThread::WEBKIT:
      return ThreadUnresponsive_WEBKIT();
    case BrowserThread::FILE:
      return ThreadUnresponsive_FILE();
    case BrowserThread::PROCESS_LAUNCHER:
      return ThreadUnresponsive_PROCESS_LAUNCHER();
    case BrowserThread::CACHE:
      return ThreadUnresponsive_CACHE();
    case BrowserThread::IO:
      return ThreadUnresponsive_IO();
#if defined(OS_CHROMEOS)
    case BrowserThread::WEB_SOCKET_PROXY:
      return ThreadUnresponsive_WEB_SOCKET_PROXY();
#endif
    case BrowserThread::ID_COUNT:
      CHECK(false);  // This shouldn't actually be reached!
      break;

    // Omission of the default hander is intentional -- that way the compiler
    // should warn if our switch becomes outdated.
  }

  CHECK(false);  // Shouldn't be reached.
}

}  // namespace

// ThreadWatcher methods and members.
ThreadWatcher::ThreadWatcher(const WatchingParams& params)
    : thread_id_(params.thread_id),
      thread_name_(params.thread_name),
      watched_loop_(
          BrowserThread::GetMessageLoopProxyForThread(params.thread_id)),
      sleep_time_(params.sleep_time),
      unresponsive_time_(params.unresponsive_time),
      ping_time_(base::TimeTicks::Now()),
      pong_time_(ping_time_),
      ping_sequence_number_(0),
      active_(false),
      ping_count_(params.unresponsive_threshold),
      response_time_histogram_(NULL),
      unresponsive_time_histogram_(NULL),
      unresponsive_count_(0),
      hung_processing_complete_(false),
      unresponsive_threshold_(params.unresponsive_threshold),
      crash_on_hang_(params.crash_on_hang),
      live_threads_threshold_(params.live_threads_threshold),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  DCHECK(WatchDogThread::CurrentlyOnWatchDogThread());
  Initialize();
}

ThreadWatcher::~ThreadWatcher() {}

// static
void ThreadWatcher::StartWatching(const WatchingParams& params) {
  DCHECK_GE(params.sleep_time.InMilliseconds(), 0);
  DCHECK_GE(params.unresponsive_time.InMilliseconds(),
            params.sleep_time.InMilliseconds());

  // If we are not on WatchDogThread, then post a task to call StartWatching on
  // WatchDogThread.
  if (!WatchDogThread::CurrentlyOnWatchDogThread()) {
    WatchDogThread::PostTask(
        FROM_HERE,
        base::Bind(&ThreadWatcher::StartWatching, params));
    return;
  }

  DCHECK(WatchDogThread::CurrentlyOnWatchDogThread());

  // Create a new thread watcher object for the given thread and activate it.
  ThreadWatcher* watcher = new ThreadWatcher(params);

  DCHECK(watcher);
  // If we couldn't register the thread watcher object, we are shutting down,
  // then don't activate thread watching.
  if (!ThreadWatcherList::IsRegistered(params.thread_id))
    return;
  watcher->ActivateThreadWatching();
}

void ThreadWatcher::ActivateThreadWatching() {
  DCHECK(WatchDogThread::CurrentlyOnWatchDogThread());
  if (active_) return;
  active_ = true;
  ping_count_ = unresponsive_threshold_;
  ResetHangCounters();
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&ThreadWatcher::PostPingMessage,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ThreadWatcher::DeActivateThreadWatching() {
  DCHECK(WatchDogThread::CurrentlyOnWatchDogThread());
  active_ = false;
  ping_count_ = 0;
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void ThreadWatcher::WakeUp() {
  DCHECK(WatchDogThread::CurrentlyOnWatchDogThread());
  // There is some user activity, PostPingMessage task of thread watcher if
  // needed.
  if (!active_) return;

  // Throw away the previous |unresponsive_count_| and start over again. Just
  // before going to sleep, |unresponsive_count_| could be very close to
  // |unresponsive_threshold_| and when user becomes active,
  // |unresponsive_count_| can go over |unresponsive_threshold_| if there was no
  // response for ping messages. Reset |unresponsive_count_| to start measuring
  // the unresponsiveness of the threads when system becomes active.
  unresponsive_count_ = 0;

  if (ping_count_ <= 0) {
    ping_count_ = unresponsive_threshold_;
    ResetHangCounters();
    PostPingMessage();
  } else {
    ping_count_ = unresponsive_threshold_;
  }
}

void ThreadWatcher::PostPingMessage() {
  DCHECK(WatchDogThread::CurrentlyOnWatchDogThread());
  // If we have stopped watching or if the user is idle, then stop sending
  // ping messages.
  if (!active_ || ping_count_ <= 0)
    return;

  // Save the current time when we have sent ping message.
  ping_time_ = base::TimeTicks::Now();

  // Send a ping message to the watched thread. Callback will be called on
  // the WatchDogThread.
  base::Closure callback(
      base::Bind(&ThreadWatcher::OnPongMessage, weak_ptr_factory_.GetWeakPtr(),
                 ping_sequence_number_));
  if (watched_loop_->PostTask(
          FROM_HERE,
          base::Bind(&ThreadWatcher::OnPingMessage, thread_id_,
                     callback))) {
      // Post a task to check the responsiveness of watched thread.
      MessageLoop::current()->PostDelayedTask(
          FROM_HERE,
          base::Bind(&ThreadWatcher::OnCheckResponsiveness,
                     weak_ptr_factory_.GetWeakPtr(), ping_sequence_number_),
          unresponsive_time_.InMilliseconds());
  } else {
    // Watched thread might have gone away, stop watching it.
    DeActivateThreadWatching();
  }
}

void ThreadWatcher::OnPongMessage(uint64 ping_sequence_number) {
  DCHECK(WatchDogThread::CurrentlyOnWatchDogThread());

  // Record watched thread's response time.
  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta response_time = now - ping_time_;
  response_time_histogram_->AddTime(response_time);

  // Save the current time when we have got pong message.
  pong_time_ = now;

  // Check if there are any extra pings in flight.
  DCHECK_EQ(ping_sequence_number_, ping_sequence_number);
  if (ping_sequence_number_ != ping_sequence_number)
    return;

  // Increment sequence number for the next ping message to indicate watched
  // thread is responsive.
  ++ping_sequence_number_;

  // If we have stopped watching or if the user is idle, then stop sending
  // ping messages.
  if (!active_ || --ping_count_ <= 0)
    return;

  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&ThreadWatcher::PostPingMessage,
                 weak_ptr_factory_.GetWeakPtr()),
      sleep_time_.InMilliseconds());
}

void ThreadWatcher::OnCheckResponsiveness(uint64 ping_sequence_number) {
  DCHECK(WatchDogThread::CurrentlyOnWatchDogThread());
  // If we have stopped watching then consider thread as responding.
  if (!active_) {
    responsive_ = true;
    return;
  }
  // If the latest ping_sequence_number_ is not same as the ping_sequence_number
  // that is passed in, then we can assume OnPongMessage was called.
  // OnPongMessage increments ping_sequence_number_.
  if (ping_sequence_number_ != ping_sequence_number) {
    // Reset unresponsive_count_ to zero because we got a response from the
    // watched thread.
    ResetHangCounters();

    responsive_ = true;
    return;
  }
  // Record that we got no response from watched thread.
  GotNoResponse();

  // Post a task to check the responsiveness of watched thread.
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&ThreadWatcher::OnCheckResponsiveness,
                 weak_ptr_factory_.GetWeakPtr(), ping_sequence_number_),
      unresponsive_time_.InMilliseconds());
  responsive_ = false;
}

void ThreadWatcher::Initialize() {
  DCHECK(WatchDogThread::CurrentlyOnWatchDogThread());
  ThreadWatcherList::Register(this);

  const std::string response_time_histogram_name =
      "ThreadWatcher.ResponseTime." + thread_name_;
  response_time_histogram_ = base::Histogram::FactoryTimeGet(
      response_time_histogram_name,
      base::TimeDelta::FromMilliseconds(1),
      base::TimeDelta::FromSeconds(100), 50,
      base::Histogram::kUmaTargetedHistogramFlag);

  const std::string unresponsive_time_histogram_name =
      "ThreadWatcher.Unresponsive." + thread_name_;
  unresponsive_time_histogram_ = base::Histogram::FactoryTimeGet(
      unresponsive_time_histogram_name,
      base::TimeDelta::FromMilliseconds(1),
      base::TimeDelta::FromSeconds(100), 50,
      base::Histogram::kUmaTargetedHistogramFlag);

  const std::string responsive_count_histogram_name =
      "ThreadWatcher.ResponsiveThreads." + thread_name_;
  responsive_count_histogram_ = base::LinearHistogram::FactoryGet(
      responsive_count_histogram_name, 1, 10, 11,
      base::Histogram::kUmaTargetedHistogramFlag);

  const std::string unresponsive_count_histogram_name =
      "ThreadWatcher.UnresponsiveThreads." + thread_name_;
  unresponsive_count_histogram_ = base::LinearHistogram::FactoryGet(
      unresponsive_count_histogram_name, 1, 10, 11,
      base::Histogram::kUmaTargetedHistogramFlag);
}

// static
void ThreadWatcher::OnPingMessage(const BrowserThread::ID& thread_id,
                                  const base::Closure& callback_task) {
  // This method is called on watched thread.
  DCHECK(BrowserThread::CurrentlyOn(thread_id));
  WatchDogThread::PostTask(FROM_HERE, callback_task);
}

void ThreadWatcher::ResetHangCounters() {
  DCHECK(WatchDogThread::CurrentlyOnWatchDogThread());
  unresponsive_count_ = 0;
  hung_processing_complete_ = false;
}

void ThreadWatcher::GotNoResponse() {
  DCHECK(WatchDogThread::CurrentlyOnWatchDogThread());

  ++unresponsive_count_;
  if (!IsVeryUnresponsive())
    return;

  // Record total unresponsive_time since last pong message.
  base::TimeDelta unresponse_time = base::TimeTicks::Now() - pong_time_;
  unresponsive_time_histogram_->AddTime(unresponse_time);

  // We have already collected stats for the non-responding watched thread.
  if (hung_processing_complete_)
    return;

  // Record how other threads are responding.
  uint32 responding_thread_count = 0;
  uint32 unresponding_thread_count = 0;
  ThreadWatcherList::GetStatusOfThreads(&responding_thread_count,
                                        &unresponding_thread_count);

  // Record how many watched threads are responding.
  responsive_count_histogram_->Add(responding_thread_count);

  // Record how many watched threads are not responding.
  unresponsive_count_histogram_->Add(unresponding_thread_count);

  // Crash the browser if the watched thread is to be crashed on hang and if the
  // number of other threads responding is equal to live_threads_threshold_.
  if (crash_on_hang_ && responding_thread_count == live_threads_threshold_) {
    static bool crashed_once = false;
    if (!crashed_once) {
      crashed_once = true;
      CrashBecauseThreadWasUnresponsive(thread_id_);
    }
  }

  hung_processing_complete_ = true;
}

bool ThreadWatcher::IsVeryUnresponsive() {
  DCHECK(WatchDogThread::CurrentlyOnWatchDogThread());
  return unresponsive_count_ >= unresponsive_threshold_;
}

// ThreadWatcherList methods and members.
//
// static
ThreadWatcherList* ThreadWatcherList::g_thread_watcher_list_ = NULL;
// static
const int ThreadWatcherList::kSleepSeconds = 1;
// static
const int ThreadWatcherList::kUnresponsiveSeconds = 2;
// static
const int ThreadWatcherList::kUnresponsiveCount = 9;
// static
const int ThreadWatcherList::kLiveThreadsThreshold = 1;

// static
void ThreadWatcherList::StartWatchingAll(const CommandLine& command_line) {
  uint32 unresponsive_threshold;
  std::set<std::string> crash_on_hang_thread_names;
  uint32 live_threads_threshold;
  ParseCommandLine(command_line,
                   &unresponsive_threshold,
                   &crash_on_hang_thread_names,
                   &live_threads_threshold);

  ThreadWatcherObserver::SetupNotifications(
      base::TimeDelta::FromSeconds(kSleepSeconds * unresponsive_threshold));

  WatchDogThread::PostDelayedTask(
      FROM_HERE,
      base::Bind(&ThreadWatcherList::InitializeAndStartWatching,
                 unresponsive_threshold,
                 crash_on_hang_thread_names,
                 live_threads_threshold),
      base::TimeDelta::FromSeconds(120).InMilliseconds());
}

// static
void ThreadWatcherList::StopWatchingAll() {
  ThreadWatcherObserver::RemoveNotifications();
  DeleteAll();
}

// static
void ThreadWatcherList::Register(ThreadWatcher* watcher) {
  DCHECK(WatchDogThread::CurrentlyOnWatchDogThread());
  if (!g_thread_watcher_list_)
    return;
  DCHECK(!g_thread_watcher_list_->Find(watcher->thread_id()));
  g_thread_watcher_list_->registered_[watcher->thread_id()] = watcher;
}

// static
bool ThreadWatcherList::IsRegistered(const BrowserThread::ID thread_id) {
  DCHECK(WatchDogThread::CurrentlyOnWatchDogThread());
  return NULL != ThreadWatcherList::Find(thread_id);
}

// static
void ThreadWatcherList::GetStatusOfThreads(uint32* responding_thread_count,
                                           uint32* unresponding_thread_count) {
  DCHECK(WatchDogThread::CurrentlyOnWatchDogThread());
  *responding_thread_count = 0;
  *unresponding_thread_count = 0;
  if (!g_thread_watcher_list_)
    return;

  for (RegistrationList::iterator it =
           g_thread_watcher_list_->registered_.begin();
       g_thread_watcher_list_->registered_.end() != it;
       ++it) {
    if (it->second->IsVeryUnresponsive())
      ++(*unresponding_thread_count);
    else
      ++(*responding_thread_count);
  }
}

// static
void ThreadWatcherList::WakeUpAll() {
  DCHECK(WatchDogThread::CurrentlyOnWatchDogThread());
  if (!g_thread_watcher_list_)
    return;

  for (RegistrationList::iterator it =
           g_thread_watcher_list_->registered_.begin();
       g_thread_watcher_list_->registered_.end() != it;
       ++it)
    it->second->WakeUp();
}

ThreadWatcherList::ThreadWatcherList() {
  DCHECK(WatchDogThread::CurrentlyOnWatchDogThread());
  CHECK(!g_thread_watcher_list_);
  g_thread_watcher_list_ = this;
}

ThreadWatcherList::~ThreadWatcherList() {
  DCHECK(WatchDogThread::CurrentlyOnWatchDogThread());
  DCHECK(this == g_thread_watcher_list_);
  g_thread_watcher_list_ = NULL;
}

// static
void ThreadWatcherList::ParseCommandLine(
    const CommandLine& command_line,
    uint32* unresponsive_threshold,
    std::set<std::string>* crash_on_hang_thread_names,
    uint32* live_threads_threshold) {
  // Determine |unresponsive_threshold| based on switches::kCrashOnHangSeconds.
  *unresponsive_threshold = kUnresponsiveCount;

  // Increase the unresponsive_threshold on the Stable and Beta channels to
  // reduce the number of crashes due to ThreadWatcher.
  chrome::VersionInfo::Channel channel = chrome::VersionInfo::GetChannel();
  if (channel == chrome::VersionInfo::CHANNEL_STABLE) {
    *unresponsive_threshold *= 4;
  } else if (channel == chrome::VersionInfo::CHANNEL_BETA) {
    *unresponsive_threshold *= 2;
  }

#if defined(OS_WIN)
  // For Windows XP (old systems), double the unresponsive_threshold to give
  // the OS a chance to schedule UI/IO threads a time slice to respond with a
  // pong message (to get around limitations with the OS).
  if (base::win::GetVersion() <= base::win::VERSION_XP)
    *unresponsive_threshold *= 2;
#endif

  std::string crash_on_hang_seconds =
      command_line.GetSwitchValueASCII(switches::kCrashOnHangSeconds);
  if (!crash_on_hang_seconds.empty()) {
    int crash_seconds = atoi(crash_on_hang_seconds.c_str());
    if (crash_seconds > 0) {
      *unresponsive_threshold = static_cast<uint32>(
          ceil(static_cast<float>(crash_seconds) / kUnresponsiveSeconds));
    }
  }

  std::string crash_on_hang_threads;

  // Default to crashing the browser if UI or IO threads are not responsive
  // except in stable channel.
  if (channel == chrome::VersionInfo::CHANNEL_STABLE)
    crash_on_hang_threads = "";
  else
    crash_on_hang_threads = "UI,IO";

  if (command_line.HasSwitch(switches::kCrashOnHangThreads)) {
    crash_on_hang_threads =
        command_line.GetSwitchValueASCII(switches::kCrashOnHangThreads);
  }
  StringTokenizer tokens(crash_on_hang_threads, ",");
  while (tokens.GetNext())
    crash_on_hang_thread_names->insert(tokens.token());

  // Determine |live_threads_threshold| based on switches::kCrashOnLive.
  *live_threads_threshold = kLiveThreadsThreshold;
  if (command_line.HasSwitch(switches::kCrashOnLive)) {
    std::string live_threads =
        command_line.GetSwitchValueASCII(switches::kCrashOnLive);
    *live_threads_threshold = static_cast<uint32>(atoi(live_threads.c_str()));
  }
}

// static
void ThreadWatcherList::InitializeAndStartWatching(
    uint32 unresponsive_threshold,
    const std::set<std::string>& crash_on_hang_thread_names,
    uint32 live_threads_threshold) {
  DCHECK(WatchDogThread::CurrentlyOnWatchDogThread());

  ThreadWatcherList* thread_watcher_list = new ThreadWatcherList();
  CHECK(thread_watcher_list);

  const base::TimeDelta kSleepTime =
      base::TimeDelta::FromSeconds(kSleepSeconds);
  const base::TimeDelta kUnresponsiveTime =
      base::TimeDelta::FromSeconds(kUnresponsiveSeconds);

  StartWatching(BrowserThread::UI, "UI", kSleepTime, kUnresponsiveTime,
                unresponsive_threshold, crash_on_hang_thread_names,
                live_threads_threshold);
  StartWatching(BrowserThread::IO, "IO", kSleepTime, kUnresponsiveTime,
                unresponsive_threshold, crash_on_hang_thread_names,
                live_threads_threshold);
  StartWatching(BrowserThread::DB, "DB", kSleepTime, kUnresponsiveTime,
                unresponsive_threshold, crash_on_hang_thread_names,
                live_threads_threshold);
  StartWatching(BrowserThread::FILE, "FILE", kSleepTime, kUnresponsiveTime,
                unresponsive_threshold, crash_on_hang_thread_names,
                live_threads_threshold);
  StartWatching(BrowserThread::CACHE, "CACHE", kSleepTime, kUnresponsiveTime,
                unresponsive_threshold, crash_on_hang_thread_names,
                live_threads_threshold);

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&StartupTimeBomb::DisarmStartupTimeBomb));
}

// static
void ThreadWatcherList::StartWatching(
    const BrowserThread::ID& thread_id,
    const std::string& thread_name,
    const base::TimeDelta& sleep_time,
    const base::TimeDelta& unresponsive_time,
    uint32 unresponsive_threshold,
    const std::set<std::string>& crash_on_hang_thread_names,
    uint32 live_threads_threshold) {
  DCHECK(WatchDogThread::CurrentlyOnWatchDogThread());

  std::set<std::string>::const_iterator it =
      crash_on_hang_thread_names.find(thread_name);
  bool crash_on_hang = (it != crash_on_hang_thread_names.end());

  ThreadWatcher::StartWatching(
      ThreadWatcher::WatchingParams(thread_id,
                                    thread_name,
                                    sleep_time,
                                    unresponsive_time,
                                    unresponsive_threshold,
                                    crash_on_hang,
                                    live_threads_threshold));
}

// static
void ThreadWatcherList::DeleteAll() {
  if (!WatchDogThread::CurrentlyOnWatchDogThread()) {
    WatchDogThread::PostTask(
        FROM_HERE,
        base::Bind(&ThreadWatcherList::DeleteAll));
    return;
  }

  DCHECK(WatchDogThread::CurrentlyOnWatchDogThread());
  if (!g_thread_watcher_list_)
    return;

  // Delete all thread watcher objects.
  while (!g_thread_watcher_list_->registered_.empty()) {
    RegistrationList::iterator it = g_thread_watcher_list_->registered_.begin();
    delete it->second;
    g_thread_watcher_list_->registered_.erase(it);
  }

  delete g_thread_watcher_list_;
}

// static
ThreadWatcher* ThreadWatcherList::Find(const BrowserThread::ID& thread_id) {
  DCHECK(WatchDogThread::CurrentlyOnWatchDogThread());
  if (!g_thread_watcher_list_)
    return NULL;
  RegistrationList::iterator it =
      g_thread_watcher_list_->registered_.find(thread_id);
  if (g_thread_watcher_list_->registered_.end() == it)
    return NULL;
  return it->second;
}

// ThreadWatcherObserver methods and members.
//
// static
ThreadWatcherObserver* ThreadWatcherObserver::g_thread_watcher_observer_ = NULL;

ThreadWatcherObserver::ThreadWatcherObserver(
    const base::TimeDelta& wakeup_interval)
    : last_wakeup_time_(base::TimeTicks::Now()),
      wakeup_interval_(wakeup_interval) {
  CHECK(!g_thread_watcher_observer_);
  g_thread_watcher_observer_ = this;
}

ThreadWatcherObserver::~ThreadWatcherObserver() {
  DCHECK(this == g_thread_watcher_observer_);
  g_thread_watcher_observer_ = NULL;
}

// static
void ThreadWatcherObserver::SetupNotifications(
    const base::TimeDelta& wakeup_interval) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ThreadWatcherObserver* observer = new ThreadWatcherObserver(wakeup_interval);
  MetricsService::SetUpNotifications(&observer->registrar_, observer);
}

// static
void ThreadWatcherObserver::RemoveNotifications() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!g_thread_watcher_observer_)
    return;
  g_thread_watcher_observer_->registrar_.RemoveAll();
  delete g_thread_watcher_observer_;
}

void ThreadWatcherObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  // There is some user activity, see if thread watchers are to be awakened.
  base::TimeTicks now = base::TimeTicks::Now();
  if ((now - last_wakeup_time_) < wakeup_interval_)
    return;
  last_wakeup_time_ = now;
  WatchDogThread::PostTask(
      FROM_HERE,
      base::Bind(&ThreadWatcherList::WakeUpAll));
}

// WatchDogThread methods and members.

// This lock protects g_watchdog_thread.
static base::LazyInstance<base::Lock,
                          base::LeakyLazyInstanceTraits<base::Lock> >
    g_watchdog_lock = LAZY_INSTANCE_INITIALIZER;

// The singleton of this class.
static WatchDogThread* g_watchdog_thread = NULL;


// The WatchDogThread object must outlive any tasks posted to the IO thread
// before the Quit task.
DISABLE_RUNNABLE_METHOD_REFCOUNT(WatchDogThread);

WatchDogThread::WatchDogThread() : Thread("BrowserWatchdog") {
}

WatchDogThread::~WatchDogThread() {
  Stop();
}

// static
bool WatchDogThread::CurrentlyOnWatchDogThread() {
  base::AutoLock lock(g_watchdog_lock.Get());
  return g_watchdog_thread &&
    g_watchdog_thread->message_loop() == MessageLoop::current();
}

// static
bool WatchDogThread::PostTask(const tracked_objects::Location& from_here,
                              const base::Closure& task) {
  return PostTaskHelper(from_here, task, 0);
}

// static
bool WatchDogThread::PostDelayedTask(const tracked_objects::Location& from_here,
                                     const base::Closure& task,
                                     int64 delay_ms) {
  return PostTaskHelper(from_here, task, delay_ms);
}

// static
bool WatchDogThread::PostTaskHelper(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    int64 delay_ms) {
  {
    base::AutoLock lock(g_watchdog_lock.Get());

    MessageLoop* message_loop = g_watchdog_thread ?
        g_watchdog_thread->message_loop() : NULL;
    if (message_loop) {
      message_loop->PostDelayedTask(from_here, task, delay_ms);
      return true;
    }
  }

  return false;
}

void WatchDogThread::Init() {
  // This thread shouldn't be allowed to perform any blocking disk I/O.
  base::ThreadRestrictions::SetIOAllowed(false);

  base::AutoLock lock(g_watchdog_lock.Get());
  CHECK(!g_watchdog_thread);
  g_watchdog_thread = this;
}

void WatchDogThread::CleanUp() {
  base::AutoLock lock(g_watchdog_lock.Get());
  g_watchdog_thread = NULL;
}

namespace {

// StartupWatchDogThread methods and members.
//
// Class for detecting hangs during startup.
class StartupWatchDogThread : public base::Watchdog {
 public:
  // Constructor specifies how long the StartupWatchDogThread will wait before
  // alarming.
  explicit StartupWatchDogThread(const base::TimeDelta& duration)
      : base::Watchdog(duration, "Startup watchdog thread", true) {
  }

  // Alarm is called if the time expires after an Arm() without someone calling
  // Disarm(). When Alarm goes off, in release mode we get the crash dump
  // without crashing and in debug mode we break into the debugger.
  virtual void Alarm() {
#ifndef NDEBUG
    DCHECK(false);
#else
    logging::DumpWithoutCrashing();
#endif
  }

  DISALLOW_COPY_AND_ASSIGN(StartupWatchDogThread);
};

// ShutdownWatchDogThread methods and members.
//
// Class for detecting hangs during shutdown.
class ShutdownWatchDogThread : public base::Watchdog {
 public:
  // Constructor specifies how long the ShutdownWatchDogThread will wait before
  // alarming.
  explicit ShutdownWatchDogThread(const base::TimeDelta& duration)
      : base::Watchdog(duration, "Shutdown watchdog thread", true) {
  }

  // Alarm is called if the time expires after an Arm() without someone calling
  // Disarm(). We crash the browser if this method is called.
  virtual void Alarm() {
    CHECK(false);
  }

  DISALLOW_COPY_AND_ASSIGN(ShutdownWatchDogThread);
};
}  // namespace

// StartupTimeBomb methods and members.
//
// static
StartupTimeBomb* StartupTimeBomb::g_startup_timebomb_ = NULL;

StartupTimeBomb::StartupTimeBomb()
    : startup_watchdog_(NULL),
      thread_id_(base::PlatformThread::CurrentId()) {
  CHECK(!g_startup_timebomb_);
  g_startup_timebomb_ = this;
}

StartupTimeBomb::~StartupTimeBomb() {
  DCHECK(this == g_startup_timebomb_);
  DCHECK_EQ(thread_id_, base::PlatformThread::CurrentId());
  if (startup_watchdog_)
    Disarm();
  g_startup_timebomb_ = NULL;
}

void StartupTimeBomb::Arm(const base::TimeDelta& duration) {
  DCHECK_EQ(thread_id_, base::PlatformThread::CurrentId());
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!startup_watchdog_);
  startup_watchdog_ = new StartupWatchDogThread(duration);
  startup_watchdog_->Arm();
}

void StartupTimeBomb::Disarm() {
  DCHECK_EQ(thread_id_, base::PlatformThread::CurrentId());
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (startup_watchdog_) {
    startup_watchdog_->Disarm();
    // Allow the watchdog thread to shutdown on UI. Watchdog thread shutdowns
    // very fast.
    base::ThreadRestrictions::SetIOAllowed(true);
    delete startup_watchdog_;
    startup_watchdog_ = NULL;
  }
}

// static
void StartupTimeBomb::DisarmStartupTimeBomb() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (g_startup_timebomb_)
    g_startup_timebomb_->Disarm();
}

// ShutdownWatcherHelper methods and members.
//
// ShutdownWatcherHelper is a wrapper class for detecting hangs during
// shutdown.
ShutdownWatcherHelper::ShutdownWatcherHelper()
    : shutdown_watchdog_(NULL),
      thread_id_(base::PlatformThread::CurrentId()) {
}

ShutdownWatcherHelper::~ShutdownWatcherHelper() {
  DCHECK_EQ(thread_id_, base::PlatformThread::CurrentId());
  if (shutdown_watchdog_) {
    shutdown_watchdog_->Disarm();
    delete shutdown_watchdog_;
    shutdown_watchdog_ = NULL;
  }
}

void ShutdownWatcherHelper::Arm(const base::TimeDelta& duration) {
  DCHECK_EQ(thread_id_, base::PlatformThread::CurrentId());
  DCHECK(!shutdown_watchdog_);
  base::TimeDelta actual_duration = duration;

  chrome::VersionInfo::Channel channel = chrome::VersionInfo::GetChannel();
  if (channel == chrome::VersionInfo::CHANNEL_STABLE) {
    actual_duration *= 50;
  } else if (channel == chrome::VersionInfo::CHANNEL_BETA ||
             channel == chrome::VersionInfo::CHANNEL_DEV) {
    actual_duration *= 25;
  }

#if defined(OS_WIN)
  // On Windows XP, give twice the time for shutdown.
  if (base::win::GetVersion() <= base::win::VERSION_XP)
    actual_duration *= 2;
#endif

  shutdown_watchdog_ = new ShutdownWatchDogThread(actual_duration);
  shutdown_watchdog_->Arm();
}
