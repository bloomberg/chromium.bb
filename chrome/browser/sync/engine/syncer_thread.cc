// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/syncer_thread.h"

#ifdef OS_MACOSX
#include <CoreFoundation/CFNumber.h>
#include <IOKit/IOTypes.h>
#include <IOKit/IOKitLib.h>
#endif

#include <algorithm>
#include <map>
#include <queue>

#include "chrome/browser/sync/engine/auth_watcher.h"
#include "chrome/browser/sync/engine/model_safe_worker.h"
#include "chrome/browser/sync/engine/net/server_connection_manager.h"
#include "chrome/browser/sync/engine/syncer.h"
#include "chrome/browser/sync/notifier/listener/talk_mediator.h"
#include "chrome/browser/sync/notifier/listener/talk_mediator_impl.h"
#include "chrome/browser/sync/syncable/directory_manager.h"

using std::priority_queue;
using std::min;

static inline bool operator < (const timespec& a, const timespec& b) {
  return a.tv_sec == b.tv_sec ? a.tv_nsec < b.tv_nsec : a.tv_sec < b.tv_sec;
}

namespace {

// returns the amount of time since the user last interacted with
// the computer, in milliseconds
int UserIdleTime() {
#ifdef OS_WINDOWS
  LASTINPUTINFO last_input_info;
  last_input_info.cbSize = sizeof(LASTINPUTINFO);

  // get time in windows ticks since system start of last activity
  BOOL b = ::GetLastInputInfo(&last_input_info);
  if (b == TRUE)
    return ::GetTickCount() - last_input_info.dwTime;
#elif defined(OS_MACOSX)
  // It would be great to do something like:
  //
  // return 1000 *
  //     CGEventSourceSecondsSinceLastEventType(
  //         kCGEventSourceStateCombinedSessionState,
  //         kCGAnyInputEventType);
  //
  // Unfortunately, CGEvent* lives in ApplicationServices, and we're a daemon
  // and can't link that high up the food chain. Thus this mucking in IOKit.

  io_service_t hid_service =
      IOServiceGetMatchingService(kIOMasterPortDefault,
                                  IOServiceMatching("IOHIDSystem"));
  if (!hid_service) {
    LOG(WARNING) << "Could not obtain IOHIDSystem";
    return 0;
  }

  CFTypeRef object = IORegistryEntryCreateCFProperty(hid_service,
                                                     CFSTR("HIDIdleTime"),
                                                     kCFAllocatorDefault,
                                                     0);
  if (!object) {
    LOG(WARNING) << "Could not get IOHIDSystem's HIDIdleTime property";
    IOObjectRelease(hid_service);
    return 0;
  }

  int64 idle_time;  // in nanoseconds
  Boolean success;
  if (CFGetTypeID(object) == CFNumberGetTypeID()) {
    success = CFNumberGetValue((CFNumberRef)object,
                               kCFNumberSInt64Type,
                               &idle_time);
  } else {
    LOG(WARNING) << "IOHIDSystem's HIDIdleTime property isn't a number!";
  }

  CFRelease(object);
  IOObjectRelease(hid_service);

  if (!success) {
    LOG(WARNING) << "Could not get IOHIDSystem's HIDIdleTime property's value";
    return 0;
  } else {
    return idle_time / 1000000;  // nano to milli
  }
#else
  static bool was_logged = false;
  if (!was_logged) {
    was_logged = true;
    LOG(INFO) << "UserIdleTime unimplemented on this platform, "
        "synchronization will not throttle when user idle";
  }
#endif

  return 0;
}

}  // namespace

namespace browser_sync {

bool SyncerThread::NudgeSyncer(int milliseconds_from_now, NudgeSource source) {
  MutexLock lock(&mutex_);
  if (syncer_ == NULL) {
    return false;
  }
  NudgeSyncImpl(milliseconds_from_now, source);
  return true;
}

void* RunSyncerThread(void* syncer_thread) {
  return (reinterpret_cast<SyncerThread*>(syncer_thread))->ThreadMain();
}

SyncerThread::SyncerThread(
    ClientCommandChannel* command_channel,
    syncable::DirectoryManager* mgr,
    ServerConnectionManager* connection_manager,
    AllStatus* all_status,
    ModelSafeWorker* model_safe_worker)
    : dirman_(mgr), scm_(connection_manager),
      syncer_(NULL), syncer_events_(NULL), thread_running_(false),
      syncer_short_poll_interval_seconds_(kDefaultShortPollIntervalSeconds),
      syncer_long_poll_interval_seconds_(kDefaultLongPollIntervalSeconds),
      syncer_polling_interval_(kDefaultShortPollIntervalSeconds),
      syncer_max_interval_(kDefaultMaxPollIntervalMs),
      stop_syncer_thread_(false), connected_(false), conn_mgr_hookup_(NULL),
      p2p_authenticated_(false), p2p_subscribed_(false),
      allstatus_(all_status), talk_mediator_hookup_(NULL),
      command_channel_(command_channel), directory_manager_hookup_(NULL),
      model_safe_worker_(model_safe_worker),
      client_command_hookup_(NULL), disable_idle_detection_(false) {

  SyncerEvent shutdown = { SyncerEvent::SHUTDOWN_USE_WITH_CARE };
  syncer_event_channel_.reset(new SyncerEventChannel(shutdown));

  if (dirman_) {
    directory_manager_hookup_.reset(NewEventListenerHookup(
        dirman_->channel(), this, &SyncerThread::HandleDirectoryManagerEvent));
  }

  if (scm_) {
    WatchConnectionManager(scm_);
  }

  if (command_channel_) {
    WatchClientCommands(command_channel_);
  }
}

SyncerThread::~SyncerThread() {
  client_command_hookup_.reset();
  conn_mgr_hookup_.reset();
  syncer_event_channel_.reset();
  directory_manager_hookup_.reset();
  syncer_events_.reset();
  delete syncer_;
  talk_mediator_hookup_.reset();
  CHECK(!thread_running_);
}

// Creates and starts a syncer thread.
// Returns true if it creates a thread or if there's currently a thread
// running and false otherwise.
bool SyncerThread::Start() {
  MutexLock lock(&mutex_);
  if (thread_running_) {
    return true;
  }
  thread_running_ =
    (0 == pthread_create(&thread_, NULL, RunSyncerThread, this));
  if (thread_running_) {
    pthread_detach(thread_);
  }
  return thread_running_;
}

// Stop processing. A max wait of at least 2*server RTT time is recommended.
// returns true if we stopped, false otherwise.
bool SyncerThread::Stop(int max_wait) {
  MutexLock lock(&mutex_);
  if (!thread_running_)
    return true;
  stop_syncer_thread_ = true;
  if (NULL != syncer_) {
    // try to early exit the syncer
    syncer_->RequestEarlyExit();
  }
  pthread_cond_broadcast(&changed_.condvar_);
  timespec deadline = { time(NULL) + (max_wait / 1000), 0 };
  do {
    const int wait_result = max_wait < 0 ?
      pthread_cond_wait(&changed_.condvar_, &mutex_.mutex_) :
      pthread_cond_timedwait(&changed_.condvar_, &mutex_.mutex_,
                             &deadline);
    if (ETIMEDOUT == wait_result) {
      LOG(ERROR) << "SyncerThread::Stop timed out. Problems likely.";
      return false;
    }
  } while (thread_running_);
  return true;
}

void SyncerThread::WatchClientCommands(ClientCommandChannel* channel) {
  PThreadScopedLock<PThreadMutex> lock(&mutex_);
  client_command_hookup_.reset(NewEventListenerHookup(channel, this,
      &SyncerThread::HandleClientCommand));
}

void SyncerThread::HandleClientCommand(ClientCommandChannel::EventType
                                       event) {
  if (!event) {
    return;
  }

  // mutex not really necessary for these
  if (event->has_set_sync_poll_interval()) {
    syncer_short_poll_interval_seconds_ = event->set_sync_poll_interval();
  }

  if (event->has_set_sync_long_poll_interval()) {
    syncer_long_poll_interval_seconds_ = event->set_sync_long_poll_interval();
  }
}

void SyncerThread::ThreadMainLoop() {
  // Use the short poll value by default.
  int poll_seconds = syncer_short_poll_interval_seconds_;
  int user_idle_milliseconds = 0;
  timespec last_sync_time = { 0 };
  bool initial_sync_for_thread = true;
  bool continue_sync_cycle = false;

  while (!stop_syncer_thread_) {
    if (!connected_) {
      LOG(INFO) << "Syncer thread waiting for connection.";
      while (!connected_ && !stop_syncer_thread_)
        pthread_cond_wait(&changed_.condvar_, &mutex_.mutex_);
      LOG_IF(INFO, connected_) << "Syncer thread found connection.";
      continue;
    }

    if (syncer_ == NULL) {
      LOG(INFO) << "Syncer thread waiting for database initialization.";
      while (syncer_ == NULL && !stop_syncer_thread_)
        pthread_cond_wait(&changed_.condvar_, &mutex_.mutex_);
      LOG_IF(INFO, !(syncer_ == NULL)) << "Syncer was found after DB started.";
      continue;
    }

    timespec const next_poll = { last_sync_time.tv_sec + poll_seconds,
                                 last_sync_time.tv_nsec };
    const timespec wake_time =
      !nudge_queue_.empty() && nudge_queue_.top().first < next_poll ?
      nudge_queue_.top().first : next_poll;
    LOG(INFO) << "wake time is " << wake_time.tv_sec;
    LOG(INFO) << "next poll is " << next_poll.tv_sec;

    const int error = pthread_cond_timedwait(&changed_.condvar_, &mutex_.mutex_,
                                             &wake_time);
    if (ETIMEDOUT != error) {
      continue;  // Check all the conditions again.
    }

    const timespec now = GetPThreadAbsoluteTime(0);

    // Handle a nudge, caused by either a notification or a local bookmark
    // event.  This will also update the source of the following SyncMain call.
    UpdateNudgeSource(now, &continue_sync_cycle, &initial_sync_for_thread);

    LOG(INFO) << "Calling Sync Main at time " << now.tv_sec;
    SyncMain(syncer_);
    last_sync_time = now;

    LOG(INFO) << "Updating the next polling time after SyncMain";
    poll_seconds = CalculatePollingWaitTime(allstatus_->status(),
                                            poll_seconds,
                                            &user_idle_milliseconds,
                                            &continue_sync_cycle);
  }
}

// We check how long the user's been idle and sync less often if the
// machine is not in use. The aim is to reduce server load.
int SyncerThread::CalculatePollingWaitTime(
    const AllStatus::Status& status,
    int last_poll_wait,  // in s
    int* user_idle_milliseconds,
    bool* continue_sync_cycle) {
  bool is_continuing_sync_cyle = *continue_sync_cycle;
  *continue_sync_cycle = false;

  // Determine if the syncer has unfinished work to do from allstatus_.
  const bool syncer_has_work_to_do =
    status.updates_available > status.updates_received
    || status.unsynced_count > 0;
  LOG(INFO) << "syncer_has_work_to_do is " << syncer_has_work_to_do;

  // First calculate the expected wait time, figuring in any backoff because of
  // user idle time.  next_wait is in seconds
  syncer_polling_interval_ = (!status.notifications_enabled) ?
      syncer_short_poll_interval_seconds_ :
      syncer_long_poll_interval_seconds_;
  int default_next_wait = syncer_polling_interval_;
  int actual_next_wait = default_next_wait;

  if (syncer_has_work_to_do) {
    // Provide exponential backoff due to consecutive errors, else attempt to
    // complete the work as soon as possible.
    if (!is_continuing_sync_cyle) {
      actual_next_wait = AllStatus::GetRecommendedDelaySeconds(0);
    } else {
      actual_next_wait = AllStatus::GetRecommendedDelaySeconds(last_poll_wait);
    }
    *continue_sync_cycle = true;
  } else if (!status.notifications_enabled) {
    // Ensure that we start exponential backoff from our base polling
    // interval when we are not continuing a sync cycle.
    last_poll_wait = std::max(last_poll_wait, syncer_polling_interval_);

    // Did the user start interacting with the computer again?
    // If so, revise our idle time (and probably next_sync_time) downwards
    int new_idle_time = disable_idle_detection_ ? 0 : UserIdleTime();
    if (new_idle_time < *user_idle_milliseconds) {
      *user_idle_milliseconds = new_idle_time;
    }
    actual_next_wait = CalculateSyncWaitTime(last_poll_wait * 1000,
                                              *user_idle_milliseconds) / 1000;
    DCHECK_GE(actual_next_wait, default_next_wait);
  }

  LOG(INFO) << "Sync wait: idle " << default_next_wait
            << " non-idle or backoff " << actual_next_wait << ".";

  return actual_next_wait;
}

void* SyncerThread::ThreadMain() {
  NameCurrentThreadForDebugging("SyncEngine_SyncerThread");
  mutex_.Lock();
  ThreadMainLoop();
  thread_running_ = false;
  pthread_cond_broadcast(&changed_.condvar_);
  mutex_.Unlock();
  LOG(INFO) << "Syncer thread exiting.";
  return 0;
}

void SyncerThread::SyncMain(Syncer* syncer) {
  CHECK(syncer);
  mutex_.Unlock();
  while (syncer->SyncShare()) {
    LOG(INFO) << "Looping in sync share";
  }
  LOG(INFO) << "Done looping in sync share";

  mutex_.Lock();
}

void SyncerThread::UpdateNudgeSource(const timespec& now,
                                     bool* continue_sync_cycle,
                                     bool* initial_sync) {
  bool nudged = false;
  NudgeSource nudge_source = kUnknown;
  // Has the previous sync cycle completed?
  if (continue_sync_cycle) {
    nudge_source = kContinuation;
  }
  // Update the nudge source if a new nudge has come through during the
  // previous sync cycle.
  while (!nudge_queue_.empty() && !(now < nudge_queue_.top().first)) {
    if (!nudged) {
      nudge_source = nudge_queue_.top().second;
      *continue_sync_cycle = false;  //  Reset the continuation token on nudge.
      nudged = true;
    }
    nudge_queue_.pop();
  }
  SetUpdatesSource(nudged, nudge_source, initial_sync);
}

void SyncerThread::SetUpdatesSource(bool nudged, NudgeSource nudge_source,
                                    bool* initial_sync) {
  sync_pb::GetUpdatesCallerInfo::GET_UPDATES_SOURCE updates_source =
      sync_pb::GetUpdatesCallerInfo::UNKNOWN;
  if (*initial_sync) {
    updates_source = sync_pb::GetUpdatesCallerInfo::FIRST_UPDATE;
    *initial_sync = false;
  } else if (!nudged) {
    updates_source = sync_pb::GetUpdatesCallerInfo::PERIODIC;
  } else {
    switch (nudge_source) {
      case kNotification:
        updates_source = sync_pb::GetUpdatesCallerInfo::NOTIFICATION;
        break;
      case kLocal:
        updates_source = sync_pb::GetUpdatesCallerInfo::LOCAL;
        break;
      case kContinuation:
        updates_source = sync_pb::GetUpdatesCallerInfo::SYNC_CYCLE_CONTINUATION;
        break;
      case kUnknown:
      default:
        updates_source = sync_pb::GetUpdatesCallerInfo::UNKNOWN;
        break;
    }
  }
  syncer_->set_updates_source(updates_source);
}

void SyncerThread::HandleSyncerEvent(const SyncerEvent& event) {
  MutexLock lock(&mutex_);
  channel()->NotifyListeners(event);
  if (SyncerEvent::REQUEST_SYNC_NUDGE != event.what_happened) {
    return;
  }
  NudgeSyncImpl(event.nudge_delay_milliseconds, kUnknown);
}

void SyncerThread::HandleDirectoryManagerEvent(
    const syncable::DirectoryManagerEvent& event) {
  LOG(INFO) << "Handling a directory manager event";
  if (syncable::DirectoryManagerEvent::OPENED == event.what_happened) {
    MutexLock lock(&mutex_);
    LOG(INFO) << "Syncer starting up for: " << event.dirname;
    // The underlying database structure is ready, and we should create
    // the syncer.
    CHECK(syncer_ == NULL);
    syncer_ =
        new Syncer(dirman_, event.dirname, scm_, model_safe_worker_.get());

    syncer_->set_command_channel(command_channel_);
    syncer_events_.reset(NewEventListenerHookup(
        syncer_->channel(), this, &SyncerThread::HandleSyncerEvent));
    pthread_cond_broadcast(&changed_.condvar_);
  }
}

static inline void CheckConnected(bool* connected,
                                  HttpResponse::ServerConnectionCode code,
                                  pthread_cond_t* condvar) {
  if (*connected) {
    if (HttpResponse::CONNECTION_UNAVAILABLE == code) {
      *connected = false;
      pthread_cond_broadcast(condvar);
    }
  } else {
    if (HttpResponse::SERVER_CONNECTION_OK == code) {
      *connected = true;
      pthread_cond_broadcast(condvar);
    }
  }
}

void SyncerThread::WatchConnectionManager(ServerConnectionManager* conn_mgr) {
  conn_mgr_hookup_.reset(NewEventListenerHookup(conn_mgr->channel(), this,
                         &SyncerThread::HandleServerConnectionEvent));
  CheckConnected(&connected_, conn_mgr->server_status(),
                 &changed_.condvar_);
}

void SyncerThread::HandleServerConnectionEvent(
    const ServerConnectionEvent& event) {
  if (ServerConnectionEvent::STATUS_CHANGED == event.what_happened) {
    MutexLock lock(&mutex_);
    CheckConnected(&connected_, event.connection_code,
                   &changed_.condvar_);
  }
}

SyncerEventChannel* SyncerThread::channel() {
  return syncer_event_channel_.get();
}

// inputs and return value in milliseconds
int SyncerThread::CalculateSyncWaitTime(int last_interval, int user_idle_ms) {
  // syncer_polling_interval_ is in seconds
  int syncer_polling_interval_ms = syncer_polling_interval_ * 1000;

  // This is our default and lower bound.
  int next_wait = syncer_polling_interval_ms;

  // Get idle time, bounded by max wait.
  int idle = min(user_idle_ms, syncer_max_interval_);

  // If the user has been idle for a while,
  // we'll start decreasing the poll rate.
  if (idle >= kPollBackoffThresholdMultiplier * syncer_polling_interval_ms) {
    next_wait = std::min(AllStatus::GetRecommendedDelaySeconds(
        last_interval / 1000), syncer_max_interval_ / 1000) * 1000;
  }

  return next_wait;
}

// Called with mutex_ already locked
void SyncerThread::NudgeSyncImpl(int milliseconds_from_now,
                                 NudgeSource source) {
  const timespec nudge_time = GetPThreadAbsoluteTime(milliseconds_from_now);
  NudgeObject nudge_object(nudge_time, source);
  nudge_queue_.push(nudge_object);
  pthread_cond_broadcast(&changed_.condvar_);
}

void SyncerThread::WatchTalkMediator(TalkMediator* mediator) {
  talk_mediator_hookup_.reset(
      NewEventListenerHookup(
          mediator->channel(),
          this,
          &SyncerThread::HandleTalkMediatorEvent));
}

void SyncerThread::HandleTalkMediatorEvent(const TalkMediatorEvent& event) {
  MutexLock lock(&mutex_);
  switch (event.what_happened) {
    case TalkMediatorEvent::LOGIN_SUCCEEDED:
      LOG(INFO) << "P2P: Login succeeded.";
      p2p_authenticated_ = true;
      break;
    case TalkMediatorEvent::LOGOUT_SUCCEEDED:
      LOG(INFO) << "P2P: Login succeeded.";
      p2p_authenticated_ = false;
      break;
    case TalkMediatorEvent::SUBSCRIPTIONS_ON:
      LOG(INFO) << "P2P: Subscriptions successfully enabled.";
      p2p_subscribed_ = true;
      if (NULL != syncer_) {
        LOG(INFO) << "Subscriptions on.  Nudging syncer for initial push.";
        NudgeSyncImpl(0, kLocal);
      }
      break;
    case TalkMediatorEvent::SUBSCRIPTIONS_OFF:
      LOG(INFO) << "P2P: Subscriptions are not enabled.";
      p2p_subscribed_ = false;
      break;
    case TalkMediatorEvent::NOTIFICATION_RECEIVED:
      LOG(INFO) << "P2P: Updates on server, pushing syncer";
      if (NULL != syncer_) {
        NudgeSyncImpl(0, kNotification);
      }
      break;
    default:
      break;
  }

  if (NULL != syncer_) {
    syncer_->set_notifications_enabled(p2p_authenticated_ && p2p_subscribed_);
  }
}

}  // namespace browser_sync
