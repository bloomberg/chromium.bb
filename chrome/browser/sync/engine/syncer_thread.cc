// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/sync/engine/syncer_thread.h"

#include "build/build_config.h"

#if defined(OS_MACOSX)
#include <CoreFoundation/CFNumber.h>
#include <IOKit/IOTypes.h>
#include <IOKit/IOKitLib.h>
#endif

#include <algorithm>
#include <map>
#include <queue>

#include "base/command_line.h"
#include "chrome/browser/sync/engine/auth_watcher.h"
#include "chrome/browser/sync/engine/model_safe_worker.h"
#include "chrome/browser/sync/engine/net/server_connection_manager.h"
#include "chrome/browser/sync/engine/syncer.h"
#include "chrome/browser/sync/notifier/listener/talk_mediator.h"
#include "chrome/browser/sync/notifier/listener/talk_mediator_impl.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/common/chrome_switches.h"

using std::priority_queue;
using std::min;
using base::Time;
using base::TimeDelta;
using base::TimeTicks;

namespace {

// Returns the amount of time since the user last interacted with the computer,
// in milliseconds
int UserIdleTime() {
#if defined(OS_WIN)
  LASTINPUTINFO last_input_info;
  last_input_info.cbSize = sizeof(LASTINPUTINFO);

  // Get time in windows ticks since system start of last activity.
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
  Boolean success = false;
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

const int SyncerThread::kDefaultShortPollIntervalSeconds = 60;
const int SyncerThread::kDefaultLongPollIntervalSeconds = 3600;
const int SyncerThread::kDefaultMaxPollIntervalMs = 30 * 60 * 1000;

void SyncerThread::NudgeSyncer(int milliseconds_from_now, NudgeSource source) {
  AutoLock lock(lock_);
  if (vault_.syncer_ == NULL) {
    return;
  }

  NudgeSyncImpl(milliseconds_from_now, source);
}

SyncerThread::SyncerThread(sessions::SyncSessionContext* context,
    AllStatus* all_status)
    : thread_main_started_(false, false),
      thread_("SyncEngine_SyncerThread"),
      vault_field_changed_(&lock_),
      p2p_authenticated_(false),
      p2p_subscribed_(false),
      conn_mgr_hookup_(NULL),
      allstatus_(all_status),
      syncer_short_poll_interval_seconds_(kDefaultShortPollIntervalSeconds),
      syncer_long_poll_interval_seconds_(kDefaultLongPollIntervalSeconds),
      syncer_polling_interval_(kDefaultShortPollIntervalSeconds),
      syncer_max_interval_(kDefaultMaxPollIntervalMs),
      talk_mediator_hookup_(NULL),
      directory_manager_hookup_(NULL),
      syncer_events_(NULL),
      session_context_(context),
      disable_idle_detection_(false) {
  DCHECK(context);
  syncer_event_relay_channel_.reset(new SyncerEventChannel(SyncerEvent(
      SyncerEvent::SHUTDOWN_USE_WITH_CARE)));

  if (context->directory_manager()) {
    directory_manager_hookup_.reset(NewEventListenerHookup(
        context->directory_manager()->channel(), this,
            &SyncerThread::HandleDirectoryManagerEvent));
  }

  if (context->connection_manager())
    WatchConnectionManager(context->connection_manager());

}

SyncerThread::~SyncerThread() {
  conn_mgr_hookup_.reset();
  syncer_event_relay_channel_.reset();
  directory_manager_hookup_.reset();
  syncer_events_.reset();
  delete vault_.syncer_;
  talk_mediator_hookup_.reset();
  CHECK(!thread_.IsRunning());
}

// Creates and starts a syncer thread.
// Returns true if it creates a thread or if there's currently a thread running
// and false otherwise.
bool SyncerThread::Start() {
  {
    AutoLock lock(lock_);
    if (thread_.IsRunning()) {
      return true;
    }

    if (!thread_.Start()) {
      return false;
    }
  }

  thread_.message_loop()->PostTask(FROM_HERE, NewRunnableMethod(this,
      &SyncerThread::ThreadMain));

  // Wait for notification that our task makes it safely onto the message
  // loop before returning, so the caller can't call Stop before we're
  // actually up and running.  This is for consistency with the old pthread
  // impl because pthread_create would do this in one step.
  thread_main_started_.Wait();
  LOG(INFO) << "SyncerThread started.";
  return true;
}

// Stop processing. A max wait of at least 2*server RTT time is recommended.
// Returns true if we stopped, false otherwise.
bool SyncerThread::Stop(int max_wait) {
  {
    AutoLock lock(lock_);
    // If the thread has been started, then we either already have or are about
    // to enter ThreadMainLoop so we have to proceed with shutdown and wait for
    // it to finish.  If the thread has not been started --and we now own the
    // lock-- then we can early out because the caller has not called Start().
    if (!thread_.IsRunning())
      return true;

    LOG(INFO) << "SyncerThread::Stop - setting ThreadMain exit condition to "
              << "true (vault_.stop_syncer_thread_)";
    // Exit the ThreadMainLoop once the syncer finishes (we tell it to exit
    // below).
    vault_.stop_syncer_thread_ = true;
    if (NULL != vault_.syncer_) {
      // Try to early exit the syncer itself, which could be looping inside
      // SyncShare.
      vault_.syncer_->RequestEarlyExit();
    }

    // stop_syncer_thread_ is now true and the Syncer has been told to exit.
    // We want to wake up all waiters so they can re-examine state. We signal,
    // causing all waiters to try to re-acquire the lock, and then we release
    // the lock, and join on our internal thread which should soon run off the
    // end of ThreadMain.
    vault_field_changed_.Broadcast();
  }

  // This will join, and finish when ThreadMain terminates.
  thread_.Stop();
  return true;
}

void SyncerThread::OnReceivedLongPollIntervalUpdate(
    const base::TimeDelta& new_interval) {
  syncer_long_poll_interval_seconds_ = static_cast<int>(
      new_interval.InSeconds());
}

void SyncerThread::OnReceivedShortPollIntervalUpdate(
    const base::TimeDelta& new_interval) {
  syncer_short_poll_interval_seconds_ = static_cast<int>(
      new_interval.InSeconds());
}

void SyncerThread::OnSilencedUntil(const base::TimeTicks& silenced_until) {
  silenced_until_ = silenced_until;
}

bool SyncerThread::IsSyncingCurrentlySilenced() {
  return (silenced_until_ - TimeTicks::Now()) >= TimeDelta::FromSeconds(0);
}

void SyncerThread::ThreadMainLoop() {
  // This is called with lock_ acquired.
  lock_.AssertAcquired();
  LOG(INFO) << "In thread main loop.";

  // Use the short poll value by default.
  vault_.current_wait_interval_.poll_delta =
      TimeDelta::FromSeconds(syncer_short_poll_interval_seconds_);
  int user_idle_milliseconds = 0;
  TimeTicks last_sync_time;
  bool initial_sync_for_thread = true;
  bool continue_sync_cycle = false;

  while (!vault_.stop_syncer_thread_) {
    // The Wait()s in these conditionals using |vault_| are not TimedWait()s (as
    // below) because we cannot poll until these conditions are met, so we wait
    // indefinitely.
    if (!vault_.connected_) {
      LOG(INFO) << "Syncer thread waiting for connection.";
      while (!vault_.connected_ && !vault_.stop_syncer_thread_)
        vault_field_changed_.Wait();
      LOG_IF(INFO, vault_.connected_) << "Syncer thread found connection.";
      continue;
    }

    if (vault_.syncer_ == NULL) {
      LOG(INFO) << "Syncer thread waiting for database initialization.";
      while (vault_.syncer_ == NULL && !vault_.stop_syncer_thread_)
        vault_field_changed_.Wait();
      LOG_IF(INFO, !(vault_.syncer_ == NULL))
          << "Syncer was found after DB started.";
      continue;
    }

    const TimeTicks next_poll = last_sync_time +
        vault_.current_wait_interval_.poll_delta;
    bool throttled = vault_.current_wait_interval_.mode ==
        WaitInterval::THROTTLED;
    // If we are throttled, we must wait.  Otherwise, wait until either the next
    // nudge (if one exists) or the poll interval.
    const TimeTicks end_wait = throttled ? next_poll :
        !vault_.nudge_queue_.empty() &&
        vault_.nudge_queue_.top().first < next_poll ?
        vault_.nudge_queue_.top().first : next_poll;
    LOG(INFO) << "end_wait is " << end_wait.ToInternalValue();
    LOG(INFO) << "next_poll is " << next_poll.ToInternalValue();

    // We block until the CV is signaled (e.g a control field changed, loss of
    // network connection, nudge, spurious, etc), or the poll interval elapses.
    TimeDelta sleep_time = end_wait - TimeTicks::Now();
    if (sleep_time > TimeDelta::FromSeconds(0)) {
      vault_field_changed_.TimedWait(sleep_time);

      if (TimeTicks::Now() < end_wait) {
        // Didn't timeout. Could be a spurious signal, or a signal corresponding
        // to an actual change in one of our control fields.  By continuing here
        // we perform the typical "always recheck conditions when signaled",
        // (typically handled by a while(condition_not_met) cv.wait() construct)
        // because we jump to the top of the loop.  The main difference is we
        // recalculate the wait interval, but last_sync_time won't have changed.
        // So if we were signaled by a nudge (for ex.) we'll grab the new nudge
        // off the queue and wait for that delta.  If it was a spurious signal,
        // we'll keep waiting for the same moment in time as we just were.
        continue;
       }
    }

    // Handle a nudge, caused by either a notification or a local bookmark
    // event.  This will also update the source of the following SyncMain call.
    bool nudged = UpdateNudgeSource(throttled, continue_sync_cycle,
                                    &initial_sync_for_thread);

    LOG(INFO) << "Calling Sync Main at time " << Time::Now().ToInternalValue();
    SyncMain(vault_.syncer_);
    last_sync_time = TimeTicks::Now();

    LOG(INFO) << "Updating the next polling time after SyncMain";
    vault_.current_wait_interval_ = CalculatePollingWaitTime(
        allstatus_->status(),
        static_cast<int>(vault_.current_wait_interval_.poll_delta.InSeconds()),
        &user_idle_milliseconds, &continue_sync_cycle, nudged);
  }
}

// We check how long the user's been idle and sync less often if the machine is
// not in use. The aim is to reduce server load.
// TODO(timsteele): Should use Time(Delta).
SyncerThread::WaitInterval SyncerThread::CalculatePollingWaitTime(
    const AllStatus::Status& status,
    int last_poll_wait,  // Time in seconds.
    int* user_idle_milliseconds,
    bool* continue_sync_cycle,
    bool was_nudged) {
  lock_.AssertAcquired();  // We access 'vault' in here, so we need the lock.
  WaitInterval return_interval;

  // Server initiated throttling trumps everything.
  if (!silenced_until_.is_null()) {
    // We don't need to reset other state, it can continue where it left off.
    return_interval.mode = WaitInterval::THROTTLED;
    return_interval.poll_delta = silenced_until_ - TimeTicks::Now();
    return return_interval;
  }

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
  return_interval.poll_delta = TimeDelta::FromSeconds(default_next_wait);

  if (syncer_has_work_to_do) {
    // Provide exponential backoff due to consecutive errors, else attempt to
    // complete the work as soon as possible.
    if (is_continuing_sync_cyle) {
      return_interval.mode = WaitInterval::EXPONENTIAL_BACKOFF;
      if (was_nudged && vault_.current_wait_interval_.mode ==
          WaitInterval::EXPONENTIAL_BACKOFF) {
          // We were nudged, it failed, and we were already in backoff.
          return_interval.had_nudge_during_backoff = true;
          // Keep exponent for exponential backoff the same in this case.
          return_interval.poll_delta = vault_.current_wait_interval_.poll_delta;
      } else {
        // We weren't nudged, or we were in a NORMAL wait interval until now.
        return_interval.poll_delta = TimeDelta::FromSeconds(
            AllStatus::GetRecommendedDelaySeconds(last_poll_wait));
      }
    } else {
      // No consecutive error.
      return_interval.poll_delta = TimeDelta::FromSeconds(
           AllStatus::GetRecommendedDelaySeconds(0));
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
    return_interval.poll_delta = TimeDelta::FromMilliseconds(
        CalculateSyncWaitTime(last_poll_wait * 1000,
                              *user_idle_milliseconds));
    DCHECK_GE(return_interval.poll_delta.InSeconds(), default_next_wait);
  }

  LOG(INFO) << "Sync wait: idle " << default_next_wait
            << " non-idle or backoff "
            << return_interval.poll_delta.InSeconds() << ".";

  return return_interval;
}

void SyncerThread::ThreadMain() {
  AutoLock lock(lock_);
  // Signal Start() to let it know we've made it safely onto the message loop,
  // and unblock it's caller.
  thread_main_started_.Signal();
  ThreadMainLoop();
  LOG(INFO) << "Syncer thread ThreadMain is done.";
}

void SyncerThread::SyncMain(Syncer* syncer) {
  CHECK(syncer);

  // Since we are initiating a new session for which we are the delegate, we
  // are not currently silenced so reset this state for the next session which
  // may need to use it.
  silenced_until_ = base::TimeTicks();

  AutoUnlock unlock(lock_);
  while (syncer->SyncShare(this) && silenced_until_.is_null()) {
    LOG(INFO) << "Looping in sync share";
  }
  LOG(INFO) << "Done looping in sync share";
}

bool SyncerThread::UpdateNudgeSource(bool was_throttled,
                                     bool continue_sync_cycle,
                                     bool* initial_sync) {
  bool nudged = false;
  NudgeSource nudge_source = kUnknown;
  // Has the previous sync cycle completed?
  if (continue_sync_cycle) {
    nudge_source = kContinuation;
  }
  // Update the nudge source if a new nudge has come through during the
  // previous sync cycle.
  while (!vault_.nudge_queue_.empty() &&
         TimeTicks::Now() >= vault_.nudge_queue_.top().first) {
    if (!was_throttled && !nudged) {
      nudge_source = vault_.nudge_queue_.top().second;
      nudged = true;
    }
    vault_.nudge_queue_.pop();
  }
  SetUpdatesSource(nudged, nudge_source, initial_sync);
  return nudged;
}

void SyncerThread::SetUpdatesSource(bool nudged, NudgeSource nudge_source,
                                    bool* initial_sync) {
  sync_pb::GetUpdatesCallerInfo::GetUpdatesSource updates_source =
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
  vault_.syncer_->set_updates_source(updates_source);
}

void SyncerThread::HandleSyncerEvent(const SyncerEvent& event) {
  AutoLock lock(lock_);
  relay_channel()->NotifyListeners(event);
  if (SyncerEvent::REQUEST_SYNC_NUDGE != event.what_happened) {
    return;
  }
  NudgeSyncImpl(event.nudge_delay_milliseconds, kUnknown);
}

void SyncerThread::HandleDirectoryManagerEvent(
    const syncable::DirectoryManagerEvent& event) {
  LOG(INFO) << "Handling a directory manager event";
  if (syncable::DirectoryManagerEvent::OPENED == event.what_happened) {
    AutoLock lock(lock_);
    LOG(INFO) << "Syncer starting up for: " << event.dirname;
    // The underlying database structure is ready, and we should create
    // the syncer.
    CHECK(vault_.syncer_ == NULL);
    session_context_->set_account_name(event.dirname);
    vault_.syncer_ = new Syncer(session_context_.get());

    syncer_events_.reset(NewEventListenerHookup(
        session_context_->syncer_event_channel(), this,
        &SyncerThread::HandleSyncerEvent));
    vault_field_changed_.Broadcast();
  }
}

static inline void CheckConnected(bool* connected,
                                  HttpResponse::ServerConnectionCode code,
                                  ConditionVariable* condvar) {
  if (*connected) {
    if (HttpResponse::CONNECTION_UNAVAILABLE == code) {
      *connected = false;
      condvar->Broadcast();
    }
  } else {
    if (HttpResponse::SERVER_CONNECTION_OK == code) {
      *connected = true;
      condvar->Broadcast();
    }
  }
}

void SyncerThread::WatchConnectionManager(ServerConnectionManager* conn_mgr) {
  conn_mgr_hookup_.reset(NewEventListenerHookup(conn_mgr->channel(), this,
                         &SyncerThread::HandleServerConnectionEvent));
  CheckConnected(&vault_.connected_, conn_mgr->server_status(),
                 &vault_field_changed_);
}

void SyncerThread::HandleServerConnectionEvent(
    const ServerConnectionEvent& event) {
  if (ServerConnectionEvent::STATUS_CHANGED == event.what_happened) {
    AutoLock lock(lock_);
    CheckConnected(&vault_.connected_, event.connection_code,
                   &vault_field_changed_);
  }
}

SyncerEventChannel* SyncerThread::relay_channel() {
  return syncer_event_relay_channel_.get();
}

// Inputs and return value in milliseconds.
int SyncerThread::CalculateSyncWaitTime(int last_interval, int user_idle_ms) {
  // syncer_polling_interval_ is in seconds
  int syncer_polling_interval_ms = syncer_polling_interval_ * 1000;

  // This is our default and lower bound.
  int next_wait = syncer_polling_interval_ms;

  // Get idle time, bounded by max wait.
  int idle = min(user_idle_ms, syncer_max_interval_);

  // If the user has been idle for a while, we'll start decreasing the poll
  // rate.
  if (idle >= kPollBackoffThresholdMultiplier * syncer_polling_interval_ms) {
    next_wait = std::min(AllStatus::GetRecommendedDelaySeconds(
        last_interval / 1000), syncer_max_interval_ / 1000) * 1000;
  }

  return next_wait;
}

// Called with mutex_ already locked.
void SyncerThread::NudgeSyncImpl(int milliseconds_from_now,
                                 NudgeSource source) {
  if (vault_.current_wait_interval_.mode == WaitInterval::THROTTLED ||
      vault_.current_wait_interval_.had_nudge_during_backoff) {
    // Drop nudges on the floor if we've already had one since starting this
    // stage of exponential backoff or we are throttled.
    return;
  }

  const TimeTicks nudge_time = TimeTicks::Now() +
      TimeDelta::FromMilliseconds(milliseconds_from_now);
  NudgeObject nudge_object(nudge_time, source);
  vault_.nudge_queue_.push(nudge_object);
  vault_field_changed_.Broadcast();
}

void SyncerThread::WatchTalkMediator(TalkMediator* mediator) {
  talk_mediator_hookup_.reset(
      NewEventListenerHookup(
          mediator->channel(),
          this,
          &SyncerThread::HandleTalkMediatorEvent));
}

void SyncerThread::HandleTalkMediatorEvent(const TalkMediatorEvent& event) {
  AutoLock lock(lock_);
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
      if (NULL != vault_.syncer_) {
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
      if (NULL != vault_.syncer_) {
        NudgeSyncImpl(0, kNotification);
      }
      break;
    default:
      break;
  }

  session_context_->set_notifications_enabled(p2p_authenticated_ &&
                                              p2p_subscribed_);
}

}  // namespace browser_sync
