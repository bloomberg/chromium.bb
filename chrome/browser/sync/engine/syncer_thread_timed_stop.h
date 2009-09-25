// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A class to run the syncer on a thread.  This guy is the closest chrome-based
// (as opposed to pthreads based) SyncerThread to the old pthread implementation
// in semantics, as it supports a timeout on Stop() -- It is just an override of
// two methods from SyncerThread: ThreadMain and Stop -- to provide this.
#ifndef CHROME_BROWSER_SYNC_ENGINE_SYNCER_THREAD_TIMED_STOP_H_
#define CHROME_BROWSER_SYNC_ENGINE_SYNCER_THREAD_TIMED_STOP_H_

#include <list>
#include <map>
#include <queue>
#include <vector>

#include "chrome/browser/sync/engine/syncer_thread.h"

namespace browser_sync {

class SyncerThreadTimedStop : public SyncerThread {
  FRIEND_TEST(SyncerThreadTest, CalculateSyncWaitTime);
  FRIEND_TEST(SyncerThreadTest, CalculatePollingWaitTime);
  FRIEND_TEST(SyncerThreadWithSyncerTest, Polling);
  FRIEND_TEST(SyncerThreadWithSyncerTest, Nudge);
  friend class SyncerThreadWithSyncerTest;
  friend class SyncerThreadFactory;
 public:
  virtual ~SyncerThreadTimedStop() {}

  // Stop processing.  This version comes with a supported max_wait.
  // A max wait of at least 2*server RTT time is recommended.
  // Returns true if we stopped, false otherwise.
  virtual bool Stop(int max_wait);

 private:
  SyncerThreadTimedStop(ClientCommandChannel* command_channel,
      syncable::DirectoryManager* mgr,
      ServerConnectionManager* connection_manager, AllStatus* all_status,
      ModelSafeWorker* model_safe_worker);
  virtual void ThreadMain();

  // We use this to track when our synthesized thread loop is active, so we can
  // timed-wait for it to become false.  For this and only this (temporary)
  // implementation, we protect this variable using our parent lock_.
  bool in_thread_main_loop_;

  DISALLOW_COPY_AND_ASSIGN(SyncerThreadTimedStop);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_ENGINE_SYNCER_THREAD_TIMED_STOP_H_