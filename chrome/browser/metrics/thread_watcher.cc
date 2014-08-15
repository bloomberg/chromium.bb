// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/thread_watcher.h"

#include <math.h>  // ceil

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "base/debug/debugger.h"
#include "base/debug/dump_without_crashing.h"
#include "base/lazy_instance.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/logging_chrome.h"
#include "content/public/browser/notification_service.h"

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
MSVC_DISABLE_OPTIMIZE()
MSVC_PUSH_DISABLE_WARNING(4748)

void ReportThreadHang() {
#if defined(NDEBUG)
  base::debug::DumpWithoutCrashing();
#else
  base::debug::BreakDebugger();
#endif
}

#if !defined(OS_ANDROID) || !defined(NDEBUG)
// TODO(rtenneti): Enabled crashing, after getting data.
NOINLINE void StartupHang() {
  ReportThreadHang();
}
#endif  // OS_ANDROID

NOINLINE void ShutdownHang() {
  ReportThreadHang();
}

NOINLINE void ThreadUnresponsive_UI() {
  ReportThreadHang();
}

NOINLINE void ThreadUnresponsive_DB() {
  ReportThreadHang();
}

NOINLINE void ThreadUnresponsive_FILE() {
  ReportThreadHang();
}

NOINLINE void ThreadUnresponsive_FILE_USER_BLOCKING() {
  ReportThreadHang();
}

NOINLINE void ThreadUnresponsive_PROCESS_LAUNCHER() {
  ReportThreadHang();
}

NOINLINE void ThreadUnresponsive_CACHE() {
  ReportThreadHang();
}

NOINLINE void ThreadUnresponsive_IO() {
  ReportThreadHang();
}

MSVC_POP_WARNING()
MSVC_ENABLE_OPTIMIZE();

void CrashBecauseThreadWasUnresponsive(BrowserThread::ID thread_id) {
  base::debug::Alias(&thread_id);

  switch (thread_id) {
    case BrowserThread::UI:
      return ThreadUnresponsive_UI();
    case BrowserThread::DB:
      return ThreadUnresponsive_DB();
    case BrowserThread::FILE:
      return ThreadUnresponsive_FILE();
    case BrowserThread::FILE_USER_BLOCKING:
      return ThreadUnresponsive_FILE_USER_BLOCKING();
    case BrowserThread::PROCESS_LAUNCHER:
      return ThreadUnresponsive_PROCESS_LAUNCHER();
    case BrowserThread::CACHE:
      return ThreadUnresponsive_CACHE();
    case BrowserThread::IO:
      return ThreadUnresponsive_IO();
    case BrowserThread::ID_COUNT:
      CHECK(false);  // This shouldn't actually be reached!
      break;

    // Omission of the default hander is intentional -- that way the compiler
    // should warn if our switch becomes outdated.
  }

  CHECK(false) << "Unknown thread was unresponsive.";  // Shouldn't be reached.
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
      weak_ptr_factory_(this) {
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
  base::MessageLoop::current()->PostTask(
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
      base::MessageLoop::current()->PostDelayedTask(
          FROM_HERE,
          base::Bind(&ThreadWatcher::OnCheckResponsiveness,
                     weak_ptr_factory_.GetWeakPtr(), ping_sequence_number_),
          unresponsive_time_);
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

  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&ThreadWatcher::PostPingMessage,
                 weak_ptr_factory_.GetWeakPtr()),
      sleep_time_);
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
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&ThreadWatcher::OnCheckResponsiveness,
                 weak_ptr_factory_.GetWeakPtr(), ping_sequence_number_),
      unresponsive_time_);
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
  // number of other threads responding is less than or equal to
  // live_threads_threshold_ and at least one other thread is responding.
  if (crash_on_hang_ &&
      responding_thread_count > 0 &&
      responding_thread_count <= live_threads_threshold_) {
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
bool ThreadWatcherList::g_stopped_ = false;
// static
const int ThreadWatcherList::kSleepSeconds = 1;
// static
const int ThreadWatcherList::kUnresponsiveSeconds = 2;
// static
const int ThreadWatcherList::kUnresponsiveCount = 9;
// static
const int ThreadWatcherList::kLiveThreadsThreshold = 2;
// static, non-const for tests.
int ThreadWatcherList::g_initialize_delay_seconds = 120;

ThreadWatcherList::CrashDataThresholds::CrashDataThresholds(
    uint32 live_threads_threshold,
    uint32 unresponsive_threshold)
    : live_threads_threshold(live_threads_threshold),
      unresponsive_threshold(unresponsive_threshold) {
}

ThreadWatcherList::CrashDataThresholds::CrashDataThresholds()
    : live_threads_threshold(kLiveThreadsThreshold),
      unresponsive_threshold(kUnresponsiveCount) {
}

// static
void ThreadWatcherList::StartWatchingAll(const CommandLine& command_line) {
  // TODO(rtenneti): Enable ThreadWatcher.
  uint32 unresponsive_threshold;
  CrashOnHangThreadMap crash_on_hang_threads;
  ParseCommandLine(command_line,
                   &unresponsive_threshold,
                   &crash_on_hang_threads);

  ThreadWatcherObserver::SetupNotifications(
      base::TimeDelta::FromSeconds(kSleepSeconds * unresponsive_threshold));

  WatchDogThread::PostTask(
      FROM_HERE,
      base::Bind(&ThreadWatcherList::SetStopped, false));

  WatchDogThread::PostDelayedTask(
      FROM_HERE,
      base::Bind(&ThreadWatcherList::InitializeAndStartWatching,
                 unresponsive_threshold,
                 crash_on_hang_threads),
      base::TimeDelta::FromSeconds(g_initialize_delay_seconds));
}

// static
void ThreadWatcherList::StopWatchingAll() {
  // TODO(rtenneti): Enable ThreadWatcher.
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
    CrashOnHangThreadMap* crash_on_hang_threads) {
  // Initialize |unresponsive_threshold| to a default value.
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

  uint32 crash_seconds = *unresponsive_threshold * kUnresponsiveSeconds;
  std::string crash_on_hang_thread_names;
  bool has_command_line_overwrite = false;
  if (command_line.HasSwitch(switches::kCrashOnHangThreads)) {
    crash_on_hang_thread_names =
        command_line.GetSwitchValueASCII(switches::kCrashOnHangThreads);
    has_command_line_overwrite = true;
  } else if (channel != chrome::VersionInfo::CHANNEL_STABLE) {
    // Default to crashing the browser if UI or IO or FILE threads are not
    // responsive except in stable channel.
    crash_on_hang_thread_names = base::StringPrintf(
        "UI:%d:%d,IO:%d:%d,FILE:%d:%d",
        kLiveThreadsThreshold, crash_seconds,
        kLiveThreadsThreshold, crash_seconds,
        kLiveThreadsThreshold, crash_seconds * 5);
  }

  ParseCommandLineCrashOnHangThreads(crash_on_hang_thread_names,
                                     kLiveThreadsThreshold,
                                     crash_seconds,
                                     crash_on_hang_threads);

  if (channel != chrome::VersionInfo::CHANNEL_CANARY ||
      has_command_line_overwrite) {
    return;
  }

  const char* kFieldTrialName = "ThreadWatcher";

  // Nothing else to be done if the trial has already been set (i.e., when
  // StartWatchingAll() has been already called once).
  if (base::FieldTrialList::TrialExists(kFieldTrialName))
    return;

  // Set up a field trial for 100% of the users to crash if either UI or IO
  // thread is not responsive for 30 seconds (or 15 pings).
  scoped_refptr<base::FieldTrial> field_trial(
      base::FieldTrialList::FactoryGetFieldTrial(
          kFieldTrialName, 100, "default_hung_threads",
          2014, 10, 30, base::FieldTrial::SESSION_RANDOMIZED, NULL));
  int hung_thread_group = field_trial->AppendGroup("hung_thread", 100);
  if (field_trial->group() == hung_thread_group) {
    for (CrashOnHangThreadMap::iterator it = crash_on_hang_threads->begin();
         crash_on_hang_threads->end() != it;
         ++it) {
      if (it->first == "FILE")
        continue;
      it->second.live_threads_threshold = INT_MAX;
      if (it->first == "UI") {
        // TODO(rtenneti): set unresponsive threshold to 120 seconds to catch
        // the worst UI hangs and for fewer crashes due to ThreadWatcher. Reduce
        // it to a more reasonable time ala IO thread.
        it->second.unresponsive_threshold = 60;
      } else {
        it->second.unresponsive_threshold = 15;
      }
    }
  }
}

// static
void ThreadWatcherList::ParseCommandLineCrashOnHangThreads(
    const std::string& crash_on_hang_thread_names,
    uint32 default_live_threads_threshold,
    uint32 default_crash_seconds,
    CrashOnHangThreadMap* crash_on_hang_threads) {
  base::StringTokenizer tokens(crash_on_hang_thread_names, ",");
  std::vector<std::string> values;
  while (tokens.GetNext()) {
    const std::string& token = tokens.token();
    base::SplitString(token, ':', &values);
    std::string thread_name = values[0];

    uint32 live_threads_threshold = default_live_threads_threshold;
    uint32 crash_seconds = default_crash_seconds;
    if (values.size() >= 2 &&
        (!base::StringToUint(values[1], &live_threads_threshold))) {
      continue;
    }
    if (values.size() >= 3 &&
        (!base::StringToUint(values[2], &crash_seconds))) {
      continue;
    }
    uint32 unresponsive_threshold = static_cast<uint32>(
        ceil(static_cast<float>(crash_seconds) / kUnresponsiveSeconds));

    CrashDataThresholds crash_data(live_threads_threshold,
                                   unresponsive_threshold);
    // Use the last specifier.
    (*crash_on_hang_threads)[thread_name] = crash_data;
  }
}

// static
void ThreadWatcherList::InitializeAndStartWatching(
    uint32 unresponsive_threshold,
    const CrashOnHangThreadMap& crash_on_hang_threads) {
  DCHECK(WatchDogThread::CurrentlyOnWatchDogThread());

  // Disarm the startup timebomb, even if stop has been called.
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&StartupTimeBomb::DisarmStartupTimeBomb));

  // This method is deferred in relationship to its StopWatchingAll()
  // counterpart. If a previous initialization has already happened, or if
  // stop has been called, there's nothing left to do here.
  if (g_thread_watcher_list_ || g_stopped_)
    return;

  ThreadWatcherList* thread_watcher_list = new ThreadWatcherList();
  CHECK(thread_watcher_list);

  const base::TimeDelta kSleepTime =
      base::TimeDelta::FromSeconds(kSleepSeconds);
  const base::TimeDelta kUnresponsiveTime =
      base::TimeDelta::FromSeconds(kUnresponsiveSeconds);

  StartWatching(BrowserThread::UI, "UI", kSleepTime, kUnresponsiveTime,
                unresponsive_threshold, crash_on_hang_threads);
  StartWatching(BrowserThread::IO, "IO", kSleepTime, kUnresponsiveTime,
                unresponsive_threshold, crash_on_hang_threads);
  StartWatching(BrowserThread::DB, "DB", kSleepTime, kUnresponsiveTime,
                unresponsive_threshold, crash_on_hang_threads);
  StartWatching(BrowserThread::FILE, "FILE", kSleepTime, kUnresponsiveTime,
                unresponsive_threshold, crash_on_hang_threads);
  StartWatching(BrowserThread::CACHE, "CACHE", kSleepTime, kUnresponsiveTime,
                unresponsive_threshold, crash_on_hang_threads);
}

// static
void ThreadWatcherList::StartWatching(
    const BrowserThread::ID& thread_id,
    const std::string& thread_name,
    const base::TimeDelta& sleep_time,
    const base::TimeDelta& unresponsive_time,
    uint32 unresponsive_threshold,
    const CrashOnHangThreadMap& crash_on_hang_threads) {
  DCHECK(WatchDogThread::CurrentlyOnWatchDogThread());

  CrashOnHangThreadMap::const_iterator it =
      crash_on_hang_threads.find(thread_name);
  bool crash_on_hang = false;
  uint32 live_threads_threshold = 0;
  if (it != crash_on_hang_threads.end()) {
    crash_on_hang = true;
    live_threads_threshold = it->second.live_threads_threshold;
    unresponsive_threshold = it->second.unresponsive_threshold;
  }

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

  SetStopped(true);

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

// static
void ThreadWatcherList::SetStopped(bool stopped) {
  DCHECK(WatchDogThread::CurrentlyOnWatchDogThread());
  g_stopped_ = stopped;
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
  observer->registrar_.Add(
      observer,
      chrome::NOTIFICATION_BROWSER_OPENED,
      content::NotificationService::AllBrowserContextsAndSources());
  observer->registrar_.Add(observer,
                           chrome::NOTIFICATION_BROWSER_CLOSED,
                           content::NotificationService::AllSources());
  observer->registrar_.Add(observer,
                           chrome::NOTIFICATION_TAB_PARENTED,
                           content::NotificationService::AllSources());
  observer->registrar_.Add(observer,
                           chrome::NOTIFICATION_TAB_CLOSING,
                           content::NotificationService::AllSources());
  observer->registrar_.Add(observer,
                           content::NOTIFICATION_LOAD_START,
                           content::NotificationService::AllSources());
  observer->registrar_.Add(observer,
                           content::NOTIFICATION_LOAD_STOP,
                           content::NotificationService::AllSources());
  observer->registrar_.Add(observer,
                           content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                           content::NotificationService::AllSources());
  observer->registrar_.Add(observer,
                           content::NOTIFICATION_RENDER_WIDGET_HOST_HANG,
                           content::NotificationService::AllSources());
  observer->registrar_.Add(observer,
                           chrome::NOTIFICATION_OMNIBOX_OPENED_URL,
                           content::NotificationService::AllSources());
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
static base::LazyInstance<base::Lock>::Leaky
    g_watchdog_lock = LAZY_INSTANCE_INITIALIZER;

// The singleton of this class.
static WatchDogThread* g_watchdog_thread = NULL;

WatchDogThread::WatchDogThread() : Thread("BrowserWatchdog") {
}

WatchDogThread::~WatchDogThread() {
  Stop();
}

// static
bool WatchDogThread::CurrentlyOnWatchDogThread() {
  base::AutoLock lock(g_watchdog_lock.Get());
  return g_watchdog_thread &&
      g_watchdog_thread->message_loop() == base::MessageLoop::current();
}

// static
bool WatchDogThread::PostTask(const tracked_objects::Location& from_here,
                              const base::Closure& task) {
  return PostTaskHelper(from_here, task, base::TimeDelta());
}

// static
bool WatchDogThread::PostDelayedTask(const tracked_objects::Location& from_here,
                                     const base::Closure& task,
                                     base::TimeDelta delay) {
  return PostTaskHelper(from_here, task, delay);
}

// static
bool WatchDogThread::PostTaskHelper(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  {
    base::AutoLock lock(g_watchdog_lock.Get());

    base::MessageLoop* message_loop = g_watchdog_thread ?
        g_watchdog_thread->message_loop() : NULL;
    if (message_loop) {
      message_loop->PostDelayedTask(from_here, task, delay);
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
#if defined(OS_ANDROID)
    // TODO(rtenneti): Delete this code, after getting data.
    start_time_clock_= base::Time::Now();
    start_time_monotonic_ = base::TimeTicks::Now();
    start_time_thread_now_ = base::TimeTicks::IsThreadNowSupported()
        ? base::TimeTicks::ThreadNow() : base::TimeTicks::Now();
#endif  // OS_ANDROID
  }

  // Alarm is called if the time expires after an Arm() without someone calling
  // Disarm(). When Alarm goes off, in release mode we get the crash dump
  // without crashing and in debug mode we break into the debugger.
  virtual void Alarm() OVERRIDE {
#if !defined(NDEBUG)
    StartupHang();
    return;
#elif !defined(OS_ANDROID)
    WatchDogThread::PostTask(FROM_HERE, base::Bind(&StartupHang));
    return;
#else  // Android release: gather stats to figure out when to crash.
    // TODO(rtenneti): Delete this code, after getting data.
    UMA_HISTOGRAM_TIMES("StartupTimeBomb.Alarm.TimeDuration",
                        base::Time::Now() - start_time_clock_);
    UMA_HISTOGRAM_TIMES("StartupTimeBomb.Alarm.TimeTicksDuration",
                        base::TimeTicks::Now() - start_time_monotonic_);
    if (base::TimeTicks::IsThreadNowSupported()) {
      UMA_HISTOGRAM_TIMES(
          "StartupTimeBomb.Alarm.ThreadNowDuration",
          base::TimeTicks::ThreadNow() - start_time_thread_now_);
    }
    return;
#endif  // OS_ANDROID
  }

 private:
#if defined(OS_ANDROID)
  // TODO(rtenneti): Delete this code, after getting data.
  base::Time start_time_clock_;
  base::TimeTicks start_time_monotonic_;
  base::TimeTicks start_time_thread_now_;
#endif  // OS_ANDROID

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
  virtual void Alarm() OVERRIDE {
    ShutdownHang();
  }

 private:
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
  DCHECK(!startup_watchdog_);
  startup_watchdog_ = new StartupWatchDogThread(duration);
  startup_watchdog_->Arm();
  return;
}

void StartupTimeBomb::Disarm() {
  DCHECK_EQ(thread_id_, base::PlatformThread::CurrentId());
  if (startup_watchdog_) {
    startup_watchdog_->Disarm();
    startup_watchdog_->Cleanup();
    DeleteStartupWatchdog();
  }
}

void StartupTimeBomb::DeleteStartupWatchdog() {
  DCHECK_EQ(thread_id_, base::PlatformThread::CurrentId());
  if (startup_watchdog_->IsJoinable()) {
    // Allow the watchdog thread to shutdown on UI. Watchdog thread shutdowns
    // very fast.
    base::ThreadRestrictions::SetIOAllowed(true);
    delete startup_watchdog_;
    startup_watchdog_ = NULL;
    return;
  }
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&StartupTimeBomb::DeleteStartupWatchdog,
                 base::Unretained(this)),
      base::TimeDelta::FromSeconds(10));
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
    actual_duration *= 20;
  } else if (channel == chrome::VersionInfo::CHANNEL_BETA ||
             channel == chrome::VersionInfo::CHANNEL_DEV) {
    actual_duration *= 10;
  }

#if defined(OS_WIN)
  // On Windows XP, give twice the time for shutdown.
  if (base::win::GetVersion() <= base::win::VERSION_XP)
    actual_duration *= 2;
#endif

  shutdown_watchdog_ = new ShutdownWatchDogThread(actual_duration);
  shutdown_watchdog_->Arm();
}
