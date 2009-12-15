// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_NOTIFIER_BASE_MAC_NETWORK_STATUS_DETECTOR_TASK_MAC_H_
#define CHROME_BROWSER_SYNC_NOTIFIER_BASE_MAC_NETWORK_STATUS_DETECTOR_TASK_MAC_H_

#include <CoreFoundation/CoreFoundation.h>

#include "base/basictypes.h"
#include "base/condition_variable.h"
#include "base/lock.h"
#include "base/platform_thread.h"
#include "chrome/browser/sync/notifier/base/network_status_detector_task.h"
#include "talk/base/messagequeue.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

namespace talk_base {
class Message;
class Task;
class Thread;
}  // namespace talk_base

namespace notifier {

// The Mac OS X network status detector works as follows: a worker
// (Chrome platform) thread is spawned which simply sets up a Cocoa
// run loop and attaches the network reachability monitor to it.
// Whenever the network reachability changes, (e.g., changed wireless
// networks, unplugged ethernet cable) a callback on the worker
// thread is triggered which then tries to connect to a Google talk
// host (if the network is indicated to be up) and sends a message
// with the result to the parent thread.

class NetworkStatusDetectorTaskMac : public NetworkStatusDetectorTask,
                                     public PlatformThread::Delegate,
                                     public talk_base::MessageHandler {
 public:
  explicit NetworkStatusDetectorTaskMac(talk_base::Task* parent);

  virtual ~NetworkStatusDetectorTaskMac();

  // talk_base::Task functions (via NetworkStatusDetectorTask).
  virtual int ProcessStart();
  virtual void Stop();

  // talk_base::MessageHandler functions.
  // Currently OnMessage() simply calls SetNetworkAlive() with alive
  // set to true iff message->message_id is non-zero.
  virtual void OnMessage(talk_base::Message* message);

  // Only the following public functions are called from the worker
  // thread.

  // PlatformThread::Delegate functions.
  virtual void ThreadMain();

  // Called when network reachability changes.  network_active should
  // be set only when the network is currently active and connecting
  // to a host won't require any user intervention or create a network
  // connection (e.g., prompting for a password, causing a modem to
  // dial).
  void NetworkReachabilityChanged(bool network_active);

 private:
  enum WorkerThreadState {
    WORKER_THREAD_STOPPED = 1,
    WORKER_THREAD_RUNNING,
    WORKER_THREAD_ERROR,
  };

  // If thread_state => WORKER_THREAD_STOPPED:
  //
  //   thread_id => parent_thread_id_
  //   thread_run_loop  => NULL
  //   possible successor states =>
  //    { WORKER_THREAD_RUNNING, WORKER_THREAD_ERROR }
  //
  // If thread_state => WORKER_THREAD_RUNNING, the worker thread is
  // successfully running and will continue to run until Stop() is
  // called.
  //
  //   thread_id => id of worker thread (!= parent_thread_id_)
  //   thread_run_loop => reference to the worker thread's run loop
  //   possible successor states => { WORKER_THREAD_STOPPED }
  //
  // If thread_state => WORKER_THREAD_ERROR, the worker thread has
  // failed to start running and has stopped.  Join() must be still
  // called on the worker thread.
  //
  //   thread_id => id of worker thread (!= parent_thread_id_)
  //   thread_run_loop => NULL
  //   possible successor states => { WORKER_THREAD_STOPPED }
  //
  // Only the worker thread can change the state from
  // WORKER_THREAD_STOPPED to any other state and only the main thread
  // can change the state to WORKER_THREAD_STOPPED.
  struct WorkerInfo {
    WorkerThreadState thread_state;
    PlatformThreadId thread_id;
    CFRunLoopRef thread_run_loop;

    // This constructor sets thread_state to WORKER_THREAD_STOPPED
    // and thread_run_loop to NULL.
    explicit WorkerInfo(PlatformThreadId thread_id);

    WorkerInfo(WorkerThreadState thread_state,
               PlatformThreadId thread_id,
               CFRunLoopRef thread_run_loop);
  };

  // After this function is called, worker_shared_info_.thread_state
  // is guaranteed to be WORKER_THREAD_STOPPED and
  // network_reachability_ is guaranteed to be NULL.  Must be called
  // only from the parent thread without worker_lock_ being held.
  void ClearWorker();

  bool IsOnParentThread() const;
  // Acquires and releases worker_lock_.
  bool IsOnWorkerThread();

  // The thread ID of the thread that constructed this object.
  PlatformThreadId parent_thread_id_;
  // The libjingle thread object of the thread that constructed this
  // object.
  talk_base::Thread* parent_thread_;

  // The handle to the worker thread, or kNullThreadHandle if a worker
  // thread doesn't exist.
  PlatformThreadHandle worker_thread_;

  // This lock protects worker_shared_info_ when the worker
  // thread is running.
  Lock worker_lock_;

  // Signalled by the worker thread when
  // worker_shared_info_.thread_state moves from WORKER_THREAD_STOPPED
  // to another state.
  ConditionVariable worker_thread_not_stopped_;

  // Struct for everything that is shared between the parent and the
  // worker thread.
  WorkerInfo worker_shared_info_;

  FRIEND_TEST(NetworkStatusDetectorTaskMacTest, StartNoStopTest);
  FRIEND_TEST(NetworkStatusDetectorTaskMacTest, StartStopTest);

  DISALLOW_COPY_AND_ASSIGN(NetworkStatusDetectorTaskMac);
};

}  // namespace notifier

#endif  // CHROME_BROWSER_SYNC_NOTIFIER_BASE_MAC_NETWORK_STATUS_DETECTOR_TASK_MAC_H_

