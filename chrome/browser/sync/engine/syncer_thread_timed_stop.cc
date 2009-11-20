// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/sync/engine/syncer_thread_timed_stop.h"

#include "build/build_config.h"

#if defined(OS_MACOSX)
#include <CoreFoundation/CFNumber.h>
#include <IOKit/IOTypes.h>
#include <IOKit/IOKitLib.h>
#endif

#include <algorithm>
#include <map>
#include <queue>

#include "base/auto_reset.h"
#include "chrome/browser/sync/engine/auth_watcher.h"
#include "chrome/browser/sync/engine/model_safe_worker.h"
#include "chrome/browser/sync/engine/net/server_connection_manager.h"
#include "chrome/browser/sync/engine/syncer.h"
#include "chrome/browser/sync/notifier/listener/talk_mediator.h"
#include "chrome/browser/sync/notifier/listener/talk_mediator_impl.h"
#include "chrome/browser/sync/syncable/directory_manager.h"

using std::priority_queue;
using std::min;
using base::Time;
using base::TimeDelta;
using base::TimeTicks;

namespace browser_sync {

SyncerThreadTimedStop::SyncerThreadTimedStop(
    ClientCommandChannel* command_channel,
    syncable::DirectoryManager* mgr,
    ServerConnectionManager* connection_manager,
    AllStatus* all_status,
    ModelSafeWorker* model_safe_worker)
    : SyncerThread(command_channel, mgr, connection_manager, all_status,
                   model_safe_worker),
      in_thread_main_loop_(false) {
}

// Stop processing. A max wait of at least 2*server RTT time is recommended.
// Returns true if we stopped, false otherwise.
bool SyncerThreadTimedStop::Stop(int max_wait) {
  AutoLock lock(lock_);
  // If the thread has been started, then we either already have or are about to
  // enter ThreadMainLoop so we have to proceed with shutdown and wait for it to
  // finish.  If the thread has not been started --and we now own the lock--
  // then we can early out because the caller has not called Start().
  if (!thread_.IsRunning())
    return true;

  LOG(INFO) << "SyncerThread::Stop - setting ThreadMain exit condition to "
    << "true (vault_.stop_syncer_thread_)";
  // Exit the ThreadMainLoop once the syncer finishes (we tell it to exit
  // below).
  vault_.stop_syncer_thread_ = true;
  if (NULL != vault_.syncer_) {
    // Try to early exit the syncer.
    vault_.syncer_->RequestEarlyExit();
  }

  // stop_syncer_thread_ is now true and the Syncer has been told to exit.
  // We want to wake up all waiters so they can re-examine state. We signal,
  // causing all waiters to try to re-acquire the lock, and then we atomically
  // release the lock and wait.  Our wait can be spuriously signaled, so we
  // recalculate the remaining sleep time each time through and re-
  // check the condition before exiting the loop.
  vault_field_changed_.Broadcast();
  TimeTicks start = TimeTicks::Now();
  TimeTicks end = start + TimeDelta::FromMilliseconds(max_wait);
  bool timed_out = false;
  // Eventually the combination of RequestEarlyExit and setting
  // stop_syncer_thread_ to true above will cause in_thread_main_loop_ to become
  // false.
  while (in_thread_main_loop_) {
    TimeDelta sleep_time = end - TimeTicks::Now();
    if (sleep_time < TimeDelta::FromSeconds(0)) {
      timed_out = true;
      break;
    }
    LOG(INFO) << "Waiting in stop for " << sleep_time.InSeconds() << "s.";
    vault_field_changed_.TimedWait(sleep_time);
  }

  if (timed_out) {
    LOG(ERROR) << "SyncerThread::Stop timed out or error. Problems likely.";
    return false;
  }

  // Stop() should not block on anything at this point, given above madness.
  DLOG(INFO) << "Calling SyncerThread::thread_.Stop() at "
             << Time::Now().ToInternalValue();
  thread_.Stop();
  DLOG(INFO) << "SyncerThread::thread_.Stop() finished at "
             << Time::Now().ToInternalValue();
  return true;
}

void SyncerThreadTimedStop::ThreadMain() {
  AutoLock lock(lock_);
  // Signal Start() to let it know we've made it safely are now running on the
  // message loop, and unblock it's caller.
  thread_main_started_.Signal();

  // The only thing that could be waiting on this value is Stop, and we don't
  // release the lock until we're far enough along to Stop safely.
  {
    AutoReset auto_reset_in_thread_main_loop(&in_thread_main_loop_, true);
    vault_field_changed_.Broadcast();
    ThreadMainLoop();
  }
  vault_field_changed_.Broadcast();
  LOG(INFO) << "Syncer thread ThreadMain is done.";
}

}  // namespace browser_sync
