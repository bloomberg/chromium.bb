// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/thread_watcher.h"

#include <math.h>  // ceil

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/debug/dump_without_crashing.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/stringprintf.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/metrics/thread_watcher_report_hang.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/logging_chrome.h"
#include "components/metrics/call_stack_profile_metrics_provider.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/notification_service.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

using content::BrowserThread;

namespace {

base::StackSamplingProfiler::SamplingParams GetJankTimeBombSamplingParams() {
  base::StackSamplingProfiler::SamplingParams params;
  params.initial_delay = base::TimeDelta::FromMilliseconds(0);
  params.bursts = 1;
  // 5 seconds at 10Hz.
  params.samples_per_burst = 50;
  params.sampling_interval = base::TimeDelta::FromMilliseconds(100);
  return params;
}

}  // namespace

// ThreadWatcher methods and members.
ThreadWatcher::ThreadWatcher(const WatchingParams& params)
    : thread_id_(params.thread_id),
      thread_name_(params.thread_name),
      watched_runner_(BrowserThread::GetTaskRunnerForThread(params.thread_id)),
      sleep_time_(params.sleep_time),
      unresponsive_time_(params.unresponsive_time),
      ping_time_(base::TimeTicks::Now()),
      pong_time_(ping_time_),
      ping_sequence_number_(0),
      active_(false),
      ping_count_(params.unresponsive_threshold),
      response_time_histogram_(nullptr),
      unresponsive_time_histogram_(nullptr),
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
  std::unique_ptr<ThreadWatcher> watcher(new ThreadWatcher(params));

  // If we couldn't register the thread watcher object, we are shutting down,
  // so don't activate thread watching.
  ThreadWatcher* registered_watcher =
      ThreadWatcherList::Register(std::move(watcher));
  if (registered_watcher != nullptr)
    registered_watcher->ActivateThreadWatching();
}

void ThreadWatcher::ActivateThreadWatching() {
  DCHECK(WatchDogThread::CurrentlyOnWatchDogThread());
  if (active_) return;
  active_ = true;
  ping_count_ = unresponsive_threshold_;
  ResetHangCounters();
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&ThreadWatcher::PostPingMessage,
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
  if (watched_runner_->PostTask(
          FROM_HERE,
          base::Bind(&ThreadWatcher::OnPingMessage, thread_id_,
                     callback))) {
      // Post a task to check the responsiveness of watched thread.
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::Bind(&ThreadWatcher::OnCheckResponsiveness,
                     weak_ptr_factory_.GetWeakPtr(), ping_sequence_number_),
          unresponsive_time_);
  } else {
    // Watched thread might have gone away, stop watching it.
    DeActivateThreadWatching();
  }
}

void ThreadWatcher::OnPongMessage(uint64_t ping_sequence_number) {
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

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::Bind(&ThreadWatcher::PostPingMessage,
                            weak_ptr_factory_.GetWeakPtr()),
      sleep_time_);
}

void ThreadWatcher::OnCheckResponsiveness(uint64_t ping_sequence_number) {
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
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&ThreadWatcher::OnCheckResponsiveness,
                 weak_ptr_factory_.GetWeakPtr(), ping_sequence_number_),
      unresponsive_time_);
  responsive_ = false;
}

void ThreadWatcher::Initialize() {
  DCHECK(WatchDogThread::CurrentlyOnWatchDogThread());

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
  uint32_t responding_thread_count = 0;
  uint32_t unresponding_thread_count = 0;
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
      metrics::CrashBecauseThreadWasUnresponsive(thread_id_);
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
ThreadWatcherList* ThreadWatcherList::g_thread_watcher_list_ = nullptr;
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
    uint32_t live_threads_threshold,
    uint32_t unresponsive_threshold)
    : live_threads_threshold(live_threads_threshold),
      unresponsive_threshold(unresponsive_threshold) {}

ThreadWatcherList::CrashDataThresholds::CrashDataThresholds()
    : live_threads_threshold(kLiveThreadsThreshold),
      unresponsive_threshold(kUnresponsiveCount) {
}

// static
void ThreadWatcherList::StartWatchingAll(
    const base::CommandLine& command_line) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  uint32_t unresponsive_threshold;
  CrashOnHangThreadMap crash_on_hang_threads;
  ParseCommandLine(command_line,
                   &unresponsive_threshold,
                   &crash_on_hang_threads);

  ThreadWatcherObserver::SetupNotifications(
      base::TimeDelta::FromSeconds(kSleepSeconds * unresponsive_threshold));

  WatchDogThread::PostTask(
      FROM_HERE,
      base::Bind(&ThreadWatcherList::SetStopped, false));

  if (!WatchDogThread::PostDelayedTask(
          FROM_HERE,
          base::Bind(&ThreadWatcherList::InitializeAndStartWatching,
                     unresponsive_threshold,
                     crash_on_hang_threads),
          base::TimeDelta::FromSeconds(g_initialize_delay_seconds))) {
    // Disarm() the startup timebomb, if we couldn't post the task to start the
    // ThreadWatcher (becasue WatchDog thread is not running).
    StartupTimeBomb::DisarmStartupTimeBomb();
  }
}

// static
void ThreadWatcherList::StopWatchingAll() {
  // TODO(rtenneti): Enable ThreadWatcher.
  ThreadWatcherObserver::RemoveNotifications();
  DeleteAll();
}

// static
ThreadWatcher* ThreadWatcherList::Register(
    std::unique_ptr<ThreadWatcher> watcher) {
  DCHECK(WatchDogThread::CurrentlyOnWatchDogThread());
  if (!g_thread_watcher_list_)
    return nullptr;
  content::BrowserThread::ID thread_id = watcher->thread_id();
  DCHECK(g_thread_watcher_list_->registered_.count(thread_id) == 0);
  return g_thread_watcher_list_->registered_[thread_id] = watcher.release();
}

// static
void ThreadWatcherList::GetStatusOfThreads(
    uint32_t* responding_thread_count,
    uint32_t* unresponding_thread_count) {
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
  g_thread_watcher_list_ = nullptr;
}

// static
void ThreadWatcherList::ParseCommandLine(
    const base::CommandLine& command_line,
    uint32_t* unresponsive_threshold,
    CrashOnHangThreadMap* crash_on_hang_threads) {
  // Initialize |unresponsive_threshold| to a default value.
  *unresponsive_threshold = kUnresponsiveCount;

  // Increase the unresponsive_threshold on the Stable and Beta channels to
  // reduce the number of crashes due to ThreadWatcher.
  version_info::Channel channel = chrome::GetChannel();
  if (channel == version_info::Channel::STABLE) {
    *unresponsive_threshold *= 4;
  } else if (channel == version_info::Channel::BETA) {
    *unresponsive_threshold *= 2;
  }

#if defined(OS_WIN)
  // For Windows XP (old systems), double the unresponsive_threshold to give
  // the OS a chance to schedule UI/IO threads a time slice to respond with a
  // pong message (to get around limitations with the OS).
  if (base::win::GetVersion() <= base::win::VERSION_XP)
    *unresponsive_threshold *= 2;
#endif

  uint32_t crash_seconds = *unresponsive_threshold * kUnresponsiveSeconds;
  std::string crash_on_hang_thread_names;
  if (command_line.HasSwitch(switches::kCrashOnHangThreads)) {
    crash_on_hang_thread_names =
        command_line.GetSwitchValueASCII(switches::kCrashOnHangThreads);
  } else if (channel != version_info::Channel::STABLE) {
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
}

// static
void ThreadWatcherList::ParseCommandLineCrashOnHangThreads(
    const std::string& crash_on_hang_thread_names,
    uint32_t default_live_threads_threshold,
    uint32_t default_crash_seconds,
    CrashOnHangThreadMap* crash_on_hang_threads) {
  base::StringTokenizer tokens(crash_on_hang_thread_names, ",");
  while (tokens.GetNext()) {
    std::vector<base::StringPiece> values = base::SplitStringPiece(
        tokens.token_piece(), ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    std::string thread_name = values[0].as_string();

    uint32_t live_threads_threshold = default_live_threads_threshold;
    uint32_t crash_seconds = default_crash_seconds;
    if (values.size() >= 2 &&
        (!base::StringToUint(values[1], &live_threads_threshold))) {
      continue;
    }
    if (values.size() >= 3 &&
        (!base::StringToUint(values[2], &crash_seconds))) {
      continue;
    }
    uint32_t unresponsive_threshold = static_cast<uint32_t>(
        ceil(static_cast<float>(crash_seconds) / kUnresponsiveSeconds));

    CrashDataThresholds crash_data(live_threads_threshold,
                                   unresponsive_threshold);
    // Use the last specifier.
    (*crash_on_hang_threads)[thread_name] = crash_data;
  }
}

// static
void ThreadWatcherList::InitializeAndStartWatching(
    uint32_t unresponsive_threshold,
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

  // TODO(rtenneti): Because we don't generate crash dumps for ThreadWatcher in
  // stable channel, disable ThreadWatcher in stable and unknown channels. We
  // will also not collect histogram data in these channels until
  // http://crbug.com/426203 is fixed.
  version_info::Channel channel = chrome::GetChannel();
  if (channel == version_info::Channel::STABLE ||
      channel == version_info::Channel::UNKNOWN) {
    return;
  }

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
    uint32_t unresponsive_threshold,
    const CrashOnHangThreadMap& crash_on_hang_threads) {
  DCHECK(WatchDogThread::CurrentlyOnWatchDogThread());

  CrashOnHangThreadMap::const_iterator it =
      crash_on_hang_threads.find(thread_name);
  bool crash_on_hang = false;
  uint32_t live_threads_threshold = 0;
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
    return nullptr;
  RegistrationList::iterator it =
      g_thread_watcher_list_->registered_.find(thread_id);
  if (g_thread_watcher_list_->registered_.end() == it)
    return nullptr;
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
ThreadWatcherObserver*
ThreadWatcherObserver::g_thread_watcher_observer_ = nullptr;

ThreadWatcherObserver::ThreadWatcherObserver(
    const base::TimeDelta& wakeup_interval)
    : last_wakeup_time_(base::TimeTicks::Now()),
      wakeup_interval_(wakeup_interval) {
  CHECK(!g_thread_watcher_observer_);
  g_thread_watcher_observer_ = this;
}

ThreadWatcherObserver::~ThreadWatcherObserver() {
  DCHECK(this == g_thread_watcher_observer_);
  g_thread_watcher_observer_ = nullptr;
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
  observer->omnibox_url_opened_subscription_ =
      OmniboxEventGlobalTracker::GetInstance()->RegisterCallback(
          base::Bind(&ThreadWatcherObserver::OnURLOpenedFromOmnibox,
                     base::Unretained(observer)));
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
  OnUserActivityDetected();
}

void ThreadWatcherObserver::OnURLOpenedFromOmnibox(OmniboxLog* log) {
  OnUserActivityDetected();
}

void ThreadWatcherObserver::OnUserActivityDetected() {
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
static WatchDogThread* g_watchdog_thread = nullptr;

WatchDogThread::WatchDogThread() : Thread("BrowserWatchdog") {
}

WatchDogThread::~WatchDogThread() {
  Stop();
}

// static
bool WatchDogThread::CurrentlyOnWatchDogThread() {
  base::AutoLock lock(g_watchdog_lock.Get());
  return g_watchdog_thread &&
         g_watchdog_thread->task_runner()->BelongsToCurrentThread();
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
        g_watchdog_thread->message_loop() : nullptr;
    if (message_loop) {
      message_loop->task_runner()->PostDelayedTask(from_here, task, delay);
      return true;
    }
  }

  return false;
}

bool WatchDogThread::Started() const {
  base::AutoLock lock(g_watchdog_lock.Get());
  return g_watchdog_thread != nullptr;
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
  g_watchdog_thread = nullptr;
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
  void Alarm() override {
#if !defined(NDEBUG)
    metrics::StartupHang();
    return;
#elif !defined(OS_ANDROID)
    WatchDogThread::PostTask(FROM_HERE, base::Bind(&metrics::StartupHang));
    return;
#else
    // TODO(rtenneti): Enable crashing for Android.
#endif  // OS_ANDROID
  }

 private:
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
  void Alarm() override { metrics::ShutdownHang(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(ShutdownWatchDogThread);
};

}  // namespace

// StartupTimeBomb methods and members.
//
// static
StartupTimeBomb* StartupTimeBomb::g_startup_timebomb_ = nullptr;

StartupTimeBomb::StartupTimeBomb()
    : startup_watchdog_(nullptr),
      thread_id_(base::PlatformThread::CurrentId()) {
  CHECK(!g_startup_timebomb_);
  g_startup_timebomb_ = this;
}

StartupTimeBomb::~StartupTimeBomb() {
  DCHECK(this == g_startup_timebomb_);
  DCHECK_EQ(thread_id_, base::PlatformThread::CurrentId());
  if (startup_watchdog_)
    Disarm();
  g_startup_timebomb_ = nullptr;
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
    base::Watchdog* startup_watchdog = startup_watchdog_;
    startup_watchdog_ = nullptr;

    startup_watchdog->Disarm();
    startup_watchdog->Cleanup();
    DeleteStartupWatchdog(thread_id_, startup_watchdog);
  }
}

// static
void StartupTimeBomb::DeleteStartupWatchdog(
    const base::PlatformThreadId thread_id,
    base::Watchdog* startup_watchdog) {
  DCHECK_EQ(thread_id, base::PlatformThread::CurrentId());
  if (startup_watchdog->IsJoinable()) {
    // Allow the watchdog thread to shutdown on UI. Watchdog thread shutdowns
    // very fast.
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    delete startup_watchdog;
    return;
  }
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::Bind(&StartupTimeBomb::DeleteStartupWatchdog, thread_id,
                            base::Unretained(startup_watchdog)),
      base::TimeDelta::FromSeconds(10));
}

// static
void StartupTimeBomb::DisarmStartupTimeBomb() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (g_startup_timebomb_)
    g_startup_timebomb_->Disarm();
}

// JankTimeBomb methods and members.
//
JankTimeBomb::JankTimeBomb(base::TimeDelta duration,
                           metrics::CallStackProfileParams::Thread thread)
    : thread_(thread), weak_ptr_factory_(this) {
  if (IsEnabled()) {
    WatchDogThread::PostDelayedTask(
        FROM_HERE,
        base::Bind(&JankTimeBomb::Alarm,
                   weak_ptr_factory_.GetWeakPtr(),
                   base::PlatformThread::CurrentId()),
        duration);
  }
}

JankTimeBomb::~JankTimeBomb() {
}

bool JankTimeBomb::IsEnabled() const {
  version_info::Channel channel = chrome::GetChannel();
  return channel == version_info::Channel::UNKNOWN ||
      channel == version_info::Channel::CANARY ||
      channel == version_info::Channel::DEV;
}

void JankTimeBomb::Alarm(base::PlatformThreadId thread_id) {
  DCHECK(WatchDogThread::CurrentlyOnWatchDogThread());
  sampling_profiler_.reset(new base::StackSamplingProfiler(
      thread_id,
      GetJankTimeBombSamplingParams(),
      metrics::CallStackProfileMetricsProvider::GetProfilerCallback(
          metrics::CallStackProfileParams(
              metrics::CallStackProfileParams::BROWSER_PROCESS,
              thread_,
              metrics::CallStackProfileParams::JANKY_TASK,
              metrics::CallStackProfileParams::PRESERVE_ORDER))));
  // Use synchronous profiler. It will automatically stop collection when
  // destroyed.
  sampling_profiler_->Start();
}

// ShutdownWatcherHelper methods and members.
//
// ShutdownWatcherHelper is a wrapper class for detecting hangs during
// shutdown.
ShutdownWatcherHelper::ShutdownWatcherHelper()
    : shutdown_watchdog_(nullptr),
      thread_id_(base::PlatformThread::CurrentId()) {
}

ShutdownWatcherHelper::~ShutdownWatcherHelper() {
  DCHECK_EQ(thread_id_, base::PlatformThread::CurrentId());
  if (shutdown_watchdog_) {
    shutdown_watchdog_->Disarm();
    delete shutdown_watchdog_;
    shutdown_watchdog_ = nullptr;
  }
}

void ShutdownWatcherHelper::Arm(const base::TimeDelta& duration) {
  DCHECK_EQ(thread_id_, base::PlatformThread::CurrentId());
  DCHECK(!shutdown_watchdog_);
  base::TimeDelta actual_duration = duration;

  version_info::Channel channel = chrome::GetChannel();
  if (channel == version_info::Channel::STABLE) {
    actual_duration *= 20;
  } else if (channel == version_info::Channel::BETA) {
    actual_duration *= 10;
  } else if (channel == version_info::Channel::DEV) {
    actual_duration *= 4;
  } else {
    actual_duration *= 2;
  }

#if defined(OS_WIN)
  // On Windows XP, give twice the time for shutdown.
  if (base::win::GetVersion() <= base::win::VERSION_XP)
    actual_duration *= 2;
#endif

  shutdown_watchdog_ = new ShutdownWatchDogThread(actual_duration);
  shutdown_watchdog_->Arm();
}
